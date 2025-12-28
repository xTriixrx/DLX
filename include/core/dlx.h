#ifndef DLX_H
#define DLX_H

#include <vector>
#include <stdio.h>
#include <istream>
#include <ostream>
#include <stdint.h>
#include "core/solution_sink.h"

// Forward declaration of binary types.
namespace dlx::binary {
struct DlxCoverHeader;
struct DlxProblem;
struct DlxRowChunk;
class DlxSolutionStreamWriter;
} // namespace dlx::binary

/**************************************************************************************************************
 *                                            DLX Application                                                 *
 *        DLX is a powerful backtracking, depth-first algorithm that solves exact cover problems.             *
 *                                                                                                            * 
 * Version: 0.0.1                                                                                             *
 * Author: Vincent Nigro                                                                                      *
 * Last Modified: 3/12/22                                                                                     *
 **************************************************************************************************************/

/**
 * @brief Intrusive Dancing Links node used to model the sparse exact-cover matrix.
 *
 * Each node participates in four doubly linked lists (up/down/left/right) anchored by
 * a column header in @ref generateHeadNode. Column headers maintain the current column
 * length through @ref len while @ref data stores metadata such as row index or column id.
 */
struct node
{
    int len;              /**< Number of nodes currently linked beneath this column header. */
    int data;             /**< Column or row identifier associated with this node. */
    struct node* top;     /**< Column header for this node. */
    struct node* up;      /**< Pointer to the previous node in the column. */
    struct node* down;    /**< Pointer to the next node in the column. */
    struct node* left;    /**< Pointer to the previous node in the row. */
    struct node* right;   /**< Pointer to the next node in the row. */
};

namespace dlx {

struct SolutionOutput
{
    using BinaryRowCallback = void (*)(void* ctx, const uint32_t* row_ids, int level);

    sink::SolutionSink* sink;
    std::ostream* binary_stream;
    BinaryRowCallback binary_callback;
    void* binary_context;
    uint32_t next_solution_id;
    uint32_t column_count;
    std::vector<std::vector<uint32_t>> binary_rows;
    dlx::binary::DlxSolutionStreamWriter* binary_writer;

    SolutionOutput()
        : sink(nullptr)
        , binary_stream(nullptr)
        , binary_callback(nullptr)
        , binary_context(nullptr)
        , next_solution_id(1)
        , column_count(0)
        , binary_rows()
        , binary_writer(nullptr)
    {}
    void emit_binary_row(const uint32_t* row_ids, int level);
};

class Core
{
public:
    static struct node* generateMatrixBinary(struct dlx::binary::DlxProblem& problem,
                                             char*** solutions_out,
                                             int* item_count_out,
                                             int* option_count_out);
    static struct node* generateMatrixBinaryFromRows(const struct dlx::binary::DlxCoverHeader& header,
                                                     std::vector<dlx::binary::DlxRowChunk>& rows,
                                                     char*** solutions_out,
                                                     int* item_count_out,
                                                     int* option_count_out);
    static void setMatrixDumpStream(std::ostream* stream);
    static void search(struct node*, int, char**, uint32_t*, SolutionOutput&);
    static void freeMemory(struct node*, char**);
    static int dlx_enable_binary_solution_output(SolutionOutput& output_ctx, std::ostream& output, uint32_t column_count);
    static void dlx_disable_binary_solution_output(SolutionOutput& output_ctx);
    static void dlx_set_stdout_suppressed(bool suppressed);

private:
    static void hide(struct node*);
    static void cover(struct node*);
    static void unhide(struct node*);
    static void uncover(struct node*);
    static void printSolutions(char**, const uint32_t*, int, SolutionOutput&);
    static struct node* pickConstraint(struct node*);
    static struct node* generateMatrixBinaryImpl(const struct dlx::binary::DlxCoverHeader& header,
                                                 std::vector<dlx::binary::DlxRowChunk>& rows,
                                                 char*** solutions_out,
                                                 int* item_count_out,
                                                 int* option_count_out);
};

} // namespace dlx

#endif
