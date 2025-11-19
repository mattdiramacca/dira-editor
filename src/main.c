/* main.c - DIRA editor entry point */
#define _POSIX_C_SOURCE 200809L

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include "buffer.h"
#include "history.h"
#include "selection.h"
#include "syntax.h"
#include "config.h"

#define ABUF_SIZE 32768
#define TAB_STOP 4

/* -------- key definitions -------- */
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/* -------- editor state -------- */
struct editorConfig {
    int cx, cy;
    int rowoff, coloff;
    int screenrows, screencols;
    struct termios orig_termios;
    char *filename;
    int dirty;
    char statusmsg[80];
    struct editHistory history;
    struct selection sel;
    struct clipboard clip;
    char *search_query;
    int search_direction;
    int search_match_pos;
    int show_welcome;
};

static struct editorConfig E;
static struct gapbuf g;

/* -------- append buffer -------- */
static char abuf[ABUF_SIZE];
static int abuf_len = 0;

void abufAppend(const char *s, int len) { 
    if (abuf_len + len < ABUF_SIZE) {
        memcpy(abuf + abuf_len, s, len); 
        abuf_len += len; 
    }
}

void abufFlush(void) { 
    write(STDOUT_FILENO, abuf, abuf_len); 
    abuf_len = 0; 
}

/* -------- raw mode -------- */
void disableRawMode(void) { 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios); 
}

void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) exit(1);
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0; 
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* -------- terminal size -------- */
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) return -1;
    *cols = ws.ws_col; 
    *rows = ws.ws_row; 
    return 0;
}

/* -------- position helpers -------- */
int get_line_length(int row) {
    int pos = rowcol_to_pos(&g, row, 0);
    int len = gap_length(&g);
    int line_len = 0;
    
    while (pos < len) {
        char c = gap_char_at(&g, pos);
        if (c == '\n') break;
        line_len++;
        pos++;
    }
    
    return line_len;
}

int get_line_indent(int row) {
    int pos = rowcol_to_pos(&g, row, 0);
    int len = gap_length(&g);
    int indent = 0;
    
    while (pos < len) {
        char c = gap_char_at(&g, pos);
        if (c == ' ') indent++;
        else if (c == '\t') indent += TAB_STOP;
        else break;
        pos++;
    }
    
    return indent;
}

int count_rows(void) {
    int len = gap_length(&g);
    int rows = 1;
    for (int i = 0; i < len; i++) {
        if (gap_char_at(&g, i) == '\n') rows++;
    }
    return rows;
}

/* -------- file I/O -------- */
void editorOpen(char *filename) {
    E.filename = strdup(filename);
    
    FILE *fp = fopen(filename, "r");
    if (!fp) return;
    
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        for (ssize_t i = 0; i < linelen; i++) {
            gap_insert(&g, line[i]);
        }
    }
    
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void editorSave(void) {
    if (E.filename == NULL) {
        snprintf(E.statusmsg, sizeof(E.statusmsg), "No filename!");
        return;
    }
    
    char tmp[65536];
    int len = gap_get(&g, tmp, sizeof(tmp));
    
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, tmp, len) == len) {
                close(fd);
                E.dirty = 0;
                snprintf(E.statusmsg, sizeof(E.statusmsg), "Saved! %d bytes", len);
                return;
            }
        }
        close(fd);
    }
    snprintf(E.statusmsg, sizeof(E.statusmsg), "Save failed!");
}

/* -------- status bar -------- */
void editorDrawStatusBar(void) {
    abufAppend("\x1b[7m", 4);
    
    char status[80];
    char rstatus[80];
    int len = snprintf(status, sizeof(status), " %.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]",
        count_rows(),
        E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d,%d ", E.cy + 1, E.cx + 1);
    
    if (len > E.screencols) len = E.screencols;
    abufAppend(status, len);
    
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abufAppend(rstatus, rlen);
            break;
        } else {
            abufAppend(" ", 1);
            len++;
        }
    }
    
    abufAppend("\x1b[m", 3);
    abufAppend("\r\n", 2);
    
    abufAppend("\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen) abufAppend(E.statusmsg, msglen);
}

