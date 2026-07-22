#ifndef PPSP_CSV_H
#define PPSP_CSV_H

#include <stddef.h>

typedef struct {
    char **fields;
    size_t count;
} CsvRow;

typedef struct {
    void *file;
    char *buffer;
    size_t capacity;
    size_t line_number;
    char delimiter;
} CsvReader;

int csv_open(CsvReader *reader, const char *path);
int csv_read_row(CsvReader *reader, CsvRow *row);
void csv_row_free(CsvRow *row);
void csv_close(CsvReader *reader);
int csv_column(const CsvRow *header, const char *name);

#endif

