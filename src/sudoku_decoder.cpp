#include "sudoku_decoder.h"
#include "sudoku_matrix.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct candidate_vector
{
    struct SudokuCandidate* data;
    int size;
    int capacity;
};

static void free_candidate_vector(struct candidate_vector* vector)
{
    free(vector->data);
    vector->data = NULL;
    vector->size = 0;
    vector->capacity = 0;
}

static int ensure_capacity(struct candidate_vector* vector, int desired_capacity)
{
    if (vector->capacity >= desired_capacity)
    {
        return 0;
    }

    int new_capacity = vector->capacity == 0 ? 128 : vector->capacity * 2;
    while (new_capacity < desired_capacity)
    {
        new_capacity *= 2;
    }

    struct SudokuCandidate* new_data = static_cast<struct SudokuCandidate*>(
        realloc(vector->data, sizeof(struct SudokuCandidate) * new_capacity));
    if (new_data == NULL)
    {
        fprintf(stderr, "Failed to allocate candidate vector\n");
        return 1;
    }

    vector->data = new_data;
    vector->capacity = new_capacity;
    return 0;
}

static int collect_candidate(int row, int col, int digit, void* ctx)
{
    struct candidate_vector* vector = static_cast<struct candidate_vector*>(ctx);
    if (ensure_capacity(vector, vector->size + 1) != 0)
    {
        return 1;
    }

    vector->data[vector->size].row = row;
    vector->data[vector->size].col = col;
    vector->data[vector->size].digit = digit;
    vector->size += 1;
    return 0;
}

static void copy_usage(bool dest[GRID_SIZE][DIGIT_COUNT + 1],
                       const bool src[GRID_SIZE][DIGIT_COUNT + 1])
{
    for (int r = 0; r < GRID_SIZE; r++)
    {
        for (int d = 0; d <= DIGIT_COUNT; d++)
        {
            dest[r][d] = src[r][d];
        }
    }
}

static void copy_grid(int dest[GRID_SIZE][GRID_SIZE], const int src[GRID_SIZE][GRID_SIZE])
{
    for (int r = 0; r < GRID_SIZE; r++)
    {
        for (int c = 0; c < GRID_SIZE; c++)
        {
            dest[r][c] = src[r][c];
        }
    }
}

static bool allowed_digit(int row,
                          int col,
                          int digit,
                          const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                          const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                          const bool box_used[GRID_SIZE][DIGIT_COUNT + 1])
{
    int box = sudoku_box_index(row, col);
    return !(row_used[row][digit] || col_used[col][digit] || box_used[box][digit]);
}

static int apply_solution_line(const char* line,
                               const struct candidate_vector* candidates,
                               const int base_grid[GRID_SIZE][GRID_SIZE],
                               const bool base_row_used[GRID_SIZE][DIGIT_COUNT + 1],
                               const bool base_col_used[GRID_SIZE][DIGIT_COUNT + 1],
                               const bool base_box_used[GRID_SIZE][DIGIT_COUNT + 1],
                               int solved_grid[GRID_SIZE][GRID_SIZE])
{
    copy_grid(solved_grid, base_grid);

    bool row_used[GRID_SIZE][DIGIT_COUNT + 1];
    bool col_used[GRID_SIZE][DIGIT_COUNT + 1];
    bool box_used[GRID_SIZE][DIGIT_COUNT + 1];
    copy_usage(row_used, base_row_used);
    copy_usage(col_used, base_col_used);
    copy_usage(box_used, base_box_used);

    char* buffer = strdup(line);
    if (buffer == NULL)
    {
        fprintf(stderr, "Failed to duplicate solution line\n");
        return 1;
    }

    char* token = strtok(buffer, " \t\r\n");
    while (token != NULL)
    {
        if (*token == '\0')
        {
            token = strtok(NULL, " \t\r\n");
            continue;
        }

        char* endptr = NULL;
        long value = strtol(token, &endptr, 10);
        if (endptr == token || value <= 0 || value > candidates->size)
        {
            fprintf(stderr, "Invalid row identifier '%s' in solution\n", token);
            free(buffer);
            return 1;
        }

        const struct SudokuCandidate* candidate = &candidates->data[value - 1];
        int row = candidate->row;
        int col = candidate->col;
        int digit = candidate->digit;

        if (base_grid[row][col] != 0)
        {
            if (base_grid[row][col] != digit)
            {
                fprintf(stderr, "Solution digit %d conflicts with given value at (%d,%d)\n",
                        digit,
                        row,
                        col);
                free(buffer);
                return 1;
            }

            token = strtok(NULL, " \t\r\n");
            continue;
        }

        if (solved_grid[row][col] != 0 && solved_grid[row][col] != digit)
        {
            fprintf(stderr, "Conflicting assignment for cell (%d,%d)\n", row, col);
            free(buffer);
            return 1;
        }

        if (!allowed_digit(row, col, digit, row_used, col_used, box_used))
        {
            fprintf(stderr, "Digit %d invalid at cell (%d,%d)\n", digit, row, col);
            free(buffer);
            return 1;
        }

        solved_grid[row][col] = digit;
        row_used[row][digit] = true;
        col_used[col][digit] = true;
        box_used[sudoku_box_index(row, col)][digit] = true;

        token = strtok(NULL, " \t\r\n");
    }

    free(buffer);

    for (int row = 0; row < GRID_SIZE; row++)
    {
        for (int col = 0; col < GRID_SIZE; col++)
        {
            if (solved_grid[row][col] == 0)
            {
                fprintf(stderr, "Solution line did not fill cell (%d,%d)\n", row, col);
                return 1;
            }
        }
    }

    return 0;
}

