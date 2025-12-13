#ifndef DLX_H
#define DLX_H

#include <stdint.h>
#include <stdio.h>
#include <istream>
#include <ostream>
#include <vector>
#include "core/solution_sink.h"

struct DlxCoverHeader;

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

    SolutionSink* sink;
    FILE* binary_file;
    BinaryRowCallback binary_callback;
    void* binary_context;
    uint32_t next_solution_id;
    uint32_t column_count;

    SolutionOutput()
        : sink(nullptr)
        , binary_file(nullptr)
        , binary_callback(nullptr)
        , binary_context(nullptr)
        , next_solution_id(1)
        , column_count(0)
    {}
    void emit_binary_row(const uint32_t* row_ids, int level);
};

class Core
{
public:
    static int getNodeCount(FILE*);
    static int getItemCount(FILE*);
    static int getOptionsCount(FILE*);
    static struct node* generateMatrix(FILE*, int);
    static struct node* generateMatrixBinary(std::istream& input,
                                             const struct DlxCoverHeader& header,
                                             char*** solutions_out,
                                             int* item_count_out,
                                             int* option_count_out);
    static void setMatrixDumpStream(std::ostream* stream);
    static void search(struct node*, int, char**, uint32_t*, SolutionOutput&);
    static void freeMemory(struct node*, char**);
    static int dlx_enable_binary_solution_output(SolutionOutput& output_ctx, FILE* output, uint32_t column_count);
    static void dlx_disable_binary_solution_output(SolutionOutput& output_ctx);
    static void dlx_set_stdout_suppressed(bool suppressed);

private:
    static void hide(struct node*);
    static void cover(struct node*);
    static void unhide(struct node*);
    static void uncover(struct node*);
    static int getOptionNodesCount(FILE*);
    static void printSolutions(char**, const uint32_t*, int, SolutionOutput&);
    static struct node* generateHeadNode(int);
    static struct node* pickItem(struct node*);
    static int generateTitles(struct node*, char*);
    static void handleSpacerNodes(struct node*, int*, int, int);
    static struct node* generateMatrixBinaryImpl(std::istream& input,
                                                 const struct DlxCoverHeader& header,
                                                 char*** solutions_out,
                                                 int* item_count_out,
                                                 int* option_count_out);
};

} // namespace dlx

#endif
