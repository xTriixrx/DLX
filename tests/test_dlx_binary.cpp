#include "core/dlx.h"
#include "core/dlx_binary.h"
#include "sudoku/encoder/sudoku_encoder.h"
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>
#include <vector>

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

std::vector<uint32_t> parse_row_list(const char* rows)
{
    std::vector<uint32_t> values;
    const char* cursor = rows;
    while (*cursor != '\0')
    {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
        {
            cursor++;
        }
        if (*cursor == '\0')
        {
            break;
        }

        char* endptr = nullptr;
        long value = strtol(cursor, &endptr, 10);
        if (endptr == cursor || value <= 0)
        {
            break;
        }

        values.push_back(static_cast<uint32_t>(value));
        cursor = endptr;
    }

    return values;
}

TEST(DlxBinaryTest, SolutionRoundTrip)
{
    char file_template[] = "tests/tmp_solutionXXXXXX";
    int fd = mkstemp(file_template);
    ASSERT_NE(fd, -1);
    close(fd);

    FILE* file = fopen(file_template, "wb");
    ASSERT_NE(file, nullptr);

    struct DlxSolutionHeader header = {
        .magic = DLX_SOLUTION_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = 10,
    };

    ASSERT_EQ(dlx_write_solution_header(file, &header), 0);

    uint32_t rows[] = {10, 20, 30, 40};
    ASSERT_EQ(dlx_write_solution_row(file, 7, rows, 4), 0);
    fclose(file);

    file = fopen(file_template, "rb");
    ASSERT_NE(file, nullptr);

    struct DlxSolutionHeader read_header;
    ASSERT_EQ(dlx_read_solution_header(file, &read_header), 0);
    EXPECT_EQ(read_header.magic, DLX_SOLUTION_MAGIC);
    EXPECT_EQ(read_header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(read_header.flags, 0);
    EXPECT_EQ(read_header.column_count, 10u);

    struct DlxSolutionRow row = {0};
    ASSERT_EQ(dlx_read_solution_row(file, &row), 1);
    EXPECT_EQ(row.solution_id, 7u);
    ASSERT_EQ(row.entry_count, 4);
    for (int i = 0; i < row.entry_count; i++)
    {
        EXPECT_EQ(row.row_indices[i], rows[i]);
    }

    EXPECT_EQ(dlx_read_solution_row(file, &row), 0);

    dlx_free_solution_row(&row);
    fclose(file);
    remove(file_template);
}

TEST(DlxBinaryTest, DlxSolvesFromBinaryCoverAndEmitsBinarySolutions)
{
    const char* expected_rows =
        "1 2 8 24 31 32 33 47 48 60 64 75 87 88 95 96 89 97 103 93 99 104 105 113 "
        "73 114 124 128 138 52 53 7 12 45 50 58 63 79 76 67 71 83 106 109 116 119 "
        "34 40 17 16 21 5 27 28 44 122 127 136 140 129 141 142 143 148 151 152 153 "
        "154 156 157 144 158 161 164 170 171 175 177 178 182 183\n";

    char cover_template[] = "tests/tmp_binary_coverXXXXXX";
    int cover_fd = mkstemp(cover_template);
    ASSERT_NE(cover_fd, -1);
    close(cover_fd);

    ASSERT_EQ(convert_sudoku_to_cover("tests/sudoku_test.txt", cover_template, true), 0);

    FILE* cover = fopen(cover_template, "rb");
    ASSERT_NE(cover, nullptr);

    char** titles = NULL;
    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;
    struct node* matrix = dlx_read_binary(cover, &titles, &solutions, &itemCount, &optionCount);
    fclose(cover);
    ASSERT_NE(matrix, nullptr);
    ASSERT_NE(titles, nullptr);
    ASSERT_NE(solutions, nullptr);
    ASSERT_GT(itemCount, 0);
    ASSERT_GT(optionCount, 0);

    FILE* binary_output = tmpfile();
    ASSERT_NE(binary_output, nullptr);
    ASSERT_EQ(dlx::Core::dlx_enable_binary_solution_output(binary_output, static_cast<uint32_t>(itemCount)), 0);

    testing::internal::CaptureStdout();
    dlx::Core::search(matrix, 0, solutions, NULL);
    dlx::Core::dlx_disable_binary_solution_output();
    std::string stdout_capture = testing::internal::GetCapturedStdout();
    EXPECT_EQ(stdout_capture, expected_rows);

    fflush(binary_output);
    rewind(binary_output);

    struct DlxSolutionHeader solution_header;
    ASSERT_EQ(dlx_read_solution_header(binary_output, &solution_header), 0);
    EXPECT_EQ(solution_header.magic, DLX_SOLUTION_MAGIC);
    EXPECT_EQ(solution_header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(solution_header.column_count, static_cast<uint32_t>(itemCount));

    struct DlxSolutionRow row = {0};
    ASSERT_EQ(dlx_read_solution_row(binary_output, &row), 1);
    std::vector<uint32_t> expected_values = parse_row_list(expected_rows);
    ASSERT_EQ(row.entry_count, expected_values.size());
    for (size_t i = 0; i < expected_values.size(); i++)
    {
        EXPECT_EQ(row.row_indices[i], expected_values[i]);
    }
    EXPECT_EQ(dlx_read_solution_row(binary_output, &row), 0);

    dlx_free_solution_row(&row);
    fclose(binary_output);
    dlx::Core::freeMemory(matrix, solutions, titles, itemCount);
    remove(cover_template);
}

} // namespace
