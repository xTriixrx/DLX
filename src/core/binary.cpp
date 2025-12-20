#include "core/binary.h"
#include "core/dlx.h"
#include <arpa/inet.h>
#include <climits>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <utility>
#include <vector>

namespace dlx::binary {

namespace detail
{

uint16_t dlx_htons(uint16_t value)
{
    return htons(value);
}

uint32_t dlx_htonl(uint32_t value)
{
    return htonl(value);
}

uint16_t dlx_ntohs(uint16_t value)
{
    return ntohs(value);
}

uint32_t dlx_ntohl(uint32_t value)
{
    return ntohl(value);
}

class StreamBinaryReader
{
public:
    explicit StreamBinaryReader(std::istream& stream)
        : stream_(stream)
    {}

    bool read_exact(void* buffer, size_t bytes)
    {
        stream_.read(static_cast<char*>(buffer), static_cast<std::streamsize>(bytes));
        return static_cast<size_t>(stream_.gcount()) == bytes;
    }

    bool eof() const
    {
        return stream_.eof();
    }

private:
    std::istream& stream_;
};

class StreamBinaryWriter
{
public:
    explicit StreamBinaryWriter(std::ostream& stream)
        : stream_(stream)
    {}

    bool write_exact(const void* buffer, size_t bytes)
    {
        stream_.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(bytes));
        return stream_.good();
    }

private:
    std::ostream& stream_;
};

int ensure_chunk_capacity(struct DlxRowChunk* chunk, uint16_t required);
int ensure_solution_capacity(struct DlxSolutionRow* row, uint16_t required);
int write_cover_header(std::ostream& output, const struct DlxCoverHeader* header);
int read_cover_header(std::istream& input, struct DlxCoverHeader* header);
int write_row_chunk(std::ostream& output, uint32_t row_id, const uint32_t* columns, uint16_t column_count);
int read_row_chunk(std::istream& input, struct DlxRowChunk* chunk);
int write_solution_header(std::ostream& output, const struct DlxSolutionHeader* header);
int read_solution_header(std::istream& input, struct DlxSolutionHeader* header);
int write_solution_row(std::ostream& output,
                       uint32_t solution_id,
                       const uint32_t* row_indices,
                       uint16_t row_count);
int read_solution_row(std::istream& input, struct DlxSolutionRow* row);
void free_solution_row(struct DlxSolutionRow* row);
void free_row_chunk(struct DlxRowChunk* chunk);

} // namespace detail

DlxProblem::DlxProblem()
    : header{0}
    , rows()
{}

DlxProblem::~DlxProblem()
{
    clear();
}

DlxProblem::DlxProblem(DlxProblem&& other) noexcept
    : header{0}
    , rows()
{
    *this = std::move(other);
}

DlxProblem& DlxProblem::operator=(DlxProblem&& other) noexcept
{
    if (this != &other)
    {
        clear();
        header = other.header;
        rows = std::move(other.rows);
        other.header = DlxCoverHeader{0};
    }
    return *this;
}

void DlxProblem::clear()
{
    for (auto& row : rows)
    {
        detail::free_row_chunk(&row);
    }
    rows.clear();
    header = DlxCoverHeader{0};
}

DlxSolution::DlxSolution()
    : header{0}
    , rows()
{}

DlxSolution::~DlxSolution()
{
    clear();
}

DlxSolution::DlxSolution(DlxSolution&& other) noexcept
    : header{0}
    , rows()
{
    *this = std::move(other);
}

DlxSolution& DlxSolution::operator=(DlxSolution&& other) noexcept
{
    if (this != &other)
    {
        clear();
        header = other.header;
        rows = std::move(other.rows);
        other.header = DlxSolutionHeader{0};
    }
    return *this;
}

void DlxSolution::clear()
{
    for (auto& row : rows)
    {
        detail::free_solution_row(&row);
    }
    rows.clear();
    header = DlxSolutionHeader{0};
}

DlxProblemStreamReader::DlxProblemStreamReader(std::istream& input)
    : input_(&input)
    , scratch_{0}
    , remaining_rows_(0)
    , has_row_count_(false)
    , header_active_(false)
{}

