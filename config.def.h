/* If no other argument vector is specified via the UNIX socket, then a
 * newly created terminal window will launch this process. Note that
 * this is not the same type of vector as is used for the exec() system
 * calls -- especially, you can't tell a shell to be a login shell by
 * using "-sh" or "-bash" as argv[0]. */
char *args_default[] = { "/bin/bash", "-l", "-i", NULL };

/* Whether or not to enable the usage of bold fonts. See also c_bold,
 * which is always used for bold text with the default color, regardless
 * of this setting. */
gboolean enable_bold = TRUE;

/* Default font and font size. This string will be parsed by pango, see
 * the following URL:
 * https://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
 */
char *font_default = "DejaVu Sans Mono 9";

/* This is the number of pixels that will be used as padding between the
 * terminal widget and the window border. */
gint internal_border = 2;

/* Use 0 to disable scrolling completely or a negative value for
 * infinite scrolling. Keep the memory footprint in mind, though. */
guint scrollback_lines = 50000;

/* When double clicking, the terminal will select a "word". By default,
 * only alphanumeric characters will be considered as part of a word.
 * The following set of characters is meant to make selecting URLs a bit
 * easier. Can be set to NULL for the default behaviour. */
char *word_chars = "-+_=#?&%:/.,@";

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
