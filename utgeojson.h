#ifndef __UTGEOJSON_H
#define __UTGEOJSON_H

typedef struct record {
    void          *data;
    struct record *next;
} Record;

typedef struct header_dbf {
    int   record_num;
    short header_size;
    short record_size;
} Header;

typedef struct field_dbf {
    char          name[12];
    char          type;
    unsigned char len;
} Field;

int     parse_headerSHP(FILE *fp);  // Parse SHP header
Header *parse_headerDBF(FILE *fp);  // Parse DBF header
Field  *parse_field(FILE *fp);      // Parse DBF field
Record *new_record(void *data);

// Common functions
int         record_prepend(Record **p_head, Record *new_record);
void        record_reverse(Record **p_head);
int         record_length(Record *head);
int        *parse_parts(unsigned char *buf, int num_parts);
void        parse_int32(unsigned char *buf, int *p, int n, int big_endian);
void        parse_int16(unsigned char *buf, short *p, int n);
void        parse_double(unsigned char *buf, double *p, int n);
void        parse_string(unsigned char *buf, char *string);
const char *shape_type(int x);

#endif
