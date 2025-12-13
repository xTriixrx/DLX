#ifndef DLX_BINARY_H
#define DLX_BINARY_H

#include <stdint.h>
#include <stdio.h>
#include <istream>
#include "core/dlx.h"

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

int dlx_write_cover_header(FILE* output, const struct DlxCoverHeader* header);
int dlx_read_cover_header(std::istream& input, struct DlxCoverHeader* header);
int dlx_write_row_chunk(FILE* output, uint32_t row_id, const uint32_t* columns, uint16_t column_count);
int dlx_read_row_chunk(std::istream& input, struct DlxRowChunk* chunk);
void dlx_free_row_chunk(struct DlxRowChunk* chunk);
int dlx_write_solution_header(FILE* output, const struct DlxSolutionHeader* header);
int dlx_read_solution_header(std::istream& input, struct DlxSolutionHeader* header);
int dlx_write_solution_row(FILE* output,
                           uint32_t solution_id,
                           const uint32_t* row_indices,
                           uint16_t row_count);
int dlx_read_solution_row(std::istream& input, struct DlxSolutionRow* row);
void dlx_free_solution_row(struct DlxSolutionRow* row);
struct node* dlx_read_binary(std::istream& input,
                             char*** solutions_out,
                             int* item_count_out,
                             int* option_count_out);

#endif
