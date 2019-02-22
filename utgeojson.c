#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct record_shp {
    int            num;
    int            len;
    int            shape_type;
    void          *shape;
    struct record_shp *next;
} RecordSHP;

typedef struct header_dbf {
    int   record_num;
    short header_size;
    short record_size;
} HeaderDBF;

typedef struct field_dbf {
    char          name[12];
    char          type;
    unsigned char len;
} FieldDBF;

typedef struct record_dbf {
    unsigned char  is_deleted;
    unsigned char *content;
    struct record_dbf *next;
} RecordDBF;

void write_geojson(FILE *fp, RecordSHP *shp, RecordDBF *dbf, HeaderDBF *head_dbf, FieldDBF **array);

// SHP functions
int       parse_headerSHP(FILE *fp);
RecordSHP *parse_recordSHP(FILE *fp);
int       recordSHP_prepend(RecordSHP **p_head, RecordSHP *new_record);
void      recordSHP_reverse(RecordSHP **p_head);
int       recordSHP_length(RecordSHP *head);
void      recordSHP_nth_print(RecordSHP *head, int n);
void      recordSHP_free(RecordSHP *head, int shape_type);

// DBF functions
HeaderDBF *parse_headerDBF(FILE *fp);
FieldDBF  *parse_field(FILE *fp);
RecordDBF *parse_recordDBF(FILE *fp, HeaderDBF *header);
void       recordDBF_nth_print(RecordDBF *head, HeaderDBF *header, FieldDBF **array, int n);
int        recordDBF_prepend(RecordDBF **p_head, RecordDBF *new_record);
void       recordDBF_reverse(RecordDBF **p_head);
void       recordDBF_free(RecordDBF *head);

// Common functions
PolyLine   *parse_polyline(unsigned char *buf);
Point      *parse_points(unsigned char *buf, int num_points);
int        *parse_parts(unsigned char *buf, int num_parts);
void        parse_int32(unsigned char *buf, int *p, int n, int big_endian);
void        parse_int16(unsigned char *buf, short *p, int n);
void        parse_double(unsigned char *buf, double *p, int n);
void        parse_string(unsigned char *buf, char *string);
const char *shape_type(int x);

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
        fprintf(fp, "{\n\"type\": \"Feature\", \"properties\": {");

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
// SHP
int parse_headerSHP(FILE *fp)
{
    unsigned char header[100];
    int           code;
    int           length;
    int           vs[2];
    double        ranges[8];

    fread(header, sizeof(header), 1, fp);       // Read header
    parse_int32(header, &code, 1, 1);           // Parse file code, int
    parse_int32(header + 24, &length, 1, 1);    // Parse file length, int
    parse_int32(header + 28, vs, 2, 0);         // Parse version, shapetype
    parse_double(header + 36, ranges, 8);       // Parse boundary ranges

    return vs[1];
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

int recordSHP_prepend(RecordSHP **p_head, RecordSHP *new_record)
{
    if (new_record == NULL)
        return -1;

    new_record->next = *p_head;
    *p_head = new_record;

    return 0;
}

void recordSHP_reverse(RecordSHP **p_head)
{
    RecordSHP *prev = NULL;
    RecordSHP *current = *p_head;
    RecordSHP *next;

    while (current != NULL)
    {
        next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }

    *p_head = prev;
}

int recordSHP_length(RecordSHP *head)
{
    int len = 0;

    for (RecordSHP *p = head; p != NULL; p = p->next)
        len++;

    return len;
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

// DBF
HeaderDBF *parse_headerDBF(FILE *fp)
{
    HeaderDBF    *head = malloc(sizeof(HeaderDBF));
    unsigned char header[32];
    int           num;
    short         head_record[2];

    fread(header, sizeof(header), 1, fp);
    parse_int32(header + 4, &num, 1, 0);         // Parse num of records
    parse_int16(header + 8, head_record, 2);    // Parse num of bytes in header & record

    head->record_num  = num;
    head->header_size = head_record[0];
    head->record_size = head_record[1];

    return head;
}

FieldDBF *parse_field(FILE *fp)
{
    FieldDBF     *fd = malloc(sizeof(FieldDBF));
    unsigned char field[32];
    unsigned char name[12];

    fread(field, 1, 1, fp);
    if (field[0] == 0x0D)
        return NULL;

    fread(field + 1, 31, 1, fp);

    fd->type = field[11];
    fd->len  = field[16];

    for (int i = 0; i < 11; i++)
        name[i] = field[i];
    parse_string(name, fd->name);

    return fd;
}

RecordDBF *parse_recordDBF(FILE *fp, HeaderDBF *header)
{
    int           size = header->record_size;
    RecordDBF    *rec = malloc(sizeof(RecordDBF));
    unsigned char record[size];

    int nitems = fread(record, sizeof(record), 1, fp);
    if (nitems != 1)     // EOF
        return NULL;

    rec->is_deleted = record[0];
    rec->next       = NULL;

    rec->content    = malloc(size - 1);
    for (int i = 1, j = 0; i < size; i++, j++)
        rec->content[j] = record[i];

    return rec;
}

int recordDBF_prepend(RecordDBF **p_head, RecordDBF *new_record)
{
    if (new_record == NULL)
        return -1;

    new_record->next = *p_head;
    *p_head = new_record;

    return 0;
}

void recordDBF_reverse(RecordDBF **p_head)
{
    RecordDBF *prev = NULL;
    RecordDBF *current = *p_head;
    RecordDBF *next;

    while (current != NULL)
    {
        next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }

    *p_head = prev;
}

void recordDBF_free(RecordDBF *head)
{
    for (RecordDBF *p = head; p != NULL; p = head)
    {
        head = head->next;
        free(p->content);
        free(p);
    }
}

// Common
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

int *parse_parts(unsigned char *buf, int num_parts)
{
    int *parts = malloc(sizeof(int) * num_parts);
    parse_int32(buf, parts, num_parts, 0);

    return parts;
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

void parse_int32(unsigned char *buf, int *p, int n, int big_endian)
{
    if (big_endian == 0)
    {
        int *q = (int *)buf;
        for (int i = 0; i < n; i++, q++)
            p[i] = *q;
    }
    else
    {
        for (int k = 0; k < n; k++, buf += 4)
        {
            unsigned char b[4];
            for (int i = 0, j = 3; i < 4; i++, j--)
                b[j] = buf[i];
            int *q = (int *)b;
            p[k] = *q;
        }
    }
}

void parse_int16(unsigned char *buf, short *p, int n)
{
    short *q = (short *)buf;
    for (int i = 0; i < n; i++, q++)
        p[i] = *q;
}

void parse_string(unsigned char *buf, char *string)
{
    char *str = (char *)buf;
    for(int i = 0; i < strlen(str); i++)
        string[i] = str[i];
}

void parse_double(unsigned char *buf, double *p, int n)
{
    double *q = (double *)buf;
    for (int i = 0; i < n; i++, q++)
        p[i] = *q;
}

const char *shape_type(int type)
{
    switch (type)
    {
        case 0:
            return "Null shape";
        case 1:
            return "Point";
        case 3:
            return "MultiLineString";
        case 5:
            return "Polygon";
        case 8:
            return "MultiPoint";
        case 11:
            return "PointZ";
        case 13:
            return "PolylineZ";
        case 15:
            return "PolygonZ";
        case 18:
            return "MultiPointZ";
        case 21:
            return "PointM";
        case 23:
            return "PolylineM";
        case 25:
            return "PolygonM";
        case 28:
            return "MultiPointM";
        case 31:
            return "MultiPatch";
        default:
            return "Unknown shape type";
    }
}
