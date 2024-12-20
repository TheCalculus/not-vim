#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "termbox2.h"

#include "assert.h"
#include "buffer.h"
#include "color.h"
#include "cvector.h"
#include "editor.h"

static void _nv_get_input(struct nv_editor* editor, struct tb_event* ev);
static void _nv_redraw_all(struct nv_editor* editor);
static size_t _nv_get_line_length(struct nv_buff_line line);
static void _nv_get_input(struct nv_editor* editor, struct tb_event* ev);
static struct nv_buff* _nv_get_active_buffer(struct nv_editor* editor);
static int count_recur(int n);
static void _nv_draw_buffer(struct nv_editor* editor);
static void _nv_draw_status(struct nv_editor* editor);
static void move_vertical(struct nv_editor* editor, struct cursor* cursor, struct nv_buff* buffer, int direction);
static void move_horizontal(struct cursor* cursor, struct nv_buff* buffer, int direction);

void nv_editor_init(struct nv_editor* editor) {
    NV_ASSERT(editor);

    cvector_reserve(editor->buffers, 8);
    
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
    struct nv_buff* buffer = _nv_get_active_buffer(editor);
    struct cursor c = buffer->cursors[0];
    tb_set_cell(buffer->_lines_col_size + c.x + 1, c.y, ' ', TB_256_BLACK, TB_256_WHITE);
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
    tb_set_output_mode(TB_OUTPUT_256);

    editor->running = true;
    _nv_redraw_all(editor);

    struct tb_event ev;

    while (editor->running) {
        editor->status = tb_poll_event(&ev);

        switch (ev.type) {
        case TB_EVENT_MOUSE:
        case TB_EVENT_KEY:
            if (ev.key == TB_KEY_ESC) return;

            _nv_draw_buffer(editor);
            _nv_get_input(editor, &ev);

            break;

        case TB_EVENT_RESIZE:
            if (editor->nv_conf.show_headless) break;

            editor->height = tb_height();
            editor->width = tb_width();
   
            _nv_redraw_all(editor);

            break;

        default: break;
        }
    }
}

static size_t _nv_get_line_length(struct nv_buff_line line) {
    int end_of_line = line.end - line.begin - 1;
    return end_of_line > 0 ? end_of_line : 0;
}

static struct nv_buff_line current_line(struct cursor* cursor, struct nv_buff* buffer) {
    return buffer->lines[cursor->line + 1];
}

static void move_vertical(struct nv_editor* editor, struct cursor* cursor, struct nv_buff* buffer, int direction) {
    if ((direction > 0 && cursor->y < tb_height() - 2) || (direction < 0 && cursor->y >= 1)) {
        cursor->y += direction;
        cursor->line += direction;
    } else if (cursor->line > 0 && cursor->line < (int)cvector_size(buffer->lines)) {
        buffer->_begin_line += direction;
        cursor->line += direction;
        _nv_draw_buffer(editor);
    }

    // the line we moved to
    struct nv_buff_line line = current_line(cursor, buffer);

    if (cursor->xmem >= line.length)
        cursor->x = line.length;
    else 
        cursor->x = cursor->xmem;
}

static void move_horizontal(struct cursor* cursor, struct nv_buff* buffer, int direction) {
    struct nv_buff_line curr_line = buffer->lines[cursor->line];

    if ((direction > 0 && cursor->x < (int)_nv_get_line_length(curr_line)) || (direction < 0 && cursor->x > 0)) {
        cursor->xmem += direction;
        cursor->x = cursor->xmem;
    }
}

static void _nv_insert_character(struct nv_buff* buffer, struct cursor* cursor, char ch) {
    struct nv_buff_line* line = &buffer->lines[cursor->line];
    size_t pos_index = line->begin + cursor->x;

    cursor->x++;

    cvector_insert(buffer->buffer, pos_index, ch);
    cvector_clear(buffer->lines);

    _nv_load_file_buffer(buffer, &buffer->_line_count);
}

static void _nv_get_input(struct nv_editor* editor, struct tb_event* ev) {
    if (editor->nv_conf.show_headless) 
        return;

    struct nv_buff* buffer = _nv_get_active_buffer(editor);
    if (!buffer) return;
    struct cursor* cursor = &buffer->cursors[0];
    if (!cursor) return;

    if (ev->type == TB_EVENT_MOUSE) {
        switch (ev->key) {
        case TB_KEY_MOUSE_WHEEL_UP:
            move_vertical(editor, cursor, buffer, -1);
            break;

        case TB_KEY_MOUSE_WHEEL_DOWN:
            move_vertical(editor, cursor, buffer, 1);
            break;
        }
    } else {
        switch (ev->ch) {
        case 'j': 
            move_vertical(editor, cursor, buffer, 1);
            break;

        case 'k': 
            move_vertical(editor, cursor, buffer, -1);
            break;

        case 'h':
            move_horizontal(cursor, buffer, -1);
            break;

        case 'l':
            move_horizontal(cursor, buffer, 1);
            break;
       
        default:
            _nv_insert_character(buffer, cursor, ev->ch);
            _nv_draw_buffer(editor);
            break;
        }
    }

    _nv_draw_cursor(editor);
}

void nv_push_buffer(struct nv_editor* editor, struct nv_buff buffer) {
    buffer.id = cvector_size(editor->buffers);
    cvector_push_back(editor->buffers, buffer);
}

static struct nv_buff*
_nv_get_active_buffer(struct nv_editor* editor) {
    struct nv_buff* buffer = (struct nv_buff*)&editor->buffers[editor->peek];
    editor->current = buffer;
    return buffer;
}

// calculate width of number
static int count_recur(int n) {
    if (n < 0) return count_recur((n == INT_MIN) ? INT_MAX : -n);
    if (n < 10) return 1;
    return 1 + count_recur(n / 10);
}

static void
_nv_draw_buffer(struct nv_editor* editor) {
    tb_clear_region(0, tb_height() - 1);

    struct nv_buff* buffer = _nv_get_active_buffer(editor);

    switch (buffer->type) {
    case NV_BUFFTYPE_PLAINTEXT:     
    case NV_BUFFTYPE_SOURCE:
        int top = buffer->cursors[0].line - buffer->cursors[0].y;

        if (!buffer->loaded) {
            _nv_load_file_buffer(buffer, &buffer->_line_count);
            buffer->_lines_col_size = count_recur(buffer->_line_count);
            buffer->loaded = true;
        }

        for (int row = 0; row < tb_height() - 1; row++) {
            size_t lineno, linesz;
            lineno = top + row;
            if ((int)lineno >= buffer->_line_count) return;

            struct nv_buff_line l = buffer->lines[lineno];
            linesz = l.end - l.begin;
         
            char* line = malloc(linesz + 1);
            memcpy(line, &buffer->buffer[l.begin], linesz);
            line[linesz] = '\0';

            tb_printf(0, row, TB_256_WHITE, TB_256_BLACK, "%*d %s", buffer->_lines_col_size, lineno + 1, line);
            free(line);
        }

        break;

    case NV_BUFFTYPE_BROWSER:
        tb_print(0, 0, TB_256_WHITE, TB_256_BLACK, "netrw");
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
    tb_printf(0, editor->height - 1, TB_256_BLACK, TB_256_WHITE, "%-*.*s", editor->width, editor->width, prompt);
    free(prompt);
}