static int write_solution(FILE* output, const int grid[GRID_SIZE][GRID_SIZE], int solution_index)
{
    fprintf(output, "Solution #%d\n", solution_index);
    for (int row = 0; row < GRID_SIZE; row++)
    {
        for (int col = 0; col < GRID_SIZE; col++)
        {
            fprintf(output, "%d", grid[row][col]);
        }
        fprintf(output, "\n");
    }
    fprintf(output, "\n");
    return 0;
}

extern "C" {

int decode_sudoku_solution(const char* puzzle_path,
                           const char* solution_rows_path,
                           const char* output_path)
{
    int grid[GRID_SIZE][GRID_SIZE] = {{0}};
    bool row_used[GRID_SIZE][DIGIT_COUNT + 1] = {{false}};
    bool col_used[GRID_SIZE][DIGIT_COUNT + 1] = {{false}};
    bool box_used[GRID_SIZE][DIGIT_COUNT + 1] = {{false}};

    if (load_sudoku_state(puzzle_path, grid, row_used, col_used, box_used) != 0)
    {
        return 1;
    }

    struct candidate_vector vector = {0};
    if (iterate_sudoku_candidates(grid, row_used, col_used, box_used, collect_candidate, &vector) != 0)
    {
        free_candidate_vector(&vector);
        return 1;
    }

    FILE* rows_file = fopen(solution_rows_path, "r");
    if (rows_file == NULL)
    {
        fprintf(stderr, "Unable to open solution rows file %s\n", solution_rows_path);
        free_candidate_vector(&vector);
        return 1;
    }

    FILE* output = fopen(output_path, "w");
    if (output == NULL)
    {
        fprintf(stderr, "Unable to create output file %s\n", output_path);
        fclose(rows_file);
        free_candidate_vector(&vector);
        return 1;
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int solution_index = 1;
    int solved_grid[GRID_SIZE][GRID_SIZE];
    int status = 0;

    while ((read = getline(&line, &len, rows_file)) != -1)
    {
        bool has_digit = false;
        for (ssize_t i = 0; i < read; i++)
        {
            if (!isspace((unsigned char)line[i]))
            {
                has_digit = true;
                break;
            }
        }

        if (!has_digit)
        {
            continue;
        }

        if (apply_solution_line(line,
                                &vector,
                                grid,
                                row_used,
                                col_used,
                                box_used,
                                solved_grid) != 0)
        {
            status = 1;
            break;
        }

        write_solution(output, solved_grid, solution_index++);
    }

    free(line);
    fclose(rows_file);
    fclose(output);
    free_candidate_vector(&vector);
    return status;
}

} // extern "C"
