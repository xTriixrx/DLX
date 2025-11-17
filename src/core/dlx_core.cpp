#include "core/dlx.h"
#include "core/dlx_binary.h"
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

const char* STR_ONE = "1";
const char* STR_ZERO = "0";
const char* READ_ONLY = "r";
const char* SPACE_DELIMITER = " ";

struct binary_solution_output_ctx
{
    FILE* file;
    uint32_t next_solution_id;
};

static struct binary_solution_output_ctx g_binary_output = {NULL, 1};
static bool g_suppress_stdout_output = false;

void dlx_set_stdout_suppressed(bool suppressed)
{
    g_suppress_stdout_output = suppressed;
}

static void write_binary_solution_row(char** solutions, int level)
{
    if (g_binary_output.file == NULL || level <= 0)
    {
        return;
    }

    if (level > UINT16_MAX)
    {
        fprintf(stderr, "Solution row count exceeds binary format limit\n");
        return;
    }

    uint32_t* indices = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * level));
    if (indices == NULL)
    {
        fprintf(stderr, "Unable to allocate binary solution buffer\n");
        return;
    }

    for (int i = 0; i < level; i++)
    {
        char* endptr = NULL;
        unsigned long value = strtoul(solutions[i], &endptr, 10);
        if (endptr == solutions[i] || value == 0 || value > UINT32_MAX)
        {
            fprintf(stderr, "Invalid solution row identifier '%s'\n", solutions[i]);
            free(indices);
            return;
        }

        indices[i] = static_cast<uint32_t>(value);
    }

    if (dlx_write_solution_row(g_binary_output.file,
                               g_binary_output.next_solution_id,
                               indices,
                               static_cast<uint16_t>(level)) != 0)
    {
        fprintf(stderr, "Failed to write binary solution row\n");
        free(indices);
        return;
    }

    g_binary_output.next_solution_id += 1;
    free(indices);
}

int dlx_enable_binary_solution_output(FILE* output, uint32_t column_count)
{
    if (output == NULL)
    {
        return -1;
    }

    struct DlxSolutionHeader header = {
        .magic = DLX_SOLUTION_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = column_count,
    };

    if (dlx_write_solution_header(output, &header) != 0)
    {
        fprintf(stderr, "Unable to write DLX solution header\n");
        return -1;
    }

    g_binary_output.file = output;
    g_binary_output.next_solution_id = 1;
    return 0;
}

void dlx_disable_binary_solution_output(void)
{
    g_binary_output.file = NULL;
    g_binary_output.next_solution_id = 1;
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
void search(struct node* head, int level, char** solutions, FILE* solution_output)
{
    // If all items have been covered, output a found solution.
    if (head->right == head)
    {
        printSolutions(solutions, level, solution_output);
        return;
    }
    
    // Pick an item i (column), and cover the item.
    struct node* item = pickItem(head);
    cover(item);
    
    // Pick an option xl (row), and set potential partial solution
    struct node* option = item->down;
    struct node* optionNumber = option;
    
    // Traverse through matrix until the choosen options' associated spacer node is found
    while (optionNumber->data > 0)
    {
        optionNumber += 1;
    }

    // Create string for potential partial of solution
    char* optionStr = (char *) malloc(snprintf(NULL, 0, "%d", abs(optionNumber->data)) + 1);
    sprintf(optionStr, "%d", abs(optionNumber->data));
    solutions[level] = optionStr;

    // While node of a particular option row doesn't loop back to item node.
    struct node* optionPart;
    while (option != item)
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
        search(head, level + 1, solutions, solution_output);

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

        // Update item to top of option, option to next option for item
        item = option->top;
        option = option->down;
        optionNumber = option;

        // Traverse through matrix until the choosen options' associated spacer node is found
        while (optionNumber->data > 0)
        {
            optionNumber += 1;
        }
        
        // Free malloc'd memory for old potential partial of solution and create new string for potential partial of solution
        free(optionStr);
        optionStr = (char *) malloc(snprintf(NULL, 0, "%d", abs(optionNumber->data)) + 1);
        sprintf(optionStr, "%d", abs(optionNumber->data));
        solutions[level] = optionStr;
    }

    // Free malloc'd memory and uncover the item
    free(optionStr);
    uncover(item);
}

/**
 * An auxilary function used by search to cover an item column i. Covering an item column covers all options
 * associated with that item column, removing them from the set of options to be selectable.
 * 
 * @param struct node* A node pointer to some item column.
 * @return void
 */
