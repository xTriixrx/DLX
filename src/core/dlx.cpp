#include "core/dlx.h"
#include "core/dlx_binary.h"
#include "core/dlx_tcp_server.h"
#include "core/solution_sink.h"
#include <filesystem>
#include <iostream>
#include <memory>
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
    printf("./dlx [cover_file] [solution_output]\n");
    printf("./dlx --server [problem_port] [solution_port]\n");
    printf("Hints:\n");
    printf("  Omit arguments or pass '-' to stream via stdin/stdout.\n");
}

int main(int argc, char** argv)
{
    if (argc > 3 && strcmp(argv[1], "--server") != 0)
    {
        print_usage();
        exit(1);
    }

    if (argc == 4 && strcmp(argv[1], "--server") == 0)
    {
        long request_port = strtol(argv[2], nullptr, 10);
        long solution_port = strtol(argv[3], nullptr, 10);
        if (request_port <= 0 || request_port > 65535 || solution_port <= 0 || solution_port > 65535)
        {
            print_usage();
            exit(1);
        }

        dlx::TcpServerConfig config{static_cast<uint16_t>(request_port), static_cast<uint16_t>(solution_port)};
        dlx::DlxTcpServer server(config);
        if (!server.start())
        {
            printf("Failed to start DLX TCP server.\n");
            exit(1);
        }

        server.wait();
        return EXIT_SUCCESS;
    }

    const char* cover_path = (argc >= 2) ? argv[1] : "-";
    const char* solution_output_path = (argc == 3) ? argv[2] : "-";

    if (strcmp(cover_path, "-") != 0 && !std::filesystem::exists(cover_path))
    {
        printf("Cover file %s does not exist.\n", cover_path);
        exit(1);
    }

    struct node* matrix = NULL;
    char** solutions = NULL;
    int itemCount = 0;
    int optionCount = 0;

    FILE* cover_stream = (strcmp(cover_path, "-") == 0) ? stdin : fopen(cover_path, "rb");
    if (cover_stream == NULL)
    {
        printf("Unable to open cover file %s.\n", cover_path);
        exit(1);
    }

    struct DlxCoverHeader header;
    if (dlx_read_cover_header(cover_stream, &header) != 0)
    {
        if (cover_stream != stdin)
        {
            fclose(cover_stream);
        }
        printf("Failed to read binary cover header from %s.\n", cover_path);
        exit(1);
    }

    matrix = dlx::Core::generateMatrixBinary(cover_stream,
                                             header,
                                             &solutions,
                                             &itemCount,
                                             &optionCount);
    if (matrix == NULL)
    {
        if (cover_stream != stdin)
        {
            fclose(cover_stream);
        }
        printf("Failed to parse binary cover file %s.\n", cover_path);
        exit(1);
    }

    if (cover_stream != stdin)
    {
        fclose(cover_stream);
    }

    uint32_t* solution_row_ids = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * optionCount));
    if (solution_row_ids == NULL)
    {
        printf("Unable to allocate solution buffer.\n");
        dlx::Core::freeMemory(matrix, solutions);
        exit(1);
    }

    const bool write_to_stdout = (strcmp(solution_output_path, "-") == 0);
    FILE* binary_output = write_to_stdout ? stdout : fopen(solution_output_path, "wb");
    if (binary_output == NULL)
    {
        printf("Unable to create output file %s.\n", solution_output_path);
        dlx::Core::freeMemory(matrix, solutions);
        free(solution_row_ids);
        exit(1);
    }

    const bool suppress_stdout = write_to_stdout;
    if (suppress_stdout)
    {
        dlx::Core::dlx_set_stdout_suppressed(true);
    }

    std::unique_ptr<dlx::OstreamSolutionSink> console_sink;
    dlx::CompositeSolutionSink sink_router;
    dlx::SolutionOutput output_ctx;

    if (!suppress_stdout)
    {
        console_sink = std::make_unique<dlx::OstreamSolutionSink>(std::cout);
        sink_router.add_sink(console_sink.get());
    }
    output_ctx.sink = sink_router.empty() ? nullptr : &sink_router;

    if (dlx::Core::dlx_enable_binary_solution_output(output_ctx, binary_output, static_cast<uint32_t>(itemCount)) != 0)
    {
        if (!write_to_stdout)
        {
            fclose(binary_output);
        }
        dlx::Core::freeMemory(matrix, solutions);
        free(solution_row_ids);
        exit(1);
    }

    dlx::Core::search(matrix, 0, solutions, solution_row_ids, output_ctx);
    dlx::Core::dlx_disable_binary_solution_output(output_ctx);

    if (suppress_stdout)
    {
        dlx::Core::dlx_set_stdout_suppressed(false);
    }

    if (!write_to_stdout)
    {
        fclose(binary_output);
    }
    else
    {
        fflush(binary_output);
    }

    free(solution_row_ids);
    dlx::Core::freeMemory(matrix, solutions);
    return EXIT_SUCCESS;
}
