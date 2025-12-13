#include "sudoku/encoder/sudoku_encoder.h"
#include "core/dlx_binary.h"
#include <gtest/gtest.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <fstream>

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

    struct DlxCoverHeader header;
    if (dlx_read_cover_header(file, &header) != 0)
    {
        ADD_FAILURE() << "Failed to read cover header from " << path;
        return 0;
    }
    EXPECT_EQ(header.column_count, COLUMN_COUNT);

    struct DlxRowChunk chunk = {0};
    int rows = 0;
    while (true)
    {
        int status = dlx_read_row_chunk(file, &chunk);
        if (status == -1)
        {
            ADD_FAILURE() << "Corrupt row chunk in " << path;
            break;
        }
        if (status == 0)
        {
            break;
        }

        if (rows == 0 && first_row != nullptr)
        {
            first_row->assign(chunk.columns, chunk.columns + chunk.entry_count);
        }

        rows++;
    }

    dlx_free_row_chunk(&chunk);
    return rows;
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

    struct DlxCoverHeader header;
    ASSERT_EQ(dlx_read_cover_header(cover, &header), 0);
    EXPECT_EQ(header.magic, DLX_COVER_MAGIC);
    EXPECT_EQ(header.version, DLX_BINARY_VERSION);
    EXPECT_EQ(header.column_count, COLUMN_COUNT);

    struct DlxRowChunk chunk = {0};
    int read_status = dlx_read_row_chunk(cover, &chunk);
    ASSERT_EQ(read_status, 1);
    EXPECT_EQ(chunk.row_id, 1u);
    EXPECT_EQ(chunk.entry_count, 4);

    const uint32_t expected_indices[4] = {0, 81, 162, 243};
    for (int i = 0; i < 4; i++)
    {
        EXPECT_EQ(chunk.columns[i], expected_indices[i]);
    }

    dlx_free_row_chunk(&chunk);
    remove(puzzle_template);
    remove(cover_template);
}

} // namespace
