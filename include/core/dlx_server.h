#ifndef DLX_SERVER_H
#define DLX_SERVER_H

#include <stdio.h>
#include "core/dlx.h"

struct node* dlx_server_read_binary(FILE* input,
                                    char*** titles,
                                    char*** solutions,
                                    int* item_count,
                                    int* option_count);

#endif
