#include "sudoku/encoder/sudoku_encoder.h"
#include "core/dlx_binary.h"
#include <gtest/gtest.h>
#include <string.h>
#include <unistd.h>

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

void count_header_and_rows(const char* path, int* header_cols, int* row_count)
{
    FILE* file = fopen(path, "r");
    ASSERT_NE(file, nullptr);

    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, file);
    ASSERT_NE(read, -1);

    int columns = 0;
    char* token = strtok(line, " \r\n");
    while (token != NULL)
    {
        columns++;
        token = strtok(NULL, " \r\n");
    }
    *header_cols = columns;

    int rows = 0;
    while ((read = getline(&line, &len, file)) != -1)
    {
        int values = 0;
        int ones = 0;
        token = strtok(line, " \r\n");
        while (token != NULL)
        {
            ASSERT_TRUE(strcmp(token, "0") == 0 || strcmp(token, "1") == 0);
            if (strcmp(token, "1") == 0)
            {
                ones++;
            }
            values++;
            token = strtok(NULL, " \r\n");
        }
        ASSERT_EQ(values, COLUMN_COUNT);
        ASSERT_EQ(ones, 4);
        rows++;
    }

    *row_count = rows;
    free(line);
    fclose(file);
}

void assert_first_row_columns(const char* path, const int expected_indices[4])
{
    FILE* file = fopen(path, "r");
    ASSERT_NE(file, nullptr);

    char* line = NULL;
    size_t len = 0;

    // Skip header
    ssize_t read = getline(&line, &len, file);
    ASSERT_NE(read, -1);

    // Read first data row
    read = getline(&line, &len, file);
    ASSERT_NE(read, -1);

    int indices[4] = {0};
    int discovered = 0;
    char* token = strtok(line, " \r\n");
    int column_index = 0;
    while (token != NULL)
    {
        if (strcmp(token, "1") == 0)
        {
            ASSERT_LT(discovered, 4);
            indices[discovered++] = column_index;
        }

        column_index++;
        token = strtok(NULL, " \r\n");
    }

    ASSERT_EQ(discovered, 4);
    for (int i = 0; i < 4; i++)
    {
        EXPECT_EQ(indices[i], expected_indices[i]);
    }

    free(line);
    fclose(file);
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

    int result = convert_sudoku_to_cover(puzzle_template, cover_template, false);
    ASSERT_EQ(result, 0);

    int header_cols = 0;
    int row_count = 0;
    count_header_and_rows(cover_template, &header_cols, &row_count);
    EXPECT_EQ(header_cols, COLUMN_COUNT);
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

    int result = convert_sudoku_to_cover(puzzle_template, cover_template, false);
    ASSERT_EQ(result, 0);

    int header_cols = 0;
    int row_count = 0;
    count_header_and_rows(cover_template, &header_cols, &row_count);
    EXPECT_EQ(header_cols, COLUMN_COUNT);
    EXPECT_EQ(row_count, 701);

    const int expected_indices[4] = {0, 81, 162, 243};
    assert_first_row_columns(cover_template, expected_indices);

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

    int result = convert_sudoku_to_cover(puzzle_template, cover_template, true);
    ASSERT_EQ(result, 0);

    FILE* cover = fopen(cover_template, "rb");
    ASSERT_NE(cover, nullptr);

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
    fclose(cover);
    remove(puzzle_template);
    remove(cover_template);
}

} // namespace
