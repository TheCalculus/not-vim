#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buffer.h"
#include "cvector.h"
#include "editor.h"
#include "error.h"
#include "nvtree/nvtree.h"
#include "std.h"
#include "view.h"

// extern'd
char* nv_buffers[NV_MAX_BUFFERS];

static size_t nv_calculate_tree_node_granularity(struct nv_buff* buff);

bool is_elf(const char* buffer)
{
    const char e_ident[] = { 0x7f, 45, 0x4c, 46 };
    for (int i = 0; i < 4; i++) {
        if (e_ident[i] != buffer[i]) {
            return false;
        }
    }
    return true;
}

int nv_buffer_open_file(struct nv_buff* buff, const char* path)
{
    if (!buff || !path || !buff->buffer) {
        return NV_ERR_NOT_INIT;
    }

    struct stat sb;
    if (stat(buff->path, &sb) == -1) {
        return NV_ERR;
    }

    switch (sb.st_mode & S_IFMT) {
    case S_IFLNK: // symlink
    case S_IFDIR:
        buff->type = NV_BUFF_TYPE_BROWSER;
        break;

    case S_IFREG:
        buff->type = NV_BUFF_TYPE_SOURCE;

        if (access(buff->path, W_OK) == 0) {
            buff->file = fopen(buff->path, "rb+");
        }
        else {
            // TODO: set readonly indicator
            buff->file = fopen(buff->path, "rb");
        }

        if (buff->file == NULL) {
            return NV_ERR;
        }

        buff->bytes_loaded = fread(buff->buffer, sizeof(char), buff->chunk_size, buff->file);
        cvector_set_size(buff->buffer, buff->chunk_size);

        break;

    case S_IFSOCK:
        buff->type = NV_BUFF_TYPE_NETWORK;
        break;

    default:
        return NV_ERR;
    }

    return NV_OK;
}

// returns no. lines not put into cache
static size_t nv_buffer_put_lines_into_cache(struct nv_buff* buff, nv_tree* node, size_t skip, size_t amt)
{
    if (!buff || !node) {
        NV_EDITOR_SET_STATUS(NV_ERR_NOT_INIT);
        return 0;
    }

    struct nv_line_cache* cache = &buff->cache;
    size_t lines_put_in = 0;
    size_t lines_witnessed = 0;
    size_t index_inside_node = 0;
    size_t line_begin = 0;
    char* node_buffer = nv_buffers[node->data.buff_id];
    cvector_set_size(cache->buffer[cache->first_line_index % NV_BUFFER_LINE_CACHE_CAPACITY], 0);

    while (lines_put_in < amt) {
        size_t cache_insertion_index = (cache->first_line_index + lines_put_in) % NV_BUFFER_LINE_CACHE_CAPACITY;

        if (index_inside_node >= node->data.size) {
            if (lines_witnessed >= skip) {
                nv_render_line hunk = (nv_render_line) {
                    .ptr = &node_buffer[line_begin],
                    .size = index_inside_node - line_begin,
                };
                cvector_push_back(cache->buffer[cache_insertion_index], hunk);
            }

            // WARN
            if (node->right) {
                node = node->right;
                line_begin = 0;
                index_inside_node = 0;
                node_buffer = nv_buffers[node->data.buff_id];
                continue;
            }
            else {
                break;
            }
        }

        if (node_buffer[index_inside_node] == '\n') {
            size_t prev_line_begin = line_begin;
            lines_witnessed++;
            line_begin = index_inside_node + 1;

            if (lines_witnessed > skip) {
                nv_render_line line = (nv_render_line) {
                    .ptr = &node_buffer[prev_line_begin],
                    .size = index_inside_node - prev_line_begin,
                };
                cvector_push_back(cache->buffer[cache_insertion_index], line);
                lines_put_in++;

                if (lines_put_in < amt) {
                    size_t next_slot = (cache->first_line_index + lines_put_in) % NV_BUFFER_LINE_CACHE_CAPACITY;
                    cvector_set_size(cache->buffer[next_slot], 0);
                }
            }
        }

        index_inside_node++;
    }

    return amt - lines_put_in;
}

