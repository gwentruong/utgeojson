#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utgeojson.h"

typedef struct point {
    double x;
    double y;
} Point;

typedef struct polyline {
    double  box[4];
    int     num_parts;
    int     num_points;
    int    *parts;
    Point  *points;
} PolyLine;

void       write_geojson(FILE *fp, RecordSHP *shp, RecordDBF *dbf,
                         HeaderDBF *head_dbf, FieldDBF **array);
// SHP related
RecordSHP *parse_recordSHP(FILE *fp);
void       recordSHP_free(RecordSHP *head, int shape_type);
PolyLine  *parse_polyline(unsigned char *buf);
Point     *parse_points(unsigned char *buf, int num_points);

int main(void)
{
    FILE *fshp = fopen("./sample/polyline.shp", "rb");
    FILE *fdbf = fopen("./sample/polyline.dbf", "rb");
    FILE *fjs  = fopen("polyline.geojson", "w");
    if ((fshp == NULL) || (fdbf == NULL))
    {
        printf("Filename not found\n");
        return -1;
    }

    // SHP
    RecordSHP *record_shp = NULL;
    int        shape_type = parse_headerSHP(fshp);

    while (recordSHP_prepend(&record_shp, parse_recordSHP(fshp)) == 0)
        ;
    recordSHP_reverse(&record_shp);
    printf("Finished SHP\n");

    // DBF
    FieldDBF  *field;
    RecordDBF *record_dbf = NULL;
    int        i = 0;

    HeaderDBF *header = parse_headerDBF(fdbf);
    int        num_fields = (header->header_size - 33) / 32;
    FieldDBF **array = malloc(sizeof(FieldDBF *) * num_fields);
    while ((field = parse_field(fdbf)) != NULL)
    {
        array[i] = field;
        i++;
    }

    while(recordDBF_prepend(&record_dbf, parse_recordDBF(fdbf, header)) == 0)
        ;
    recordDBF_reverse(&record_dbf);
    printf("Finished DBF\n");

    // Write GeoJSON
    write_geojson(fjs, record_shp, record_dbf, header, array);

    // End SHP
    recordSHP_free(record_shp, shape_type);
    // End DBF
    recordDBF_free(record_dbf);
    for (i = 0; i < num_fields; i++)
        free(array[i]);
    free(array);
    free(header);
    // End
    fclose(fshp);
    fclose(fdbf);
    fclose(fjs);
    return 0;
}

// Write GeoJSON
void write_geojson(FILE *fp, RecordSHP *shp, RecordDBF *dbf, HeaderDBF *header_dbf, FieldDBF **array)
{
   char *name = "polyline";
   int   num_fields = (header_dbf->header_size - 33) / 32;
   int   num_rec = header_dbf->record_num;
   // int num_rec = 3;
   printf("num_fields %d, record_num %d\n", num_fields, header_dbf->record_num);
   int   i, j, k, ptr, field_len;
   char  type;
   Point    *point;
   PolyLine *polyline;

   // Head
   fprintf(fp, "{\n\"type\": \"FeatureCollection\",\n\"name\":\"%s\",\n\"feature\": [", name);

   // Print each record
   for (i = 0; i < num_rec; i++, shp = shp->next, dbf = dbf->next)
   {
       field_len = 0;
       ptr       = 0;
       fprintf(fp, "\n{\"type\": \"Feature\", \"properties\": {");

       for (j = 0; j < num_fields; j++)            // Print dbf-record
       {
           type       = array[j]->type;
           field_len += array[j]->len;
           fprintf(fp, "\"%s\": ", array[j]->name);

           switch (type)
           {
               case 'N':
                   for (k = ptr; k < field_len; k++)
                   {
                       if (dbf->content[k] != 0x20)
                           fprintf(fp, "%c", dbf->content[k]);
                       ptr++;
                   }
                   if (k != (header_dbf->record_size - 1))
                       fprintf(fp, ", ");
                   else
                       fprintf(fp, "}, ");
                   break;
               default:
                   printf("Unknown data types\n");
                   break;
           }
       }

       fprintf(fp, "\"geometry\": {\"type\": \"%s\", \"coordinates\": [ [ ",
               shape_type(shp->shape_type));
       switch (shp->shape_type)
       {
           case 1:
               point = shp->shape;
               fprintf(fp, "[%.3f, %.3f]", point->x, point->y);
               break;
           case 3:
           case 5:
               polyline = shp->shape;
               for (j = 0; j < polyline->num_points; j++)
               {
                   fprintf(fp, "[%.3f, %.3f]", polyline->points[j].x,
                                               polyline->points[j].y);
                   if (j != polyline->num_points - 1)
                       fprintf(fp, ", ");
               }
               break;
       }
       fprintf(fp, " ] ] }");
       if (i != num_rec - 1)
           fprintf(fp, ", ");
       else
           fprintf(fp, "\n}");
   }
}

RecordSHP *parse_recordSHP(FILE *fp)
{
   unsigned char header[8];
   int           num_len[2];

   int nitems = fread(header, sizeof(header), 1, fp);
   if (nitems != 1)
       return NULL;

   parse_int32(header, num_len, 2, 1);             // Parse record number, length

   unsigned char content[num_len[1] * 2];
   int           shape_type;

   fread(content, sizeof(content), 1, fp);         // Read content of record
   parse_int32(content, &shape_type, 1, 0);        // Parse shapetype

   RecordSHP *record  = malloc(sizeof(RecordSHP));
   record->num        = num_len[0];
   record->len        = num_len[1] * 2;
   record->shape_type = shape_type;
   record->next       = NULL;

   switch (shape_type)
   {
       case 1:
           record->shape = parse_points(content + 4, 1);   // Parse Point
           break;
       case 3:
       case 5:                                             // Parse Polygon
           record->shape = parse_polyline(content + 4);    // Parse PolyLine
           break;
       default:
           printf("Unknown shape type\n");
           break;
   }

   return record;
}

void recordSHP_free(RecordSHP *head, int shape_type)
{
    PolyLine *polyline;

    for (RecordSHP *p = head; p != NULL; p = head)
    {
        head = head->next;
        switch (shape_type)
        {
            case 1:
            case 21:
                free(p->shape);
                free(p);
                break;
            case 3:
            case 5:
                polyline = p->shape;
                free(polyline->points);
                free(polyline->parts);
                free(polyline);
                free(p);
                break;
        }
    }
}

PolyLine *parse_polyline(unsigned char *buf)
{
    PolyLine *polyline = malloc(sizeof(PolyLine));
    double    box[4];
    int       parts_points[2];
    int       i;

    parse_double(buf, box, 4);
    parse_int32(buf + 32, parts_points, 2, 0);

    for (i = 0; i < 4; i++)
        polyline->box[i] = box[i];
    polyline->num_parts  = parts_points[0];
    polyline->num_points = parts_points[1];
    polyline->parts      = parse_parts(buf + 40, polyline->num_parts);
    polyline->points     = parse_points(buf + 40 + (4 * polyline->num_parts),
                                        polyline->num_points);

    return polyline;
}

Point *parse_points(unsigned char *buf, int num_points)
{
    Point *points = malloc(sizeof(Point) * num_points);
#if 1
    // Almost safe. Not exactly portable: only works on 64-bit machines
    parse_double(buf, (double *)points, num_points * 2);
#else
    for (int i = 0; i < num_points; i++, buf += 16)
    {
        double xy[2];
        parse_double(buf, xy, 2);
        points[i].x = xy[0];
        points[i].y = xy[1];
    }
#endif
    return points;
}
