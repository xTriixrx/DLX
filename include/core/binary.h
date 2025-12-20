#ifndef CORE_BINARY_H
#define CORE_BINARY_H

#include <stdint.h>
#include <istream>
#include <ostream>
#include <vector>
#include "core/dlx.h"

namespace dlx::binary {

/** @brief Magic constant that prefixes serialized cover headers (ASCII 'DLXB'). */
#define DLX_COVER_MAGIC 0x444C5842u /* 'DLXB' */

/** @brief Magic constant that prefixes serialized solution sections (ASCII 'DLXS'). */
#define DLX_SOLUTION_MAGIC 0x444C5853u /* 'DLXS' */

/** @brief Version of the DLX binary interchange format understood by this library. */
#define DLX_BINARY_VERSION 1

/**
 * @brief Binary file preamble describing the cover matrix serialization.
 */
struct DlxCoverHeader
{
    uint32_t magic;             /**< Magic constant (DLX_COVER_MAGIC). */
    uint16_t version;           /**< File format version, see DLX_BINARY_VERSION. */
    uint16_t flags;             /**< Reserved flags for future binary features. */
    uint32_t column_count;      /**< Number of constraint columns in the matrix. */
    uint32_t row_count;         /**< Number of option rows present in the file. */
};

/**
 * @brief Chunked streaming representation of a DLX row.
 */
struct DlxRowChunk
{
    uint32_t row_id;        /**< Unique identifier for the serialized row. */
    uint16_t entry_count;   /**< Number of populated column indices. */
    uint16_t capacity;      /**< Allocated capacity for the @ref columns buffer. */
    uint32_t* columns;      /**< Array of column indices enabled by this row. */
};

/**
 * @brief Header that prefixes solution metadata before explicit rows.
 */
struct DlxSolutionHeader
{
    uint32_t magic;        /**< Magic constant (DLX_SOLUTION_MAGIC). */
    uint16_t version;      /**< File format version (DLX_BINARY_VERSION). */
    uint16_t flags;        /**< Reserved flags for future binary features. */
    uint32_t column_count; /**< Number of columns required to interpret rows. */
};

/**
 * @brief Serialized representation of a DLX solution row.
 */
struct DlxSolutionRow
{
    uint32_t solution_id;  /**< Sequential identifier for the solution. */
    uint16_t entry_count;  /**< Number of row indices stored in @ref row_indices. */
    uint16_t capacity;     /**< Allocated capacity for the @ref row_indices buffer. */
    uint32_t* row_indices; /**< References to the rows that compose this solution. */
};

/**
 * @brief RAII-owned aggregate for a DLX cover problem.
 *
 * Owns the row chunk buffers and releases them in the destructor.
 * Move-only to prevent double-free of row buffers.
 */
struct DlxProblem
{
    DlxCoverHeader header;          /**< Cover header metadata. */
    std::vector<DlxRowChunk> rows;  /**< Row chunk data sized to header.row_count. */

    DlxProblem();
    ~DlxProblem();

    DlxProblem(const DlxProblem&) = delete;
    DlxProblem& operator=(const DlxProblem&) = delete;
    DlxProblem(DlxProblem&& other) noexcept;
    DlxProblem& operator=(DlxProblem&& other) noexcept;

    void clear();
};

/**
 * @brief RAII-owned aggregate for a DLX solution set.
 *
 * Owns the solution row buffers and releases them in the destructor.
 * Move-only to prevent double-free of row buffers.
 */
struct DlxSolution
{
    DlxSolutionHeader header;           /**< Solution header metadata. */
    std::vector<DlxSolutionRow> rows;   /**< Solution rows read from the stream. */

    DlxSolution();
    ~DlxSolution();

    DlxSolution(const DlxSolution&) = delete;
    DlxSolution& operator=(const DlxSolution&) = delete;
    DlxSolution(DlxSolution&& other) noexcept;
    DlxSolution& operator=(DlxSolution&& other) noexcept;

    void clear();
};

/**
 * @brief Streaming reader for DLX cover problems.
 *
 * Reads the cover header once, then returns one row at a time.
 */
class DlxProblemStreamReader
{
public:
    explicit DlxProblemStreamReader(std::istream& input);
    ~DlxProblemStreamReader();

