#ifndef __UTGEOJSON_H
#define __UTGEOJSON_H

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
    unsigned char      is_deleted;
    unsigned char     *content;
    struct record_dbf *next;
} RecordDBF;

// SHP functions
int       parse_headerSHP(FILE *fp);
int       recordSHP_prepend(RecordSHP **p_head, RecordSHP *new_record);
void      recordSHP_reverse(RecordSHP **p_head);
int       recordSHP_length(RecordSHP *head);
void      recordSHP_nth_print(RecordSHP *head, int n);

// DBF functions
HeaderDBF *parse_headerDBF(FILE *fp);
FieldDBF  *parse_field(FILE *fp);
RecordDBF *parse_recordDBF(FILE *fp, HeaderDBF *header);
void       recordDBF_nth_print(RecordDBF *head, HeaderDBF *header, FieldDBF **array, int n);
int        recordDBF_prepend(RecordDBF **p_head, RecordDBF *new_record);
void       recordDBF_reverse(RecordDBF **p_head);
void       recordDBF_free(RecordDBF *head);

// Common functions
int        *parse_parts(unsigned char *buf, int num_parts);
void        parse_int32(unsigned char *buf, int *p, int n, int big_endian);
void        parse_int16(unsigned char *buf, short *p, int n);
void        parse_double(unsigned char *buf, double *p, int n);
void        parse_string(unsigned char *buf, char *string);
const char *shape_type(int x);

#endif
