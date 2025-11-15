#include "dlx_binary.h"
#include <gtest/gtest.h>
#include <unistd.h>

namespace
{

void write_header_and_chunk(const char* path)
{
    FILE* file = fopen(path, "wb");
    ASSERT_NE(file, nullptr);

    struct DlxCoverHeader header = {
        .magic = DLX_COVER_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0x2,
        .column_count = 10,
        .row_count = 2,
        .option_node_count = 6,
    };

    ASSERT_EQ(dlx_write_cover_header(file, &header), 0);

    uint32_t row1[] = {1, 5, 9};
    uint32_t row2[] = {0, 4, 8};

    ASSERT_EQ(dlx_write_row_chunk(file, 1, row1, 3), 0);
    ASSERT_EQ(dlx_write_row_chunk(file, 2, row2, 3), 0);
    fclose(file);
}

TEST(DlxBinaryTest, RoundTrip)
{
    char file_template[] = "tests/tmp_binaryXXXXXX";
    int fd = mkstemp(file_template);
    ASSERT_NE(fd, -1);
    close(fd);

    write_header_and_chunk(file_template);

    FILE* file = fopen(file_template, "rb");
    ASSERT_NE(file, nullptr);

    struct DlxCoverHeader header;
    ASSERT_EQ(dlx_read_cover_header(file, &header), 0);
    EXPECT_EQ(header.magic, DLX_COVER_MAGIC);
    EXPECT_EQ(header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(header.flags, 0x2);
    EXPECT_EQ(header.column_count, 10u);
    EXPECT_EQ(header.row_count, 2u);
    EXPECT_EQ(header.option_node_count, 6u);

    struct DlxRowChunk chunk = {0};
    int status = dlx_read_row_chunk(file, &chunk);
    ASSERT_EQ(status, 1);
    EXPECT_EQ(chunk.row_id, 1u);
    EXPECT_EQ(chunk.entry_count, 3);
    EXPECT_EQ(chunk.columns[0], 1u);
    EXPECT_EQ(chunk.columns[1], 5u);
    EXPECT_EQ(chunk.columns[2], 9u);

    status = dlx_read_row_chunk(file, &chunk);
    ASSERT_EQ(status, 1);
    EXPECT_EQ(chunk.row_id, 2u);
    EXPECT_EQ(chunk.entry_count, 3);
    EXPECT_EQ(chunk.columns[0], 0u);
    EXPECT_EQ(chunk.columns[1], 4u);
    EXPECT_EQ(chunk.columns[2], 8u);

    EXPECT_EQ(dlx_read_row_chunk(file, &chunk), 0); // EOF

    dlx_free_row_chunk(&chunk);
    fclose(file);
    remove(file_template);
}

} // namespace
