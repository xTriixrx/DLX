#include "core/dlx_server.h"
#include "core/dlx_binary.h"

struct node* dlx_server_read_binary(std::istream& input,
                                    char*** solutions,
                                    int* item_count,
                                    int* option_count)
{
    return dlx_read_binary(input, solutions, item_count, option_count);
}
