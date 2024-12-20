#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#define TUI
#include "buffer.h"
#include "cvector.h"

typedef struct nv_buff* nv_buff_vec;

struct nv_editor {
    nv_buff_vec buffers;     // buffers
    size_t      peek;        // index of current buffer
    struct nv_buff* current; // &buffers[peek]
#ifdef TUI                   // if there is ever a qt/gui version
    int         width;       // not necessarily termbox window geometry
    int         height;      // custom wrap width (80 cols) might change this
#endif
    int         status;      // status codes of termbox + notvim operations
    bool        running;     // mainloop
    struct nv_conf nv_conf;  // default config for buffers
};

// config defaults
#define NV_TAB_WIDTH     4
#define NV_EXPAND_TAB    true
#define NV_AUTO_INDENT   true
#define NV_LINE_NUMBERS  true
#define NV_SHOW_RELATIVE true
#define NV_COMMAND_DELAY 350
#define NV_STATUS_HEIGHT 1
#define NV_SHOW_STATUS   true
#define NV_SHOW_BUFFER   true
#define NV_HEADLESS      false

#if NV_HEADLESS
#undef TUI
#endif

void nv_editor_init(struct nv_editor* editor);
void nv_render_term(struct nv_editor* editor);
void nv_mainloop(struct nv_editor* editor);
void nv_push_buffer(struct nv_editor* editor, struct nv_buff buffer);
void nv_free_buffers(struct nv_editor* editor);

#endif