void nv_buffer_line_cache(struct nv_buff* buff, size_t first_line_number, size_t amt)
{
    if (!buff) {
        NV_EDITOR_SET_STATUS(NV_ERR_NOT_INIT);
        return;
    }

    struct nv_line_cache* cache = &buff->cache;

    size_t cache_end_line = cache->first_line_number + cache->size;
    if (first_line_number >= cache->first_line_number && first_line_number + amt <= cache_end_line) {
        // nocache
        return;
    }

    size_t cache_top = (size_t)nv_clamp(
        (int)first_line_number - NV_BUFF_LINE_CACHE_EXPAND_LINES,
        1,
        (int)buff->line_count
    );

    nv_tree* stack[NV_TREE_MAX_STACK_DEPTH];
    int top = 0;
    size_t lines_into_node = 0;
    nv_tree* first_line = nv_tree_find_by_line(buff->tree, cache_top, stack, &top, &lines_into_node);

    if (cache->size == 0 || first_line_number >= cache->first_line_number + cache->size) {
        nv_log("full recache\n");
        cache->first_line_number = cache_top;
        cache->first_line_index = 0;
        cache->size = 0;

        size_t skip = lines_into_node ? lines_into_node - 1 : 0;
        size_t lines_unsuccess = nv_buffer_put_lines_into_cache(buff, first_line, skip, NV_BUFFER_LINE_CACHE_CAPACITY);
        cache->size = NV_BUFFER_LINE_CACHE_CAPACITY - lines_unsuccess;
    }
    else if (first_line_number < cache->first_line_number) {
        nv_log("precache\n");
        size_t delta = cache->first_line_number - cache_top;
        size_t cache_free_space = NV_BUFFER_LINE_CACHE_CAPACITY - cache->size;
        cache->first_line_number = cache_top;

        size_t wrapped_delta = delta % NV_BUFFER_LINE_CACHE_CAPACITY;
        size_t cache_insertion_index =
            (cache->first_line_index + NV_BUFFER_LINE_CACHE_CAPACITY - wrapped_delta) % NV_BUFFER_LINE_CACHE_CAPACITY;
        cache->first_line_index = cache_insertion_index;

        size_t skip = lines_into_node ? lines_into_node - 1 : 0;
        if (delta <= cache_free_space) {
            cache->size += delta;
            (void)nv_buffer_put_lines_into_cache(buff, first_line, skip, delta);
        }
        else {
            (void)nv_buffer_put_lines_into_cache(buff, first_line, skip, delta);
            cache->size += delta;
            if (cache->size > NV_BUFFER_LINE_CACHE_CAPACITY) {
                cache->size = NV_BUFFER_LINE_CACHE_CAPACITY;
            }
        }
    }
    else {
        // Bro
        nv_log("splitcache\n");

        if (cache_top > cache->first_line_number) {
            size_t delta = cache_top - cache->first_line_number;
            if (delta > cache->size) {
                delta = cache->size;
            }
            cache->first_line_index = (cache->first_line_index + delta) % NV_BUFFER_LINE_CACHE_CAPACITY;
            cache->first_line_number = cache_top;
            cache->size -= delta;
        }

        size_t cache_end_line = cache->first_line_number + cache->size;
        size_t desired_end = first_line_number + amt + NV_BUFF_LINE_CACHE_EXPAND_LINES;

        if (desired_end <= cache_end_line || cache->size >= NV_BUFFER_LINE_CACHE_CAPACITY) {
            return;
        }

        size_t lines_to_append = nv_max(desired_end - cache_end_line, NV_BUFF_LINE_CACHE_EXPAND_LINES);
        size_t free_space = NV_BUFFER_LINE_CACHE_CAPACITY - cache->size;
        if (lines_to_append > free_space) {
            lines_to_append = free_space;
        }

        nv_tree* append_stack[NV_TREE_MAX_STACK_DEPTH];
        int append_top = 0;
        size_t lines_into_node = 0;

        nv_tree* append_node =
            nv_tree_find_by_line(buff->tree, cache_end_line, append_stack, &append_top, &lines_into_node);

        size_t saved_index = cache->first_line_index;

        cache->first_line_index =
            (cache->first_line_index + cache->size) % NV_BUFFER_LINE_CACHE_CAPACITY;

        size_t skip = lines_into_node ? lines_into_node - 1 : 0;
        size_t not_inserted = nv_buffer_put_lines_into_cache(buff, append_node, skip, lines_to_append);

        cache->first_line_index = saved_index;
        cache->size += lines_to_append - not_inserted;
    }
}


// FIXME: receive BUFFTYPE instead of filepath, more flexible
struct nv_view* nv_view_init(const char* buffer_file_path)
{
    struct nv_view* view = (struct nv_view*)calloc(1, sizeof(struct nv_view));

    if (!view) {
        NV_EDITOR_SET_STATUS(NV_ERR_MEM);
        return NULL;
    }

    view->top_line_index = 1;
    view->buffer = nv_buffer_init(buffer_file_path);
    view->gutter_gap = 1;

    static_assert(NV_CURSOR_CAP > NV_PRIMARY_CURSOR, "");
    cvector_reserve(view->cursors, NV_CURSOR_CAP);
    view->cursors[NV_PRIMARY_CURSOR] = (struct cursor) { .line = 1 };
    cvector_set_size(view->cursors, 1);

    if (nv_editor->status != NV_OK) {
        return NULL;
    }

    cvector_push_back(nv_editor->views, view);
    NV_EDITOR_SET_STATUS(NV_OK);
    return view;
}

#define NV_MIN_GRANULARITY 64
#define NV_MAX_GRANULARITY (64 * 1024)

