#include "core/dlx.h"
#include "core/binary.h"
#include "sudoku/encoder/encoder.h"
#include "ascii_binary_utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace binary = dlx::binary;

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

void write_header_and_chunk(const char* path)
{
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    binary::DlxProblem problem;
    problem.header = {
        .magic = DLX_COVER_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0x2,
        .column_count = 10,
        .row_count = 2,
    };

    uint32_t row1[] = {1, 5, 9};
    uint32_t row2[] = {0, 4, 8};

    binary::DlxRowChunk chunk1 = {0};
    chunk1.row_id = 1;
    chunk1.entry_count = 3;
    chunk1.capacity = 3;
    chunk1.columns = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * 3));
    ASSERT_NE(chunk1.columns, nullptr);
    memcpy(chunk1.columns, row1, sizeof(row1));

    binary::DlxRowChunk chunk2 = {0};
    chunk2.row_id = 2;
    chunk2.entry_count = 3;
    chunk2.capacity = 3;
    chunk2.columns = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * 3));
    ASSERT_NE(chunk2.columns, nullptr);
    memcpy(chunk2.columns, row2, sizeof(row2));

    problem.rows.push_back(chunk1);
    problem.rows.push_back(chunk2);

    ASSERT_EQ(binary::dlx_write_problem(file, &problem), 0);
    file.close();
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

    binary::DlxProblem problem;
    ASSERT_EQ(binary::dlx_read_problem(cover_stream, &problem), 0);
    EXPECT_EQ(problem.header.magic, DLX_COVER_MAGIC);
    EXPECT_EQ(problem.header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(problem.header.flags, 0x2);
    EXPECT_EQ(problem.header.column_count, 10u);
    EXPECT_EQ(problem.header.row_count, 2u);
    ASSERT_EQ(problem.rows.size(), 2u);

    const auto& chunk1 = problem.rows[0];
    EXPECT_EQ(chunk1.row_id, 1u);
    EXPECT_EQ(chunk1.entry_count, 3);
    EXPECT_EQ(chunk1.columns[0], 1u);
    EXPECT_EQ(chunk1.columns[1], 5u);
    EXPECT_EQ(chunk1.columns[2], 9u);

    const auto& chunk2 = problem.rows[1];
    EXPECT_EQ(chunk2.row_id, 2u);
    EXPECT_EQ(chunk2.entry_count, 3);
    EXPECT_EQ(chunk2.columns[0], 0u);
    EXPECT_EQ(chunk2.columns[1], 4u);
    EXPECT_EQ(chunk2.columns[2], 8u);
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

    std::ofstream file(file_template, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    binary::DlxSolution solution;
    solution.header = {
        .magic = DLX_SOLUTION_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = 10,
    };

    uint32_t rows[] = {10, 20, 30, 40};
    binary::DlxSolutionRow row = {0};
    row.solution_id = 7;
    row.entry_count = 4;
    row.capacity = 4;
    row.row_indices = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * 4));
    ASSERT_NE(row.row_indices, nullptr);
    memcpy(row.row_indices, rows, sizeof(rows));

    solution.rows.push_back(row);
    ASSERT_EQ(binary::dlx_write_solution(file, &solution), 0);
    file.close();

    std::ifstream solution_stream(file_template, std::ios::binary);
    ASSERT_TRUE(solution_stream.is_open());

    binary::DlxSolution read_solution;
    ASSERT_EQ(binary::dlx_read_solution(solution_stream, &read_solution), 0);
    EXPECT_EQ(read_solution.header.magic, DLX_SOLUTION_MAGIC);
    EXPECT_EQ(read_solution.header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(read_solution.header.flags, 0);
    EXPECT_EQ(read_solution.header.column_count, 10u);
    ASSERT_EQ(read_solution.rows.size(), 1u);
    EXPECT_EQ(read_solution.rows[0].solution_id, 7u);
    ASSERT_EQ(read_solution.rows[0].entry_count, 4);
    for (int i = 0; i < read_solution.rows[0].entry_count; i++)
    {
        EXPECT_EQ(read_solution.rows[0].row_indices[i], rows[i]);
    }
    solution_stream.close();
    remove(file_template);
}

