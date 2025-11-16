#ifndef SUDOKU_DECODER_H
#define SUDOKU_DECODER_H

#include <stdbool.h>

/**
 * @brief Decode DLX solution rows back into a solved Sudoku grid.
 *
 * @param puzzle_path Path to the original puzzle used as a template.
 * @param solution_rows_path Path to the file containing DLX solution rows.
 * @param output_path Destination file for the solved puzzle.
 * @param binary_input Set to true if solution rows are stored in DLX binary format.
 * @return 0 on success, non-zero if decoding fails.
 */
int decode_sudoku_solution(const char* puzzle_path,
                           const char* solution_rows_path,
                           const char* output_path,
                           bool binary_input);

#endif
