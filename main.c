#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utgeojson.h"

typedef struct record_shp {
    int   num;
    int   len;
    int   shape_type;
    void *shape;
} RecordSHP;

typedef struct record_dbf {
    unsigned char  is_deleted;
    unsigned char *content;
} RecordDBF;

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

void       write_geojson(FILE *fp, Record *record_shp, Record *record_dbf,
                         Header *header_dbf, Field **array);
// SHP related
RecordSHP *parse_recordSHP(FILE *fp);
RecordDBF *parse_recordDBF(FILE *fp, Header *header);
void       record_free(Record *head, int shape_type);
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
    Record *record_shp = NULL;
    int     shape_type = parse_headerSHP(fshp);

    while (record_prepend(&record_shp, new_record(parse_recordSHP(fshp))) == 0)
        ;
    record_reverse(&record_shp);
    printf("Finished SHP\n");

    // DBF
    Field  *field;
    Record *record_dbf = NULL;
    int     i = 0;

    Header *header = parse_headerDBF(fdbf);
    int     num_fields = (header->header_size - 33) / 32;
    Field **array = malloc(sizeof(Field *) * num_fields);
    while ((field = parse_field(fdbf)) != NULL)
    {
        array[i] = field;
        i++;
    }

    while(record_prepend(&record_dbf,
          new_record(parse_recordDBF(fdbf, header))) == 0)
        ;
    record_reverse(&record_dbf);
    printf("Finished DBF\n");

    // Write GeoJSON
    write_geojson(fjs, record_shp, record_dbf, header, array);

    record_free(record_shp, shape_type);    // Free SHP
    record_free(record_dbf, 0);             // Free DBF
    for (i = 0; i < num_fields; i++)
        free(array[i]);
    free(array);
    free(header);

    fclose(fshp);
    fclose(fdbf);
    fclose(fjs);
    return 0;
}

// Write GeoJSON
void write_geojson(FILE *fp, Record *record_shp, Record *record_dbf,
                   Header *header_dbf, Field **array)
{
   char *name = "polyline";
   int   num_fields = (header_dbf->header_size - 33) / 32;
   int   num_rec = header_dbf->record_num;
   // int num_rec = 3;
   printf("num_fields %d, record_num %d\n", num_fields, header_dbf->record_num);
   int   i, j, k, ptr, field_len;
   char  type;
   Point     *point;
   PolyLine  *polyline;
   RecordSHP *shp;
   RecordDBF *dbf;

   // Head
   fprintf(fp, "{\n\"type\": \"FeatureCollection\",\n\"name\":\"%s\",\n\"feature\": [", name);

   // Print each record
   for (i = 0; i < num_rec; i++, record_shp = record_shp->next, record_dbf = record_dbf->next)
   {
       shp = record_shp->data;
       dbf = record_dbf->data;
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

RecordDBF *parse_recordDBF(FILE *fp, Header *header)
{
    int           size = header->record_size;
    RecordDBF    *rec = malloc(sizeof(RecordDBF));
    unsigned char record[size];

    int nitems = fread(record, sizeof(record), 1, fp);
    if (nitems != 1)     // EOF
        return NULL;

    rec->is_deleted = record[0];
    rec->content    = malloc(size - 1);
    for (int i = 1, j = 0; i < size; i++, j++)
        rec->content[j] = record[i];

    return rec;
}

void record_free(Record *head, int shape_type)
{
    RecordSHP *shp;
    RecordDBF *dbf;
    PolyLine  *polyline;

    for (Record *p = head; p != NULL; p = head)
    {
        head = head->next;
        switch (shape_type)
        {
            case 1:
            case 21:
                shp = p->data;
                free(shp->shape);
                free(shp);
                break;
            case 3:
            case 5:
                shp = p->data;
                polyline = shp->shape;
                free(polyline->points);
                free(polyline->parts);
                free(polyline);
                free(shp);
                break;
            default:
                dbf = p->data;
                free(dbf->content);
                free(dbf);
                break;
        }
        free(p);
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
