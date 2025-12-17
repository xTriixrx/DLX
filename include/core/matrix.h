#ifndef DLX_MATRIX_H
#define DLX_MATRIX_H

#include <ostream>

struct node;

namespace dlx::matrix {

struct node* generateHeadNode(int nodeCount);
void handleSpacerNodes(struct node* matrix,
                       int* spaceNodeCount,
                       int currNodeCount,
                       int prevRowCount);
void dumpMatrixStructure(const struct node* matrix,
                         int total_nodes,
                         int itemCount,
                         std::ostream& output);

} // namespace dlx::matrix

#endif
