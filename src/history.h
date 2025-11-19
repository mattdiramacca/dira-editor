/* history.h - Undo/redo system */
#ifndef HISTORY_H
#define HISTORY_H

#include "buffer.h"

enum editType {
    EDIT_INSERT,
    EDIT_DELETE,
    EDIT_INSERT_NEWLINE,
    EDIT_DELETE_NEWLINE
};

struct edit {
    enum editType type;
    int pos;
    char ch;
    struct edit *next;
    struct edit *prev;
};

struct editHistory {
    struct edit *undoStack;
    struct edit *redoStack;
    int grouping;
};

/* Initialize history system */
void history_init(struct editHistory *h);

/* Free all history */
void history_free(struct editHistory *h);

/* Push new edit to undo stack */
void history_push(struct editHistory *h, enum editType type, int pos, char ch);

/* Undo last edit */
int history_undo(struct editHistory *h, struct gapbuf *g);

/* Redo last undone edit */
int history_redo(struct editHistory *h, struct gapbuf *g);

#endif /* HISTORY_H */
