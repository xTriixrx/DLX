#include <cstdlib>
#include <ostream>
#include "core/dlx.h"
#include "core/matrix.h"

namespace dlx::matrix {

namespace {

/**
 * Computes the zero-based index of a node within the contiguous matrix allocation.
 *
 * `generateHeadNode` allocates a single block for the head, column headers,
 * spacer nodes, and option nodes. This helper converts raw pointers back into
 * the corresponding index relative to the block's base, allowing human-readable
 * diagnostics to list pointer relationships deterministically.
 *
 * @param base Pointer to the first element in the contiguous allocation.
 * @param ptr  Pointer to a node in the same allocation (may be nullptr).
 * @return The index of @p ptr relative to @p base, or -1 when @p ptr is null.
 */
int matrix_index(const struct node* base, const struct node* ptr)
{
    if (ptr == nullptr)
    {
        return -1;
    }
    
    return static_cast<int>(ptr - base);
}

} // namespace

/**
 * Allocates the contiguous node array used for Dancing Links matrices.
 *
 * The layout reserves slot 0 for the matrix head and appends every column
 * header, spacer node, and option node sequentially. Callers are responsible
 * for populating the contents after allocation.
 *
 * @param nodeCount Total number of nodes (including the head) required.
 * @return Pointer to the newly allocated array, or nullptr on allocation failure.
 */
struct node* generateHeadNode(int nodeCount)
{
    struct node* matrix = static_cast<struct node*>(malloc(sizeof(struct node) * (nodeCount + 1)));
    
    if (matrix == nullptr)
    {
        return nullptr;
    }

    matrix[0].data = 0;
    matrix[0].top = matrix;
    matrix[0].left = matrix;
    matrix[0].right = matrix;

    return matrix;
}

/**
 * Inserts and links the spacer node that terminates the current option row.
 *
 * Spacer nodes behave as row sentinels: they connect the final option node in
 * the previous row to the first option node in the next row. This helper wires
 * the spacer's up/down pointers and records its logical row id via the data
 * field.
 *
 * @param matrix Pointer to the matrix allocation.
 * @param spaceNodeCount Counter tracking remaining spacer identifiers.
 * @param currNodeCount Current index of the allocation cursor.
 * @param prevRowCount Length of the option row most recently emitted.
 */
void handleSpacerNodes(struct node* matrix, int* spaceNodeCount, int currNodeCount, int prevRowCount)
{
    // Top of spacer nodes should always point to matrix[0], data represents row number.
    matrix[currNodeCount + 1].top = matrix;
    matrix[currNodeCount + 1].data = (*spaceNodeCount)--;

    // If prevRowCount is 0, means starting first line of nodes
    if (prevRowCount == 0)
    {
        matrix[currNodeCount + 1].up = matrix;
    }
    else // Need to update previous spacers' down and current spacers' up
    {
        matrix[currNodeCount - prevRowCount].down = &matrix[currNodeCount];
        matrix[currNodeCount + 1].up = &matrix[(currNodeCount + 1) - prevRowCount];
    }
}

/**
 * Emits a deterministic textual representation of the matrix structure.
 *
 * Every node is labeled with its semantic role (HEAD, COLUMN, SPACER, NODE),
 * its positional index, and the indices of its adjacency pointers. The format
 * mirrors the legacy text-dump output so unit tests and tooling can continue
 * to compare dumps produced from either ASCII or binary loaders.
 *
 * @param matrix Pointer to the head of the contiguous allocation returned by generateHeadNode.
 * @param total_nodes Total number of nodes allocated (columns + spacers + option nodes).
 * @param itemCount Number of column headers in the matrix.
 * @param output Stream receiving the formatted dump.
 */
void dumpMatrixStructure(const struct node* matrix,
                         int total_nodes,
                         int itemCount,
                         std::ostream& output)
{
    output << "MATRIX item_count=" << itemCount << " total_nodes=" << total_nodes << "\n";
    
    for (int i = 0; i <= total_nodes; ++i)
    {
        const char* type = "NODE";
        const struct node& node = matrix[i];
        
        if (i == 0)
        {
            type = "HEAD";
        }
        else if (i <= itemCount)
        {
            type = "COLUMN";
        }
        else if (node.top == matrix)
        {
            type = "SPACER";
        }

        output << type << " index=" << i
               << " data=" << node.data
               << " len=" << node.len
               << " top=" << matrix_index(matrix, node.top)
               << " left=" << matrix_index(matrix, node.left)
               << " right=" << matrix_index(matrix, node.right)
               << " up=" << matrix_index(matrix, node.up)
               << " down=" << matrix_index(matrix, node.down);
        output << "\n";
    }

    output.flush();
}

} // namespace dlx::matrix