void cover(struct node* i)
{
    struct node *p, *l, *r;
    
    p = i->down;

    // While option node p does not loop back to item column node, hide associated options
    while (p != i)
    {
        hide(p);
        p = p->down;
    }

    // Cover item i by updating its left and right to point to each other
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
void hide(struct node* p)
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
void uncover(struct node* i)
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
void unhide(struct node* p)
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
struct node* pickItem(struct node* head)
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

/**
 * The generation function is used for creating the internal memory mapping of the matrix structure to be used for 
 * the dlx search algorithm. This function generates all nodes including the item column nodes, spacer nodes, and the
 * actual option nodes that need to be placed throughout the structure. Through each iteration, the appropriate links
 * are updated as the file is being read.
 * 
 * @param FILE* A file pointer to a file which contains a cover definition.
 * @param char** A char pointer of pointers to access the titles array.
 * @param int An integer representing the total number of nodes that need to be created.
 * @return struct node* Returns the address of the malloc'd matrix structure.
 */ 
struct node* generateMatrix(FILE* cover, char** titles, int nodeCount)
{
    ssize_t read;
    size_t len = 0;
    char* newlinePtr;
    int lineCount = 1;
    char* buffer = NULL;
    int prevRowCount = 0;
    int currNodeCount = 0;
    int spaceNodeCount = 0;

    // Malloc matrix and populate head node to begin insertion
    struct node* matrix = generateHeadNode(nodeCount);

    // Read first line of cover file and generate titles array
    read = getline(&buffer, &len, cover);
    currNodeCount = generateTitles(matrix, titles, buffer);

    // Read line by line of entire cover file
    while ((read = getline(&buffer, &len, cover)) != -1)
    {
        // Create/update spacer nodes prior to dealing with next option
        handleSpacerNodes(matrix, &spaceNodeCount, currNodeCount, prevRowCount);

        currNodeCount++;
        prevRowCount = 0;
        int assocItemCount = 1;
        char* optionToken = strtok(buffer, SPACE_DELIMITER);
        
        // Iterate through option row and create nodes where appropriate
        while (optionToken != NULL)
        {
            // If newline character exists in optionToken, remove the character
            if ((newlinePtr = strstr(optionToken, "\n")) != NULL)
            {
                strncpy(newlinePtr, "\0", 1);
            }

            // If "0", move to next item; no need to create empty nodes.
            if (strcmp(optionToken, STR_ZERO) == 0)
            {
                assocItemCount++;
                optionToken = strtok(NULL, SPACE_DELIMITER);
                continue;
            }
            // If "1", create node for associated item
            else if (strcmp(optionToken, STR_ONE) == 0)
            {
                matrix[currNodeCount + 1].data = currNodeCount + 1;

                // Retrieve associated item node
                struct node* item = &matrix[assocItemCount];
                struct node* last = &matrix[assocItemCount];
                
                // Move last ptr to last item in item's column
                while (last->down != item)
                {
                    last = last->down;
                }

                // Update item and last ptr's to point to new node
                item->len += 1;
                item->up = &matrix[currNodeCount + 1];
                last->down = &matrix[currNodeCount + 1];
                
                // Set new nodes' ptr's
                matrix[currNodeCount + 1].up = last;
                matrix[currNodeCount + 1].top = item;
                matrix[currNodeCount + 1].down = item;
                
                // Increment counters
                prevRowCount++;
                currNodeCount++;
                assocItemCount++;
            }
            else
            {
                printf("Invalid token found in complete cover mapping: %s, aborting.", optionToken);
                exit(1);
            }

            optionToken = strtok(NULL, SPACE_DELIMITER);
        }

        lineCount++;
    }

    // Lastly, populate final spacer node prior to finishing matrix generation and update last - 1 spacer.
    matrix[currNodeCount + 1].top = matrix;
    matrix[currNodeCount + 1].data = spaceNodeCount--;

    matrix[currNodeCount + 1].down = matrix;
    matrix[currNodeCount - prevRowCount].down = &matrix[currNodeCount];
    matrix[currNodeCount + 1].up = &matrix[(currNodeCount + 1) - prevRowCount];

    free(buffer);

    return matrix;
}

/**
 * A utility function to assist in the generation of the matrix structure and appropriate links, this function
 * is dedicated to the construction of the item nodes (column headers) and reading the title tokens from the titleLine
 * into the title array.
 * 
 * @param struct node* A node pointer to the head of the matrix.
 * @param char** A char pointer of pointers to access the titles array.
 * @param char* A char pointer to the item line which is the first line in the cover file.
 * @return int Returns the current node count of how many item nodes were created from this method.
 */ 
int generateTitles(struct node* matrix, char** titles, char* titleLine)
{
    char* newlinePtr;
    int currNodeCount = 0;

    char* itemTitle = strtok(titleLine, SPACE_DELIMITER);

    // While delimited substring is not null and current item count is less than
    // the desired item length, continue processing item columns
    while (itemTitle != NULL)
    {
        // If newline character exists in title string, remove the character
        if ((newlinePtr = strstr(itemTitle, "\n")) != NULL)
        {
            strncpy(newlinePtr, "\0", 1);
        }

        // Malloc title entry and insert title data into title array
        titles[currNodeCount] = static_cast<char*>(malloc(strlen(itemTitle) + 1));
        strcpy(titles[currNodeCount], itemTitle);

        // Populate item node
        matrix[currNodeCount + 1].len = 0;
        matrix[currNodeCount + 1].top = matrix;
        matrix[currNodeCount + 1].name = titles[currNodeCount];
        matrix[currNodeCount + 1].left = &matrix[currNodeCount];
        matrix[currNodeCount + 1].right = matrix;
        matrix[currNodeCount + 1].up = &matrix[currNodeCount + 1];
        matrix[currNodeCount + 1].down = &matrix[currNodeCount + 1];
        matrix[currNodeCount + 1].data = currNodeCount + 1;

        // Adjust previous right pointer and heads left pointer to new element.
        matrix[currNodeCount].right = &matrix[currNodeCount + 1];
        matrix[0].left = &matrix[currNodeCount + 1];

        itemTitle = strtok(NULL, SPACE_DELIMITER);
        currNodeCount++;
    }

    return currNodeCount;
}

/**
 * A utility function to assist in the generation of the matrix structure and appropriate links, this function
 * is dedicated to all spacer nodes except the last one. This function will update the pointers to the current
 * and previous spacer nodes up until the last spacer node.
 * 
 * @param struct node* A node pointer to the head of the matrix.
 * @param int* A pointer to the current space node counter.
 * @param int A copy of the current node count in the generation process.
 * @param int A copy of the previous option's node count in the generation process.
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
 * A utility function to assist in the generation of the matrix structure and appropriate links, this function
 * malloc's the entire matrix structure, populates the first entry and returns the address of the malloc'd memory.
 * 
 * @param int An integer representing the total number of nodes that need to be created.
 * @return struct node* Returns the address of the malloc'd matrix structure.
 */ 
struct node* generateHeadNode(int nodeCount)
{
    struct node* matrix = static_cast<struct node*>(malloc(sizeof(struct node) * (nodeCount + 1)));

    matrix[0].data = 0;
    matrix[0].top = matrix;
    matrix[0].name = const_cast<char*>("HEAD");
    matrix[0].left = matrix;
    matrix[0].right = matrix;

    return matrix;
}

/**
 * A utility printing function that shows the read cover description structure in memory.
 * 
 * @param const struct node* A node pointer to the head of the matrix.
 * @param int The length of the matrix array.
 * @param int The number of item nodes (columns) defined in the matrix.
 * @return void
 */ 
void printMatrix(const struct node* matrix, int arr_len, int item_len)
{
    for (int i = 1; i < arr_len; i++)
    {
        // Generate item columns output by defined name 
        if (i <= item_len)
        {
            if (i == item_len)
            {
                printf("%s\n", matrix[i].name);
            }
            else
            {
                printf("%s\t", matrix[i].name);
            }
        }
        else
        {
            // If top is pointing to a valid column
            if (matrix[i].top->data > 0)
            {
                // Just print data value if associated with first column
                if ((matrix[i].top->data - 1) == 0)
                {
                    printf("%d", matrix[i].data);
                }
                else
                {
                    char* spaces;

                    // If last node was not a spacer, use last top data to calculate spaces for this nodes' output
                    if (matrix[i - 1].top->data > 0)
                    {
                        spaces = repeatStr("\t", matrix[i].top->data - matrix[i - 1].top->data);
                    }
                    else // Otherwise, just take the top - 1.
                    {
                        spaces = repeatStr("\t", matrix[i].top->data - 1);
                    }

                    printf("%s%d",spaces, matrix[i].data);
                    free(spaces);
                }

                // If next node is a spacer, move to next line, generate a spacer and return again.
                if (matrix[i + 1].top->data <= 0)
                {   
                    char* sep = repeatStr("-", item_len * 4);

                    if (item_len - matrix[i].top->data != 0)
                    {
                        char* spaces = repeatStr("\t", (item_len - matrix[i].top->data) + 1);
                        printf("%s%d\n%s\n", spaces, matrix[i + 1].data, sep);
                        free(spaces);
                    }
                    else
                    {
                        printf("\t%d\n%s\n", matrix[i + 1].data, sep);
                    }
                    
                    free(sep);
                }
            }
        }
    }
}

/**
 * A printing function used by the main search method that prints out the found solutions to stdout. Found solutions
 * could be piped to a file or to some other application for post-processing.
 */
void printSolutions(char** solutions, int level, FILE* solution_output)
{
    bool write_console =
        !g_suppress_stdout_output && (solution_output == NULL || solution_output != stdout);
    bool write_stream = (solution_output != NULL);

    for (int i = 0; i < level; i++)
    {
        const bool last_value = ((i + 1) == level);
        const char* fmt = last_value ? "%s\n" : "%s ";

        if (write_console)
        {
            printf(fmt, solutions[i]);
        }
        if (write_stream)
        {
            fprintf(solution_output, fmt, solutions[i]);
        }
    }

    if (solution_output != NULL)
    {
        fflush(solution_output);
    }

    write_binary_solution_row(solutions, level);
}

/**
 * A printing function that prints out the first row of the matrix, which is the set of items defined to stdout. 
 * This function really is only useful for debugging purposes but could be used to see the state of the column's 
 * being covered in a debugging fashion.
 * 
 * @param struct node* A pointer to the beginning of the matrix.
 * @return void
 */
void printItems(struct node* head)
{
    struct node* curr = head;

    // Printed in order to symbolize the circular connection
    printf("| ");

    do
    {
        printf("%s - %d ", curr->name, curr->data);
        curr = curr->right;

        if (curr != head)
            printf("| ");
        else
            printf("|\n"); // printed on end to symbolize the circular connection
    } while (curr != head);
}

/**
 * A printing function that prints out an items' column to stdout. This function really is only useful for debugging
 * purposes. This function assumes the debugger know's precisely the column node that should be investigated as this
 * function does not provide any checks for correctness of starting point.
 * 
 * @param struct node* A pointer to the item node in a given column.
 * @return void
 */
void printItemColumn(struct node* itemColumn)
{
    struct node* curr = itemColumn;

    // Printed in order to symbolize the circular connection
    printf("Column %s %p: | ", itemColumn->name, itemColumn);

    do
    {
        printf("%d ", curr->data);
        curr = curr->down;

        if (curr != itemColumn)
            printf("| ");
        else
            printf("| len(%s) = %d\n", itemColumn->name, itemColumn->len); // printed on end to symbolize the circular connection
    } while (curr != itemColumn);
}

/**
 * A printing function that prints out an option row to stdout. This function really is only useful for debugging
 * purposes. This function assumes the debugger know's precisely the row node that should be investigated as this
 * function does not provide any checks for correctness of starting point.
 * 
 * @param struct node* A pointer to the first node in a given option row.
 * @return void
 */ 
void printOptionRow(struct node* optionRow)
{
    struct node* curr = optionRow;

    while (curr->data > 0)
    {
        curr += 1;
    }

    printf("Row %d %p: | ", abs(curr->data), optionRow);

    curr = optionRow;

    do
    {
        printf("%d ", curr->data);
        curr += 1;

        if (curr->data > 0)
            printf("| ");
        else
            printf("|\n");
    } while (curr->data > 0);
}

/**
 * An auxilary function for reading the provided cover file and counting the total number of actual nodes that need to be 
 * created for the provided cover file. The value returned is added into the total number of items (column headers) that
 * were counted previously.
 * 
 * @param FILE* A file pointer to a file which contains a cover definition.
 * @return int Returns the number of nodes within the defined cover definition.
 */
int getNodeCount(FILE* coverFile)
{
    size_t len = 0;
    ssize_t read = 0;
    int nodeCount = 0;
    char* buffer = NULL;

    // Count number of item options which is equivalent to number of actual nodes in structure
    nodeCount += getOptionNodesCount(coverFile);
    
    // Reset file descriptor back to beginning of file
    fseek(coverFile, 0L, SEEK_SET);

    // Count number of lines which is equivalent to number of spacer nodes that need to be created 
    while ((read = getline(&buffer, &len, coverFile)) != -1)
    {
        nodeCount++;
    }
    
    // Reset file descriptor back to beginning of file
    fseek(coverFile, 0L, SEEK_SET);

    free(buffer);

    return nodeCount;
}

/**
 * An auxilary function for reading the provided cover file and counting the number of titles defined as
 * column headers that are defined in the cover file.
 * 
 * @param FILE* A file pointer to a file which contains a cover definition.
 * @return int Returns the number of items (column headers) defined in the cover file.
 */
int getItemCount(FILE* coverFile)
{
    size_t len = 0;
    ssize_t read = 0;
    int itemCount = 0;
    char* buffer = NULL;

    // Count number of lines which is equivalent to number of spacer nodes that need to be created 
    while ((read = getline(&buffer, &len, coverFile)) != -1)
    {
        char* newlinePtr;
        char* itemTitle = strtok(buffer, SPACE_DELIMITER);

        // While delimited substring is not null and current item count is less than
        // the desired item length, continue processing item columns
        while (itemTitle != NULL)
        {
            // If newline character exists in title string, remove the character
            if ((newlinePtr = strstr(itemTitle, "\n")) != NULL)
            {
                strncpy(newlinePtr, "\0", 1);
            }

            itemCount++;
            itemTitle = strtok(NULL, SPACE_DELIMITER);
        }

        break;
    }

    // Reset file descriptor back to beginning of file
    fseek(coverFile, 0L, SEEK_SET);

    free(buffer);

    return itemCount;
}

/**
 * An auxilary function for reading the provided cover file and counting the number of lines that are 
 * defined in the cover file. The number of lines are associated with the number of spacer nodes that will
 * need to be created.
 * 
 * @param FILE* A file pointer to a file which contains a cover definition.
 * @return int Returns the number of lines defined in the cover file; associated with number of spacer nodes.
 */
int getOptionsCount(FILE* coverFile)
{
    size_t len = 0;
    ssize_t read = 0;
    char* buffer = NULL;
    int optionsCount = 0;

    // Count number of lines which is equivalent to number of spacer nodes that need to be created 
    while ((read = getline(&buffer, &len, coverFile)) != -1)
    {
        optionsCount++;
    }

    // Reset file descriptor back to beginning of file
    fseek(coverFile, 0L, SEEK_SET);

    free(buffer);

    return optionsCount;
}

/**
 * An auxilary function for reading the provided cover file and counting the number of option nodes that
 * are defined in the cover file.
 * 
 * @param FILE* A file pointer to a file which contains a cover definition.
 * @return int Returns the number of option nodes defined in the cover file.
 */ 
int getOptionNodesCount(FILE* coverFile)
{
    size_t len = 0;
    ssize_t read = 0;
    char* buffer = NULL;
    int optionCount = 0;

    // Skip over title line
    read = getline(&buffer, &len, coverFile);

    // Feed in line by line chunks for processing
    while ((read = getline(&buffer, &len, coverFile)) != -1)
    {
        // Read sparse matrix and count number of 1's that represent a node.
        for (char* ch = buffer; *ch; ch++)
        {
            // Count number of 1's that are described in the file.
            if (*ch == '1')
            {
                optionCount++;
            }
        }
    }

    // Reset file descriptor back to beginning of file
    fseek(coverFile, 0L, SEEK_SET);

    free(buffer);

    return optionCount;
}

/**
 * An auxilary function for repeating a string N number of times.
 * 
 * @param char* A character pointer of a sequence to be repeated.
 * @param int The number of times the character pointer should be repeated.
 * @return char* A malloc'd repeated character array of the provided parameter.
 */ 
char* repeatStr(const char* str, int count)
{
    if (count == 0)
    {
        return NULL;
    }

    char *ret = static_cast<char*>(malloc(strlen(str) * count + count));
    
    if (ret == NULL)
    {
        return NULL;
    }

    strcpy(ret, str);
    while (--count > 0)
    {
        strcat(ret, " ");
        strcat(ret, str);
    }

    return ret;
}

/**
 * An auxilary function for freeing heap memory used by dlx program.
 * 
 * @param struct node* The matrix array associated with the read cover file.
 * @param char** An array of solutions
 * @param char** An array of titles
 * @param int The number of titles in the titles array.
 * @return void
 */ 
void freeMemory(struct node* matrix, char** solutions, char** titles, int itemCount)
{
    for (int i = 0; i < itemCount; i++)
    {
        free(titles[i]);
    }

    free(titles);
    free(matrix);
    free(solutions);
}

/**
 * An auxilary function for checking if file exists or not;
 * 
 * @param char* A char pointer for a path to a file.
 * @return void
 */ 
int fileExists(char* file)
{
    struct stat buffer;
    return (stat(file, &buffer) == 0);
}
