/* config.c - Configuration system */
#include "config.h"
#include <string.h>

void config_default(Config *cfg) {
    cfg->tab_width = 4;
    cfg->show_line_numbers = 1;
    cfg->auto_indent = 1;
    cfg->syntax_highlighting = 1;
}
