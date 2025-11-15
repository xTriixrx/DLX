#include "dlx_binary.h"
#include <arpa/inet.h>
#include <climits>
#include <cstdio>
#include <cstdlib>

static uint16_t dlx_htons(uint16_t value)
{
    return htons(value);
}

static uint32_t dlx_htonl(uint32_t value)
{
    return htonl(value);
}

static uint16_t dlx_ntohs(uint16_t value)
{
    return ntohs(value);
}

static uint32_t dlx_ntohl(uint32_t value)
{
    return ntohl(value);
}

extern "C" {

int dlx_write_cover_header(FILE* output, const struct DlxCoverHeader* header)
{
    if (output == NULL || header == NULL)
    {
        return -1;
    }

    struct DlxCoverHeader writable = *header;
    writable.magic = dlx_htonl(header->magic);
    writable.version = dlx_htons(header->version);
    writable.flags = dlx_htons(header->flags);
    writable.column_count = dlx_htonl(header->column_count);
    writable.row_count = dlx_htonl(header->row_count);
    writable.option_node_count = dlx_htonl(header->option_node_count);

    size_t written = fwrite(&writable, sizeof(writable), 1, output);
    return written == 1 ? 0 : -1;
}

int dlx_read_cover_header(FILE* input, struct DlxCoverHeader* header)
{
    if (input == NULL || header == NULL)
    {
        return -1;
    }

    struct DlxCoverHeader readable;
    size_t read = fread(&readable, sizeof(readable), 1, input);
    if (read != 1)
    {
        return -1;
    }

    header->magic = dlx_ntohl(readable.magic);
    header->version = dlx_ntohs(readable.version);
    header->flags = dlx_ntohs(readable.flags);
    header->column_count = dlx_ntohl(readable.column_count);
    header->row_count = dlx_ntohl(readable.row_count);
    header->option_node_count = dlx_ntohl(readable.option_node_count);
    return 0;
}

int dlx_write_row_chunk(FILE* output, uint32_t row_id, const uint32_t* columns, uint16_t column_count)
{
    if (output == NULL || (column_count > 0 && columns == NULL))
    {
        return -1;
    }

    uint32_t row_id_net = dlx_htonl(row_id);
    uint16_t entry_count_net = dlx_htons(column_count);

    if (fwrite(&row_id_net, sizeof(row_id_net), 1, output) != 1)
    {
        return -1;
    }

    if (fwrite(&entry_count_net, sizeof(entry_count_net), 1, output) != 1)
    {
        return -1;
    }

    for (uint16_t i = 0; i < column_count; i++)
    {
        uint32_t col_net = dlx_htonl(columns[i]);
        if (fwrite(&col_net, sizeof(col_net), 1, output) != 1)
        {
            return -1;
        }
    }

    return 0;
}

static int ensure_chunk_capacity(struct DlxRowChunk* chunk, uint16_t required)
{
    if (chunk->capacity >= required)
    {
        return 0;
    }

    uint16_t new_capacity = chunk->capacity == 0 ? required : chunk->capacity;
    while (new_capacity < required)
    {
        if (new_capacity > UINT16_MAX / 2)
        {
            new_capacity = required;
            break;
        }
        new_capacity *= 2;
    }

    uint32_t* new_columns = static_cast<uint32_t*>(
        realloc(chunk->columns, sizeof(uint32_t) * new_capacity));
    if (new_columns == NULL)
    {
        return -1;
    }

    chunk->columns = new_columns;
    chunk->capacity = new_capacity;
    return 0;
}

int dlx_read_row_chunk(FILE* input, struct DlxRowChunk* chunk)
{
    if (input == NULL || chunk == NULL)
    {
        return -1;
    }

    uint32_t row_id_net;
    size_t read = fread(&row_id_net, sizeof(row_id_net), 1, input);

    if (read != 1)
    {
        if (feof(input))
        {
            return 0; // No more rows.
        }
        return -1;
    }

    uint16_t entry_count_net;
    if (fread(&entry_count_net, sizeof(entry_count_net), 1, input) != 1)
    {
        return -1;
    }

    uint16_t entry_count = dlx_ntohs(entry_count_net);
    if (ensure_chunk_capacity(chunk, entry_count) != 0)
    {
        return -1;
    }

    for (uint16_t i = 0; i < entry_count; i++)
    {
        uint32_t column_net;
        if (fread(&column_net, sizeof(column_net), 1, input) != 1)
        {
            return -1;
        }
        chunk->columns[i] = dlx_ntohl(column_net);
    }

    chunk->row_id = dlx_ntohl(row_id_net);
    chunk->entry_count = entry_count;
    return 1;
}

void dlx_free_row_chunk(struct DlxRowChunk* chunk)
{
    if (chunk == NULL)
    {
        return;
    }

    free(chunk->columns);
    chunk->columns = NULL;
    chunk->entry_count = 0;
    chunk->capacity = 0;
}

} // extern "C"
