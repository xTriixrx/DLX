#ifndef ASCII_BINARY_UTILS_H
#define ASCII_BINARY_UTILS_H

#include <istream>
#include <ostream>
#include <string>

int ascii_cover_to_binary_stream(const std::string& ascii_cover, std::ostream& output);
int binary_solution_to_ascii(std::istream& input, std::string* ascii_output);

#endif
