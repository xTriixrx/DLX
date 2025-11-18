#include "sudoku/encoder/sudoku_encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void)
{
    fprintf(stderr,
            "Usage: ./sudoku_encoder [puzzle_file] [cover_output]\n"
            "       puzzle_file defaults to '-' (stdin) when omitted.\n"
            "       cover_output defaults to '-' (stdout) when omitted.\n");
}

int main(int argc, char** argv)
{
    if (argc > 3)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    const char* puzzle_path = (argc >= 2) ? argv[1] : "-";
    const char* cover_path = (argc == 3) ? argv[2] : "-";

    if (convert_sudoku_to_cover(puzzle_path, cover_path) != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
