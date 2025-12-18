#include "core/dlx.h"
#include "core/binary.h"
#include "core/tcp_server.h"
#include "core/util.h"
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

static bool open_cover_stream(const char* cover_path, CoverStream& cover_stream)
{
    // If cover path is defined as a pipe, set the cover_stream structs
    // stream pointer to a reference of stdin
    if (strcmp(cover_path, "-") == 0)
    {
        cover_stream.stream = &std::cin;
        return true;
    }

    // Otherwise, create an ifstream and move the reference to be owned by the cover_stream_owner
    auto file_stream = std::make_unique<std::ifstream>(cover_path, std::ios::binary);
    
    // If cover file is unable to be opened, abort
    if (!file_stream->is_open())
    {
        printf("Unable to open cover file %s.\n", cover_path);
        return false;
    }

    // Retrieve and set cover_stream structs stream pointer to be owned by unique_ptr
    cover_stream.stream = file_stream.get();
    cover_stream.owner = std::move(file_stream);
    return true;
}

static bool build_matrix_context(std::istream& cover_stream, const char* cover_path, MatrixContext& ctx)
{
    struct DlxCoverHeader header;
    
    //
    ctx.reset();

    //
    if (dlx_read_cover_header(cover_stream, &header) != 0)
    {
        printf("Failed to read binary cover header from %s.\n", cover_path);
        return false;
    }

    //
    ctx.matrix = dlx::Core::generateMatrixBinary(cover_stream,
                                                 header,
                                                 &ctx.solutions,
                                                 &ctx.item_count,
                                                 &ctx.option_count);
    
    //
    if (ctx.matrix == NULL)
    {
        printf("Failed to parse binary cover file %s.\n", cover_path);
        return false;
    }

    return true;
}

static bool allocate_solution_buffer(int option_count, SolutionBuffer& buffer)
{
    buffer.reset();
    buffer.rows = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * option_count));
    if (buffer.rows == NULL)
    {
        printf("Unable to allocate solution buffer.\n");
        return false;
    }

    buffer.option_count = option_count;
    return true;
}

static bool setup_output_context(const char* solution_path, int item_count, OutputContext& ctx)
{
    ctx.reset();
    ctx.write_to_stdout = (strcmp(solution_path, "-") == 0);
    if (ctx.write_to_stdout)
    {
        ctx.stream = &std::cout;
    }
    else
    {
        auto file_stream = std::make_unique<std::ofstream>(solution_path, std::ios::binary);
        if (!file_stream->is_open())
        {
            printf("Unable to create output file %s.\n", solution_path);
            return false;
        }
        ctx.stream = file_stream.get();
        ctx.owned_file_stream = std::move(file_stream);
    }

    ctx.stdout_suppressed = ctx.write_to_stdout;
    if (ctx.stdout_suppressed)
    {
        dlx::Core::dlx_set_stdout_suppressed(true);
    }

    if (!ctx.stdout_suppressed)
    {
        ctx.console_sink = std::make_unique<dlx::OstreamSolutionSink>(std::cout);
        ctx.sink_router.add_sink(ctx.console_sink.get());
    }
    ctx.output.sink = ctx.sink_router.empty() ? nullptr : &ctx.sink_router;

    if (ctx.stream == nullptr
        || dlx::Core::dlx_enable_binary_solution_output(ctx.output,
                                                        *ctx.stream,
                                                        static_cast<uint32_t>(item_count))
            != 0)
    {
        printf("Failed to enable binary solution output.\n");
        ctx.reset();
        return false;
    }

    ctx.binary_output_enabled = true;
    return true;
}

/**
 * @param const char* The path to a binary cover file to read in DLXB format or a piped input stream
 * @param const char* The path to write a binary solution file in DLXS format or a piped output stream
 */
int handle_cli(const char* cover_path, const char* solution_path)
{
    CoverStream cover_stream;
    MatrixContext matrix_ctx;
    OutputContext output_ctx;
    SolutionBuffer solution_buffer;
    
    //
    if (!open_cover_stream(cover_path, cover_stream))
    {
        return EXIT_FAILURE;
    }

    //
    if (!build_matrix_context(*cover_stream.stream, cover_path, matrix_ctx))
    {
        return EXIT_FAILURE;
    }

    //
    if (!allocate_solution_buffer(matrix_ctx.option_count, solution_buffer))
    {
        return EXIT_FAILURE;
    }

    //
    if (!setup_output_context(solution_path, matrix_ctx.item_count, output_ctx))
    {
        return EXIT_FAILURE;
    }

    //
    dlx::Core::search(matrix_ctx.matrix,
                      0,
                      matrix_ctx.solutions,
                      solution_buffer.rows,
                      output_ctx.output);
    
    //
    output_ctx.disable_binary_output();

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
