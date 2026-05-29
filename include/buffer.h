#ifndef NV_BUFFER_H
#define NV_BUFFER_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "context.h"
#include "cvector.h"
#include "view.h"
#include "nvtree/nvtree.h"
#include "window.h"

#define NV_BUFFID_UNSET        0
#define NV_BUFF_CHUNK_SIZE     1024 * 16
#define NV_LINE_CAP            2048

typedef enum {
    NV_BUFF_TYPE_STDIN        = 0,
    NV_BUFF_TYPE_STDOUT       = 1,
    NV_BUFF_TYPE_BROWSER      = 2,
    NV_BUFF_TYPE_NETWORK      = 3,
    NV_BUFF_TYPE_SOURCE       = 4,
    NV_BUFF_TYPE_PLAINTEXT    = 5,
    NV_BUFF_TYPE_LOG          = 6,
    NV_BUFF_TYPE_END,
} nv_buff_type;

typedef enum {
    NV_FILE_FORMAT_BINARY    = 0,
    NV_FILE_FORMAT_SOURCE    = 1, // lsp + treesitter impl
    NV_FILE_FORMAT_PLAINTEXT = 2,
    NV_FILE_FORMAT_END,
} nv_buff_fmt;

typedef enum {
    NV_BUFF_ID_ORIGINAL       = 0,
    NV_BUFF_ID_ADD,
    NV_BUFF_ID_DEL,
    NV_BUFF_ID_END,
} nv_buff_id;

extern char* nv_str_buff_type[NV_BUFF_TYPE_END];
extern char* nv_str_buff_fmt[NV_FILE_FORMAT_END];
#define NV_MAX_BUFFERS 256
extern char* nv_buffers[NV_MAX_BUFFERS]; // nv_buffers[buff_id + nv_buff_id]

typedef struct nv_render_line_s {
    char* ptr;
    size_t size;
} nv_render_line;

// TODO: make this dynamic, vary on viewport height
// circular buffer
#define NV_BUFFER_LINE_CACHE_CAPACITY 128
struct nv_line_cache {
    size_t first_line_number;
    size_t first_line_index;
    size_t size;
    cvector(nv_render_line) buffer[NV_BUFFER_LINE_CACHE_CAPACITY]; // easier to integrate lines spread across nodes
};

struct nv_buff {
    FILE* file;
    char* path;
    int line_count;
    size_t buff_id;
    size_t chunk_size;
    size_t bytes_loaded;
    size_t append_cursor;
    nv_buff_type type;
    nv_buff_fmt format;
    nv_tree* tree;
    struct nv_line_cache cache;
    cvector(char) scratch;
    cvector(char) buffer;
    cvector(char) add_buffer;
};

struct nv_view* nv_view_init(const char* buffer_file_path);
struct nv_buff* nv_buffer_init(const char* path);
int nv_buffer_build_tree(struct nv_buff* buff);
int nv_buffer_open_file(struct nv_buff* buff, const char* path);
int nv_free_view(struct nv_view* view);
int nv_free_buffer(struct nv_buff* buff);
void nv_buffer_line_cache(struct nv_buff* buff, size_t first_line, size_t amt);
cvector(nv_render_line) nv_get_computed_line(struct nv_context* ctx, int lineno);
int nv_clamp(int x, int min, int max);


#endif
