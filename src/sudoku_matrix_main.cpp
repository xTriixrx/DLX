#include "sudoku_matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc < 3 || argc > 4)
    {
        fprintf(stderr, "Usage: ./sudoku_matrix <puzzle_file> <cover_output> [--binary|--text]\n");
        return EXIT_FAILURE;
    }

    bool binary_format = false;
    if (argc == 4)
    {
        if (strcmp(argv[3], "--binary") == 0)
        {
            binary_format = true;
        }
        else if (strcmp(argv[3], "--text") == 0)
        {
            binary_format = false;
        }
        else
        {
            fprintf(stderr, "Unknown option %s\n", argv[3]);
            return EXIT_FAILURE;
        }
    }

    if (convert_sudoku_to_cover(argv[1], argv[2], binary_format) != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