DlxProblemStreamReader::~DlxProblemStreamReader()
{
    detail::free_row_chunk(&scratch_);
}

int DlxProblemStreamReader::read_header(struct DlxCoverHeader* header)
{
    int status = detail::read_cover_header(*input_, header);
    if (status != 0)
    {
        header_active_ = false;
        remaining_rows_ = 0;
        has_row_count_ = false;
        return status;
    }

    remaining_rows_ = header->row_count;
    has_row_count_ = (header->row_count > 0);
    header_active_ = true;
    return 0;
}

int DlxProblemStreamReader::read_chunk(struct DlxRowChunk* chunk)
{
    if (chunk == nullptr)
    {
        return -1;
    }

    if (!header_active_)
    {
        return -1;
    }

    if (has_row_count_ && remaining_rows_ == 0)
    {
        header_active_ = false;
        return 0;
    }

    int status = detail::read_row_chunk(*input_, chunk);
    if (status != 1)
    {
        if (status == 0 && !has_row_count_)
        {
            header_active_ = false;
            return 0;
        }
        return status;
    }
    if (has_row_count_ && remaining_rows_ > 0)
    {
        remaining_rows_ -= 1;
    }
    return 1;
}

int DlxProblemStreamReader::read_row(uint32_t* row_id, std::vector<uint32_t>* columns)
{
    if (row_id == nullptr || columns == nullptr)
    {
        return -1;
    }

    int status = read_chunk(&scratch_);
    if (status != 1)
    {
        return status;
    }

    *row_id = scratch_.row_id;
    columns->assign(scratch_.columns, scratch_.columns + scratch_.entry_count);
    return 1;
}

DlxProblemStreamWriter::DlxProblemStreamWriter(std::ostream& output, const struct DlxCoverHeader& header)
    : output_(&output)
    , remaining_rows_(0)
    , has_row_count_(false)
    , started_(false)
{
    start(header);
}

int DlxProblemStreamWriter::start(const struct DlxCoverHeader& header)
{
    remaining_rows_ = header.row_count;
    has_row_count_ = (header.row_count > 0);
    started_ = (detail::write_cover_header(*output_, &header) == 0);
    return started_ ? 0 : -1;
}

int DlxProblemStreamWriter::write_row(uint32_t row_id, const uint32_t* columns, uint16_t column_count)
{
    if (!started_)
    {
        return -1;
    }

    if (has_row_count_)
    {
        if (remaining_rows_ == 0)
        {
            return -1;
        }
        remaining_rows_ -= 1;
    }

    return detail::write_row_chunk(*output_, row_id, columns, column_count);
}

int DlxProblemStreamWriter::finish()
{
    started_ = false;
    remaining_rows_ = 0;
    has_row_count_ = false;
    return 0;
}

DlxSolutionStreamReader::DlxSolutionStreamReader(std::istream& input)
    : input_(&input)
    , scratch_{0}
    , header_active_(false)
{}

DlxSolutionStreamReader::~DlxSolutionStreamReader()
{
    detail::free_solution_row(&scratch_);
}

int DlxSolutionStreamReader::read_header(struct DlxSolutionHeader* header)
{
    int status = detail::read_solution_header(*input_, header);
    header_active_ = (status == 0);
    return status;
}

int DlxSolutionStreamReader::read_row(uint32_t* solution_id, std::vector<uint32_t>* row_indices)
{
    if (solution_id == nullptr || row_indices == nullptr)
    {
        return -1;
    }

    if (!header_active_)
    {
        return -1;
    }

    int status = detail::read_solution_row(*input_, &scratch_);
    if (status != 1)
    {
        return status;
    }

    if (scratch_.solution_id == 0 && scratch_.entry_count == 0)
    {
        header_active_ = false;
        return 0;
    }

    *solution_id = scratch_.solution_id;
    row_indices->assign(scratch_.row_indices, scratch_.row_indices + scratch_.entry_count);
    return 1;
}

DlxSolutionStreamWriter::DlxSolutionStreamWriter(std::ostream& output, const struct DlxSolutionHeader& header)
    : output_(&output)
    , next_solution_id_(1)
    , finished_(false)
    , started_(false)
{
    start(header);
}

