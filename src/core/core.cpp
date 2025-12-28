#include "core/dlx.h"
#include "core/binary.h"
#include "core/solution_sink.h"
#include "core/text.h"
#include "core/matrix.h"
#include <stdio.h>
#include <iostream>
#include <wchar.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <limits.h>
#include <new>
#include <sys/stat.h>

namespace {

void initialize_column_headers(struct node* matrix, uint32_t column_count)
{
    matrix[0].right = matrix;
    matrix[0].left = matrix;

    for (uint32_t i = 0; i < column_count; ++i)
    {
        struct node* column = &matrix[i + 1];
        column->len = 0;
        column->top = matrix;
        column->left = &matrix[i];
        column->right = matrix;
        column->up = column;
        column->down = column;
        column->data = static_cast<int>(i + 1);

        matrix[i].right = column;
        matrix[0].left = column;
    }
}

} // namespace

namespace dlx {

bool g_suppress_stdout_output = false;
std::ostream* g_matrix_dump_stream = nullptr;

void Core::dlx_set_stdout_suppressed(bool suppressed)
{
    g_suppress_stdout_output = suppressed;
}

void Core::setMatrixDumpStream(std::ostream* stream)
{
    g_matrix_dump_stream = stream;
}

void SolutionOutput::emit_binary_row(const uint32_t* row_ids, int level)
{
    if (binary_writer == nullptr || level <= 0)
    {
        if (binary_callback != nullptr)
        {
            binary_callback(binary_context, row_ids, level);
        }
        return;
    }

    if (level > UINT16_MAX)
    {
        fprintf(stderr, "Solution row count exceeds binary format limit\n");
        return;
    }

    if (binary_writer->write_row(row_ids, static_cast<uint16_t>(level)) != 0)
    {
        fprintf(stderr, "Failed to write binary solution row\n");
        return;
    }
    next_solution_id += 1;

    if (binary_callback != nullptr)
    {
        binary_callback(binary_context, row_ids, level);
    }
}

int Core::dlx_enable_binary_solution_output(SolutionOutput& output_ctx, std::ostream& output, uint32_t column_count)
{
    binary::DlxSolutionHeader header = {
        .magic = DLX_SOLUTION_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = column_count,
    };

    delete output_ctx.binary_writer;
    output_ctx.binary_writer = new (std::nothrow) binary::DlxSolutionStreamWriter(output, header);
    if (output_ctx.binary_writer == nullptr)
    {
        fprintf(stderr, "Unable to allocate DLX solution writer\n");
        return -1;
    }

    output_ctx.binary_stream = &output;
    output_ctx.column_count = column_count;
    output_ctx.next_solution_id = 1;
    output_ctx.binary_rows.clear();
    return 0;
}

void Core::dlx_disable_binary_solution_output(SolutionOutput& output_ctx)
{
    if (output_ctx.binary_writer != nullptr)
    {
        if (output_ctx.binary_writer->finish() != 0)
        {
            fprintf(stderr, "Failed to write binary solution output\n");
        }
        delete output_ctx.binary_writer;
        output_ctx.binary_writer = nullptr;
    }

    output_ctx.binary_stream = nullptr;
    output_ctx.next_solution_id = 1;
    output_ctx.column_count = 0;
    output_ctx.binary_rows.clear();
}

/**************************************************************************************************************
 *                                            DLX Application                                                 *
 *        DLX is a powerful backtracking, depth-first algorithm that solves exact cover problems.             *
 *                                                                                                            * 
 * Version: 0.0.1                                                                                             *
 * Author: Vincent Nigro                                                                                      *
 * Last Modified: 3/12/22                                                                                     *
 **************************************************************************************************************/

/**
 * DLX is a powerful backtracking, depth-first algorithm that solves exact cover problems. Exact cover problems
 * can represent a wide range of applications such as a sudoku solver, to scheduling based applications.
 * 
 * @param struct node* A node pointer to the head of the matrix.
 * @param int An integer representing the current level of the recursive search.
 * @param char** A char pointer to pointers containing partials of a solutions.
 * @return void
 */ 
void Core::search(struct node* head, int level, char** solutions, uint32_t* row_ids, SolutionOutput& output)
{
    // If all items have been covered, output a found solution.
    if (head->right == head)
    {
        printSolutions(solutions, row_ids, level, output);
        return;
    }
    
    // Pick an item i (column constraint), and cover the item.
    struct node* constraint = pickConstraint(head);
    cover(constraint);
    
    // Pick an option xl (row), and set potential partial solution
    struct node* option = constraint->down;
    struct node* optionNumber = option;
    
    // Traverse through matrix until the choosen options' associated spacer node is found
    while (optionNumber->data > 0)
    {
        optionNumber += 1;
    }

    uint32_t optionRowId = static_cast<uint32_t>(abs(optionNumber->data));
    // Create string for potential partial of solution
    char* optionStr = static_cast<char*>(malloc(snprintf(NULL, 0, "%u", optionRowId) + 1));
    sprintf(optionStr, "%u", optionRowId);
    solutions[level] = optionStr;
    row_ids[level] = optionRowId;

    // While node of a particular option row doesn't loop back to item node.
    struct node* optionPart;
    while (option != constraint)
    {
        // Select next part of current option;
        optionPart = option + 1;

        // Cover each option parts' column until the options' space node is reached
        while (optionPart != option)
        {
            struct node* optionColumn = optionPart->top;

            if (optionColumn == head) // spacer has been reached
            {
                optionPart = optionPart->up;
            }
            else
            {
                cover(optionColumn);
                optionPart += 1;
            }
        }
        
        // Recursively search for potential solutions...
        search(head, level + 1, solutions, row_ids, output);

        // Iterate optionPart to options' spacer node
        optionPart = option + 1;
        while (optionPart->data > 0)
        {
            optionPart += 1;
        }
        optionPart -= 1; // Step back one to options' last actual node

        // Uncover each option parts' column until the previous options' space node is reached
        while (optionPart != option)
        {
            struct node* optionColumn = optionPart->top;

            if (optionColumn == head) // Previous options' spacer has been reached.
            {
                optionPart = optionPart->down;
            }
            else
            {
                uncover(optionColumn);
                optionPart -= 1;
            }
        }

        // Update constraint to top of option, option to next option for constraint
        constraint = option->top;
        option = option->down;
        optionNumber = option;

        // Traverse through matrix until the choosen options' associated spacer node is found
        while (optionNumber->data > 0)
        {
            optionNumber += 1;
        }
        
        // Free malloc'd memory for old potential partial of solution and create new string for potential partial of solution
        free(optionStr);
        optionRowId = static_cast<uint32_t>(abs(optionNumber->data));
        optionStr = static_cast<char*>(malloc(snprintf(NULL, 0, "%u", optionRowId) + 1));
        sprintf(optionStr, "%u", optionRowId);
        solutions[level] = optionStr;
        row_ids[level] = optionRowId;
    }

    // Free malloc'd memory and uncover the constraint
    free(optionStr);
    uncover(constraint);
}

/**
 * An auxilary function used by search to cover a constraint item column i. Covering a constraint
 * column covers all options associated with that item column, removing them from the set of
 * options to be selectable.
 * 
 * @param struct node* A node pointer to some item column.
 * @return void
 */
void Core::cover(struct node* i)
{
    struct node *p, *l, *r;
    
    p = i->down;

    // While option node p does not loop back to item column node, hide associated options
    while (p != i)
    {
        hide(p);
        p = p->down;
    }

    // Cover constraint i by updating its left and right to point to each other
    l = i->left;
    r = i->right;

    l->right = r;
    r->left = l;
}

/**
 * An auxilary function used by the cover function to hide an option associated with node p. Hiding an option removes
 * the option from being selectable in further recursive calls to the main search method. The hiding protocol 
 * essentially updates its up pointer and down pointers to point towards each other, in essence hiding the node in
 * the current option. This procedure ends when a spacer node is hit, which will terminate the while loop.
 * 
 * @param struct node* A node pointer to some option node.
 * @return void
 */
void Core::hide(struct node* p)
{
    struct node *q, *x, *u, *d;
    
    q = p + 1;
    
    while (q != p)
    {
        x = q->top;
        u = q->up;
        d = q->down;

        // q was a spacer
        if (x->data <= 0)
        {
            q = u;
        }
        else
        {
            u->down = d;
            d->up = u;

            x->len -= 1;

            q = q + 1;
        }
    }
}

/**
 * An auxilary function used by search to uncover an item column i. Uncovering an item column uncovers all options
 * associated with that item column, restoring them into the set of options to be selectable.
 * 
 * @param struct node* A node pointer to some item column.
 * @return void
 */
void Core::uncover(struct node* i)
{
    struct node *p, *l, *r;

    // Uncover item i by updating its left and right to point back to i
    l = i->left;
    r = i->right;

    l->right = i;
    r->left = i;

    p = i->up;
    
    // While option node p does not loop back to item column node, unhide associated options
    while (p != i)
    {
        unhide(p);
        p = p->up;
    }
}

/**
 * An auxilary function used by the uncover function to unhide an option associated with node p. Unhiding an option
 * restores the option back to being selectable in further recursive calls to the main search method. The unhiding
 * protocol essentially updates its up pointer and down pointers to point back towards itself, in essence unhiding
 * the node in the current option. This procedure ends when a spacer node is hit, which will terminate the while loop.
 * 
 * @param struct node* A node pointer to some option node.
 * @return void
 */ 
void Core::unhide(struct node* p)
{
    struct node *q, *x, *u, *d;

    q = p - 1;

    while (q != p)
    {
        x = q->top;
        u = q->up;
        d = q->down;

        // q was a spacer
        if (x->data <= 0)
        {
            q = d;
        }
        else
        {
            u->down = q;
            d->up = q;

            x->len += 1;

            q = q - 1;
        }
    }
}

/**
 * This is a function for picking an item column using the MRV heuristic. Essentially, this function iterate through
 * the set of item columns and checks the number of options associated with that item. The item column with the
 * smallest length (number of options to go through) will be returned. When multiple item columns share the same length,
 * the first item column is returned.
 * 
 * @param struct node* A node pointer to the head of the matrix.
 * @return struct node* Returns the address to the item node with the smallest option length.
 */ 
struct node* Core::pickConstraint(struct node* head)
{
    int theta = INT_MAX;
    struct node* i = NULL;
    struct node* p = head->right;

