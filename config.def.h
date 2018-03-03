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
char *fonts[] = {
    "qsqs,Ttyp0 10",
    "VGA 12",
    "Terminus 12",
};

/* Use 0 to disable scrolling completely or a negative value for
 * infinite scrolling. Keep the memory footprint in mind, though. */
guint scrollback_lines = 50000;

/* This regular expression is used to match URLs. You can easily spot
 * them by hovering over them with your mouse. Use your right mouse
 * button to invoke the link handler (defined below). */
char *link_regex = "[a-z]+://[[:graph:]]+";

/* Set this to the path of a tool to handle links. It will be invoked
 * with the following arguments:
 *
 *     argv[1] = "explicit" or "match"
 *     argv[2] = The link in question
 *
 * "explicit" will be used for explicit hyperlinks. They are explained
 * over here:
 *
 * https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
 *
 * "match" will be used for links that have been found using
 * "link_regex" as defined above.
 *
 * If you set this to NULL, no action will be taken. */
char *link_handler = NULL;

/* Set this to the path of a tool to handle history dumps.
 *
 * History dumps work like this: You press Ctrl+Shift+F to tell xiate to
 * write the entire history of the terminal to a temporary file. Once
 * that's done, your tool will be called with the path to that file as
 * first argument.
 *
 * What you do with this file is entirely up to you. It's also your job
 * to remove it once you're done with it. */
char *history_handler = NULL;

/* Background color of text under the cursor. There's a special mode:
 * "If NULL, text under the cursor will be drawn with foreground and
 * background colors reversed." */
char *c_cursor = "#00FF00";

/* Foreground color of text under the cursor. Just like the background
 * color, NULL reverses foreground and background. */
char *c_cursor_foreground = "#000000";

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