int DlxSolutionStreamWriter::start(const struct DlxSolutionHeader& header)
{
    next_solution_id_ = 1;
    finished_ = false;
    started_ = (detail::write_solution_header(*output_, &header) == 0);
    return started_ ? 0 : -1;
}

int DlxSolutionStreamWriter::write_row(const uint32_t* row_indices, uint16_t row_count)
{
    if (!started_ || finished_)
    {
        return -1;
    }

    int status = detail::write_solution_row(*output_, next_solution_id_, row_indices, row_count);
    if (status == 0)
    {
        next_solution_id_ += 1;
    }
    return status;
}

int DlxSolutionStreamWriter::finish()
{
    if (!started_ || finished_)
    {
        return 0;
    }

    finished_ = true;
    return detail::write_solution_row(*output_, 0, nullptr, 0);
}

int detail::write_cover_header(std::ostream& output, const struct DlxCoverHeader* header)
{
    if (header == NULL)
    {
        return -1;
    }

    struct DlxCoverHeader writable = *header;
    writable.magic = detail::dlx_htonl(header->magic);
    writable.version = detail::dlx_htons(header->version);
    writable.flags = detail::dlx_htons(header->flags);
    writable.column_count = detail::dlx_htonl(header->column_count);
    writable.row_count = detail::dlx_htonl(header->row_count);

    detail::StreamBinaryWriter writer(output);
    return writer.write_exact(&writable, sizeof(writable)) ? 0 : -1;
}

int detail::read_cover_header(std::istream& input, struct DlxCoverHeader* header)
{
    if (header == NULL)
    {
        return -1;
    }

    detail::StreamBinaryReader reader(input);
    struct DlxCoverHeader readable;
    if (!reader.read_exact(&readable, sizeof(readable)))
    {
        return -1;
    }

    header->magic = detail::dlx_ntohl(readable.magic);
    header->version = detail::dlx_ntohs(readable.version);
    header->flags = detail::dlx_ntohs(readable.flags);
    header->column_count = detail::dlx_ntohl(readable.column_count);
    header->row_count = detail::dlx_ntohl(readable.row_count);
    return 0;
}

int detail::write_row_chunk(std::ostream& output, uint32_t row_id, const uint32_t* columns, uint16_t column_count)
{
    if (column_count > 0 && columns == NULL)
    {
        return -1;
    }

    uint32_t row_id_net = detail::dlx_htonl(row_id);
    uint16_t entry_count_net = detail::dlx_htons(column_count);

    detail::StreamBinaryWriter writer(output);
    if (!writer.write_exact(&row_id_net, sizeof(row_id_net)))
    {
        return -1;
    }

    if (!writer.write_exact(&entry_count_net, sizeof(entry_count_net)))
    {
        return -1;
    }

    for (uint16_t i = 0; i < column_count; i++)
    {
        uint32_t col_net = detail::dlx_htonl(columns[i]);
        if (!writer.write_exact(&col_net, sizeof(col_net)))
        {
            return -1;
        }
    }

    return 0;
}

int detail::ensure_chunk_capacity(struct DlxRowChunk* chunk, uint16_t required)
{
    if (chunk->capacity >= required)
    {
        return 0;
    }

    uint16_t new_capacity = chunk->capacity == 0 ? required : chunk->capacity;
    while (new_capacity < required)
    {
        if (new_capacity > UINT16_MAX / 2)
        {
            new_capacity = required;
            break;
        }
        new_capacity *= 2;
    }

    uint32_t* new_columns = static_cast<uint32_t*>(
        realloc(chunk->columns, sizeof(uint32_t) * new_capacity));
    if (new_columns == NULL)
    {
        return -1;
    }

    chunk->columns = new_columns;
    chunk->capacity = new_capacity;
    return 0;
}

