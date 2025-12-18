#include "core/binary.h"
#include "sudoku/decoder/decoder.h"
#include "sudoku/encoder/encoder.h"
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>
#include <string.h>
#include <unistd.h>

namespace
{

void write_string_to_file(const char* path, const char* contents)
{
    FILE* file = fopen(path, "w");
    ASSERT_NE(file, nullptr);
    fputs(contents, file);
    fclose(file);
}

void read_file_into_buffer(const char* path, std::string& buffer)
{
    FILE* file = fopen(path, "r");
    ASSERT_NE(file, nullptr);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer.resize(size);
    size_t read = fread(buffer.data(), 1, size, file);
    ASSERT_EQ(read, static_cast<size_t>(size));
    buffer.push_back('\0');
    fclose(file);
}

std::vector<uint32_t> parse_row_indices(const char* line)
{
    std::vector<uint32_t> values;
    const char* cursor = line;
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

TEST(SudokuDecoderTest, ReconstructsBinarySolution)
{
    const char* solution_numbers =
        "1 2 8 24 31 32 33 47 48 60 64 75 87 88 95 96 89 97 103 93 99 104 105 113 "
        "73 114 124 128 138 52 53 7 12 45 50 58 63 79 76 67 71 83 106 109 116 119 "
        "34 40 17 16 21 5 27 28 44 122 127 136 140 129 141 142 143 148 151 152 153 "
        "154 156 157 144 158 161 164 170 171 175 177 178 182 183\n";

    std::vector<uint32_t> indices = parse_row_indices(solution_numbers);

    char binary_template[] = "tests/tmp_binary_rowsXXXXXX";
    int binary_fd = mkstemp(binary_template);
    ASSERT_NE(binary_fd, -1);
    close(binary_fd);

    std::ofstream binary_rows(binary_template, std::ios::binary);
    ASSERT_TRUE(binary_rows.is_open());

    struct DlxSolutionHeader header = {
        .magic = DLX_SOLUTION_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = COLUMN_COUNT,
    };

    ASSERT_EQ(dlx_write_solution_header(binary_rows, &header), 0);
    ASSERT_EQ(dlx_write_solution_row(
                  binary_rows, 1, indices.data(), static_cast<uint16_t>(indices.size())),
              0);
    binary_rows.close();

    char output_template[] = "tests/tmp_solvedXXXXXX";
    int output_fd = mkstemp(output_template);
    ASSERT_NE(output_fd, -1);
    close(output_fd);

    int status = decode_sudoku_solution("tests/sudoku_test.txt", binary_template, output_template);
    ASSERT_EQ(status, 0);

    std::string buffer;
    read_file_into_buffer(output_template, buffer);

    const char* expected =
        "Solution #1\n"
        "534678912\n"
        "672195348\n"
        "198342567\n"
        "859761423\n"
        "426853791\n"
        "713924856\n"
        "961537284\n"
        "287419635\n"
        "345286179\n\n";

    EXPECT_STREQ(buffer.c_str(), expected);

    remove(binary_template);
    remove(output_template);
}
} // namespace
