#include "ppsp/csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *duplicate_range(const char *start, size_t length) {
    char *value = (char *)malloc(length + 1);
    if (!value) return NULL;
    memcpy(value, start, length);
    value[length] = '\0';
    return value;
}

int csv_open(CsvReader *reader, const char *path) {
    FILE *file;
    if (!reader || !path) return 0;
    memset(reader, 0, sizeof(*reader));
#ifdef _MSC_VER
    if (fopen_s(&file, path, "rb") != 0) file = NULL;
#else
    file = fopen(path, "rb");
#endif
    if (!file) return 0;
    reader->file = file;
    reader->delimiter = ',';
    return 1;
}

static int read_record(CsvReader *reader) {
    FILE *file = (FILE *)reader->file;
    size_t length = 0;
    int quoted = 0;
    int ch;
    if (!reader->buffer) {
        reader->capacity = 4096;
        reader->buffer = (char *)malloc(reader->capacity);
        if (!reader->buffer) return -1;
    }
    while ((ch = fgetc(file)) != EOF) {
        if (length + 2 > reader->capacity) {
            size_t next_capacity = reader->capacity * 2;
            char *next = (char *)realloc(reader->buffer, next_capacity);
            if (!next) return -1;
            reader->buffer = next;
            reader->capacity = next_capacity;
        }
        reader->buffer[length++] = (char)ch;
        if (ch == '"') {
            int next = fgetc(file);
            if (quoted && next == '"') {
                reader->buffer[length++] = (char)next;
            } else {
                quoted = !quoted;
                if (next != EOF) ungetc(next, file);
            }
        }
        if (ch == '\n' && !quoted) break;
    }
    if (length == 0 && ch == EOF) return 0;
    while (length && (reader->buffer[length - 1] == '\n' || reader->buffer[length - 1] == '\r')) {
        --length;
    }
    reader->buffer[length] = '\0';
    ++reader->line_number;
    return 1;
}

int csv_read_row(CsvReader *reader, CsvRow *row) {
    const char *cursor;
    size_t count = 0, capacity = 16;
    char **fields;
    int status;
    if (!reader || !row || !reader->file) return -1;
    memset(row, 0, sizeof(*row));
    status = read_record(reader);
    if (status <= 0) return status;
    fields = (char **)calloc(capacity, sizeof(*fields));
    if (!fields) return -1;
    cursor = reader->buffer;
    for (;;) {
        char *field;
        if (count == capacity) {
            char **next;
            capacity *= 2;
            next = (char **)realloc(fields, capacity * sizeof(*fields));
            if (!next) goto fail;
            fields = next;
        }
        if (*cursor == '"') {
            const char *p = ++cursor;
            size_t out_length = 0;
            int closed = 0;
            char *out = (char *)malloc(strlen(cursor) + 1);
            if (!out) goto fail;
            while (*p) {
                if (*p == '"' && p[1] == '"') {
                    out[out_length++] = '"'; p += 2;
                } else if (*p == '"') {
                    ++p; closed = 1; break;
                } else {
                    out[out_length++] = *p++;
                }
            }
            if (!closed || (*p && *p != reader->delimiter)) {
                free(out);
                goto fail;
            }
            out[out_length] = '\0';
            field = out;
            cursor = p;
            if (*cursor == reader->delimiter) ++cursor;
        } else {
            const char *end = strchr(cursor, reader->delimiter);
            size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
            field = duplicate_range(cursor, length);
            if (!field) goto fail;
            cursor = end ? end + 1 : cursor + length;
        }
        fields[count++] = field;
        if (!*cursor) {
            size_t record_length = strlen(reader->buffer);
            if (record_length && reader->buffer[record_length - 1] == reader->delimiter) {
                if (count == capacity) {
                    char **next;
                    capacity *= 2;
                    next = (char **)realloc(fields, capacity * sizeof(*fields));
                    if (!next) goto fail;
                    fields = next;
                }
                fields[count] = duplicate_range("", 0);
                if (!fields[count]) goto fail;
                ++count;
            }
            break;
        }
    }
    row->fields = fields;
    row->count = count;
    return 1;
fail:
    while (count) free(fields[--count]);
    free(fields);
    return -1;
}

void csv_row_free(CsvRow *row) {
    size_t i;
    if (!row) return;
    for (i = 0; i < row->count; ++i) free(row->fields[i]);
    free(row->fields);
    memset(row, 0, sizeof(*row));
}

void csv_close(CsvReader *reader) {
    if (!reader) return;
    if (reader->file) fclose((FILE *)reader->file);
    free(reader->buffer);
    memset(reader, 0, sizeof(*reader));
}

int csv_column(const CsvRow *header, const char *name) {
    size_t i;
    if (!header || !name) return -1;
    for (i = 0; i < header->count; ++i) {
        const char *value = header->fields[i];
        if (i == 0 && (unsigned char)value[0] == 0xef &&
            (unsigned char)value[1] == 0xbb && (unsigned char)value[2] == 0xbf) value += 3;
        if (strcmp(value, name) == 0) return (int)i;
    }
    return -1;
}
