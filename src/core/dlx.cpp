#include "core/dlx.h"
#include "core/dlx_binary.h"
#include "core/dlx_tcp_server.h"
#include "core/solution_sink.h"
#include <filesystem>
#include <fstream>
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

/**
 * Instantiates the TCP server ports and waits for input until server socket threads finish.
 * 
 * @param char** Set of command line arguments as strings.
 * @return int
 */
int instantiate_server(char** argv)
{
    // String to long conversion
    long request_port = strtol(argv[2], nullptr, 10);
    long solution_port = strtol(argv[3], nullptr, 10);

    // Verify request port and solution port are within valid port range
    if (request_port <= 0 || request_port > 65535 || solution_port <= 0 || solution_port > 65535)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    // Instantiate TcpServerConfig struct with request_port & solution_port
    dlx::TcpServerConfig config {
        static_cast<uint16_t>(request_port),
        static_cast<uint16_t>(solution_port)
    };

    // Instantiate DlxTcpServer with TcpServerConfig structure
    dlx::DlxTcpServer server(config);

    // Attempts to instantiate the server ports, if unable to start abort
    if (!server.start())
    {
        printf("Failed to start DLX TCP server.\n");
        return EXIT_FAILURE;
    }

    // Blocks thread until request and solution threads are closed
    server.wait();

    return EXIT_SUCCESS;
}

/**
 * @param const char* The path to a binary cover file to read in DLXB format or a piped input stream
 * @param const char* The path to write a binary solution file in DLXS format or a piped output stream
 */
int handle_cli(const char* cover_path, const char* solution_path)
{
    int itemCount = 0;
    int optionCount = 0;
    char** solutions = NULL;
    struct node* matrix = NULL;
    std::istream* cover_stream = nullptr;
    std::unique_ptr<std::istream> cover_stream_owner;

    // If cover path is defined as a pipe, set the cover_stream pointer to a reference of stdin
    if (strcmp(cover_path, "-") == 0)
    {
        cover_stream = &std::cin;
    }
    else // Otherwise, create an ifstream and move the reference to be owned by the cover_stream_owner
    {
        auto file_stream = std::make_unique<std::ifstream>(cover_path, std::ios::binary);
        
        // If cover file is unable to be opened, abort
        if (!file_stream->is_open())
        {
            printf("Unable to open cover file %s.\n", cover_path);
            return EXIT_FAILURE;
        }

        // Retrieve and cover_stream pointer to be owned by unique_ptr
        cover_stream = file_stream.get();
        cover_stream_owner = std::move(file_stream);
    }

    struct DlxCoverHeader header;
    if (dlx_read_cover_header(*cover_stream, &header) != 0)
    {
        printf("Failed to read binary cover header from %s.\n", cover_path);
        return EXIT_FAILURE;
    }

    matrix = dlx::Core::generateMatrixBinary(*cover_stream,
                                             header,
                                             &solutions,
                                             &itemCount,
                                             &optionCount);
    if (matrix == NULL)
    {
        printf("Failed to parse binary cover file %s.\n", cover_path);
        return EXIT_FAILURE;
    }

    uint32_t* solution_row_ids = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * optionCount));
    if (solution_row_ids == NULL)
    {
        printf("Unable to allocate solution buffer.\n");
        dlx::Core::freeMemory(matrix, solutions);
        return EXIT_FAILURE;
    }

    const bool write_to_stdout = (strcmp(solution_path, "-") == 0);
    FILE* binary_output = write_to_stdout ? stdout : fopen(solution_path, "wb");
    if (binary_output == NULL)
    {
        printf("Unable to create output file %s.\n", solution_path);
        dlx::Core::freeMemory(matrix, solutions);
        free(solution_row_ids);
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
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

/**
 * Main entry point for the DLX solver.
 * 
 * @param int Number of command line arguments provided to program.
 * @param char** Set of command line arguments as strings.
 * @return int
 */
int main(int argc, char** argv)
{
    // If an unknown set of arguments were provided, abort and print the cli usage.
    if (argc > 3 && strcmp(argv[1], "--server") != 0)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    // If dlx application was started as a TCP server, instantiate the server
    if (argc == 4 && strcmp(argv[1], "--server") == 0)
    {
        return instantiate_server(argv);
    }

    // Determine if paths are files or pipes
    const char* cover_path = (argc >= 2) ? argv[1] : "-";
    const char* solution_output_path = (argc == 3) ? argv[2] : "-";

    // If cover path is a file path and doesn't exist on the system, 
    if (strcmp(cover_path, "-") != 0 && !std::filesystem::exists(cover_path))
    {
        printf("Cover file %s does not exist.\n", cover_path);
        return EXIT_FAILURE;
    }

    // Handle cli execution with provided cover path and solution output path
    return handle_cli(cover_path, solution_output_path);
}
