#ifndef DLX_BINARY_H
#define DLX_BINARY_H

#include <stdint.h>
#include <stdio.h>
#include "core/dlx.h"

#define DLX_COVER_MAGIC 0x444C5842u /* 'DLXB' */
#define DLX_SOLUTION_MAGIC 0x444C5853u /* 'DLXS' */
#define DLX_BINARY_VERSION 1

struct DlxCoverHeader
{
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t column_count;
    uint32_t row_count;
    uint32_t option_node_count;
};

struct DlxRowChunk
{
    uint32_t row_id;
    uint16_t entry_count;
    uint16_t capacity;
    uint32_t* columns;
};

int dlx_write_cover_header(FILE* output, const struct DlxCoverHeader* header);
int dlx_read_cover_header(FILE* input, struct DlxCoverHeader* header);
int dlx_write_row_chunk(FILE* output, uint32_t row_id, const uint32_t* columns, uint16_t column_count);
int dlx_read_row_chunk(FILE* input, struct DlxRowChunk* chunk);
void dlx_free_row_chunk(struct DlxRowChunk* chunk);
struct node* dlx_read_binary(FILE* input,
                            char*** titles_out,
                            char*** solutions_out,
                            int* item_count_out,
                            int* option_count_out);

#endif