/* -------- welcome screen -------- */
const char* welcome_lines[] = {
    "",
    "        ########  #### ########     ###    ",
    "        ##     ##  ##  ##     ##   ## ##   ",
    "        ##     ##  ##  ##     ##  ##   ##  ",
    "        ##     ##  ##  ########  ##     ## ",
    "        ##     ##  ##  ##   ##   ######### ",
    "        ##     ##  ##  ##    ##  ##     ## ",
    "        ########  #### ##     ## ##     ## ",
    "",
    "                DIRA version 1.0",
    "            Terminal Text Editor",
    "",
    "  +------------------------------------------------------------------+",
    "  |                      QUICK START GUIDE                           |",
    "  +------------------------------------------------------------------+",
    "  |                                                                  |",
    "  |  BASIC EDITING            SELECTION & CLIPBOARD                 |",
    "  |  ==============            =====================                 |",
    "  |  Arrow Keys ..... Move     Shift+Arrows ... Select text         |",
    "  |  Home/End ....... Line     Ctrl-A ......... Select all          |",
    "  |  Page Up/Down ... Scroll   Ctrl-C ......... Copy                |",
    "  |  Backspace/Del .. Remove   Ctrl-X ......... Cut                 |",
    "  |  Tab ............ Spaces   Ctrl-V ......... Paste               |",
    "  |  Enter .......... Newline  Escape ......... Clear selection     |",
    "  |                                                                  |",
    "  |  FILE OPERATIONS          EDITING COMMANDS                      |",
    "  |  ================          ================                      |",
    "  |  Ctrl-S ......... Save     Ctrl-Z ......... Undo                |",
    "  |  Ctrl-Q ......... Quit     Ctrl-Y ......... Redo                |",
    "  |  ./editor file .. Open     Ctrl-F ......... Find (soon!)        |",
    "  |                                                                  |",
    "  |  FEATURES                                                        |",
    "  |  ========                                                        |",
    "  |  * Syntax highlighting for C/C++                                |",
    "  |  * Line numbers with dynamic width                              |",
    "  |  * Auto-indentation                                             |",
    "  |  * Efficient gap buffer                                         |",
    "  |  * Memory-efficient undo/redo                                   |",
    "  |                                                                  |",
    "  +------------------------------------------------------------------+",
    "",
    "                Press any key to start editing...",
    "",
    NULL
};

void drawWelcomeScreen(void) {
    abuf_len = 0;
    abufAppend("\x1b[?25l", 6);
    abufAppend("\x1b[H", 3);
    abufAppend("\x1b[2J", 4);
    
    int welcome_lines_count = 0;
    while (welcome_lines[welcome_lines_count] != NULL) {
        welcome_lines_count++;
    }
    
    int padding = (E.screenrows - welcome_lines_count) / 2;
    if (padding < 0) padding = 0;
    
    for (int i = 0; i < padding && i < E.screenrows - 2; i++) {
        abufAppend("~\x1b[K\r\n", 6);
    }
    
    for (int i = 0; welcome_lines[i] != NULL && padding + i < E.screenrows - 2; i++) {
        const char *line = welcome_lines[i];
        int len = strlen(line);
        
        int left_padding = 0;
        if (len < E.screencols) {
            left_padding = (E.screencols - len) / 2;
        }
        
        for (int j = 0; j < left_padding; j++) {
            abufAppend(" ", 1);
        }
        
        if (strstr(line, "####") != NULL) {
            abufAppend("\x1b[1;36m", 7);
        } else if (strstr(line, "DIRA version") != NULL) {
            abufAppend("\x1b[1;33m", 7);
        } else if (strstr(line, "Terminal Text Editor") != NULL) {
            abufAppend("\x1b[90m", 5);
        } else if (strstr(line, "QUICK START GUIDE") != NULL) {
            abufAppend("\x1b[1;32m", 7);
        } else if (strstr(line, "+---") != NULL || strstr(line, "| ") != NULL) {
            abufAppend("\x1b[34m", 5);
        } else if (strstr(line, "BASIC EDITING") != NULL || 
                   strstr(line, "SELECTION") != NULL ||
                   strstr(line, "FILE OPERATIONS") != NULL || 
                   strstr(line, "EDITING COMMANDS") != NULL ||
                   strstr(line, "FEATURES") != NULL) {
            abufAppend("\x1b[1;37m", 7);
        } else if (strstr(line, "Press any key") != NULL) {
            abufAppend("\x1b[1;35m", 7);
        }
        
        int write_len = len;
        if (left_padding + len > E.screencols) {
            write_len = E.screencols - left_padding;
        }
        if (write_len > 0) {
            abufAppend(line, write_len);
        }
        
        abufAppend("\x1b[0m", 4);
        abufAppend("\x1b[K\r\n", 5);
    }
    
    int drawn_lines = padding + welcome_lines_count;
    while (drawn_lines < E.screenrows - 2) {
        abufAppend("~\x1b[K\r\n", 6);
        drawn_lines++;
    }
    
    abufAppend("\x1b[7m", 4);
    char status[] = " Welcome to DIRA - Press any key to start";
    int slen = strlen(status);
    if (slen > E.screencols) slen = E.screencols;
    abufAppend(status, slen);
    while (slen < E.screencols) {
        abufAppend(" ", 1);
        slen++;
    }
    abufAppend("\x1b[m\r\n", 5);
    
    abufAppend("\x1b[K", 3);
    abufAppend("\x1b[?25h", 6);
    abufFlush();
}

