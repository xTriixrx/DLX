#include "sudoku_decoder.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: ./sudoku_decoder <puzzle_file> <solution_rows> <output_file>\n");
        return EXIT_FAILURE;
    }

    if (decode_sudoku_solution(argv[1], argv[2], argv[3]) != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
