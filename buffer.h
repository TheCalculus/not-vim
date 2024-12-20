#ifndef NOT_VIM_H
#define NOT_VIM_H

#include <stdlib.h>
#include <stdbool.h>

#include "cvector.h"

struct nv_conf {
    int  tab_width;
    bool expand_tab;
    bool auto_indent;
    bool line_numbers;
    bool show_relative;
    int  command_delay;
    int  status_height;
    bool show_status;
    bool show_buffer;
    bool show_headless;
};

enum nv_buff_t {
    NV_BUFFTYPE_STDIN     = 1,
    NV_BUFFTYPE_STDOUT    = 2,
    NV_BUFFTYPE_BROWSER   = 4,
    NV_BUFFTYPE_NETWORK   = 8,
    NV_BUFFTYPE_SOURCE    = 16,
    NV_BUFFTYPE_PLAINTEXT = 32,
};

enum nv_buff_f {
    NV_FILE_FORMAT_BINARY,
    NV_FILE_FORMAT_SOURCE,  // lsp + treesitter impl
    NV_FILE_FORMAT_PLAINTEXT,
};

struct nv_buff_line {
    size_t begin;
    size_t end;
    size_t length;
};

struct cursor {
    int x;
    int y;
    int xmem;
    int line;
    char ch;
    size_t buff_id;
};

struct nv_window {
    int x;
    int y;
    int w;
    int h;
    size_t buff_id;
};

struct nv_buff {
    size_t         id;      // id for buffer
    enum nv_buff_t type;    // what the buffer shows
    char*          path;    // path of buffer
    FILE*          file;    // FILE* if applicable
    enum nv_buff_f format;
   
    size_t         chunk;   // bytes to load into buffer at a time
    bool           loaded;  // is nv_buff_line* lines "calculated"
    cvector_vector_type(char) buffer;  // char buffer in memory written to file on write
    cvector_vector_type(struct nv_buff_line) lines;
    cvector_vector_type(struct cursor) cursors;

    int _begin_line;
    int _lines_col_size;
    int _line_count;
};

void nv_buffer_init(struct nv_buff* buff, char* path);
void _nv_load_file_buffer(struct nv_buff* buffer, int* out_line_count);

#endif
