#ifndef DLX_UTIL_H
#define DLX_UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <istream>
#include <memory>
#include <ostream>

#include "core/dlx.h"

struct CoverStream
{
    std::istream* stream = nullptr;
    std::unique_ptr<std::istream> owner;
};

struct MatrixContext
{
    struct node* matrix = nullptr;
    char** solutions = nullptr;
    int item_count = 0;
    int option_count = 0;

    MatrixContext() = default;
    ~MatrixContext()
    {
        reset();
    }

    MatrixContext(const MatrixContext&) = delete;
    MatrixContext& operator=(const MatrixContext&) = delete;

    void reset()
    {
        if (matrix != nullptr || solutions != nullptr)
        {
            dlx::Core::freeMemory(matrix, solutions);
        }
        matrix = nullptr;
        solutions = nullptr;
        item_count = 0;
        option_count = 0;
    }
};

struct SolutionBuffer
{
    uint32_t* rows = nullptr;
    int option_count = 0;

    SolutionBuffer() = default;
    ~SolutionBuffer()
    {
        reset();
    }

    SolutionBuffer(const SolutionBuffer&) = delete;
    SolutionBuffer& operator=(const SolutionBuffer&) = delete;

    void reset()
    {
        if (rows != nullptr)
        {
            free(rows);
        }
        rows = nullptr;
        option_count = 0;
    }
};

struct OutputContext
{
    dlx::SolutionOutput output {};
    std::ostream* stream = nullptr;
    std::unique_ptr<std::ofstream> owned_file_stream;
    bool write_to_stdout = false;
    bool stdout_suppressed = false;
    bool binary_output_enabled = false;
    std::unique_ptr<dlx::OstreamSolutionSink> console_sink;
    dlx::CompositeSolutionSink sink_router;

    OutputContext() = default;
    ~OutputContext()
    {
        reset();
    }

    OutputContext(const OutputContext&) = delete;
    OutputContext& operator=(const OutputContext&) = delete;

    void disable_binary_output()
    {
        if (binary_output_enabled)
        {
            dlx::Core::dlx_disable_binary_solution_output(output);
            binary_output_enabled = false;
        }
    }

    void reset()
    {
        disable_binary_output();
        if (stdout_suppressed)
        {
            dlx::Core::dlx_set_stdout_suppressed(false);
            stdout_suppressed = false;
        }

        if (stream != nullptr)
        {
            if (write_to_stdout)
            {
                stream->flush();
            }
            else
            {
                if (owned_file_stream)
                {
                    owned_file_stream->close();
                    owned_file_stream.reset();
                }
            }
        }

        console_sink.reset();
        owned_file_stream.reset();
        sink_router = dlx::CompositeSolutionSink();
        stream = nullptr;
        write_to_stdout = false;
        stdout_suppressed = false;
        binary_output_enabled = false;
        output = {};
    }
};

#endif
