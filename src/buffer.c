#include "buffer.h"
#include <stdlib.h>
#include <string.h>


struct gapbuf g;

void gap_init(struct gapbuf *g, int initial_cap) {
    g->cap = initial_cap > 0 ? initial_cap : 1024;
    g->buf = malloc(g->cap);
    g->gap_start = 0;
    g->gap_end = g->cap;
}

void gap_free(struct gapbuf *g) { free(g->buf); }

int gap_length(struct gapbuf *g) { return g->cap - (g->gap_end - g->gap_start); }

void gap_move(struct gapbuf *g, int pos) {
    if (pos < 0) pos = 0;
    int len = gap_length(g);
    if (pos > len) pos = len;
    if (pos < g->gap_start) {
        int move_len = g->gap_start - pos;
        g->gap_end -= move_len;
        memmove(g->buf + g->gap_end, g->buf + pos, move_len);
        g->gap_start = pos;
    } else if (pos > g->gap_start) {
        int move_len = pos - g->gap_start;
        memmove(g->buf + g->gap_start, g->buf + g->gap_end, move_len);
        g->gap_start += move_len;
        g->gap_end += move_len;
    }
}

void gap_insert(struct gapbuf *g, char c) {
    if (g->gap_start == g->gap_end) {
        int gap_size = g->cap / 2;
        int newcap = g->cap + gap_size;
        char *nb = malloc(newcap);
        int prefix = g->gap_start;
        int suffix = g->cap - g->gap_end;
        if (prefix) memcpy(nb, g->buf, prefix);
        if (suffix) memcpy(nb + newcap - suffix, g->buf + g->gap_end, suffix);
        g->gap_end = newcap - suffix;
        g->cap = newcap;
        free(g->buf);
        g->buf = nb;
    }
    g->buf[g->gap_start++] = c;
}

int gap_backspace(struct gapbuf *g) {
    if (g->gap_start == 0) return 0;
    g->gap_start--;
    return 1;
}

int gap_delete(struct gapbuf *g) {
    if (g->gap_end == g->cap) return 0;
    g->gap_end++;
    return 1;
}

int gap_get(struct gapbuf *g, char *out, int outcap) {
    int len = gap_length(g);
    if (outcap < len) return -1;
    int prefix = g->gap_start;
    int suffix = g->cap - g->gap_end;
    if (prefix) memcpy(out, g->buf, prefix);
    if (suffix) memcpy(out + prefix, g->buf + g->gap_end, suffix);
    return len;
}

char gap_char_at(struct gapbuf *g, int pos) {
    if (pos < 0 || pos >= gap_length(g)) return '\0';
    if (pos < g->gap_start) return g->buf[pos];
    return g->buf[g->gap_end + (pos - g->gap_start)];
}


