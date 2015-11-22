/* If no other argument vector is specified via the UNIX socket, then a
 * newly created terminal window will launch the user's shell. This
 * option specifies whether the shell will be a login shell or not. */
gboolean login_shell = TRUE;

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
gint internal_border = 1;

/* Use 0 to disable scrolling completely or a negative value for
 * infinite scrolling. Keep the memory footprint in mind, though. */
guint scrollback_lines = 50000;

/* This regular expression is used to match URLs. You can easily spot
 * them by hovering over them with your mouse. Use your right mouse
 * button to copy the URL to your clipboard. */
char *url_regex = "[a-z]+://[[:graph:]]+";

/* Background color of text under the cursor. Note that you can't change
 * the foreground color; it'll stay at whatever color is requested by
 * escape codes. You can, however, set this option to NULL, to use a
 * different mode: "If NULL, text under the cursor will be drawn with
 * foreground and background colors reversed." */
char *c_cursor = "#00FF00";

/* Quoting from the VTE reference: "Sets the color used to draw bold
 * text in the default foreground color. If [...] NULL then the default
 * color is used." */
char *c_bold = "#FFFFFF";

/* Set the terminal's color palette. Note that none of these values can
 * be NULL. */
char *c_foreground = "#AAAAAA";
char *c_background = "#000000";
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
