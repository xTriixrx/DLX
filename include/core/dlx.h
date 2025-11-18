#ifndef DLX_H
#define DLX_H

#include <stdint.h>
#include <stdio.h>

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
    char* name;           /**< Optional human readable label. */
    struct node* top;     /**< Column header for this node. */
    struct node* up;      /**< Pointer to the previous node in the column. */
    struct node* down;    /**< Pointer to the next node in the column. */
    struct node* left;    /**< Pointer to the previous node in the row. */
    struct node* right;   /**< Pointer to the next node in the row. */
};

namespace dlx {

extern const char* READ_ONLY;

class Core
{
public:
    static int fileExists(char*);
    static int getNodeCount(FILE*);
    static int getItemCount(FILE*);
    static int getOptionsCount(FILE*);
    static struct node* generateMatrix(FILE*, char**, int);
    static void search(struct node*, int, char**, FILE*);
    static void freeMemory(struct node*, char**, char**, int);
    static int dlx_enable_binary_solution_output(FILE* output, uint32_t column_count);
    static void dlx_disable_binary_solution_output(void);
    static void dlx_set_stdout_suppressed(bool suppressed);

private:
    static void hide(struct node*);
    static void cover(struct node*);
    static void unhide(struct node*);
    static void uncover(struct node*);
    static char* repeatStr(const char*, int);
    static void printItems(struct node*);
    static int getOptionNodesCount(FILE*);
    static void printSolutions(char**, int, FILE*);
    static void printOptionRow(struct node*);
    static struct node* generateHeadNode(int);
    static void printItemColumn(struct node*);
    static struct node* pickItem(struct node*);
    static void insertIntoSet(struct node*, int);
    static void printMatrix(const struct node*, int, int);
    static int generateTitles(struct node*, char**, char*);
    static void handleSpacerNodes(struct node*, int*, int, int);
    static struct node insertItem(struct node*, struct node*, char*);
};

} // namespace dlx

#endif
