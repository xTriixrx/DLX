#include "ascii_binary_utils.h"
#include "core/binary.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace binary = dlx::binary;

namespace
{

std::string strip_carriage_returns(const std::string& line)
{
    std::string result = line;
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
    return result;
}

std::vector<std::string> tokenize(const std::string& line)
{
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

} // namespace

int ascii_cover_to_binary_stream(const std::string& ascii_cover, std::ostream& output)
{
    std::istringstream input(ascii_cover);
    std::string raw_line;

    if (!std::getline(input, raw_line))
    {
        return -1;
    }

    std::vector<std::string> headers = tokenize(strip_carriage_returns(raw_line));
    if (headers.empty())
    {
        return -1;
    }

    const uint32_t column_count = static_cast<uint32_t>(headers.size());
    std::vector<std::vector<uint32_t>> rows;

    while (std::getline(input, raw_line))
    {
        std::string line = strip_carriage_returns(raw_line);
        if (line.empty())
        {
            continue;
        }

        std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty())
        {
            continue;
        }

        if (tokens.size() != column_count)
        {
            return -1;
        }

        std::vector<uint32_t> columns;
        columns.reserve(tokens.size());
        for (uint32_t column_index = 0; column_index < column_count; ++column_index)
        {
            const std::string& token = tokens[column_index];
            if (token == "1")
            {
                columns.push_back(column_index);
            }
            else if (token == "0")
            {
                continue;
            }
            else
            {
                return -1;
            }
        }

        rows.emplace_back(std::move(columns));
    }

    binary::DlxProblem problem;
    problem.header = {
        .magic = DLX_COVER_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = column_count,
        .row_count = static_cast<uint32_t>(rows.size()),
    };

    problem.rows.reserve(rows.size());
    for (size_t row_index = 0; row_index < rows.size(); ++row_index)
    {
        const std::vector<uint32_t>& columns = rows[row_index];
        if (columns.size() > static_cast<size_t>(UINT16_MAX))
        {
            return -1;
        }

        binary::DlxRowChunk chunk = {0};
        chunk.row_id = static_cast<uint32_t>(row_index + 1);
        chunk.entry_count = static_cast<uint16_t>(columns.size());
        chunk.capacity = chunk.entry_count;
        if (!columns.empty())
        {
            chunk.columns = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * chunk.entry_count));
            if (chunk.columns == nullptr)
            {
                return -1;
            }
            for (uint16_t i = 0; i < chunk.entry_count; ++i)
            {
                chunk.columns[i] = columns[i];
            }
        }

        problem.rows.push_back(chunk);
    }

    return binary::dlx_write_problem(output, &problem);
}

int binary_solution_to_ascii(std::istream& input, std::string* ascii_output)
{
    if (ascii_output == nullptr)
    {
        return -1;
    }

    binary::DlxSolution solution;
    if (binary::dlx_read_solution(input, &solution) != 0)
    {
        return -1;
    }

    std::ostringstream builder;
    for (const auto& row : solution.rows)
    {
        for (int i = 0; i < row.entry_count; ++i)
        {
            builder << row.row_indices[i];
            builder << (i + 1 == row.entry_count ? '\n' : ' ');
        }
    }

    *ascii_output = builder.str();
    return 0;
}
