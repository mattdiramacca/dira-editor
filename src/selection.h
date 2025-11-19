/* selection.h - Text selection system */
#ifndef SELECTION_H
#define SELECTION_H

// Forward declarations
struct gapbuf;
struct editHistory;

struct selection {
    int active;
    int start_row, start_col;
    int end_row, end_col;
};

struct clipboard {
    char *data;
    int len;
};

void selection_start(struct selection *sel, int row, int col);
void selection_update(struct selection *sel, int row, int col);
void selection_clear(struct selection *sel);
int selection_contains(struct selection *sel, int row, int col);

void clipboard_copy(struct clipboard *clip, struct selection *sel, struct gapbuf *g);
void clipboard_paste(struct clipboard *clip, struct gapbuf *g, int pos, struct editHistory *hist);
void clipboard_free(struct clipboard *clip);
void selection_delete(struct selection *sel, struct gapbuf *g, struct editHistory *hist);

int rowcol_to_pos(struct gapbuf *g, int row, int col);
void pos_to_rowcol(struct gapbuf *g, int pos, int *row, int *col);

#endif /* SELECTION_H */
