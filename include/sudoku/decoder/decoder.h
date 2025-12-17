#ifndef SUDOKU_DECODER_H
#define SUDOKU_DECODER_H

#include <stdbool.h>

/**
 * @brief Decode DLX solution rows back into a solved Sudoku grid.
 *
 * @param puzzle_path Path to the original puzzle used as a template ("-" for stdin).
 * @param solution_rows_path Path to the file containing DLX solution rows ("-" for stdin).
 * @param output_path Destination file for the solved puzzle ("-" for stdout).
 * @return 0 on success, non-zero if decoding fails.
 */
int decode_sudoku_solution(const char* puzzle_path,
                           const char* solution_rows_path,
                           const char* output_path);

#endif
