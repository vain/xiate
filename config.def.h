char     *args_default[]   = { "/bin/bash", "-l", "-i", NULL };
gboolean  enable_bold      = FALSE;
char     *font_default     = "DejaVu Sans Mono 9";
gint      internal_border  = 2;
guint     scrollback_lines = 50000;
char     *word_chars       = ":/.";

char *c_cursor = "#00FF00";
char *c_foreground = "#AAAAAA";
char *c_background = "#000000";
char *c_bold = "#FFFFFF";
char *c_palette[16] = {
    /* Dark */
    /* Black */   "#000000",
    /* Red */     "#AA0000",
    /* Green */   "#00AA00",
    /* Yellow */  "#AA5500",
    /* Blue */    "#0000AA",
    /* Magenta */ "#AA00AA",
    /* Cyan */    "#00AAAA",
    /* White */   "#AAAAAA",

    /* Bright */
    /* Black */   "#555555",
    /* Red */     "#FF5555",
    /* Green */   "#55FF55",
    /* Yellow */  "#FFFF55",
    /* Blue */    "#5555FF",
    /* Magenta */ "#FF55FF",
    /* Cyan */    "#55FFFF",
    /* White */   "#FFFFFF",
};