TEST(DlxBinaryTest, DlxSolvesFromBinaryCoverAndEmitsBinarySolutions)
{
    char cover_template[] = "tests/tmp_binary_coverXXXXXX";
    int cover_fd = mkstemp(cover_template);
    ASSERT_NE(cover_fd, -1);
    close(cover_fd);

    ASSERT_EQ(convert_sudoku_to_cover("tests/sudoku_tests/sudoku_test.txt", cover_template), 0);

    std::ifstream cover(cover_template, std::ios::binary);
    ASSERT_TRUE(cover.is_open());

    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;
    struct node* matrix = binary::dlx_read_binary(cover, &solutions, &itemCount, &optionCount);
    cover.close();
    ASSERT_NE(matrix, nullptr);
    ASSERT_NE(solutions, nullptr);
    ASSERT_GT(itemCount, 0);
    ASSERT_GT(optionCount, 0);

    uint32_t* row_ids = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * optionCount));
    ASSERT_NE(row_ids, nullptr);

    std::ostringstream binary_output;
    dlx::SolutionOutput output_ctx;
    ASSERT_EQ(dlx::Core::dlx_enable_binary_solution_output(output_ctx, binary_output, static_cast<uint32_t>(itemCount)), 0);

    testing::internal::CaptureStdout();
    dlx::Core::search(matrix, 0, solutions, row_ids, output_ctx);
    dlx::Core::dlx_disable_binary_solution_output(output_ctx);
    std::string stdout_capture = testing::internal::GetCapturedStdout();
    EXPECT_EQ(stdout_capture, kExpectedSudokuRows);

    std::istringstream binary_stream(binary_output.str());

    binary::DlxSolution solution;
    ASSERT_EQ(binary::dlx_read_solution(binary_stream, &solution), 0);
    EXPECT_EQ(solution.header.magic, DLX_SOLUTION_MAGIC);
    EXPECT_EQ(solution.header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(solution.header.column_count, static_cast<uint32_t>(itemCount));
    ASSERT_EQ(solution.rows.size(), 1u);

    std::vector<uint32_t> expected_values = parse_row_list(kExpectedSudokuRows);
    ASSERT_EQ(solution.rows[0].entry_count, expected_values.size());
    for (size_t i = 0; i < expected_values.size(); i++)
    {
        EXPECT_EQ(solution.rows[0].row_indices[i], expected_values[i]);
    }
    free(row_ids);
    dlx::Core::freeMemory(matrix, solutions);
    remove(cover_template);
}

TEST(DlxBinaryTest, AsciiCoverProducesBinarySolutionMatchingExpectedRows)
{
    std::string ascii_cover = read_file_to_string("tests/sudoku_example/sudoku_cover.txt");
    ASSERT_FALSE(ascii_cover.empty());

    std::ostringstream cover_stream_output;
    ASSERT_EQ(ascii_cover_to_binary_stream(ascii_cover, cover_stream_output), 0);
    std::istringstream cover_stream(cover_stream_output.str());

    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;
    struct node* matrix = binary::dlx_read_binary(cover_stream, &solutions, &itemCount, &optionCount);
    ASSERT_NE(matrix, nullptr);
    ASSERT_NE(solutions, nullptr);
    ASSERT_GT(itemCount, 0);
    ASSERT_GT(optionCount, 0);

    uint32_t* row_ids = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * optionCount));
    ASSERT_NE(row_ids, nullptr);

    std::ostringstream binary_output;

    dlx::SolutionOutput output_ctx;
    ASSERT_EQ(dlx::Core::dlx_enable_binary_solution_output(output_ctx, binary_output, static_cast<uint32_t>(itemCount)), 0);

    testing::internal::CaptureStdout();
    dlx::Core::search(matrix, 0, solutions, row_ids, output_ctx);
    dlx::Core::dlx_disable_binary_solution_output(output_ctx);
    std::string stdout_capture = testing::internal::GetCapturedStdout();
    EXPECT_EQ(stdout_capture, kExpectedSudokuRows);

    std::istringstream binary_solution_stream(binary_output.str());
    std::string ascii_solution;
    ASSERT_EQ(binary_solution_to_ascii(binary_solution_stream, &ascii_solution), 0);
    EXPECT_EQ(ascii_solution, kExpectedSudokuRows);

    free(row_ids);
    dlx::Core::freeMemory(matrix, solutions);
}

} // namespace
