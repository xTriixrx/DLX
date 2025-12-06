#ifndef ASCII_BINARY_UTILS_H
#define ASCII_BINARY_UTILS_H

#include <stdio.h>
#include <string>

int ascii_cover_to_binary_stream(const std::string& ascii_cover, FILE* output);
int binary_solution_to_ascii(FILE* input, std::string* ascii_output);

#endif
