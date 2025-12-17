#include "core/dlx.h"
#include "core/binary.h"
#include "sudoku/encoder/encoder.h"
#include "ascii_binary_utils.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

constexpr char kExpectedSudokuRows[] =
    "1 2 8 24 31 32 33 47 48 60 64 75 87 88 95 96 89 97 103 93 99 104 105 113 "
    "73 114 124 128 138 52 53 7 12 45 50 58 63 79 76 67 71 83 106 109 116 119 "
    "34 40 17 16 21 5 27 28 44 122 127 136 140 129 141 142 143 148 151 152 153 "
    "154 156 157 144 158 161 164 170 171 175 177 178 182 183\n";

std::string read_file_to_string(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string read_file_stream(FILE* file)
{
    if (file == nullptr)
    {
        return {};
    }

    std::string data;
    std::vector<char> buffer(4096);
    size_t bytes = 0;
    while ((bytes = fread(buffer.data(), 1, buffer.size(), file)) > 0)
    {
        data.append(buffer.data(), static_cast<size_t>(bytes));
    }
    return data;
}

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

    std::ifstream cover_stream(file_template, std::ios::binary);
    ASSERT_TRUE(cover_stream.is_open());

    struct DlxCoverHeader header;
    ASSERT_EQ(dlx_read_cover_header(cover_stream, &header), 0);
    EXPECT_EQ(header.magic, DLX_COVER_MAGIC);
    EXPECT_EQ(header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(header.flags, 0x2);
    EXPECT_EQ(header.column_count, 10u);
    EXPECT_EQ(header.row_count, 2u);

    struct DlxRowChunk chunk = {0};
    int status = dlx_read_row_chunk(cover_stream, &chunk);
    ASSERT_EQ(status, 1);
    EXPECT_EQ(chunk.row_id, 1u);
    EXPECT_EQ(chunk.entry_count, 3);
    EXPECT_EQ(chunk.columns[0], 1u);
    EXPECT_EQ(chunk.columns[1], 5u);
    EXPECT_EQ(chunk.columns[2], 9u);

    status = dlx_read_row_chunk(cover_stream, &chunk);
    ASSERT_EQ(status, 1);
    EXPECT_EQ(chunk.row_id, 2u);
    EXPECT_EQ(chunk.entry_count, 3);
    EXPECT_EQ(chunk.columns[0], 0u);
    EXPECT_EQ(chunk.columns[1], 4u);
    EXPECT_EQ(chunk.columns[2], 8u);

    EXPECT_EQ(dlx_read_row_chunk(cover_stream, &chunk), 0); // EOF

    dlx_free_row_chunk(&chunk);
    cover_stream.close();
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

    std::ifstream solution_stream(file_template, std::ios::binary);
    ASSERT_TRUE(solution_stream.is_open());

    struct DlxSolutionHeader read_header;
    ASSERT_EQ(dlx_read_solution_header(solution_stream, &read_header), 0);
    EXPECT_EQ(read_header.magic, DLX_SOLUTION_MAGIC);
    EXPECT_EQ(read_header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(read_header.flags, 0);
    EXPECT_EQ(read_header.column_count, 10u);

    struct DlxSolutionRow row = {0};
    ASSERT_EQ(dlx_read_solution_row(solution_stream, &row), 1);
    EXPECT_EQ(row.solution_id, 7u);
    ASSERT_EQ(row.entry_count, 4);
    for (int i = 0; i < row.entry_count; i++)
    {
        EXPECT_EQ(row.row_indices[i], rows[i]);
    }

    EXPECT_EQ(dlx_read_solution_row(solution_stream, &row), 0);

    dlx_free_solution_row(&row);
    solution_stream.close();
    remove(file_template);
}

TEST(DlxBinaryTest, DlxSolvesFromBinaryCoverAndEmitsBinarySolutions)
{
    char cover_template[] = "tests/tmp_binary_coverXXXXXX";
    int cover_fd = mkstemp(cover_template);
    ASSERT_NE(cover_fd, -1);
    close(cover_fd);

    ASSERT_EQ(convert_sudoku_to_cover("tests/sudoku_test.txt", cover_template), 0);

    std::ifstream cover(cover_template, std::ios::binary);
    ASSERT_TRUE(cover.is_open());

    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;
    struct node* matrix = dlx_read_binary(cover, &solutions, &itemCount, &optionCount);
    cover.close();
    ASSERT_NE(matrix, nullptr);
    ASSERT_NE(solutions, nullptr);
    ASSERT_GT(itemCount, 0);
    ASSERT_GT(optionCount, 0);

    uint32_t* row_ids = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * optionCount));
    ASSERT_NE(row_ids, nullptr);

    FILE* binary_output = tmpfile();
    ASSERT_NE(binary_output, nullptr);
    dlx::SolutionOutput output_ctx;
    ASSERT_EQ(dlx::Core::dlx_enable_binary_solution_output(output_ctx, binary_output, static_cast<uint32_t>(itemCount)), 0);

    testing::internal::CaptureStdout();
    dlx::Core::search(matrix, 0, solutions, row_ids, output_ctx);
    dlx::Core::dlx_disable_binary_solution_output(output_ctx);
    std::string stdout_capture = testing::internal::GetCapturedStdout();
    EXPECT_EQ(stdout_capture, kExpectedSudokuRows);

    fflush(binary_output);
    rewind(binary_output);

    std::string binary_data = read_file_stream(binary_output);
    std::istringstream binary_stream(binary_data);

    struct DlxSolutionHeader solution_header;
    ASSERT_EQ(dlx_read_solution_header(binary_stream, &solution_header), 0);
    EXPECT_EQ(solution_header.magic, DLX_SOLUTION_MAGIC);
    EXPECT_EQ(solution_header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(solution_header.column_count, static_cast<uint32_t>(itemCount));

    struct DlxSolutionRow row = {0};
    ASSERT_EQ(dlx_read_solution_row(binary_stream, &row), 1);
    std::vector<uint32_t> expected_values = parse_row_list(kExpectedSudokuRows);
    ASSERT_EQ(row.entry_count, expected_values.size());
    for (size_t i = 0; i < expected_values.size(); i++)
    {
        EXPECT_EQ(row.row_indices[i], expected_values[i]);
    }
    EXPECT_EQ(dlx_read_solution_row(binary_stream, &row), 0);

    dlx_free_solution_row(&row);
    fclose(binary_output);
    free(row_ids);
    dlx::Core::freeMemory(matrix, solutions);
    remove(cover_template);
}

TEST(DlxBinaryTest, AsciiCoverProducesBinarySolutionMatchingExpectedRows)
{
    std::string ascii_cover = read_file_to_string("tests/sudoku_cover.txt");
    ASSERT_FALSE(ascii_cover.empty());

    FILE* cover_stream_file = tmpfile();
    ASSERT_NE(cover_stream_file, nullptr);
    ASSERT_EQ(ascii_cover_to_binary_stream(ascii_cover, cover_stream_file), 0);
    fflush(cover_stream_file);
    rewind(cover_stream_file);

    std::string cover_data = read_file_stream(cover_stream_file);
    fclose(cover_stream_file);
    std::istringstream cover_stream(cover_data);

    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;
    struct node* matrix = dlx_read_binary(cover_stream, &solutions, &itemCount, &optionCount);
    ASSERT_NE(matrix, nullptr);
    ASSERT_NE(solutions, nullptr);
    ASSERT_GT(itemCount, 0);
    ASSERT_GT(optionCount, 0);

    uint32_t* row_ids = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * optionCount));
    ASSERT_NE(row_ids, nullptr);

    FILE* binary_output = tmpfile();
    ASSERT_NE(binary_output, nullptr);

    dlx::SolutionOutput output_ctx;
    ASSERT_EQ(dlx::Core::dlx_enable_binary_solution_output(output_ctx, binary_output, static_cast<uint32_t>(itemCount)), 0);

    testing::internal::CaptureStdout();
    dlx::Core::search(matrix, 0, solutions, row_ids, output_ctx);
    dlx::Core::dlx_disable_binary_solution_output(output_ctx);
    std::string stdout_capture = testing::internal::GetCapturedStdout();
    EXPECT_EQ(stdout_capture, kExpectedSudokuRows);

    fflush(binary_output);
    rewind(binary_output);
    std::string ascii_solution;
    ASSERT_EQ(binary_solution_to_ascii(binary_output, &ascii_solution), 0);
    EXPECT_EQ(ascii_solution, kExpectedSudokuRows);

    fclose(binary_output);
    free(row_ids);
    dlx::Core::freeMemory(matrix, solutions);
}

} // namespace