    DlxProblemStreamReader(const DlxProblemStreamReader&) = delete;
    DlxProblemStreamReader& operator=(const DlxProblemStreamReader&) = delete;

    int read_header(struct DlxCoverHeader* header);
    int read_chunk(struct DlxRowChunk* chunk);
    int read_row(uint32_t* row_id, std::vector<uint32_t>* columns);

private:
    std::istream* input_;
    DlxRowChunk scratch_;
    uint32_t remaining_rows_;
    bool has_row_count_;
    bool header_active_;
};

/**
 * @brief Streaming writer for DLX cover problems.
 *
 * Writes the header on construction and supports appending rows.
 */
class DlxProblemStreamWriter
{
public:
    DlxProblemStreamWriter(std::ostream& output, const struct DlxCoverHeader& header);

    DlxProblemStreamWriter(const DlxProblemStreamWriter&) = delete;
    DlxProblemStreamWriter& operator=(const DlxProblemStreamWriter&) = delete;

    /** @brief Start a new problem on the same stream by writing a fresh header. */
    int start(const struct DlxCoverHeader& header);
    int write_row(uint32_t row_id, const uint32_t* columns, uint16_t column_count);
    /** @brief Finish the current problem and allow a new header to be written. */
    int finish();

private:
    std::ostream* output_;
    uint32_t remaining_rows_;
    bool has_row_count_;
    bool started_;
};

/**
 * @brief Streaming reader for DLX solutions.
 *
 * Reads the solution header once, then returns one solution row at a time.
 */
class DlxSolutionStreamReader
{
public:
    explicit DlxSolutionStreamReader(std::istream& input);
    ~DlxSolutionStreamReader();

    DlxSolutionStreamReader(const DlxSolutionStreamReader&) = delete;
    DlxSolutionStreamReader& operator=(const DlxSolutionStreamReader&) = delete;

    int read_header(struct DlxSolutionHeader* header);
    int read_row(uint32_t* solution_id, std::vector<uint32_t>* row_indices);

private:
    std::istream* input_;
    DlxSolutionRow scratch_;
    bool header_active_;
};

/**
 * @brief Streaming writer for DLX solutions.
 *
 * Writes the header on construction, emits rows, and can write a terminator row.
 */
class DlxSolutionStreamWriter
{
public:
    DlxSolutionStreamWriter(std::ostream& output, const struct DlxSolutionHeader& header);

    DlxSolutionStreamWriter(const DlxSolutionStreamWriter&) = delete;
    DlxSolutionStreamWriter& operator=(const DlxSolutionStreamWriter&) = delete;

    /** @brief Start a new solution stream on the same writer instance. */
    int start(const struct DlxSolutionHeader& header);
    int write_row(const uint32_t* row_indices, uint16_t row_count);
    /** @brief Write the terminator row and allow a new header to be written. */
    int finish();

private:
    std::ostream* output_;
    uint32_t next_solution_id_;
    bool finished_;
    bool started_;
};

// Read API
/**
 * @brief Read a full DLX cover problem into a RAII-owned aggregate.
 *
 * Reads the cover header and exactly header.row_count row chunks.
 */
int dlx_read_problem(std::istream& input, struct DlxProblem* problem);
/**
 * @brief Read a full DLX solution stream into a RAII-owned aggregate.
 *
 * Reads the solution header and consumes rows until EOF or a terminating
 * empty row (solution_id == 0 and entry_count == 0).
 */
int dlx_read_solution(std::istream& input, struct DlxSolution* solution);
struct node* dlx_read_binary(std::istream& input,
                             char*** solutions_out,
                             int* item_count_out,
                             int* option_count_out);
// Write API
/**
 * @brief Write a full DLX cover problem from a RAII-owned aggregate.
 */
int dlx_write_problem(std::ostream& output, const struct DlxProblem* problem);
/**
 * @brief Write a full DLX solution stream from a RAII-owned aggregate.
 */
int dlx_write_solution(std::ostream& output, const struct DlxSolution* solution);

} // namespace dlx::binary

#endif
