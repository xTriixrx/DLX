#include "sudoku/decoder/sudoku_decoder.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    bool binary_input = false;
    int opt;
    while ((opt = getopt(argc, argv, "b")) != -1)
    {
        if (opt == 'b')
        {
            binary_input = true;
        }
        else
        {
            fprintf(stderr,
                    "Usage: ./sudoku_decoder [-b] <puzzle_file> <solution_rows> <output_file>\n");
            return EXIT_FAILURE;
        }
    }

    if (argc - optind != 3)
    {
        fprintf(stderr,
                "Usage: ./sudoku_decoder [-b] <puzzle_file> <solution_rows> <output_file>\n");
        return EXIT_FAILURE;
    }

    const char* puzzle_file = argv[optind];
    const char* solution_rows = argv[optind + 1];
    const char* output_file = argv[optind + 2];

    if (decode_sudoku_solution(puzzle_file, solution_rows, output_file, binary_input) != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
