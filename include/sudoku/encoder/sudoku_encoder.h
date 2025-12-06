#ifndef SUDOKU_ENCODER_H
#define SUDOKU_ENCODER_H

#include <stdbool.h>


/** @brief Size of the Sudoku grid (9 rows and 9 columns). */
#define GRID_SIZE 9
/** @brief Width/height of each 3x3 sub-box in the puzzle. */
#define BOX_SIZE 3
/** @brief Number of digits available for each cell (1-9). */
#define DIGIT_COUNT 9
/** @brief Number of columns in the exact-cover matrix (4 * 81). */
#define COLUMN_COUNT 324
/** @brief Constraint count for the 81 cell occupancy rules. */
#define CELL_CONSTRAINTS (GRID_SIZE * GRID_SIZE)
/** @brief Column offset for row/digit constraints. */
#define ROW_DIGIT_OFFSET CELL_CONSTRAINTS
/** @brief Total row/digit constraints (9 rows * 9 digits). */
#define ROW_DIGIT_CONSTRAINTS (GRID_SIZE * DIGIT_COUNT)
/** @brief Column offset for column/digit constraints. */
#define COL_DIGIT_OFFSET (ROW_DIGIT_OFFSET + ROW_DIGIT_CONSTRAINTS)
/** @brief Total column/digit constraints. */
#define COL_DIGIT_CONSTRAINTS (GRID_SIZE * DIGIT_COUNT)
/** @brief Column offset for box/digit constraints. */
#define BOX_DIGIT_OFFSET (COL_DIGIT_OFFSET + COL_DIGIT_CONSTRAINTS)
/** @brief Total box/digit constraints. */
#define BOX_DIGIT_CONSTRAINTS (GRID_SIZE * DIGIT_COUNT)

#ifdef __cplusplus
static_assert(CELL_CONSTRAINTS + ROW_DIGIT_CONSTRAINTS + COL_DIGIT_CONSTRAINTS + BOX_DIGIT_CONSTRAINTS == COLUMN_COUNT,
              "Sudoku constraint columns must match declared column count");
#endif

/** @brief Candidate metadata describing the row/column/digit triple. */
struct SudokuCandidate
{
    int row;
    int col;
    int digit;
};

/** @brief Callback invoked for each candidate emitted by the encoder. */
typedef int (*sudoku_candidate_handler)(int row, int col, int digit, void* ctx);

/**
 * @brief Convert a Sudoku puzzle into a DLX binary cover file.
 *
 * @param puzzle_path Path to the textual puzzle definition (81 characters). Pass "-" to read stdin.
 * @param cover_path Destination cover file. Pass "-" to write to stdout.
 * @return 0 on success, non-zero on parse or IO failure.
 */
int convert_sudoku_to_cover(const char* puzzle_path, const char* cover_path);
/**
 * @brief Load a Sudoku grid into memory and track used digits.
 */
int load_sudoku_state(const char* puzzle_path,
                      int grid[GRID_SIZE][GRID_SIZE],
                      bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                      bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                      bool box_used[GRID_SIZE][DIGIT_COUNT + 1]);
/**
 * @brief Iterate every allowed candidate and invoke a handler callback.
 */
int iterate_sudoku_candidates(const int grid[GRID_SIZE][GRID_SIZE],
                              const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool box_used[GRID_SIZE][DIGIT_COUNT + 1],
                              sudoku_candidate_handler handler,
                              void* ctx);

/**
 * @brief Compute the zero-based box index for a particular row/column.
 */
static inline int sudoku_box_index(int row, int col)
{
    return (row / BOX_SIZE) * BOX_SIZE + (col / BOX_SIZE);
}

#endif
