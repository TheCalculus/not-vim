#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "assert.h"
#include "buffer.h"
#include "editor.h"
#include "termbox2.h"
#include "vec.h"

// TODO
static void _nv_get_input(struct nv_editor* editor, struct tb_event* ev);
static void _nv_draw_buffer(struct nv_editor* editor);
static void _nv_draw_status(struct nv_editor* editor);
static struct nv_buff* _nv_get_active_buffer(struct nv_editor* editor);

void nv_editor_init(struct nv_editor* editor) {
    NV_ASSERT(editor);

    editor->buffers = vector_create();
    
    editor->nv_conf = (struct nv_conf){
        .tab_width     = NV_TAB_WIDTH,
        .expand_tab    = NV_TAB_WIDTH,
        .auto_indent   = NV_AUTO_INDENT,
        .line_numbers  = NV_LINE_NUMBERS,
        .show_relative = NV_SHOW_RELATIVE,
        .command_delay = NV_COMMAND_DELAY,
        .show_headless = NV_HEADLESS,
    };
}

static void _nv_draw_cursor(struct nv_editor* editor) {
#define LINES_COL 4
    struct nv_buff* buffer = _nv_get_active_buffer(editor);
    tb_set_cell(LINES_COL + buffer->cursor.x, buffer->cursor.y, ' ', TB_BLACK, TB_WHITE);
    tb_present();
}

static void _nv_redraw_all(struct nv_editor* editor) {
    if (editor->nv_conf.show_headless) return;

    tb_clear();
    _nv_draw_buffer(editor);
    _nv_draw_cursor(editor);
    _nv_draw_status(editor);
    tb_present();
}

void nv_mainloop(struct nv_editor* editor) {
    tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);

    editor->running = true;
    _nv_redraw_all(editor);

    struct tb_event ev;

    while (editor->running) {
        editor->status = tb_poll_event(&ev);

        switch (ev.type) {
        case TB_EVENT_KEY:
            _nv_get_input(editor, &ev);

            _nv_draw_buffer(editor);
            _nv_draw_cursor(editor);

            break;


        case TB_EVENT_RESIZE:
            if (editor->nv_conf.show_headless) break;

            editor->height = tb_height();
            editor->width = tb_width();
            editor->buffers[editor->peek].loaded = false; // redraw lines
   
            _nv_redraw_all(editor);

            break;

        default: break;
        }
    }
}

static void
_nv_get_input(struct nv_editor* editor, struct tb_event* ev) {
    if (editor->nv_conf.show_headless) return;
    editor->status = ev->key ? ev->key : ev->ch;
    if (ev->key == TB_KEY_ESC) return;
    struct nv_buff* buffer = _nv_get_active_buffer(editor);

    struct event_key {
        int ch;
        int key;
        int mod;
    };

    switch (ev->ch) {
        case 'j':
        {
            if (buffer->cursor.y < vector_size(buffer->lines))
                buffer->cursor.y += 1;

            struct nv_buff_line line = buffer->lines[buffer->cursor.y];
            size_t eol = line.end - line.begin;

            if (buffer->cursor.x >= eol)
                buffer->cursor.x = eol;
        }

            break;

        case 'k':
            if (buffer->cursor.y > 0)
                buffer->cursor.y -= 1;

            break;

        case 'h':
            if (buffer->cursor.x > 0)
                buffer->cursor.x -= 1;

            break;

        case 'l':
        {
            struct nv_buff_line line = buffer->lines[buffer->cursor.y];
            size_t eol = line.end - line.begin;

            if (buffer->cursor.x < eol)
                buffer->cursor.x += 1;
        }

            break;
    }

#define NV_BUFFER_INSERT_CHAR(editor, character)
    NV_BUFFER_INSERT_CHAR(editor, ev->ch);
}

void nv_push_buffer(struct nv_editor* editor, struct nv_buff buffer) {
    buffer.id = vector_size(editor->buffers);
    vector_add(&editor->buffers, buffer);
}

static struct nv_buff*
_nv_get_active_buffer(struct nv_editor* editor) {
    struct nv_buff* buffer = (struct nv_buff*)&editor->buffers[editor->peek];
    editor->current = buffer;
    return buffer;
}

static void
_nv_draw_buffer(struct nv_editor* editor) {
    struct nv_buff* buffer = _nv_get_active_buffer(editor);
    int line_count = 0;
    switch (buffer->type) {
    case NV_BUFFTYPE_SOURCE:
        if (!buffer->loaded) _nv_load_file_buffer(buffer, &line_count);

        for (int i = 0; i < line_count; i++) {
            if (i == editor->height - 1) break; // status bar

            struct nv_buff_line line = buffer->lines[i];
            size_t size = line.end - line.begin;
            char* line_string = malloc(size + 1);
            memcpy(line_string, buffer->buffer + line.begin, size);
            line_string[size] = '\0';
            tb_printf(0, i, TB_WHITE, TB_BLACK, "%-4.d %s", i + 1, line_string);
            free(line_string);
        }

        break;

    case NV_BUFFTYPE_PLAINTEXT:
        tb_print(0, 0, TB_WHITE, TB_BLACK, buffer->buffer);
        break;

    case NV_BUFFTYPE_BROWSER:
        tb_print(0, 0, TB_WHITE, TB_BLACK, "netrw");
        break;

    default:
        fprintf(stderr, "unsupported bufftype %d\n", buffer->type);
        break;
    }
}

static void
_nv_draw_status(struct nv_editor* editor) {
    struct nv_buff* buffer = _nv_get_active_buffer(editor);
    char* prompt;
    if (asprintf(&prompt, "[%zu] %s", buffer->id, buffer->path) == -1) return;
    tb_printf(0, editor->height - 1, TB_BLACK, TB_WHITE, "%-*.*s", editor->width, editor->width, prompt);
    free(prompt);
}
