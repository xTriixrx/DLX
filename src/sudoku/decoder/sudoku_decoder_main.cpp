#include "sudoku/decoder/sudoku_decoder.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static void print_usage(void)
{
    fprintf(stderr,
            "Usage: ./sudoku_decoder [-t] <puzzle_file> [solution_rows] [output_file]\n"
            "  -t    Interpret solution rows as text instead of binary (default)\n"
            "Hints:\n"
            "  Omit solution_rows to read from stdin.\n"
            "  Omit output_file or use '-' to write solved grids to stdout.\n");
}

int main(int argc, char** argv)
{
    bool binary_input = true;
    int opt;
    while ((opt = getopt(argc, argv, "t")) != -1)
    {
        if (opt == 't')
        {
            binary_input = false;
        }
        else
        {
            print_usage();
            return EXIT_FAILURE;
        }
    }

    int positional = argc - optind;
    if (positional < 1 || positional > 3)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    const char* puzzle_file = argv[optind];
    const char* solution_rows = positional >= 2 ? argv[optind + 1] : "-";
    const char* output_file = positional >= 3 ? argv[optind + 2] : "-";

    if (decode_sudoku_solution(puzzle_file, solution_rows, output_file, binary_input) != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
