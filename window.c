#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "cvector.h"
#include "editor.h"
#include "error.h"
#include "window.h"

struct nv_window_node* nv_window_node_init(enum nv_window_kind kind)
{
    struct nv_window_node* window = (struct nv_window_node*)malloc(sizeof(struct nv_window_node));

    if (!window) {
        NV_EDITOR_SET_STATUS(NV_ERR_MEM);
        return NULL;
    }

    static size_t id = 0;
    window->id = id++;
    window->kind = kind;
    window->leaf.view = NULL;

    cvector_push_back(nv_editor->windows, window);
    NV_EDITOR_SET_STATUS(NV_OK);
    return window;
}

nv_err nv_window_set_focus(struct nv_window_node* focus)
{
    if (!focus) {
        return NV_ERR_MEM;
    }

    if (focus->kind != NV_WM_VIEW) {
        // focus->kind MUST be NV_WM_VIEW because a split cannot be split further without making a choice
        return NV_ERR;
    }

    nv_editor->focus = focus;

    return NV_OK;
}

struct nv_window_node* nv_window_node_push_child(struct nv_window_node* root, struct nv_window_node* child)
{
    if (!child || !root) {
        NV_EDITOR_SET_STATUS(NV_ERR_NOT_INIT);
        return NULL;
    }

    assert(root->kind == NV_WM_VIEW);

    struct nv_view* view = root->leaf.view;

    if (!view) {
        // child can use this view
        (void)memcpy(root, child, sizeof(struct nv_window_node));
        return root;
    }

    root->leaf.view = NULL;

    root->kind = NV_WM_SPLIT;
    root->split.kind = NV_SPLIT_VERTICAL;
    root->split.right = child;
    root->split.ratio = 0.5f;

    root->split.left = nv_window_node_init(NV_WM_VIEW);
    root->split.left->leaf.view = view;

    return child;
}

void nv_free_windows()
{
    if (!nv_editor->windows) {
        NV_EDITOR_SET_STATUS(NV_ERR_NOT_INIT);
        return;
    }

    for (int i = 0; i < cvector_size(nv_editor->windows); i++) {
        if (nv_editor->windows[i]) {
            free(nv_editor->windows[i]);
        }
    }

    cvector_free(nv_editor->windows);
    nv_editor->windows = NULL;
}

void nv_free_views()
{
    if (!nv_editor->views) {
        NV_EDITOR_SET_STATUS(NV_ERR_NOT_INIT);
        return;
    }

    for (int i = 0; i < cvector_size(nv_editor->views); i++) {
        nv_free_view(nv_editor->views[i]);
    }

    cvector_free(nv_editor->views);
    nv_editor->views = NULL;
}

struct nv_context nv_get_context(struct nv_window_node* window)
{
    return (struct nv_context) {
        .window = window,
        .view = window ? window->leaf.view : NULL,
        .buffer = window && window->leaf.view ? window->leaf.view->buffer : NULL,
    };
}

struct nv_window_node* nv_get_focused_window()
{
    if (!nv_editor->focus) {
        return NULL;
    }

    return nv_editor->focus;
}