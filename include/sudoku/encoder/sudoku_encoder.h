#ifndef SUDOKU_ENCODER_H
#define SUDOKU_ENCODER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GRID_SIZE 9
#define BOX_SIZE 3
#define DIGIT_COUNT 9
#define COLUMN_COUNT 324
#define CELL_CONSTRAINTS (GRID_SIZE * GRID_SIZE)
#define ROW_DIGIT_OFFSET CELL_CONSTRAINTS
#define ROW_DIGIT_CONSTRAINTS (GRID_SIZE * DIGIT_COUNT)
#define COL_DIGIT_OFFSET (ROW_DIGIT_OFFSET + ROW_DIGIT_CONSTRAINTS)
#define COL_DIGIT_CONSTRAINTS (GRID_SIZE * DIGIT_COUNT)
#define BOX_DIGIT_OFFSET (COL_DIGIT_OFFSET + COL_DIGIT_CONSTRAINTS)

struct SudokuCandidate
{
    int row;
    int col;
    int digit;
};

typedef int (*sudoku_candidate_handler)(int row, int col, int digit, void* ctx);

int convert_sudoku_to_cover(const char* puzzle_path, const char* cover_path, bool binary_format);
int load_sudoku_state(const char* puzzle_path,
                      int grid[GRID_SIZE][GRID_SIZE],
                      bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                      bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                      bool box_used[GRID_SIZE][DIGIT_COUNT + 1]);
int iterate_sudoku_candidates(const int grid[GRID_SIZE][GRID_SIZE],
                              const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool box_used[GRID_SIZE][DIGIT_COUNT + 1],
                              sudoku_candidate_handler handler,
                              void* ctx);

static inline int sudoku_box_index(int row, int col)
{
    return (row / BOX_SIZE) * BOX_SIZE + (col / BOX_SIZE);
}

#ifdef __cplusplus
}
#endif

#endif
