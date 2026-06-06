#include <ctype.h>

#include "draw.h"
#include "editor.h"
#include "window.h"
#include <tui.h>

void nv_set_mode(nv_mode mode)
{
    nv_editor->mode = mode;
    nv_calculate_statline();
}

// void nv_handle_mouse_input(struct tb_event* ev)
// {
//     struct nv_context focus = nv_get_context(nv_get_focused_window());

//     if (!ev || !focus.window) {
//         return;
//     }

//     struct cursor* cursor = nv_primary_cursor(&focus);

//     switch (ev->key) {
//     case TB_KEY_MOUSE_WHEEL_UP:
//         if (focus.view) {
//             if (focus.view->top_line_index > 1) {
//                 focus.view->top_line_index--;
//                 cursor->line--;
//             }
//         }

//         break;

//     case TB_KEY_MOUSE_WHEEL_DOWN:
//         if (focus.view && focus.buffer) {
//             focus.view->top_line_index++;
//             focus.view->top_line_index = focus.view->top_line_index > focus.buffer->line_count ? focus.buffer->line_count : focus.view->top_line_index;
//             cursor->line++;
//             cursor->line = cursor->line > focus.buffer->line_count ? focus.buffer->line_count : cursor->line;
//         }

//         break;
//     }
// }

void nv_handle_key_input(unsigned char ansi)
{
    struct nv_context focus = nv_get_context(nv_get_focused_window());

    if (!focus.window) {
        return;
    }

    struct cursor* cursor = nv_primary_cursor(&focus);

    if (!cursor) {
        return;
    }

    if (nv_editor->mode == NV_MODE_INSERT) {
        if (isprint(ansi)) {
            nv_cursor_insert_ch(&focus, cursor, ansi);
        }
        else if (ansi == TB_KEY_ESC) {
            nv_set_mode(NV_MODE_NAVIGATE);
        }
    }
    else {
        if (ansi == TB_KEY_ESC) {
            nv_editor->running = false;
        }
        else {
            switch (ansi) {
                case 'i':
                    nv_set_mode(NV_MODE_INSERT);
                    break;

                case '\\':
                    nv_editor->logger->leaf.view->visible = !nv_editor->logger->leaf.view->visible;
                    break;

                case 'j': nv_cursor_move_down(&focus, cursor, 1); break;
                case 'k': nv_cursor_move_up(&focus, cursor, 1); break;
                case 'h': nv_cursor_move_x(&focus, cursor, -1); break;
                case 'l': nv_cursor_move_x(&focus, cursor, 1); break;
            }
        }
    }
}
