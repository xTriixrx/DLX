#include "core/dlx_server.h"
#include "core/dlx_binary.h"

struct node* dlx_server_read_binary(FILE* input,
                                    char*** titles,
                                    char*** solutions,
                                    int* item_count,
                                    int* option_count)
{
    return dlx_read_binary(input, titles, solutions, item_count, option_count);
}
