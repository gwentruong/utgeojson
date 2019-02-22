#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utgeojson.h"

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
int *parse_parts(unsigned char *buf, int num_parts)
{
    int *parts = malloc(sizeof(int) * num_parts);
    parse_int32(buf, parts, num_parts, 0);

    return parts;
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
