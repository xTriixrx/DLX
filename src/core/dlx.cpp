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
    printf("./dlx [-b] path/to/cover_file path/to/output_solutions\n");
}

int main(int argc, char** argv)
{
    bool binary_mode = false;
    int opt;
    while ((opt = getopt(argc, argv, "b")) != -1)
    {
        switch (opt)
        {
        case 'b':
            binary_mode = true;
            break;
        default:
            print_usage();
            exit(1);
        }
    }

    if (argc - optind != 2)
    {
        print_usage();
        exit(1);
    }

    const char* cover_path = argv[optind];
    const char* solution_output_path = argv[optind + 1];

    // If file does not exist, exit execution.
    if (!fileExists(const_cast<char*>(cover_path)))
    {
        printf("Cover file %s does not exist.\n", cover_path);
        exit(1);
    }

    struct node* matrix = NULL;
    char** titles = NULL;
    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;

    if (binary_mode)
    {
        FILE* cover = fopen(cover_path, "rb");
        if (cover == NULL)
        {
            printf("Unable to open cover file %s.\n", cover_path);
            exit(1);
        }

        matrix = dlx_read_binary(cover, &titles, &solutions, &itemCount, &optionCount);
        fclose(cover);

        if (matrix == NULL)
        {
            printf("Failed to read binary cover file %s.\n", cover_path);
            exit(1);
        }
    }
    else
    {
        FILE* cover = fopen(cover_path, READ_ONLY);

        // If unable to open cover file, exit execution.
        if (cover == NULL)
        {
            printf("Unable to open cover file %s.\n", cover_path);
            exit(1);
        }

        itemCount = getItemCount(cover);
        int nodeCount = itemCount;
        nodeCount += getNodeCount(cover);
        optionCount = getOptionsCount(cover);

        titles = static_cast<char**>(malloc(itemCount * sizeof(char*)));
        solutions = static_cast<char**>(malloc(sizeof(char*) * optionCount));
        matrix = generateMatrix(cover, titles, nodeCount);

        fclose(cover);

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
    FILE* solution_output = fopen(solution_output_path, binary_mode ? "wb" : "w");

    if (solution_output == NULL)
    {
        printf("Unable to create output file %s.\n", solution_output_path);
        freeMemory(matrix, solutions, titles, itemCount);
        exit(1);
    }

    // Search and print out found solutions to stdout and output file
    if (binary_mode)
    {
        if (dlx_enable_binary_solution_output(solution_output, static_cast<uint32_t>(itemCount)) != 0)
        {
            fclose(solution_output);
            freeMemory(matrix, solutions, titles, itemCount);
            exit(1);
        }

        search(matrix, 0, solutions, NULL);
        dlx_disable_binary_solution_output();
    }
    else
    {
        search(matrix, 0, solutions, solution_output);
    }

    fclose(solution_output);
    
    // Release all malloc'd memory
    freeMemory(matrix, solutions, titles, itemCount);
    
    return EXIT_SUCCESS;
}
