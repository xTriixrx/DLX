#include "ascii_binary_utils.h"
#include "core/dlx_binary.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <sstream>
#include <vector>

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

std::string read_stream(FILE* input)
{
    if (input == nullptr)
    {
        return {};
    }

    std::string data;
    std::vector<char> buffer(4096);
    size_t bytes = 0;
    while ((bytes = fread(buffer.data(), 1, buffer.size(), input)) > 0)
    {
        data.append(buffer.data(), static_cast<size_t>(bytes));
    }
    return data;
}

} // namespace

int ascii_cover_to_binary_stream(const std::string& ascii_cover, FILE* output)
{
    if (output == nullptr)
    {
        return -1;
    }

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

    struct DlxCoverHeader header = {
        .magic = DLX_COVER_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = column_count,
        .row_count = static_cast<uint32_t>(rows.size()),
    };

    if (dlx_write_cover_header(output, &header) != 0)
    {
        return -1;
    }

    for (size_t row_index = 0; row_index < rows.size(); ++row_index)
    {
        const std::vector<uint32_t>& columns = rows[row_index];
        if (columns.size() > static_cast<size_t>(UINT16_MAX))
        {
            return -1;
        }

        const uint32_t* column_ptr = columns.empty() ? nullptr : columns.data();
        if (dlx_write_row_chunk(output,
                                static_cast<uint32_t>(row_index + 1),
                                column_ptr,
                                static_cast<uint16_t>(columns.size()))
            != 0)
        {
            return -1;
        }
    }

    return 0;
}

int binary_solution_to_ascii(FILE* input, std::string* ascii_output)
{
    if (input == nullptr || ascii_output == nullptr)
    {
        return -1;
    }

    std::string payload = read_stream(input);
    std::istringstream stream(payload);

    struct DlxSolutionHeader header;
    if (dlx_read_solution_header(stream, &header) != 0)
    {
        return -1;
    }

    std::ostringstream builder;
    struct DlxSolutionRow row = {0};
    while (true)
    {
        int status = dlx_read_solution_row(stream, &row);
        if (status == 0)
        {
            break;
        }
        if (status == -1)
        {
            dlx_free_solution_row(&row);
            return -1;
        }

        for (int i = 0; i < row.entry_count; ++i)
        {
            builder << row.row_indices[i];
            builder << (i + 1 == row.entry_count ? '\n' : ' ');
        }
    }

    dlx_free_solution_row(&row);
    *ascii_output = builder.str();
    return 0;
}
