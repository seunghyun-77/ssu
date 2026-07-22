#include "ppsp/csv.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_text(const char *path, const char *text) {
    FILE *file = NULL;
#ifdef _MSC_VER
    assert(fopen_s(&file, path, "wb") == 0);
#else
    file = fopen(path, "wb");
#endif
    assert(file);
    assert(fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(fclose(file) == 0);
}

int main(void) {
    const char *path = "test_csv_input.csv";
    CsvReader reader;
    CsvRow row = {0};

    write_text(path, "\xef\xbb\xbfid,name,note,empty\r\n1,\"alpha,beta\",\"a\"\"b\",\r\n");
    assert(csv_open(&reader, path));
    assert(csv_read_row(&reader, &row) == 1);
    assert(row.count == 4);
    assert(csv_column(&row, "id") == 0);
    csv_row_free(&row);
    assert(csv_read_row(&reader, &row) == 1);
    assert(row.count == 4);
    assert(strcmp(row.fields[1], "alpha,beta") == 0);
    assert(strcmp(row.fields[2], "a\"b") == 0);
    assert(strcmp(row.fields[3], "") == 0);
    csv_row_free(&row);
    assert(csv_read_row(&reader, &row) == 0);
    csv_close(&reader);
    assert(remove(path) == 0);

    write_text(path, "id,name\n1,\"unclosed\n");
    assert(csv_open(&reader, path));
    assert(csv_read_row(&reader, &row) == 1);
    csv_row_free(&row);
    assert(csv_read_row(&reader, &row) == -1);
    csv_close(&reader);
    assert(remove(path) == 0);

    puts("csv tests passed");
    return 0;
}