    // Iterate through all item columns
    while (p != head)
    {
        int lambda = p->len;

        if (lambda < theta)
        {
            i = p;
            theta = lambda;
        }

        // Return early if min is 0
        if (theta == 0)
        {
            return i;
        }

        p = p->right;
    }

    return i;
}

struct node* Core::generateMatrixBinaryImpl(const struct binary::DlxCoverHeader& header,
                                            std::vector<binary::DlxRowChunk>& rows,
                                            char*** solutions_out,
                                            int* item_count_out,
                                            int* option_count_out)
{
    if (solutions_out == nullptr || item_count_out == nullptr || option_count_out == nullptr)
    {
        return nullptr;
    }

    if (header.column_count == 0 || header.column_count > static_cast<uint32_t>(INT_MAX))
    {
        return nullptr;
    }

    const uint32_t column_count = header.column_count;
    const int itemCount = static_cast<int>(column_count);

    size_t total_entries = 0;
    for (size_t row_index = 0; row_index < rows.size(); ++row_index)
    {
        auto& chunk = rows[row_index];
        uint32_t row_id = (chunk.row_id == 0)
                              ? static_cast<uint32_t>(row_index + 1)
                              : chunk.row_id;
        if (row_id > static_cast<uint32_t>(INT_MAX))
        {
            return nullptr;
        }

        if (chunk.entry_count > 1)
        {
            std::sort(chunk.columns, chunk.columns + chunk.entry_count);
        }

        bool invalid_column = false;
        for (uint16_t i = 0; i < chunk.entry_count; ++i)
        {
            uint32_t column = chunk.columns[i];
            if (column >= column_count)
            {
                invalid_column = true;
                break;
            }
        }
        if (invalid_column)
        {
            return nullptr;
        }

        total_entries += chunk.entry_count;
    }

    if (rows.size() > static_cast<size_t>(INT_MAX))
    {
        return nullptr;
    }

    char** solutions = static_cast<char**>(calloc(rows.size(), sizeof(char*)));
    if (solutions == nullptr)
    {
        return nullptr;
    }

    size_t spacer_nodes = rows.size() + 1;
    size_t total_nodes = static_cast<size_t>(column_count) + total_entries + spacer_nodes;
    if (total_nodes > static_cast<size_t>(INT_MAX))
    {
        free(solutions);
        return nullptr;
    }

    int nodeCount = static_cast<int>(total_nodes);
    struct node* matrix = matrix::generateHeadNode(nodeCount);
    if (matrix == nullptr)
    {
        free(solutions);
        return nullptr;
    }

    initialize_column_headers(matrix, column_count);

    int currNodeCount = itemCount;
    int prevRowCount = 0;
    int spaceNodeCount = 0;
    int pending_row_id = 0;
    bool has_pending_row = false;

    for (size_t row_index = 0; row_index < rows.size(); ++row_index)
    {
        matrix::handleSpacerNodes(matrix, &spaceNodeCount, currNodeCount, prevRowCount);
        if (has_pending_row)
        {
            matrix[currNodeCount + 1].data = -pending_row_id;
        }
        const auto& row = rows[row_index];
        uint32_t row_id = (row.row_id == 0) ? static_cast<uint32_t>(row_index + 1) : row.row_id;
        pending_row_id = static_cast<int>(row_id);
        has_pending_row = true;

        currNodeCount++;
        prevRowCount = 0;

        const uint32_t* columns = row.columns;
        size_t column_count_for_row = row.entry_count;
        size_t column_cursor = 0;

        for (int assocItemCount = 1; assocItemCount <= itemCount; ++assocItemCount)
        {
            while (column_cursor < column_count_for_row
                   && columns[column_cursor] + 1 < static_cast<uint32_t>(assocItemCount))
            {
                column_cursor++;
            }

            if (column_cursor >= column_count_for_row)
            {
                break;
            }

            if (columns[column_cursor] + 1 != static_cast<uint32_t>(assocItemCount))
            {
                continue;
            }

            struct node* item = &matrix[assocItemCount];
            struct node* last = item;
            while (last->down != item)
            {
                last = last->down;
            }

            int new_index = currNodeCount + 1;
            matrix[new_index].data = new_index;
            item->len += 1;
            item->up = &matrix[new_index];
            last->down = &matrix[new_index];
            matrix[new_index].up = last;
            matrix[new_index].top = item;
            matrix[new_index].down = item;

            prevRowCount++;
            currNodeCount++;
            column_cursor++;
        }
    }

    matrix[currNodeCount + 1].top = matrix;
    if (has_pending_row)
    {
        matrix[currNodeCount + 1].data = -pending_row_id;
    }
    else
    {
        matrix[currNodeCount + 1].data = spaceNodeCount;
    }
    spaceNodeCount--;
    matrix[currNodeCount + 1].down = matrix;
    if (prevRowCount == 0)
    {
        matrix[currNodeCount + 1].up = matrix;
    }
    else
    {
        matrix[currNodeCount - prevRowCount].down = &matrix[currNodeCount];
        matrix[currNodeCount + 1].up = &matrix[(currNodeCount + 1) - prevRowCount];
    }

    if (g_matrix_dump_stream != nullptr)
    {
        matrix::dumpMatrixStructure(matrix, nodeCount, itemCount, *g_matrix_dump_stream);
    }

    *solutions_out = solutions;
    *item_count_out = itemCount;
    *option_count_out = static_cast<int>(rows.size());
    return matrix;
}

struct node* Core::generateMatrixBinary(struct binary::DlxProblem& problem,
                                        char*** solutions_out,
                                        int* item_count_out,
                                        int* option_count_out)
{
    return generateMatrixBinaryImpl(problem.header, problem.rows, solutions_out, item_count_out, option_count_out);
}

struct node* Core::generateMatrixBinaryFromRows(const struct binary::DlxCoverHeader& header,
                                                std::vector<binary::DlxRowChunk>& rows,
                                                char*** solutions_out,
                                                int* item_count_out,
                                                int* option_count_out)
{
    return generateMatrixBinaryImpl(header, rows, solutions_out, item_count_out, option_count_out);
}

/**
 * A printing function used by the main search method that prints out the found solutions to stdout. Found solutions
 * could be piped to a file or to some other application for post-processing.
 */
void Core::printSolutions(char** solutions, const uint32_t* row_ids, int level, SolutionOutput& output)
{
    auto emit = [&](std::ostream& stream) {
        for (int i = 0; i < level; i++)
        {
            const bool last_value = ((i + 1) == level);
            stream << solutions[i] << (last_value ? '\n' : ' ');
        }
        stream.flush();
    };

    if (output.sink != nullptr)
    {
        sink::SolutionView view{solutions, level};
        output.sink->on_solution(view);
        output.sink->flush();
    }
    else if (!g_suppress_stdout_output)
    {
        emit(std::cout);
    }

    output.emit_binary_row(row_ids, level);
}

/**
 * An auxilary function for freeing heap memory used by dlx program.
 * 
 * @param struct node* The matrix array associated with the read cover file.
 * @param char** An array of solutions
 * @return void
 */ 
void Core::freeMemory(struct node* matrix, char** solutions)
{
    free(matrix);
    free(solutions);
}

} // namespace dlx
