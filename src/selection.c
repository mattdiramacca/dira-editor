/* selection.c - Selection implementation */
#include "selection.h"
#include "buffer.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>

void selection_start(struct selection *sel, int row, int col) {
    sel->active = 1;
    sel->start_row = sel->end_row = row;
    sel->start_col = sel->end_col = col;
}

void selection_update(struct selection *sel, int row, int col) {
    sel->end_row = row;
    sel->end_col = col;
}

void selection_clear(struct selection *sel) {
    sel->active = 0;
}

int selection_contains(struct selection *sel, int row, int col) {
    if (!sel->active) return 0;
    
    int sr = sel->start_row, sc = sel->start_col;
    int er = sel->end_row, ec = sel->end_col;
    
    // Normalize
    if (sr > er || (sr == er && sc > ec)) {
        int tmp = sr; sr = er; er = tmp;
        tmp = sc; sc = ec; ec = tmp;
    }
    
    if (row < sr || row > er) return 0;
    if (row == sr && row == er) return col >= sc && col < ec;
    if (row == sr) return col >= sc;
    if (row == er) return col < ec;
    return 1;
}

void pos_to_rowcol(struct gapbuf *g, int pos, int *row, int *col) {
    *row = 0;
    *col = 0;
    for (int i = 0; i < pos; i++) {
        char c = gap_char_at(g, i);
        if (c == '\n') {
            (*row)++;
            *col = 0;
        } else {
            (*col)++;
        }
    }
}

int rowcol_to_pos(struct gapbuf *g, int row, int col) {
    int pos = 0;
    int cur_row = 0;
    int cur_col = 0;
    int len = gap_length(g);
    
    while (pos < len && cur_row < row) {
        if (gap_char_at(g, pos) == '\n') {
            cur_row++;
            cur_col = 0;
        } else {
            cur_col++;
        }
        pos++;
    }
    
    while (pos < len && cur_col < col) {
        char c = gap_char_at(g, pos);
        if (c == '\n') break;
        cur_col++;
        pos++;
    }
    
    return pos;
}

void clipboard_copy(struct clipboard *clip, struct selection *sel, struct gapbuf *g) {
    if (!sel->active) return;
    
    int sr = sel->start_row, sc = sel->start_col;
    int er = sel->end_row, ec = sel->end_col;
    
    // Normalize
    if (sr > er || (sr == er && sc > ec)) {
        int tmp = sr; sr = er; er = tmp;
        tmp = sc; sc = ec; ec = tmp;
    }
    
    int start_pos = rowcol_to_pos(g, sr, sc);
    int end_pos = rowcol_to_pos(g, er, ec);
    int copy_len = end_pos - start_pos;
    
    if (copy_len <= 0) return;
    
    clipboard_free(clip);
    clip->data = malloc(copy_len + 1);
    clip->len = copy_len;
    
    for (int i = 0; i < copy_len; i++) {
        clip->data[i] = gap_char_at(g, start_pos + i);
    }
    clip->data[copy_len] = '\0';
}

void clipboard_paste(struct clipboard *clip, struct gapbuf *g, int pos, struct editHistory *hist) {
    if (!clip->data || clip->len == 0) return;
    
    gap_move(g, pos);
    
    for (int i = 0; i < clip->len; i++) {
        gap_insert(g, clip->data[i]);
        history_push(hist, EDIT_INSERT, pos + i, clip->data[i]);
    }
}

void clipboard_free(struct clipboard *clip) {
    if (clip->data) free(clip->data);
    clip->data = NULL;
    clip->len = 0;
}

void selection_delete(struct selection *sel, struct gapbuf *g, struct editHistory *hist) {
    if (!sel->active) return;
    
    int sr = sel->start_row, sc = sel->start_col;
    int er = sel->end_row, ec = sel->end_col;
    
    if (sr > er || (sr == er && sc > ec)) {
        int tmp = sr; sr = er; er = tmp;
        tmp = sc; sc = ec; ec = tmp;
    }
    
    int start_pos = rowcol_to_pos(g, sr, sc);
    int end_pos = rowcol_to_pos(g, er, ec);
    
    gap_move(g, start_pos);
    for (int i = start_pos; i < end_pos; i++) {
        char ch = gap_char_at(g, start_pos);
        gap_delete(g);
        history_push(hist, EDIT_DELETE, start_pos, ch);
    }
    
    selection_clear(sel);
}
