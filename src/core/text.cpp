#include "core/text.h"

#include "core/dlx.h"
#include "core/matrix.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <ostream>

namespace {

constexpr char STR_ONE[] = "1";
constexpr char STR_ZERO[] = "0";
constexpr char SPACE_DELIMITER[] = " ";

} // namespace

namespace dlx::text {

int matrix_index(const struct node* base, const struct node* ptr);

/**
 * The generation function is used for creating the internal memory mapping of the matrix structure to be used for 
 * the dlx search algorithm. This function generates all nodes including the item column nodes, spacer nodes, and the
 * actual option nodes that need to be placed throughout the structure. Through each iteration, the appropriate links
 * are updated as the file is being read.
 * 
 * @param FILE* A file pointer to a file which contains a cover definition.
 * @param int An integer representing the total number of nodes that need to be created.
 * @return struct node* Returns the address of the malloc'd matrix structure.
 */ 
struct node* generateMatrix(FILE* cover, int nodeCount, std::ostream* dump_stream)
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
    struct node* matrix = matrix::generateHeadNode(nodeCount);

    // Read first line of cover file and generate column headers
    read = getline(&buffer, &len, cover);
    currNodeCount = generateTitles(matrix, buffer);
    const int itemCount = currNodeCount;

    // Read line by line of entire cover file
    while ((read = getline(&buffer, &len, cover)) != -1)
    {
        // Create/update spacer nodes prior to dealing with next option
        matrix::handleSpacerNodes(matrix, &spaceNodeCount, currNodeCount, prevRowCount);

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

    if (dump_stream != nullptr)
    {
        matrix::dumpMatrixStructure(matrix, nodeCount, itemCount, *dump_stream);
    }

    return matrix;
}

/**
 * A utility function to assist in the generation of the matrix structure and appropriate links, this function
 * is dedicated to the construction of the item nodes (column headers) by reading tokens from the titleLine.
 * 
 * @param struct node* A node pointer to the head of the matrix.
 * @param char* A char pointer to the item line which is the first line in the cover file.
 * @return int Returns the current node count of how many item nodes were created from this method.
 */ 
int generateTitles(struct node* matrix, char* titleLine)
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

        // Populate item node
        matrix[currNodeCount + 1].len = 0;
        matrix[currNodeCount + 1].top = matrix;
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

} // namespace dlx::text
