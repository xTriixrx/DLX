#ifndef SUDOKU_DECODER_H
#define SUDOKU_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

int decode_sudoku_solution(const char* puzzle_path,
                           const char* solution_rows_path,
                           const char* output_path);

#ifdef __cplusplus
}
#endif

#endif
