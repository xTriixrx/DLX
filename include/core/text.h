#ifndef DLX_TEXT_H
#define DLX_TEXT_H

#include <cstdio>
#include <ostream>

struct node;

namespace dlx::text {

struct node* generateMatrix(FILE* cover, int nodeCount, std::ostream* dump_stream = nullptr);
int generateTitles(struct node* matrix, char* titleLine);
int getNodeCount(FILE* coverFile);
int getItemCount(FILE* coverFile);
int getOptionsCount(FILE* coverFile);
int getOptionNodesCount(FILE* coverFile);

} // namespace dlx::text

#endif
