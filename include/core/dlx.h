#ifndef DLX_H
#define DLX_H

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
 * A simple dlx node structure
 */
struct node 
{
    int len;
    int data;
    char* name;
    struct node* top;
    struct node* up;
    struct node* down;
    struct node* left;
    struct node* right;
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

#endif
