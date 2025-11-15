#include "dlx_binary.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_header_and_chunk(const char* path)
{
    FILE* file = fopen(path, "wb");
    assert(file != NULL);

    struct DlxCoverHeader header = {
        .magic = DLX_COVER_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0x2,
        .column_count = 10,
        .row_count = 2,
        .option_node_count = 6,
    };

    assert(dlx_write_cover_header(file, &header) == 0);

    uint32_t row1[] = {1, 5, 9};
    uint32_t row2[] = {0, 4, 8};

    assert(dlx_write_row_chunk(file, 1, row1, 3) == 0);
    assert(dlx_write_row_chunk(file, 2, row2, 3) == 0);
    fclose(file);
}

static void test_round_trip(void)
{
    char file_template[] = "tests/tmp_binaryXXXXXX";
    int fd = mkstemp(file_template);
    assert(fd != -1);
    close(fd);

    write_header_and_chunk(file_template);

    FILE* file = fopen(file_template, "rb");
    assert(file != NULL);

    struct DlxCoverHeader header;
    assert(dlx_read_cover_header(file, &header) == 0);
    assert(header.magic == DLX_COVER_MAGIC);
    assert(header.version == DLX_BINARY_VERSION);
    assert(header.flags == 0x2);
    assert(header.column_count == 10);
    assert(header.row_count == 2);
    assert(header.option_node_count == 6);

    struct DlxRowChunk chunk = {0};
    int status = dlx_read_row_chunk(file, &chunk);
    assert(status == 1);
    assert(chunk.row_id == 1);
    assert(chunk.entry_count == 3);
    assert(chunk.columns[0] == 1);
    assert(chunk.columns[1] == 5);
    assert(chunk.columns[2] == 9);

    status = dlx_read_row_chunk(file, &chunk);
    assert(status == 1);
    assert(chunk.row_id == 2);
    assert(chunk.entry_count == 3);
    assert(chunk.columns[0] == 0);
    assert(chunk.columns[1] == 4);
    assert(chunk.columns[2] == 8);

    status = dlx_read_row_chunk(file, &chunk);
    assert(status == 0); // EOF

    dlx_free_row_chunk(&chunk);
    fclose(file);
    remove(file_template);
}

int main(void)
{
    test_round_trip();
    printf("dlx_binary tests passed\n");
    return 0;
}
