#include "sudoku/encoder/sudoku_encoder.h"
#include "core/dlx_binary.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_puzzle(FILE* input,
                        int grid[GRID_SIZE][GRID_SIZE],
                        bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                        bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                        bool box_used[GRID_SIZE][DIGIT_COUNT + 1]);
static int write_cover(FILE* output,
                       const int grid[GRID_SIZE][GRID_SIZE],
                       const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                       const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                       const bool box_used[GRID_SIZE][DIGIT_COUNT + 1]);
static int write_binary_cover(FILE* output,
                              const int grid[GRID_SIZE][GRID_SIZE],
                              const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool box_used[GRID_SIZE][DIGIT_COUNT + 1]);
static int write_header(FILE* output);
static int emit_candidate(FILE* output, int row, int col, int digit);
static int emit_row(FILE* output, const int column_indices[4]);
static bool digit_allowed(int row,
                          int col,
                          int digit,
                          const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                          const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                          const bool box_used[GRID_SIZE][DIGIT_COUNT + 1]);
static void build_column_name(int column_index, char* buffer, size_t buffer_len);
static void build_column_indices(int row, int col, int digit, uint32_t indices[4]);

int convert_sudoku_to_cover(const char* puzzle_path, const char* cover_path, bool binary_format)
{
    const char* mode = binary_format ? "wb" : "w";
    FILE* output = fopen(cover_path, mode);
    if (output == NULL)
    {
        fprintf(stderr, "Unable to create cover file %s\n", cover_path);
        return 1;
    }

    int grid[GRID_SIZE][GRID_SIZE] = {{0}};
    bool row_used[GRID_SIZE][DIGIT_COUNT + 1] = {{false}};
    bool col_used[GRID_SIZE][DIGIT_COUNT + 1] = {{false}};
    bool box_used[GRID_SIZE][DIGIT_COUNT + 1] = {{false}};

    if (load_sudoku_state(puzzle_path, grid, row_used, col_used, box_used) != 0)
    {
        fclose(output);
        remove(cover_path);
        return 1;
    }

    int status;
    if (binary_format)
    {
        status = write_binary_cover(output, grid, row_used, col_used, box_used);
    }
    else
    {
        status = write_cover(output, grid, row_used, col_used, box_used);
    }

    if (status != 0)
    {
        fclose(output);
        remove(cover_path);
        return 1;
    }

    fclose(output);
    return 0;
}

static int parse_puzzle(FILE* input,
                        int grid[GRID_SIZE][GRID_SIZE],
                        bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                        bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                        bool box_used[GRID_SIZE][DIGIT_COUNT + 1])
{
    int row = 0;
    int col = 0;
    int ch;

    while ((ch = fgetc(input)) != EOF)
    {
        if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t')
        {
            continue;
        }

        if (row >= GRID_SIZE)
        {
            fprintf(stderr, "Puzzle contains more than 81 cells\n");
            return 1;
        }

        if (ch == '.' || ch == '0')
        {
            grid[row][col] = 0;
        }
        else if (isdigit(ch) && ch != '0')
        {
            int value = ch - '0';
            int box = sudoku_box_index(row, col);

            if (row_used[row][value] || col_used[col][value] || box_used[box][value])
            {
                fprintf(stderr, "Puzzle contains conflicting digit at row %d col %d\n", row, col);
                return 1;
            }

            grid[row][col] = value;
            row_used[row][value] = true;
            col_used[col][value] = true;
            box_used[box][value] = true;
        }
        else
        {
            fprintf(stderr, "Invalid character '%c' in puzzle\n", ch);
            return 1;
        }

        col++;
        if (col == GRID_SIZE)
        {
            row++;
            col = 0;
        }
    }

    if (!(row == GRID_SIZE && col == 0))
    {
        fprintf(stderr, "Puzzle must contain exactly 81 entries\n");
        return 1;
    }

    return 0;
}

static bool digit_allowed(int row,
                          int col,
                          int digit,
                          const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                          const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                          const bool box_used[GRID_SIZE][DIGIT_COUNT + 1])
{
    int box = sudoku_box_index(row, col);
    return !(row_used[row][digit] || col_used[col][digit] || box_used[box][digit]);
}

int load_sudoku_state(const char* puzzle_path,
                      int grid[GRID_SIZE][GRID_SIZE],
                      bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                      bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                      bool box_used[GRID_SIZE][DIGIT_COUNT + 1])
{
    FILE* input = fopen(puzzle_path, "r");
    if (input == NULL)
    {
        fprintf(stderr, "Unable to open puzzle file %s\n", puzzle_path);
        return 1;
    }

    int result = parse_puzzle(input, grid, row_used, col_used, box_used);
    fclose(input);
    return result;
}

int iterate_sudoku_candidates(const int grid[GRID_SIZE][GRID_SIZE],
                              const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool box_used[GRID_SIZE][DIGIT_COUNT + 1],
                              sudoku_candidate_handler handler,
                              void* ctx)
{
    for (int row = 0; row < GRID_SIZE; row++)
    {
        for (int col = 0; col < GRID_SIZE; col++)
        {
            int cell_value = grid[row][col];

            if (cell_value > 0)
            {
                if (handler(row, col, cell_value, ctx) != 0)
                {
                    return 1;
                }
            }
            else
            {
                bool emitted = false;
                for (int digit = 1; digit <= DIGIT_COUNT; digit++)
                {
                    if (!digit_allowed(row, col, digit, row_used, col_used, box_used))
                    {
                        continue;
                    }

                    if (handler(row, col, digit, ctx) != 0)
                    {
                        return 1;
                    }

                    emitted = true;
                }

                if (!emitted)
                {
                    fprintf(stderr, "No valid digits for cell (%d,%d)\n", row, col);
                    return 1;
                }
            }
        }
    }

    return 0;
}


