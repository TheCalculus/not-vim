#ifndef NOT_VIM_H
#define NOT_VIM_H

#include <stdlib.h>
#include <stdbool.h>

#include "vec.h"

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

typedef int nv_buff_t;

enum {
    NV_BUFFTYPE_STDIN     = 1,
    NV_BUFFTYPE_STDOUT    = 2,
    NV_BUFFTYPE_BROWSER   = 4,
    NV_BUFFTYPE_NETWORK   = 8,
    NV_BUFFTYPE_SOURCE    = 16,
    NV_BUFFTYPE_PLAINTEXT = 32,
};

struct nv_buff_line {
    size_t begin;
    size_t end;
};

// vec.h typedefs
typedef struct nv_buff_line* lines_vec;

struct nv_buff {
    size_t      id;       // id for buffer
    nv_buff_t   type;     // what the buffer shows
    char*       path;     // path of buffer
    FILE*       file;     // FILE* if applicable
    char*       buffer;   // char buffer in memory written to file on write
    size_t      chunk;
    bool        loaded;
    lines_vec   lines;
};

void nv_buffer_init(struct nv_buff* buff, char* path);
void _nv_load_file_buffer(struct nv_buff* buffer, int* out_line_count);

#endif