/* -------- screen refresh -------- */
void editorScroll(void) {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows - 2) {
        E.rowoff = E.cy - E.screenrows + 3;
    }
    
    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencols - 5) {
        E.coloff = E.cx - E.screencols + 6;
    }
}

void editorRefreshScreen(void) {
    if (E.show_welcome) {
        drawWelcomeScreen();
        return;
    }
    
    editorScroll();
    
    abuf_len = 0;
    abufAppend("\x1b[?25l", 6);
    abufAppend("\x1b[H", 3);

    char tmp[65536];
    int len = gap_get(&g, tmp, sizeof(tmp));
    
    int row = 0, col = 0;
    int screen_row = 0;
    
    int num_width = snprintf(NULL, 0, "%d", count_rows()) + 1;
    
    char linenum[16];
    int ln_len = snprintf(linenum, sizeof(linenum), "%*d ", num_width, row + 1);
    abufAppend("\x1b[36m", 5);
    abufAppend(linenum, ln_len);
    abufAppend("\x1b[0m", 4);
    
    enum editorHighlight prev_hl = HL_NORMAL;
    
    for (int i = 0; i < len && screen_row < E.screenrows - 2; i++) {
        if (row < E.rowoff) {
            if (tmp[i] == '\n') row++;
            continue;
        }
        
        if (tmp[i] == '\n') {
            abufAppend("\x1b[K\r\n", 5);
            row++;
            col = 0;
            screen_row++;
            
            if (screen_row < E.screenrows - 2) {
                ln_len = snprintf(linenum, sizeof(linenum), "%*d ", num_width, row + 1);
                abufAppend("\x1b[36m", 5);
                abufAppend(linenum, ln_len);
                abufAppend("\x1b[0m", 4);
            }
        } else {
            if (col >= E.coloff && col < E.coloff + E.screencols - num_width - 1) {
                if (selection_contains(&E.sel, row, col)) {
                    abufAppend("\x1b[7m", 4);
                    abufAppend(&tmp[i], 1);
                    abufAppend("\x1b[27m", 5);
                } else {
                    enum editorHighlight hl = get_highlight(tmp, len, i, E.filename);
                    if (hl != prev_hl) {
                        abufAppend(highlight_to_color(hl), 5);
                        prev_hl = hl;
                    }
                    abufAppend(&tmp[i], 1);
                }
            }
            col++;
        }
    }
    
    abufAppend("\x1b[0m", 4);
    
    while (screen_row < E.screenrows - 2) {
        abufAppend("~", 1);
        abufAppend("\x1b[K\r\n", 5);
        screen_row++;
    }
    
    editorDrawStatusBar();
    
    char buf[32];
    int l = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 
                     (E.cy - E.rowoff) + 1, 
                     (E.cx - E.coloff) + 1 + num_width + 1);
    abufAppend(buf, l);
    abufAppend("\x1b[?25h", 6);
    
    abufFlush();
}

/* -------- input -------- */
int editorReadKey(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) exit(1);
    }
    
    if (c == '\x1b') {
        char seq[3];
        
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
                else if (seq[2] == 'A') return ARROW_UP | 0x1000;
                else if (seq[2] == 'B') return ARROW_DOWN | 0x1000;
                else if (seq[2] == 'C') return ARROW_RIGHT | 0x1000;
                else if (seq[2] == 'D') return ARROW_LEFT | 0x1000;
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        
        return '\x1b';
    }
    
    return c;
}

int is_shift_arrow(int key) {
    return key & 0x1000;
}

int get_base_key(int key) {
    return key & 0xFFF;
}