struct cover_writer_ctx
{
    FILE* output;
};

static int cover_writer_handler(int row, int col, int digit, void* ctx)
{
    struct cover_writer_ctx* writer = static_cast<struct cover_writer_ctx*>(ctx);
    return emit_candidate(writer->output, row, col, digit);
}

static void build_column_indices(int row, int col, int digit, uint32_t indices[4])
{
    indices[0] = row * GRID_SIZE + col;
    indices[1] = ROW_DIGIT_OFFSET + row * DIGIT_COUNT + (digit - 1);
    indices[2] = COL_DIGIT_OFFSET + col * DIGIT_COUNT + (digit - 1);
    indices[3] = BOX_DIGIT_OFFSET + sudoku_box_index(row, col) * DIGIT_COUNT + (digit - 1);
}

struct binary_writer_ctx
{
    FILE* output;
    uint32_t next_row_id;
};

static int binary_row_handler(int row, int col, int digit, void* ctx)
{
    struct binary_writer_ctx* writer = static_cast<struct binary_writer_ctx*>(ctx);
    uint32_t indices[4];
    build_column_indices(row, col, digit, indices);
    if (dlx_write_row_chunk(writer->output, writer->next_row_id, indices, 4) != 0)
    {
        return 1;
    }

    writer->next_row_id++;
    return 0;
}

static int write_binary_cover(FILE* output,
                              const int grid[GRID_SIZE][GRID_SIZE],
                              const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                              const bool box_used[GRID_SIZE][DIGIT_COUNT + 1])
{
    struct DlxCoverHeader header = {
        .magic = DLX_COVER_MAGIC,
        .version = DLX_BINARY_VERSION,
        .flags = 0,
        .column_count = COLUMN_COUNT,
        .row_count = 0,
        .option_node_count = 0,
    };

    if (dlx_write_cover_header(output, &header) != 0)
    {
        return 1;
    }

    struct binary_writer_ctx ctx = {.output = output, .next_row_id = 1};
    return iterate_sudoku_candidates(grid, row_used, col_used, box_used, binary_row_handler, &ctx);
}

static int write_cover(FILE* output,
                       const int grid[GRID_SIZE][GRID_SIZE],
                       const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                       const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                       const bool box_used[GRID_SIZE][DIGIT_COUNT + 1])
{
    if (write_header(output) != 0)
    {
        return 1;
    }

    struct cover_writer_ctx ctx = {.output = output};
    return iterate_sudoku_candidates(grid, row_used, col_used, box_used, cover_writer_handler, &ctx);
}

static int write_header(FILE* output)
{
    char buffer[32];

    for (int column = 0; column < COLUMN_COUNT; column++)
    {
        build_column_name(column, buffer, sizeof(buffer));
        if (column == COLUMN_COUNT - 1)
        {
            fprintf(output, "%s\n", buffer);
        }
        else
        {
            fprintf(output, "%s ", buffer);
        }
    }

    return 0;
}

static void build_column_name(int column_index, char* buffer, size_t buffer_len)
{
    if (column_index < CELL_CONSTRAINTS)
    {
        int row = column_index / GRID_SIZE;
        int col = column_index % GRID_SIZE;
        snprintf(buffer, buffer_len, "Cell_r%d_c%d", row, col);
    }
    else if (column_index < COL_DIGIT_OFFSET)
    {
        int offset = column_index - ROW_DIGIT_OFFSET;
        int row = offset / DIGIT_COUNT;
        int digit = offset % DIGIT_COUNT + 1;
        snprintf(buffer, buffer_len, "Row_r%d_d%d", row, digit);
    }
    else if (column_index < BOX_DIGIT_OFFSET)
    {
        int offset = column_index - COL_DIGIT_OFFSET;
        int col = offset / DIGIT_COUNT;
        int digit = offset % DIGIT_COUNT + 1;
        snprintf(buffer, buffer_len, "Col_c%d_d%d", col, digit);
    }
    else
    {
        int offset = column_index - BOX_DIGIT_OFFSET;
        int box = offset / DIGIT_COUNT;
        int digit = offset % DIGIT_COUNT + 1;
        snprintf(buffer, buffer_len, "Box_b%d_d%d", box, digit);
    }
}

static int emit_candidate(FILE* output, int row, int col, int digit)
{
    int indices[4];
    uint32_t unsigned_indices[4];
    build_column_indices(row, col, digit, unsigned_indices);
    for (int i = 0; i < 4; i++)
    {
        indices[i] = (int)unsigned_indices[i];
    }
    return emit_row(output, indices);
}

static int emit_row(FILE* output, const int column_indices[4])
{
    for (int column = 0; column < COLUMN_COUNT; column++)
    {
        int value = 0;
        for (int k = 0; k < 4; k++)
        {
            if (column_indices[k] == column)
            {
                value = 1;
                break;
            }
        }

        if (column == COLUMN_COUNT - 1)
        {
            fprintf(output, "%d\n", value);
        }
        else
        {
            fprintf(output, "%d ", value);
        }
    }

    return 0;
}
