/* history.c - Undo/redo implementation */
#include "history.h"
#include "buffer.h"
#include <stdlib.h>

void history_init(struct editHistory *h) {
    h->undoStack = NULL;
    h->redoStack = NULL;
    h->grouping = 0;
}

static void history_free_stack(struct edit *stack) {
    while (stack) {
        struct edit *next = stack->next;
        free(stack);
        stack = next;
    }
}

void history_free(struct editHistory *h) {
    history_free_stack(h->undoStack);
    history_free_stack(h->redoStack);
}

void history_push(struct editHistory *h, enum editType type, int pos, char ch) {
    struct edit *e = malloc(sizeof(struct edit));
    e->type = type;
    e->pos = pos;
    e->ch = ch;
    e->next = h->undoStack;
    e->prev = NULL;
    if (h->undoStack) h->undoStack->prev = e;
    h->undoStack = e;
    
    history_free_stack(h->redoStack);
    h->redoStack = NULL;
}

int history_undo(struct editHistory *h, struct gapbuf *g) {  // ✅ Added parameter
    if (!h->undoStack) return 0;
    
    struct edit *e = h->undoStack;
    h->undoStack = e->next;
    if (h->undoStack) h->undoStack->prev = NULL;
    
    e->next = h->redoStack;
    e->prev = NULL;
    if (h->redoStack) h->redoStack->prev = e;
    h->redoStack = e;
    
    gap_move(g, e->pos);  // ✅ Now g is available
    switch (e->type) {
        case EDIT_INSERT:
        case EDIT_INSERT_NEWLINE:
            gap_delete(g);
            break;
        case EDIT_DELETE:
        case EDIT_DELETE_NEWLINE:
            gap_insert(g, e->ch);
            break;
    }
    
    return 1;
}

int history_redo(struct editHistory *h, struct gapbuf *g) {  // ✅ Added parameter
    if (!h->redoStack) return 0;
    
    struct edit *e = h->redoStack;
    h->redoStack = e->next;
    if (h->redoStack) h->redoStack->prev = NULL;
    
    e->next = h->undoStack;
    e->prev = NULL;
    if (h->undoStack) h->undoStack->prev = e;
    h->undoStack = e;
    
    gap_move(g, e->pos);  // ✅ Now g is available
    switch (e->type) {
        case EDIT_INSERT:
        case EDIT_INSERT_NEWLINE:
            gap_insert(g, e->ch);
            break;
        case EDIT_DELETE:
        case EDIT_DELETE_NEWLINE:
            gap_delete(g);
            break;
    }
    
    return 1;
}
