#include "core/dlx_binary.h"
#include "core/dlx.h"
#include <arpa/inet.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>

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

int dlx_write_solution_header(FILE* output, const struct DlxSolutionHeader* header)
{
    if (output == NULL || header == NULL)
    {
        return -1;
    }

    struct DlxSolutionHeader writable = *header;
    writable.magic = dlx_htonl(header->magic);
    writable.version = dlx_htons(header->version);
    writable.flags = dlx_htons(header->flags);
    writable.column_count = dlx_htonl(header->column_count);

    size_t written = fwrite(&writable, sizeof(writable), 1, output);
    return written == 1 ? 0 : -1;
}

int dlx_read_solution_header(FILE* input, struct DlxSolutionHeader* header)
{
    if (input == NULL || header == NULL)
    {
        return -1;
    }

    struct DlxSolutionHeader readable;
    size_t read = fread(&readable, sizeof(readable), 1, input);
    if (read != 1)
    {
        return -1;
    }

    header->magic = dlx_ntohl(readable.magic);
    header->version = dlx_ntohs(readable.version);
    header->flags = dlx_ntohs(readable.flags);
    header->column_count = dlx_ntohl(readable.column_count);
    return 0;
}

int dlx_write_solution_row(FILE* output,
                           uint32_t solution_id,
                           const uint32_t* row_indices,
                           uint16_t row_count)
{
    if (output == NULL || (row_count > 0 && row_indices == NULL))
    {
        return -1;
    }

    uint32_t id_net = dlx_htonl(solution_id);
    uint16_t count_net = dlx_htons(row_count);

    if (fwrite(&id_net, sizeof(id_net), 1, output) != 1)
    {
        return -1;
    }

    if (fwrite(&count_net, sizeof(count_net), 1, output) != 1)
    {
        return -1;
    }

    for (uint16_t i = 0; i < row_count; i++)
    {
        uint32_t value_net = dlx_htonl(row_indices[i]);
        if (fwrite(&value_net, sizeof(value_net), 1, output) != 1)
        {
            return -1;
        }
    }

    return 0;
}

static int ensure_solution_capacity(struct DlxSolutionRow* row, uint16_t required)
{
    if (row->capacity >= required)
    {
        return 0;
    }

    uint16_t new_capacity = row->capacity == 0 ? required : row->capacity;
    while (new_capacity < required)
    {
        if (new_capacity > UINT16_MAX / 2)
        {
            new_capacity = required;
            break;
        }

        new_capacity *= 2;
    }

    uint32_t* new_indices = static_cast<uint32_t*>(
        realloc(row->row_indices, sizeof(uint32_t) * new_capacity));
    if (new_indices == NULL)
    {
        return -1;
    }

    row->row_indices = new_indices;
    row->capacity = new_capacity;
    return 0;
}

int dlx_read_solution_row(FILE* input, struct DlxSolutionRow* row)
{
    if (input == NULL || row == NULL)
    {
        return -1;
    }

    uint32_t solution_id_net;
    uint16_t count_net;

    size_t read = fread(&solution_id_net, sizeof(solution_id_net), 1, input);
    if (read != 1)
    {
        if (feof(input))
        {
            row->entry_count = 0;
            return 0;
        }

        return -1;
    }

    if (fread(&count_net, sizeof(count_net), 1, input) != 1)
    {
        return -1;
    }

    uint16_t count = dlx_ntohs(count_net);
    if (ensure_solution_capacity(row, count) != 0)
    {
        return -1;
    }

    for (uint16_t i = 0; i < count; i++)
    {
        uint32_t value_net;
        if (fread(&value_net, sizeof(value_net), 1, input) != 1)
        {
            return -1;
        }

        row->row_indices[i] = dlx_ntohl(value_net);
    }

    row->solution_id = dlx_ntohl(solution_id_net);
    row->entry_count = count;
    return 1;
}

void dlx_free_solution_row(struct DlxSolutionRow* row)
{
    if (row == NULL)
    {
        return;
    }

    free(row->row_indices);
    row->row_indices = NULL;
    row->entry_count = 0;
    row->capacity = 0;
}


struct node* dlx_read_binary(FILE* input,
                            char*** titles_out,
                            char*** solutions_out,
                            int* item_count_out,
                            int* option_count_out)
{
    if (input == NULL || titles_out == NULL || solutions_out == NULL || item_count_out == NULL
        || option_count_out == NULL)
    {
        return NULL;
    }

    struct DlxCoverHeader header;
    if (dlx_read_cover_header(input, &header) != 0)
    {
        return NULL;
    }

    FILE* temp = tmpfile();
    if (temp == NULL)
    {
        return NULL;
    }

    // Write column titles
    for (uint32_t col = 0; col < header.column_count; col++)
    {
        fprintf(temp, "COL%u%s", col, (col + 1 == header.column_count) ? "\n" : " ");
    }

    struct DlxRowChunk chunk = {0};
    while (true)
    {
        int status = dlx_read_row_chunk(input, &chunk);
        if (status == 0)
            break;
        if (status == -1)
        {
            dlx_free_row_chunk(&chunk);
            fclose(temp);
            return NULL;
        }

        std::vector<char> row(header.column_count, '0');
        for (int i = 0; i < chunk.entry_count; i++)
        {
            uint32_t column = chunk.columns[i];
            if (column < header.column_count)
            {
                row[column] = '1';
            }
        }

        for (uint32_t col = 0; col < header.column_count; col++)
        {
            fputc(row[col], temp);
            if (col + 1 == header.column_count)
            {
                fputc('\n', temp);
            }
            else
            {
                fputc(' ', temp);
            }
        }
    }

    dlx_free_row_chunk(&chunk);
    fflush(temp);

    rewind(temp);
    int itemCount = getItemCount(temp);
    rewind(temp);
    int nodeCount = itemCount + getNodeCount(temp);
    rewind(temp);
    int optionCount = getOptionsCount(temp);
    rewind(temp);

    char** titles = static_cast<char**>(malloc(sizeof(char*) * itemCount));
    char** solutions = static_cast<char**>(calloc(optionCount, sizeof(char*)));
    if (titles == NULL || solutions == NULL)
    {
        fclose(temp);
        free(titles);
        free(solutions);
        return NULL;
    }

    struct node* matrix = generateMatrix(temp, titles, nodeCount);
    fclose(temp);

    if (matrix == NULL)
    {
        free(titles);
        free(solutions);
        return NULL;
    }

    *titles_out = titles;
    *solutions_out = solutions;
    *item_count_out = itemCount;
    *option_count_out = optionCount;

    return matrix;
}
