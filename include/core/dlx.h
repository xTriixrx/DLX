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

extern const char* STR_ONE;
extern const char* STR_ZERO;
extern const char* READ_ONLY;
extern const char* SPACE_DELIMITER;

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

int fileExists(char*);
int getNodeCount(FILE*);
int getItemCount(FILE*);
void hide(struct node*);
void cover(struct node*);
void unhide(struct node*);
void uncover(struct node*);
int getOptionsCount(FILE*);
char* repeatStr(const char*, int);
void printItems(struct node*);
int getOptionNodesCount(FILE*);
void printSolutions(char**, int, FILE*);
void printOptionRow(struct node*);
struct node* generateHeadNode(int);
void printItemColumn(struct node*);
struct node* pickItem(struct node*);
void insertIntoSet(struct node*, int);
void search(struct node*, int, char**, FILE*);
void printMatrix(const struct node*, int, int);
struct node* generateMatrix(FILE*, char**, int);
int generateTitles(struct node*, char**, char*);
void freeMemory(struct node*, char**, char**, int);
void handleSpacerNodes(struct node*, int*, int, int);
struct node insertItem(struct node*, struct node*, char*);
int dlx_enable_binary_solution_output(FILE* output, uint32_t column_count);
void dlx_disable_binary_solution_output(void);
void dlx_set_stdout_suppressed(bool suppressed);

#endif
