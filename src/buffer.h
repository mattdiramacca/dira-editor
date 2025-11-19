/* buffer.h - Gap buffer text storage */
#ifndef BUFFER_H
#define BUFFER_H

struct gapbuf {
    char *buf;
    int cap;
    int gap_start;
    int gap_end;
};

/* Initialize gap buffer */
void gap_init(struct gapbuf *g, int initial_cap);

/* Free gap buffer memory */
void gap_free(struct gapbuf *g);

/* Get length of text (excluding gap) */
int gap_length(struct gapbuf *g);

/* Move gap to position */
void gap_move(struct gapbuf *g, int pos);

/* Insert character at gap */
void gap_insert(struct gapbuf *g, char c);

/* Delete character before gap (backspace) */
int gap_backspace(struct gapbuf *g);

/* Delete character after gap (delete key) */
int gap_delete(struct gapbuf *g);

/* Get entire buffer contents */
int gap_get(struct gapbuf *g, char *out, int outcap);

/* Get character at specific position */
char gap_char_at(struct gapbuf *g, int pos);

#endif /* BUFFER_H */
