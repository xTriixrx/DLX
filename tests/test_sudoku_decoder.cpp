#include "sudoku_decoder.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_string_to_file(const char* path, const char* contents)
{
    FILE* file = fopen(path, "w");
    assert(file != NULL);
    fputs(contents, file);
    fclose(file);
}

static void read_file_into_buffer(const char* path, char** buffer, size_t* length)
{
    FILE* file = fopen(path, "r");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    *buffer = static_cast<char*>(malloc(size + 1));
    assert(*buffer != NULL);
    fread(*buffer, 1, size, file);
    (*buffer)[size] = '\0';
    *length = (size_t)size;
    fclose(file);
}

static void test_decoder_reconstructs_solution(void)
{
    const char* solution_numbers =
        "1 2 8 24 31 32 33 47 48 60 64 75 87 88 95 96 89 97 103 93 99 104 105 113 "
        "73 114 124 128 138 52 53 7 12 45 50 58 63 79 76 67 71 83 106 109 116 119 "
        "34 40 17 16 21 5 27 28 44 122 127 136 140 129 141 142 143 148 151 152 153 "
        "154 156 157 144 158 161 164 170 171 175 177 178 182 183\n";

    char solution_template[] = "tests/tmp_rowsXXXXXX";
    int solution_fd = mkstemp(solution_template);
    assert(solution_fd != -1);
    close(solution_fd);
    write_string_to_file(solution_template, solution_numbers);

    char output_template[] = "tests/tmp_solvedXXXXXX";
    int output_fd = mkstemp(output_template);
    assert(output_fd != -1);
    close(output_fd);

    int status = decode_sudoku_solution("tests/sudoku_test.txt",
                                        solution_template,
                                        output_template);
    assert(status == 0);

    char* buffer = NULL;
    size_t len = 0;
    read_file_into_buffer(output_template, &buffer, &len);

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

    assert(strcmp(buffer, expected) == 0);

    free(buffer);
    remove(solution_template);
    remove(output_template);
}

int main(void)
{
    test_decoder_reconstructs_solution();
    printf("sudoku_decoder tests passed\n");
    return 0;
}
