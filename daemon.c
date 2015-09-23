#include <fcntl.h>
#include <gio/gunixsocketaddress.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vte/vte.h>

#include "config.h"


#define MSG_SIZE 4096

struct term_options
{
    char **argv;
    char *cwd;
    gboolean hold;
    char *message;
    char *title;
    char *wm_class;
    char *wm_name;
};


static void setup_css(void);
static gboolean setup_term(GtkWidget *, GtkWidget *, struct term_options *);
static void setup_window(GtkWidget *, struct term_options *);
static void sig_bell(VteTerminal *, gpointer);
static void sig_child_exited(VteTerminal *, gint, gpointer);
static void sig_decrease_font_size(VteTerminal *, gpointer);
static void sig_icon_title_changed(VteTerminal *, gpointer);
static void sig_increase_font_size(VteTerminal *, gpointer);
static gboolean sig_key_press(GtkWidget *, GdkEvent *, gpointer);
static gboolean sock_incoming(GSocketService *, GSocketConnection *, GObject *,
                              gpointer);
static void socket_listen(char *);
static gboolean term_new(gpointer);


void
setup_css(void)
{
    /* GTK3 and VTE are products of ~2015 and, as such, require a
     * significant amount of computing power. On "slower" machines (even
     * on fast ones), this can result in flickering: First, the window
     * and its background is drawn, then the VTE widget is drawn. If the
     * window has a bright background, it's likely to be visible for a
     * short period of time -- a short "flash". This effect can be seen
     * in other VTE terminals, too.
     *
     * By setting the background color of the window to something close
     * to the terminals background, we can reduce this effect. */
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gchar *css;

    css = g_strdup_printf("GtkWindow { background-color: %s; }", c_background);
    gtk_style_context_add_provider_for_screen(screen,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    g_object_unref(provider);
    g_free(css);
}

gboolean
setup_term(GtkWidget *win, GtkWidget *term, struct term_options *to)
{
    char **args_use;
    PangoFontDescription *font_desc = NULL;
    size_t i;
    GdkRGBA c_cursor_gdk;
    GdkRGBA c_foreground_gdk;
    GdkRGBA c_background_gdk;
    GdkRGBA c_bold_gdk;
    GdkRGBA c_palette_gdk[16];

    if (to->argv != NULL)
        args_use = to->argv;
    else
        args_use = args_default;

    /* Appearance. */
    font_desc = pango_font_description_from_string(font_default);
    vte_terminal_set_font(VTE_TERMINAL(term), font_desc);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(term), VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_allow_bold(VTE_TERMINAL(term), enable_bold);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(term), scrollback_lines);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(term), TRUE);
    vte_terminal_set_word_char_exceptions(VTE_TERMINAL(term), word_chars);

    gdk_rgba_parse(&c_cursor_gdk, c_cursor);
    gdk_rgba_parse(&c_foreground_gdk, c_foreground);
    gdk_rgba_parse(&c_background_gdk, c_background);
    gdk_rgba_parse(&c_bold_gdk, c_bold);
    for (i = 0; i < 16; i++)
        gdk_rgba_parse(&c_palette_gdk[i], c_palette[i]);
    vte_terminal_set_colors(VTE_TERMINAL(term), &c_foreground_gdk,
                            &c_background_gdk, c_palette_gdk, 16);
    vte_terminal_set_color_bold(VTE_TERMINAL(term), &c_bold_gdk);
    vte_terminal_set_color_cursor(VTE_TERMINAL(term), &c_cursor_gdk);

    /* Signals. */
    g_signal_connect(G_OBJECT(term), "bell",
                     G_CALLBACK(sig_bell), win);
    if (!to->hold)
        g_signal_connect(G_OBJECT(term), "child-exited",
                         G_CALLBACK(sig_child_exited), win);
    g_signal_connect(G_OBJECT(term), "decrease-font-size",
                     G_CALLBACK(sig_decrease_font_size), NULL);
    g_signal_connect(G_OBJECT(term), "icon-title-changed",
                     G_CALLBACK(sig_icon_title_changed), win);
    g_signal_connect(G_OBJECT(term), "increase-font-size",
                     G_CALLBACK(sig_increase_font_size), NULL);
    g_signal_connect(G_OBJECT(term), "key-press-event",
                     G_CALLBACK(sig_key_press), NULL);

    /* Spawn child. */
    return vte_terminal_spawn_sync(VTE_TERMINAL(term), VTE_PTY_DEFAULT, to->cwd,
                                   args_use, NULL, G_SPAWN_SEARCH_PATH, NULL,
                                   NULL, NULL, NULL, NULL);
}

void
setup_window(GtkWidget *win, struct term_options *to)
{
    gtk_window_set_title(GTK_WINDOW(win), to->title);
    gtk_window_set_wmclass(GTK_WINDOW(win), to->wm_name, to->wm_class);
}

void
sig_bell(VteTerminal *term, gpointer data)
{
    GtkWidget *win = (GtkWidget *)data;

    /* Credits go to sakura. The author says:
     * Remove the urgency hint. This is necessary to signal the window
     * manager that a new urgent event happened when the urgent hint is
     * set next time.
     */
    gtk_window_set_urgency_hint(GTK_WINDOW(win), FALSE);
    gtk_window_set_urgency_hint(GTK_WINDOW(win), TRUE);
}

void
sig_child_exited(VteTerminal *term, gint status, gpointer data)
{
    GtkWidget *win = (GtkWidget *)data;

    gtk_widget_destroy(win);
}