static size_t nv_calculate_tree_node_granularity(struct nv_buff* buff)
{
    if (!buff) {
        NV_EDITOR_SET_STATUS(NV_ERR_NOT_INIT);
        return 0;
    }

    if (buff->bytes_loaded == 0) {
        return NV_MIN_GRANULARITY;
    }

    size_t granularity = (buff->chunk_size * 1024) / buff->bytes_loaded;
    granularity *= 1024;

    if (granularity < NV_MIN_GRANULARITY) {
        granularity = NV_MIN_GRANULARITY;
    }
    else if (granularity > NV_MAX_GRANULARITY) {
        granularity = NV_MAX_GRANULARITY;
    }

    if (granularity > buff->chunk_size) {
        granularity = buff->chunk_size;
    }

    return granularity;
}

int nv_buffer_build_tree(struct nv_buff* buff)
{
    if (!buff || !buff->buffer) {
        return NV_ERR_NOT_INIT;
    }

    char* b = buff->buffer;
    static int buff_id = 0;
    buff->buff_id = buff_id;
    buff_id += NV_BUFF_ID_END;
    nv_buffers[buff->buff_id + NV_BUFF_ID_ORIGINAL] = b;
    buff->tree = nv_tree_init();

    size_t line_count = 0;
    size_t abs_pos = 0; // position in buffer
    size_t granularity = nv_calculate_tree_node_granularity(buff);

    nv_tree_data data = {
        .buff_id = buff->buff_id,
    };

    while (abs_pos < buff->chunk_size) {
        if (!b[abs_pos]) {
            break;
        }

        data.size++;
        if (b[abs_pos] == '\n') {
            data.line_count++;
            line_count++;
        }

        if (data.size >= granularity) {
            buff->tree = nv_tree_insert(data, buff->tree, data.offset);
            data.offset += data.size;
            data.size = 0;
            data.line_count = 0;
        }

        abs_pos++;
    }

    if (data.size > 0) {
        buff->tree = nv_tree_insert(data, buff->tree, data.offset);
    }

    if (line_count == 0) {
        line_count = 1;
    }

    buff->line_count = line_count;

    return NV_OK;
}

struct nv_buff* nv_buffer_init(const char* path)
{
    struct nv_buff* buffer = (struct nv_buff*)calloc(1, sizeof(struct nv_buff));

    if (!buffer) {
        NV_EDITOR_SET_STATUS(NV_ERR_NOT_INIT);
        return NULL;
    }

#define NV_BUFF_RENDER_BUFF_SIZE      4096
#define NV_BUFF_INT_SCRATCH_BUFF_SIZE 256

    static size_t buff_id;
    buffer->type = NV_BUFF_TYPE_PLAINTEXT;
    buffer->chunk_size = NV_BUFF_CHUNK_SIZE;
    buffer->buff_id = buff_id++;
    cvector_reserve(buffer->scratch, NV_BUFF_INT_SCRATCH_BUFF_SIZE);
    cvector_reserve(buffer->buffer, (size_t)NV_BUFF_CHUNK_SIZE);
    cvector_reserve(buffer->add_buffer, (size_t)NV_BUFF_CHUNK_SIZE);

    buffer->cache.first_line_number = 1;
    buffer->cache.first_line_index = 0;
    buffer->cache.size = 0;
    for (int i = 0; i < NV_BUFFER_LINE_CACHE_CAPACITY; i++) {
        cvector_reserve(buffer->cache.buffer[i], 32);
    }

    if (path) {
        buffer->path = (char*)path;
        NV_EDITOR_SET_STATUS(nv_buffer_open_file(buffer, path));
        (void)nv_buffer_build_tree(buffer);
    }

    return nv_editor->status == NV_OK ? buffer : NULL;
}

cvector(nv_render_line) nv_get_computed_line(struct nv_context* ctx, int lineno)
{
    struct nv_line_cache* cache = &ctx->buffer->cache;
    if (cache->size == 0 || lineno >= cache->first_line_number + cache->size) {
        return NULL;
    }

    size_t line_index_in_cache = (cache->first_line_index + lineno - cache->first_line_number) % cache->size;
    return cache->buffer[line_index_in_cache];
}

int nv_free_view(struct nv_view* view)
{
    if (!view) {
        return NV_ERR_NOT_INIT;
    }

    cvector_free(view->cursors);
    nv_free_buffer(view->buffer);

    free(view);
    return NV_OK;
}

int nv_free_buffer(struct nv_buff* buff)
{
    if (!buff) {
        return NV_ERR_NOT_INIT;
    }

    if (buff->file) {
        (void)fclose(buff->file);
        buff->file = NULL;
    }

    if (buff->tree) {
        nv_log_unimplemented();
    }

    cvector_free(buff->scratch);
    cvector_free(buff->buffer);
    cvector_free(buff->add_buffer);
    for (int i = 0; i < NV_BUFFER_LINE_CACHE_CAPACITY; i++) {
        cvector_free(buff->cache.buffer[i]);
    }

    free(buff);
    return NV_OK;
}