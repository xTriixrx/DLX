#ifndef DLX_SERVER_H
#define DLX_SERVER_H

#include <stdio.h>
#include <istream>
#include "core/dlx.h"

struct node* dlx_server_read_binary(std::istream& input,
                                    char*** solutions,
                                    int* item_count,
                                    int* option_count);

#endif