int detail::read_row_chunk(std::istream& input, struct DlxRowChunk* chunk)
{
    if (chunk == NULL)
    {
        return -1;
    }

    detail::StreamBinaryReader reader(input);

    uint32_t row_id_net;
    if (!reader.read_exact(&row_id_net, sizeof(row_id_net)))
    {
        if (reader.eof())
        {
            return 0; // No more rows.
        }
        return -1;
    }

    uint16_t entry_count_net;
    if (!reader.read_exact(&entry_count_net, sizeof(entry_count_net)))
    {
        return -1;
    }

    uint16_t entry_count = detail::dlx_ntohs(entry_count_net);
    if (detail::ensure_chunk_capacity(chunk, entry_count) != 0)
    {
        return -1;
    }

    for (uint16_t i = 0; i < entry_count; i++)
    {
        uint32_t column_net;
        if (!reader.read_exact(&column_net, sizeof(column_net)))
        {
            return -1;
        }
        chunk->columns[i] = detail::dlx_ntohl(column_net);
    }

    chunk->row_id = detail::dlx_ntohl(row_id_net);
    chunk->entry_count = entry_count;
    return 1;
}

void detail::free_row_chunk(struct DlxRowChunk* chunk)
{
    if (chunk == NULL)
    {
        return;
    }

    free(chunk->columns);
    chunk->columns = NULL;
    chunk->entry_count = 0;
    chunk->capacity = 0;
}

int detail::write_solution_header(std::ostream& output, const struct DlxSolutionHeader* header)
{
    if (header == NULL)
    {
        return -1;
    }

    struct DlxSolutionHeader writable = *header;
    writable.magic = detail::dlx_htonl(header->magic);
    writable.version = detail::dlx_htons(header->version);
    writable.flags = detail::dlx_htons(header->flags);
    writable.column_count = detail::dlx_htonl(header->column_count);

    detail::StreamBinaryWriter writer(output);
    return writer.write_exact(&writable, sizeof(writable)) ? 0 : -1;
}

int detail::read_solution_header(std::istream& input, struct DlxSolutionHeader* header)
{
    if (header == NULL)
    {
        return -1;
    }

    detail::StreamBinaryReader reader(input);
    struct DlxSolutionHeader readable;
    if (!reader.read_exact(&readable, sizeof(readable)))
    {
        return -1;
    }

    header->magic = detail::dlx_ntohl(readable.magic);
    header->version = detail::dlx_ntohs(readable.version);
    header->flags = detail::dlx_ntohs(readable.flags);
    header->column_count = detail::dlx_ntohl(readable.column_count);
    return 0;
}

int detail::write_solution_row(std::ostream& output,
                           uint32_t solution_id,
                           const uint32_t* row_indices,
                           uint16_t row_count)
{
    if (row_count > 0 && row_indices == NULL)
    {
        return -1;
    }

    uint32_t id_net = detail::dlx_htonl(solution_id);
    uint16_t count_net = detail::dlx_htons(row_count);

    detail::StreamBinaryWriter writer(output);
    if (!writer.write_exact(&id_net, sizeof(id_net)))
    {
        return -1;
    }

    if (!writer.write_exact(&count_net, sizeof(count_net)))
    {
        return -1;
    }

    for (uint16_t i = 0; i < row_count; i++)
    {
        uint32_t value_net = detail::dlx_htonl(row_indices[i]);
        if (!writer.write_exact(&value_net, sizeof(value_net)))
        {
            return -1;
        }
    }

    return 0;
}

int detail::ensure_solution_capacity(struct DlxSolutionRow* row, uint16_t required)
{
    if (row->capacity >= required)
    {
        return 0;
    }

    uint16_t new_capacity = row->capacity == 0 ? required : row->capacity;
    while (new_capacity < required)
    {
        if (new_capacity > UINT16_MAX / 2)
        {
            new_capacity = required;
            break;
        }

        new_capacity *= 2;
    }

    uint32_t* new_indices = static_cast<uint32_t*>(
        realloc(row->row_indices, sizeof(uint32_t) * new_capacity));
    if (new_indices == NULL)
    {
        return -1;
    }

    row->row_indices = new_indices;
    row->capacity = new_capacity;
    return 0;
}

