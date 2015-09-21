char     *args_default[]   = { "/bin/bash", "-l", "-i", NULL };
gboolean  enable_bold      = FALSE;
char     *font_default     = "Ttyp0 10";
gint      internal_border  = 2;
guint     scrollback_lines = 50000;

char *c_cursor = "#00FF00";
char *c_foreground = "#AAAAAA";
char *c_background = "#000000";
char *c_bold = "#FFFFFF";
char *c_palette[16] = {
    /* Dark */
    /* Black */   "#000000",
    /* Red */     "#FF6565",
    /* Green */   "#93D44F",
    /* Yellow */  "#EAB93D",
    /* Blue */    "#204A87",
    /* Magenta */ "#CE5C00",
    /* Cyan */    "#89B6E2",
    /* White */   "#CCCCCC",

    /* Bright */
    /* Black */   "#555753",
    /* Red */     "#CF0000",
    /* Green */   "#00CF00",
    /* Yellow */  "#FFC123",
    /* Blue */    "#3465A4",
    /* Magenta */ "#F57900",
    /* Cyan */    "#46A4FF",
    /* White */   "#FFFFFF",
};
