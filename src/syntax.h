/* syntax.h - Syntax highlighting */
#ifndef SYNTAX_H
#define SYNTAX_H

enum editorHighlight {
    HL_NORMAL = 0,
    HL_KEYWORD,
    HL_STRING,
    HL_COMMENT,
    HL_NUMBER
};

/* Get highlight type for character at position */
enum editorHighlight get_highlight(const char *content, int len, int pos, const char *filename);

/* Get ANSI color code for highlight type */
const char* highlight_to_color(enum editorHighlight hl);

/* Check if character is a separator */
int is_separator(int c);

#endif /* SYNTAX_H */
