#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "editor.h"

unsigned char buffer[NV_ARENA_DEFAULT_ALLOC_SIZE]; // TODO: allocate this on the heap
struct nv_arena_alloc_head* root;

static void* nv_init_root_head(void* ptr, size_t size)
{
    char* base = (char*)ptr;
    char* addr = base;

    if (!root) {
        root = (struct nv_arena_alloc_head*)base;
        root->size = size;
        root->status = NV_ARENA_BLOCK_STATUS_RESERVED;
        root->next = NULL;

        return (char*)root + sizeof(struct nv_arena_alloc_head);
    }

    struct nv_arena_alloc_head* curr = root;

    addr = base;

    while (curr->next) {
        addr += sizeof(struct nv_arena_alloc_head) + curr->size;
        curr = curr->next;
    }

    addr += sizeof(struct nv_arena_alloc_head) + curr->size;

    if (addr + sizeof(struct nv_arena_alloc_head) + size > base + NV_ARENA_DEFAULT_ALLOC_SIZE) {
        NV_EDITOR_SET_STATUS(NV_ERR_MEM);
        return NULL;
    }

    struct nv_arena_alloc_head* new_head = (struct nv_arena_alloc_head*)addr;

    new_head->size = size;
    new_head->status = NV_ARENA_BLOCK_STATUS_RESERVED;
    new_head->next = NULL;

    curr->next = new_head;

    return (char*)new_head + sizeof(struct nv_arena_alloc_head);
}

void nv_arena_init()
{
    root = NULL;
}

void nv_arena_free_all()
{
    root = NULL;
}

void nv_arena_free(void* ptr)
{
    if (!ptr) {
        return;
    }

    struct nv_arena_alloc_head* head =
        (struct nv_arena_alloc_head*)((char*)ptr - sizeof(struct nv_arena_alloc_head));

    head->status = NV_ARENA_BLOCK_STATUS_FREE;
}

void* nv_arena_malloc(size_t size)
{
    return nv_init_root_head(buffer, size);
}

void* nv_arena_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;

    void* new = nv_arena_malloc(total);

    if (!new) {
        return NULL;
    }

    memset(new, 0, total);

    return new;
}

void* nv_arena_realloc(void* ptr, size_t size)
{
    if (!ptr) {
        return nv_arena_malloc(size);
    }

    struct nv_arena_alloc_head* head =
        (struct nv_arena_alloc_head*)((char*)ptr - sizeof(struct nv_arena_alloc_head));

    void* new = nv_arena_malloc(size);

    if (!new) {
        NV_EDITOR_SET_STATUS(NV_ERR_MEM);
        return NULL;
    }

    size_t copy_size = size < head->size ? size : head->size;

    memcpy(new, ptr, copy_size);

    return new;
}