/* -------- cursor movement -------- */
void editorMoveCursor(int key) {
    int total_rows = count_rows();
    
    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = get_line_length(E.cy);
            }
            break;
            
        case ARROW_RIGHT: {
            int line_len = get_line_length(E.cy);
            if (E.cx < line_len) {
                E.cx++;
            } else if (E.cy < total_rows - 1) {
                E.cy++;
                E.cx = 0;
            }
            break;
        }
        
        case ARROW_UP:
            if (E.cy > 0) {
                E.cy--;
                int line_len = get_line_length(E.cy);
                if (E.cx > line_len) E.cx = line_len;
            }
            break;
            
        case ARROW_DOWN:
            if (E.cy < total_rows - 1) {
                E.cy++;
                int line_len = get_line_length(E.cy);
                if (E.cx > line_len) E.cx = line_len;
            }
            break;
            
        case HOME_KEY:
            E.cx = 0;
            break;
            
        case END_KEY:
            E.cx = get_line_length(E.cy);
            break;
            
        case PAGE_UP:
            E.cy = E.rowoff;
            for (int i = 0; i < E.screenrows - 2; i++) {
                if (E.cy > 0) E.cy--;
            }
            break;
            
        case PAGE_DOWN:
            E.cy = E.rowoff + E.screenrows - 2;
            if (E.cy > total_rows - 1) E.cy = total_rows - 1;
            for (int i = 0; i < E.screenrows - 2; i++) {
                if (E.cy < total_rows - 1) E.cy++;
            }
            break;
    }
}

/* -------- editor operations -------- */
void editorInsertChar(char c) {
    int pos = rowcol_to_pos(&g, E.cy, E.cx);
    gap_move(&g, pos);
    gap_insert(&g, c);
    history_push(&E.history, EDIT_INSERT, pos, c);
    E.cx++;
    E.dirty = 1;
}

void editorInsertNewline(void) {
    int pos = rowcol_to_pos(&g, E.cy, E.cx);
    gap_move(&g, pos);
    gap_insert(&g, '\n');
    history_push(&E.history, EDIT_INSERT_NEWLINE, pos, '\n');
    
    int prev_indent = get_line_indent(E.cy);
    E.cy++;
    E.cx = 0;
    
    for (int i = 0; i < prev_indent; i++) {
        gap_insert(&g, ' ');
        history_push(&E.history, EDIT_INSERT, pos + 1 + i, ' ');
        E.cx++;
    }
    
    E.dirty = 1;
}

void editorDelChar(void) {
    if (E.cx > 0) {
        int pos = rowcol_to_pos(&g, E.cy, E.cx);
        gap_move(&g, pos);
        char ch = gap_char_at(&g, pos - 1);
        if (gap_backspace(&g)) {
            history_push(&E.history, EDIT_DELETE, pos - 1, ch);
            E.cx--;
            E.dirty = 1;
        }
    } else if (E.cy > 0) {
        int prev_line_len = get_line_length(E.cy - 1);
        int pos = rowcol_to_pos(&g, E.cy, 0);
        gap_move(&g, pos);
        if (gap_backspace(&g)) {
            history_push(&E.history, EDIT_DELETE_NEWLINE, pos - 1, '\n');
            E.cy--;
            E.cx = prev_line_len;
            E.dirty = 1;
        }
    }
}

