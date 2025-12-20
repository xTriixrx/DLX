#include "sudoku/encoder/encoder.h"
#include "core/binary.h"
#include <gtest/gtest.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <fstream>

namespace binary = dlx::binary;
#define COLUMN_COUNT 324

namespace
{

void write_string_to_file(const char* path, const char* contents)
{
    FILE* file = fopen(path, "w");
    ASSERT_NE(file, nullptr);
    fputs(contents, file);
    fclose(file);
}

int count_rows_in_binary_cover(const char* path, std::vector<uint32_t>* first_row = nullptr)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        ADD_FAILURE() << "Unable to open cover file " << path;
        return 0;
    }

    binary::DlxProblem problem;
    if (binary::dlx_read_problem(file, &problem) != 0)
    {
        ADD_FAILURE() << "Failed to read binary cover from " << path;
        return 0;
    }
    EXPECT_EQ(problem.header.column_count, COLUMN_COUNT);

    if (!problem.rows.empty() && first_row != nullptr)
    {
        const auto& chunk = problem.rows.front();
        first_row->assign(chunk.columns, chunk.columns + chunk.entry_count);
    }

    return static_cast<int>(problem.rows.size());
}

TEST(SudokuMatrixTest, EmptyGridGeneratesFullMatrix)
{
    const char* puzzle =
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n";

    char puzzle_template[] = "tests/tmp_puzzleXXXXXX";
    int puzzle_fd = mkstemp(puzzle_template);
    ASSERT_NE(puzzle_fd, -1);
    close(puzzle_fd);
    write_string_to_file(puzzle_template, puzzle);

    char cover_template[] = "tests/tmp_coverXXXXXX";
    int cover_fd = mkstemp(cover_template);
    ASSERT_NE(cover_fd, -1);
    close(cover_fd);

    int result = convert_sudoku_to_cover(puzzle_template, cover_template);
    ASSERT_EQ(result, 0);

    int row_count = count_rows_in_binary_cover(cover_template);
    EXPECT_EQ(row_count, 729);

    remove(puzzle_template);
    remove(cover_template);
}

TEST(SudokuMatrixTest, PrefilledDigitsLimitCandidates)
{
    const char* puzzle =
        "100000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n";

    char puzzle_template[] = "tests/tmp_puzzleXXXXXX";
    int puzzle_fd = mkstemp(puzzle_template);
    ASSERT_NE(puzzle_fd, -1);
    close(puzzle_fd);
    write_string_to_file(puzzle_template, puzzle);

    char cover_template[] = "tests/tmp_coverXXXXXX";
    int cover_fd = mkstemp(cover_template);
    ASSERT_NE(cover_fd, -1);
    close(cover_fd);

    int result = convert_sudoku_to_cover(puzzle_template, cover_template);
    ASSERT_EQ(result, 0);

    std::vector<uint32_t> first_row;
    int row_count = count_rows_in_binary_cover(cover_template, &first_row);
    EXPECT_EQ(row_count, 701);

    ASSERT_EQ(first_row.size(), 4u);
    const uint32_t expected_indices[4] = {0, 81, 162, 243};
    for (size_t i = 0; i < first_row.size(); i++)
    {
        EXPECT_EQ(first_row[i], expected_indices[i]);
    }

    remove(puzzle_template);
    remove(cover_template);
}

TEST(SudokuMatrixTest, BinaryCoverOutputMatchesExpectations)
{
    const char* puzzle =
        "100000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n"
        "000000000\n";

    char puzzle_template[] = "tests/tmp_puzzleXXXXXX";
    int puzzle_fd = mkstemp(puzzle_template);
    ASSERT_NE(puzzle_fd, -1);
    close(puzzle_fd);
    write_string_to_file(puzzle_template, puzzle);

    char cover_template[] = "tests/tmp_coverXXXXXX";
    int cover_fd = mkstemp(cover_template);
    ASSERT_NE(cover_fd, -1);
    close(cover_fd);

    int result = convert_sudoku_to_cover(puzzle_template, cover_template);
    ASSERT_EQ(result, 0);

    std::ifstream cover(cover_template, std::ios::binary);
    ASSERT_TRUE(cover.is_open());

    binary::DlxProblem problem;
    ASSERT_EQ(binary::dlx_read_problem(cover, &problem), 0);
    EXPECT_EQ(problem.header.magic, DLX_COVER_MAGIC);
    EXPECT_EQ(problem.header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(problem.header.column_count, COLUMN_COUNT);
    ASSERT_FALSE(problem.rows.empty());

    const auto& chunk = problem.rows.front();
    EXPECT_EQ(chunk.row_id, 1u);
    EXPECT_EQ(chunk.entry_count, 4);

    const uint32_t expected_indices[4] = {0, 81, 162, 243};
    for (int i = 0; i < 4; i++)
    {
        EXPECT_EQ(chunk.columns[i], expected_indices[i]);
    }

    remove(puzzle_template);
    remove(cover_template);
}

} // namespace