int detail::read_solution_row(std::istream& input, struct DlxSolutionRow* row)
{
    if (row == NULL)
    {
        return -1;
    }

    detail::StreamBinaryReader reader(input);

    uint32_t solution_id_net;
    uint16_t count_net;

    if (!reader.read_exact(&solution_id_net, sizeof(solution_id_net)))
    {
        if (reader.eof())
        {
            row->entry_count = 0;
            return 0;
        }
        return -1;
    }

    if (!reader.read_exact(&count_net, sizeof(count_net)))
    {
        return -1;
    }

    uint16_t count = detail::dlx_ntohs(count_net);
    if (detail::ensure_solution_capacity(row, count) != 0)
    {
        return -1;
    }

    for (uint16_t i = 0; i < count; i++)
    {
        uint32_t value_net;
        if (!reader.read_exact(&value_net, sizeof(value_net)))
        {
            return -1;
        }

        row->row_indices[i] = detail::dlx_ntohl(value_net);
    }

    row->solution_id = detail::dlx_ntohl(solution_id_net);
    row->entry_count = count;
    return 1;
}

void detail::free_solution_row(struct DlxSolutionRow* row)
{
    if (row == NULL)
    {
        return;
    }

    free(row->row_indices);
    row->row_indices = NULL;
    row->entry_count = 0;
    row->capacity = 0;
}

int dlx_read_problem(std::istream& input, struct DlxProblem* problem)
{
    if (problem == NULL)
    {
        return -1;
    }

    problem->clear();

    if (detail::read_cover_header(input, &problem->header) != 0)
    {
        return -1;
    }

    if (problem->header.row_count > 0)
    {
        problem->rows.resize(problem->header.row_count);
    }

    for (uint32_t i = 0; i < problem->header.row_count; i++)
    {
        int status = detail::read_row_chunk(input, &problem->rows[i]);
        if (status != 1)
        {
            problem->clear();
            return -1;
        }
    }

    return 0;
}

int dlx_read_solution(std::istream& input, struct DlxSolution* solution)
{
    if (solution == NULL)
    {
        return -1;
    }

    solution->clear();

    if (detail::read_solution_header(input, &solution->header) != 0)
    {
        return -1;
    }

    while (true)
    {
        DlxSolutionRow row = {0};
        int status = detail::read_solution_row(input, &row);
        if (status == 0)
        {
            break;
        }
        if (status == -1)
        {
            detail::free_solution_row(&row);
            solution->clear();
            return -1;
        }

        if (row.solution_id == 0 && row.entry_count == 0)
        {
            detail::free_solution_row(&row);
            break;
        }

        solution->rows.push_back(row);
    }

    return 0;
}

int dlx_write_problem(std::ostream& output, const struct DlxProblem* problem)
{
    if (problem == NULL)
    {
        return -1;
    }

    if (problem->rows.size() > UINT32_MAX)
    {
        return -1;
    }

    DlxCoverHeader header = problem->header;
    header.row_count = static_cast<uint32_t>(problem->rows.size());

    if (detail::write_cover_header(output, &header) != 0)
    {
        return -1;
    }

    for (const auto& row : problem->rows)
    {
        if (detail::write_row_chunk(output, row.row_id, row.columns, row.entry_count) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int dlx_write_solution(std::ostream& output, const struct DlxSolution* solution)
{
    if (solution == NULL)
    {
        return -1;
    }

    if (detail::write_solution_header(output, &solution->header) != 0)
    {
        return -1;
    }

    for (const auto& row : solution->rows)
    {
        if (detail::write_solution_row(output, row.solution_id, row.row_indices, row.entry_count) != 0)
        {
            return -1;
        }
    }

    return 0;
}


struct node* dlx_read_binary(std::istream& input,
                             char*** solutions_out,
                             int* item_count_out,
                             int* option_count_out)
{
    if (solutions_out == NULL || item_count_out == NULL || option_count_out == NULL)
    {
        return NULL;
    }

    DlxProblem problem;
    if (dlx_read_problem(input, &problem) != 0)
    {
        return NULL;
    }

    return dlx::Core::generateMatrixBinary(problem,
                                           solutions_out,
                                           item_count_out,
                                           option_count_out);
}

} // namespace dlx::binary
