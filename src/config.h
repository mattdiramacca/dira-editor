/* config.c - Configuration system */
/* config.h - Configuration system */
#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int tab_width;
    int show_line_numbers;
    int auto_indent;
    int syntax_highlighting;
    char color_scheme[32];
    int show_status_bar;
    int show_welcome;
    int create_backup;
    int auto_save_interval;
} Config;

void config_default(Config *cfg);

const char* config_get_path(void);

#endif /* CONFIG_H */
