/* If no other argument vector is specified via the UNIX socket, then a
 * newly created terminal window will launch the user's shell. This
 * option specifies whether the shell will be a login shell or not. */
gboolean login_shell = TRUE;

/* Whether or not to enable the usage of bold fonts. See also c_bold,
 * which is always used for bold text with the default color, regardless
 * of this setting. */
gboolean enable_bold = TRUE;

/* Default fonts and font sizes. These strings will be parsed by pango,
 * see the following URL:
 * https://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
 *
 * You can define up to 9 fonts, at least one font must be defined. You
 * can switch between these fonts during runtime using Ctrl+Keypad$n.
 * The first font will be index 0 and can be activated using
 * Ctrl+Keypad1. Second one with Ctrl+Keypad2 and so on.
 */
#ifndef XIATE_FONTS
#define XIATE_FONTS "qsqs,Ttyp0 10", "VGA 12", "Terminus 12",
#endif
char *fonts[] = {
    XIATE_FONTS
};

/* Use 0 to disable scrolling completely or a negative value for
 * infinite scrolling. Keep the memory footprint in mind, though.
 */
#ifndef XIATE_SCROLLBACK_LINES
#define XIATE_SCROLLBACK_LINES 50000
#endif
guint scrollback_lines = XIATE_SCROLLBACK_LINES;

/* This regular expression is used to match URLs. You can easily spot
 * them by hovering over them with your mouse. Use your right mouse
 * button to copy the URL to your clipboard. */
char *url_regex = "[a-z]+://[[:graph:]]+";

/* Background color of text under the cursor. There's a special mode:
 * "If NULL, text under the cursor will be drawn with foreground and
 * background colors reversed." */
#ifndef XIATE_C_CURSOR
#define XIATE_C_CURSOR "#00FF00"
#endif
char *c_cursor = XIATE_C_CURSOR;

/* Foreground color of text under the cursor. Just like the background
 * color, NULL reverses foreground and background. */
#ifndef XIATE_C_CURSOR_FOREGROUND
#define XIATE_C_CURSOR_FOREGROUND "#000000"
#endif
char *c_cursor_foreground = XIATE_C_CURSOR_FOREGROUND;

/* Quoting from the VTE reference: "Sets the color used to draw bold
 * text in the default foreground color. If [...] NULL then the default
 * color is used." */
#ifndef XIATE_C_BOLD
#define XIATE_C_BOLD "#FFFFFF"
#endif
char *c_bold = XIATE_C_BOLD;

/* Set the terminal's color palette. Note that none of these values can
 * be NULL.
 */
#ifndef XIATE_C_FOREGROUND
#define XIATE_C_FOREGROUND "#AAAAAA"
#endif
#ifndef XIATE_C_BACKGROUND
#define XIATE_C_BACKGROUND "#000000"
#endif
#ifndef XIATE_C_PALETTE
#define XIATE_C_PALETTE      \
    /* Dark */               \
    /* Black */   "#000000", \
    /* Red */     "#AA0000", \
    /* Green */   "#00AA00", \
    /* Yellow */  "#AA5500", \
    /* Blue */    "#0000AA", \
    /* Magenta */ "#AA00AA", \
    /* Cyan */    "#00AAAA", \
    /* White */   "#AAAAAA", \
    /* Bright */             \
    /* Black */   "#555555", \
    /* Red */     "#FF5555", \
    /* Green */   "#55FF55", \
    /* Yellow */  "#FFFF55", \
    /* Blue */    "#5555FF", \
    /* Magenta */ "#FF55FF", \
    /* Cyan */    "#55FFFF", \
    /* White */   "#FFFFFF",
#endif
char *c_foreground = XIATE_C_FOREGROUND;
char *c_background = XIATE_C_BACKGROUND;
char *c_palette[16] = {
    XIATE_C_PALETTE
};
