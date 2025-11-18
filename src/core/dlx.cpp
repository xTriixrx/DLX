#include "core/dlx_binary.h"
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

/**************************************************************************************************************
 *                                            DLX Application                                                 *
 *        DLX is a powerful backtracking, depth-first algorithm that solves exact cover problems.             *
 *                                                                                                            * 
 * Version: 0.0.1                                                                                             *
 * Author: Vincent Nigro                                                                                      *
 * Last Modified: 3/12/22                                                                                     *
 **************************************************************************************************************/

/**
 * A main function that drives the DLX application.
 * 
 * @param int Number of command line arguments provided to program.
 * @param char** Set of command line arguments as strings.
 * @return void
 */
static void print_usage(void)
{
    printf("./dlx [-t] [cover_file] [output_solutions]\n");
    printf("  -t    Read text cover input and emit text solution rows (default is binary)\n");
    printf("Hints:\n");
    printf("  Omit arguments or pass '-' to stream via stdin/stdout.\n");
}

static FILE* buffer_stream(FILE* input)
{
    FILE* temp = tmpfile();
    if (temp == NULL)
    {
        return NULL;
    }

    char buffer[8192];
    size_t read_bytes = 0;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), input)) > 0)
    {
        if (fwrite(buffer, 1, read_bytes, temp) != read_bytes)
        {
            fclose(temp);
            return NULL;
        }
    }

    if (ferror(input))
    {
        fclose(temp);
        return NULL;
    }

    fflush(temp);
    fseek(temp, 0L, SEEK_SET);
    return temp;
}

int main(int argc, char** argv)
{
    bool binary_mode = true;
    int opt;
    while ((opt = getopt(argc, argv, "t")) != -1)
    {
        switch (opt)
        {
        case 't':
            binary_mode = false;
            break;
        default:
            print_usage();
            exit(1);
        }
    }

    int positional = argc - optind;
    if (positional > 2)
    {
        print_usage();
        exit(1);
    }

    const char* cover_path = positional >= 1 ? argv[optind] : "-";
    const char* solution_output_path = positional == 2 ? argv[optind + 1] : "-";

    // If file does not exist, exit execution (unless streaming).
    if (strcmp(cover_path, "-") != 0 && !dlx::Core::fileExists(const_cast<char*>(cover_path)))
    {
        printf("Cover file %s does not exist.\n", cover_path);
        exit(1);
    }

    struct node* matrix = NULL;
    char** titles = NULL;
    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;

    bool close_cover = false;
    FILE* cover_stream = NULL;
    FILE* buffered_cover = NULL;

    if (binary_mode)
    {
        if (strcmp(cover_path, "-") == 0)
        {
            cover_stream = stdin;
        }
        else
        {
            cover_stream = fopen(cover_path, "rb");
            close_cover = true;
        }

        if (cover_stream == NULL)
        {
            printf("Unable to open cover file %s.\n", cover_path);
            exit(1);
        }

        matrix = dlx_read_binary(cover_stream, &titles, &solutions, &itemCount, &optionCount);

        if (close_cover)
        {
            fclose(cover_stream);
        }

        if (matrix == NULL)
        {
            printf("Failed to read binary cover file %s.\n", cover_path);
            exit(1);
        }
    }
    else
    {
        if (strcmp(cover_path, "-") == 0)
        {
            buffered_cover = buffer_stream(stdin);
            cover_stream = buffered_cover;
            close_cover = true;
        }
        else
        {
            cover_stream = fopen(cover_path, dlx::READ_ONLY);
            close_cover = true;
        }

        // If unable to open cover file, exit execution.
        if (cover_stream == NULL)
        {
            printf("Unable to open cover file %s.\n", cover_path);
            exit(1);
        }

        itemCount = dlx::Core::getItemCount(cover_stream);
        int nodeCount = itemCount;
        nodeCount += dlx::Core::getNodeCount(cover_stream);
        optionCount = dlx::Core::getOptionsCount(cover_stream);

        titles = static_cast<char**>(malloc(itemCount * sizeof(char*)));
        solutions = static_cast<char**>(malloc(sizeof(char*) * optionCount));
        matrix = dlx::Core::generateMatrix(cover_stream, titles, nodeCount);

        if (close_cover && cover_stream != NULL)
        {
            fclose(cover_stream);
        }

        if (matrix == NULL)
        {
            printf("Unable to generate matrix from cover file %s.\n", cover_path);
            free(titles);
            free(solutions);
            exit(1);
        }
    }

    // Examples of printing functions; tested with test.txt sample
    //printItems(matrix);
    //printItemColumn(&matrix[1]);
    //printOptionRow(&matrix[9]);
    //printMatrix(matrix, (sizeof(struct node) * nodeCount) / sizeof(struct node), itemCount);

    // Prepare output file for logging solutions
    bool write_to_stdout = (strcmp(solution_output_path, "-") == 0);
    FILE* solution_output = write_to_stdout ? stdout : fopen(solution_output_path, binary_mode ? "wb" : "w");

    if (solution_output == NULL)
    {
        printf("Unable to create output file %s.\n", solution_output_path);
        dlx::Core::freeMemory(matrix, solutions, titles, itemCount);
        exit(1);
    }

    // Search and print out found solutions to stdout and output file
    bool suppress_stdout = binary_mode && write_to_stdout;
    if (suppress_stdout)
    {
        dlx::Core::dlx_set_stdout_suppressed(true);
    }

    if (binary_mode)
    {
        if (dlx::Core::dlx_enable_binary_solution_output(solution_output, static_cast<uint32_t>(itemCount)) != 0)
        {
            fclose(solution_output);
            dlx::Core::freeMemory(matrix, solutions, titles, itemCount);
            exit(1);
        }

        dlx::Core::search(matrix, 0, solutions, NULL);
        dlx::Core::dlx_disable_binary_solution_output();
    }
    else
    {
        dlx::Core::search(matrix, 0, solutions, solution_output);
    }

    if (suppress_stdout)
    {
        dlx::Core::dlx_set_stdout_suppressed(false);
    }

    if (!write_to_stdout)
    {
        fclose(solution_output);
    }
    else
    {
        fflush(solution_output);
    }
    
    // Release all malloc'd memory
        dlx::Core::freeMemory(matrix, solutions, titles, itemCount);
    
    return EXIT_SUCCESS;
}
