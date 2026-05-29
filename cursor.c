#include "cursor.h"
#include "buffer.h"
#include "context.h"
#include "cvector.h"
#include "nvtree.h"
#include "window.h"
#include "editor.h"

void nv_cursor_insert_ch(struct nv_context* ctx, struct cursor* cursor, char ch)
{
    cvector_push_back(ctx->buffer->add_buffer, ch);
    // cvector reallocation might have changed pointer
    nv_buffers[ctx->buffer->buff_id + NV_BUFF_ID_ADD] = ctx->buffer->add_buffer;

    nv_log_unimplemented();
    // ctx->buffer->tree = nv_tree_insert(ctx->buffer->tree, 1, (struct nv_node) {
    //     .buff_id = ctx->buffer->buff_id + NV_BUFF_ID_ADD,
    //     .buff_index = cvector_size(ctx->buffer->add_buffer) - 1,
    //     .length = 1,
    //     .lfcount = 0
    // });

    // ctx->buffer->tree = nv_tree_paint(ctx->buffer->tree, B);
    // cursor->col++;
}

void nv_cursor_move_down(struct nv_context* ctx, struct cursor* cursor, int amt)
{
    if (amt < 0) {
        nv_cursor_move_up(ctx, cursor, -amt);
        return;
    }

    cursor->line += amt;

    if (cursor->line > ctx->buffer->line_count) {
        cursor->line = ctx->buffer->line_count;
    }

    if (cursor->line >= ctx->view->top_line_index + ctx->window->leaf.area.h) {
        ctx->view->top_line_index++;
    }

    if (ctx->view->top_line_index > ctx->buffer->line_count) {
        ctx->view->top_line_index = ctx->buffer->line_count;
    }
}

void nv_cursor_move_up(struct nv_context* ctx, struct cursor* cursor, int amt)
{
    if (amt < 0) {
        nv_cursor_move_down(ctx, cursor, -amt);
        return;
    }

    cursor->line -= amt;

    if (cursor->line < 1) {
        cursor->line = 1;
    }

    if (cursor->line < ctx->view->top_line_index) {
        ctx->view->top_line_index = cursor->line;
    }
}

void nv_cursor_move_x(struct nv_context* ctx, struct cursor* cursor, int amt)
{
    cvector(nv_render_line) l = nv_get_computed_line(ctx, cursor->line);

    if (!l) {
        return;
    }

    size_t length = 0;
    for (int i = 0; i < cvector_size(l); i++) {
        length += l[i].size;
    }

    // cursor could be out of range before any change to the cursor
    if (cursor->col > length)
        cursor->col = length;

    cursor->col += amt;

    if (cursor->col > length)
        cursor->col = length;

    if (cursor->col < 0)
        cursor->col = 0;
}

struct cursor* nv_primary_cursor(struct nv_context* ctx)
{
    if (!ctx || !ctx->view) {
        return NULL;
    }

    return &ctx->view->cursors[NV_PRIMARY_CURSOR];
}
