#include "sudoku/decoder/decoder.h"
#include <stdio.h>
#include <stdlib.h>

static void print_usage(void)
{
    fprintf(stderr,
            "Usage: ./sudoku_decoder [puzzle_file] [solution_rows] [output_file]\n"
            "       puzzle_file defaults to '-' (stdin) when omitted.\n"
            "       solution_rows defaults to '-' (stdin) when omitted.\n"
            "       output_file defaults to '-' (stdout) when omitted.\n");
}

int main(int argc, char** argv)
{
    if (argc > 4)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    const char* puzzle_file = (argc >= 2) ? argv[1] : "-";
    const char* solution_rows = (argc >= 3) ? argv[2] : "-";
    const char* output_file = (argc >= 4) ? argv[3] : "-";

    if (decode_sudoku_solution(puzzle_file, solution_rows, output_file) != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
