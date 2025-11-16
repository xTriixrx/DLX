#include "sudoku/encoder/sudoku_encoder.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void)
{
    fprintf(stderr,
            "Usage: ./sudoku_encoder [options] <puzzle_file> <cover_output>\n"
            "Options:\n"
            "  -b, --binary   Write the cover output in binary format\n"
            "  -t, --text     Write the cover output in text format (default)\n");
}

int main(int argc, char** argv)
{
    bool binary_format = false;

    static struct option long_options[] = {
        {"binary", no_argument, NULL, 'b'},
        {"text", no_argument, NULL, 't'},
        {0, 0, 0, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "bt", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'b':
            binary_format = true;
            break;
        case 't':
            binary_format = false;
            break;
        default:
            print_usage();
            return EXIT_FAILURE;
        }
    }

    if (argc - optind != 2)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    const char* puzzle_path = argv[optind];
    const char* cover_path = argv[optind + 1];

    if (convert_sudoku_to_cover(puzzle_path, cover_path, binary_format) != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
