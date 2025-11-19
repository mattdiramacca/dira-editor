/* syntax.c - Syntax highlighting implementation */
#include "syntax.h"
#include <string.h>
#include <ctype.h>

int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

enum editorHighlight get_highlight(const char *content, int len, int pos, const char *filename) {
    if (pos >= len) return HL_NORMAL;
    
    // Determine file type
    int is_c = 0;
    if (filename) {
        char *ext = strrchr(filename, '.');
        if (ext && (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 || 
                    strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0)) {
            is_c = 1;
        }
    }
    
    if (!is_c) return HL_NORMAL;
    
    char c = content[pos];
    
    // Check for comment
    if (pos + 1 < len && content[pos] == '/' && content[pos + 1] == '/') {
        return HL_COMMENT;
    }
    
    // Numbers
    if (isdigit(c)) {
        if (pos == 0 || is_separator(content[pos - 1])) {
            return HL_NUMBER;
        }
    }
    
    // Strings
    static int in_string = 0;
    if (c == '"') in_string = !in_string;
    if (in_string) return HL_STRING;
    
    // Keywords
    static const char *keywords[] = {
        "if", "else", "while", "for", "return", "int", "char", "void",
        "struct", "enum", "static", "const", "break", "continue", "switch",
        "case", "default", "sizeof", "typedef", NULL
    };
    
    for (int i = 0; keywords[i]; i++) {
        int klen = strlen(keywords[i]);
        if (pos + klen <= len && memcmp(content + pos, keywords[i], klen) == 0) {
            if (pos + klen >= len || is_separator(content[pos + klen])) {
                if (pos == 0 || is_separator(content[pos - 1])) {
                    return HL_KEYWORD;
                }
            }
        }
    }
    
    return HL_NORMAL;
}

const char* highlight_to_color(enum editorHighlight hl) {
    switch (hl) {
        case HL_KEYWORD: return "\x1b[33m";  // Yellow
        case HL_STRING:  return "\x1b[32m";  // Green
        case HL_COMMENT: return "\x1b[36m";  // Cyan
        case HL_NUMBER:  return "\x1b[31m";  // Red
        default:         return "\x1b[37m";  // White
    }
}