void editorProcessKeypress(void) {
    int c = editorReadKey();
    
    if (E.show_welcome) {
        E.show_welcome = 0;
        E.statusmsg[0] = '\0';
        return;
    }
    
    int shift_pressed = is_shift_arrow(c);
    int base_key = get_base_key(c);
    
    switch (base_key) {
        case '\x11':
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
            
        case '\x13':
            editorSave();
            break;
            
        case '\x1a':
            if (history_undo(&E.history, &g)) {
                pos_to_rowcol(&g, g.gap_start, &E.cy, &E.cx);
                E.dirty = 1;
            }
            selection_clear(&E.sel);
            break;
            
        case '\x19':
            if (history_redo(&E.history, &g)) {
                pos_to_rowcol(&g, g.gap_start, &E.cy, &E.cx);
                E.dirty = 1;
            }
            selection_clear(&E.sel);
            break;
            
        case '\x03':
            if (E.sel.active) {
                clipboard_copy(&E.clip, &E.sel, &g);
                snprintf(E.statusmsg, sizeof(E.statusmsg), "Copied %d bytes", E.clip.len);
                selection_clear(&E.sel);
            }
            break;
            
        case '\x16':
            if (E.sel.active) {
                selection_delete(&E.sel, &g, &E.history);
            }
            clipboard_paste(&E.clip, &g, rowcol_to_pos(&g, E.cy, E.cx), &E.history);
            break;
            
        case '\x18':
            if (E.sel.active) {
                clipboard_copy(&E.clip, &E.sel, &g);
                selection_delete(&E.sel, &g, &E.history);
                snprintf(E.statusmsg, sizeof(E.statusmsg), "Cut %d bytes", E.clip.len);
            }
            break;
            
        case '\x01':
            selection_start(&E.sel, 0, 0);
            E.cy = count_rows() - 1;
            E.cx = get_line_length(E.cy);
            selection_update(&E.sel, E.cy, E.cx);
            snprintf(E.statusmsg, sizeof(E.statusmsg), "Selected all");
            break;
            
        case '\x06':
            E.statusmsg[0] = '\0';
            break;
            
        case '\r':
            if (E.sel.active) {
                selection_delete(&E.sel, &g, &E.history);
            }
            editorInsertNewline();
            break;
            
        case 127:
        case '\x08':
            if (E.sel.active) {
                selection_delete(&E.sel, &g, &E.history);
            } else {
                editorDelChar();
            }
            break;
            
        case DEL_KEY:
            if (E.sel.active) {
                selection_delete(&E.sel, &g, &E.history);
            } else {
                int pos = rowcol_to_pos(&g, E.cy, E.cx);
                gap_move(&g, pos);
                char ch = gap_char_at(&g, pos);
                if (gap_delete(&g)) {
                    history_push(&E.history, EDIT_DELETE, pos, ch);
                    E.dirty = 1;
                }
            }
            break;
            
        case '\t':
            if (E.sel.active) {
                selection_delete(&E.sel, &g, &E.history);
            }
            for (int i = 0; i < TAB_STOP; i++) {
                editorInsertChar(' ');
            }
            break;
            
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            if (shift_pressed) {
                if (!E.sel.active) {
                    selection_start(&E.sel, E.cy, E.cx);
                }
                editorMoveCursor(base_key);
                selection_update(&E.sel, E.cy, E.cx);
            } else {
                if (E.sel.active) {
                    selection_clear(&E.sel);
                }
                editorMoveCursor(base_key);
            }
            break;
            
        case HOME_KEY:
        case END_KEY:
            if (shift_pressed && !E.sel.active) {
                selection_start(&E.sel, E.cy, E.cx);
            }
            editorMoveCursor(base_key);
            if (shift_pressed) {
                selection_update(&E.sel, E.cy, E.cx);
            } else {
                selection_clear(&E.sel);
            }
            break;
            
        case PAGE_UP:
        case PAGE_DOWN:
            if (shift_pressed && !E.sel.active) {
                selection_start(&E.sel, E.cy, E.cx);
            }
            editorMoveCursor(base_key);
            if (shift_pressed) {
                selection_update(&E.sel, E.cy, E.cx);
            } else {
                selection_clear(&E.sel);
            }
            break;
            
        case '\x1b':
            selection_clear(&E.sel);
            E.statusmsg[0] = '\0';
            break;
            
        default:
            if (base_key >= 32 && base_key < 127) {
                if (E.sel.active) {
                    selection_delete(&E.sel, &g, &E.history);
                }
                editorInsertChar((char)base_key);
            }
            break;
    }
}

/* -------- main -------- */
int main(int argc, char *argv[]) {
    enableRawMode();
    E.cx = E.cy = 0;
    E.rowoff = E.coloff = 0;
    E.filename = NULL;
    E.dirty = 0;
    E.statusmsg[0] = '\0';
    E.search_query = NULL;
    E.search_direction = 1;
    E.search_match_pos = -1;
    E.show_welcome = 0;
    
    history_init(&E.history);
    selection_clear(&E.sel);
    E.clip.data = NULL;
    E.clip.len = 0;
    
    getWindowSize(&E.screenrows, &E.screencols);
    E.screenrows -= 2;
    
    gap_init(&g, 1024);
    
    if (argc >= 2) {
        editorOpen(argv[1]);
        snprintf(E.statusmsg, sizeof(E.statusmsg), 
                 "Ctrl-S=save | Ctrl-Q=quit | Shift+Arrows=select | Ctrl-A=all | Esc=clear");
    } else {
        E.show_welcome = 1;
    }
    
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    for (;;) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    history_free(&E.history);
    clipboard_free(&E.clip);
    gap_free(&g);
    return 0;
}
