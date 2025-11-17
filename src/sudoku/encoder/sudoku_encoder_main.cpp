#include "sudoku/encoder/sudoku_encoder.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void)
{
    fprintf(stderr,
            "Usage: ./sudoku_encoder [options] <puzzle_file> [cover_output]\n"
            "Options:\n"
            "  -t, --text     Write the cover output in text format (default is binary)\n"
            "Hints:\n"
            "  Use '-' for <puzzle_file> to read from stdin.\n"
            "  Omit cover_output or pass '-' to stream the cover matrix to stdout.\n");
}

int main(int argc, char** argv)
{
    bool binary_format = true;

    static struct option long_options[] = {
        {"text", no_argument, NULL, 't'},
        {0, 0, 0, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "t", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 't':
            binary_format = false;
            break;
        default:
            print_usage();
            return EXIT_FAILURE;
        }
    }

    int positional = argc - optind;
    if (positional < 1 || positional > 2)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    const char* puzzle_path = argv[optind];
    const char* cover_path = positional == 2 ? argv[optind + 1] : "-";

    if (convert_sudoku_to_cover(puzzle_path, cover_path, binary_format) != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