void
sig_decrease_font_size(VteTerminal *term, gpointer data)
{
    PangoFontDescription *f = pango_font_description_copy(
            vte_terminal_get_font(term));
    gint sz = pango_font_description_get_size(f);
    sz -= PANGO_SCALE;
    sz = sz <= 0 ? 1 : sz;
    pango_font_description_set_size(f, sz);
    vte_terminal_set_font(term, f);
    pango_font_description_free(f);
}

void
sig_icon_title_changed(VteTerminal *term, gpointer data)
{
    GtkWidget *win = (GtkWidget *)data;

    gtk_window_set_title(GTK_WINDOW(win), vte_terminal_get_icon_title(term));
}

void
sig_increase_font_size(VteTerminal *term, gpointer data)
{
    PangoFontDescription *f = pango_font_description_copy(
            vte_terminal_get_font(term));
    gint sz = pango_font_description_get_size(f);
    sz += PANGO_SCALE;
    pango_font_description_set_size(f, sz);
    vte_terminal_set_font(term, f);
    pango_font_description_free(f);
}

gboolean
sig_key_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    VteTerminal *term = VTE_TERMINAL(widget);

    if (((GdkEventKey *)event)->state & GDK_CONTROL_MASK)
    {
        switch (((GdkEventKey *)event)->keyval)
        {
            case GDK_KEY_C:
                vte_terminal_copy_clipboard(term);
                return TRUE;
            case GDK_KEY_V:
                vte_terminal_paste_clipboard(term);
                return TRUE;
        }
    }

    return FALSE;
}

gboolean
sock_incoming(GSocketService *service, GSocketConnection *connection,
              GObject *source_object, gpointer user_data)
{
    gchar *p;
    GInputStream* s;
    gssize sz_read;
    GSList *args = NULL;
    struct term_options *to = NULL;
    guint args_i;
    char option;
    char *value;

    to = calloc(sizeof(struct term_options), 1);
    to->cwd = NULL;
    to->hold = FALSE;
    to->message = calloc(1, MSG_SIZE);
    to->title = __NAME__;
    to->wm_class = __NAME_CAPITALIZED__;
    to->wm_name = __NAME__;
    
    s = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    sz_read = g_input_stream_read(s, to->message, MSG_SIZE, NULL, NULL);

    for (p = to->message; (p - to->message) < sz_read; p++)
    {
        switch (*p)
        {
            case 'A':
                /* After the 'A' follows a NUL terminated string. Add
                 * this string to our list of arguments. */
                p++;
                args = g_slist_append(args, p);
                /* Jump over the string. */
                while (*p != 0 && (p - to->message) < sz_read)
                    p++;
                break;
            case 'H':
                to->hold = TRUE;
                break;
            case 'O':
                /* FIXME: Inner "p++" statements like these don't check
                 * if they have exceeded the buffer. */
                p++;
                option = *p;
                p++;
                value = p;
                if (option == 'C' || option == 'c' || option == 'n' ||
                    option == 't')
                {
                    while (*p != 0 && (p - to->message) < sz_read)
                        p++;
                    if (*p != 0)
                        *p = 0;

                    if (option == 'C')
                        to->cwd = value;
                    if (option == 'c')
                        to->wm_class = value;
                    if (option == 'n')
                        to->wm_name = value;
                    if (option == 't')
                        to->title = value;
                }
                else
                {
                    fprintf(stderr, __NAME__": Garbled options, aborting.\n");
                    if (to->argv != NULL)
                        free(to->argv);
                    free(to->message);
                    free(to);
                    return TRUE;
                }
                break;
        }
    }

    if (args != NULL)
    {
        to->argv = calloc(g_slist_length(args) + 1, sizeof(char *));
        for (args_i = 0; args_i < g_slist_length(args); args_i++)
            to->argv[args_i] = (char *)(g_slist_nth(args, args_i)->data);
    }

    /* We're not on the main thread. */
    g_main_context_invoke(NULL, term_new, to);

    /* Other handlers must not be called. */
    return TRUE;
}

void
socket_listen(char *suffix)
{
    GSocketService *sock;
    GSocketAddress *sa;
    GError *err = NULL;
    char *name, *path;

    name = g_strdup_printf("%s-%s", __NAME__, suffix);
    path = g_build_filename(g_get_user_runtime_dir(), name, NULL);
    g_free(name);
    unlink(path);
    sa = g_unix_socket_address_new(path);
    g_free(path);

    sock = g_threaded_socket_service_new(-1);
    if (!g_socket_listener_add_address(G_SOCKET_LISTENER(sock), sa,
                                       G_SOCKET_TYPE_STREAM,
                                       G_SOCKET_PROTOCOL_DEFAULT,
                                       NULL, NULL, &err))
    {
        fprintf(stderr, "Failed to set up socket: '%s'\n", err->message);
        exit(EXIT_FAILURE);
    }

    g_signal_connect(G_OBJECT(sock), "run", G_CALLBACK(sock_incoming), NULL);
    g_socket_service_start(sock);
}

gboolean
term_new(gpointer user_data)
{
    GtkWidget *term, *win;
    struct term_options *to = (struct term_options *)user_data;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    setup_window(win, to);
    term = vte_terminal_new();
    gtk_widget_set_margin_start(term, internal_border);
    gtk_widget_set_margin_end(term, internal_border);
    gtk_widget_set_margin_top(term, internal_border);
    gtk_widget_set_margin_bottom(term, internal_border);
    gtk_container_add(GTK_CONTAINER(win), term);
    gtk_widget_show_all(win);
    setup_term(win, term, to);

    if (to->argv != NULL)
        free(to->argv);
    free(to->message);
    free(to);

    /* Remove this source. */
    return FALSE;
}

int
main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    setup_css();
    if (argc == 2)
        socket_listen(argv[1]);
    else
        socket_listen("main");
    gtk_main();
}
