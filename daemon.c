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
static gboolean sig_button_press(GtkWidget *, GdkEvent *, gpointer);
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
    GRegex *url_gregex = NULL;
    GError *err = NULL;

    if (to->argv != NULL)
        args_use = to->argv;
    else
        args_use = args_default;

    /* Appearance. */
    font_desc = pango_font_description_from_string(font_default);
    vte_terminal_set_allow_bold(VTE_TERMINAL(term), enable_bold);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(term), VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_font(VTE_TERMINAL(term), font_desc);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(term), TRUE);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(term), scrollback_lines);

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

    url_gregex = g_regex_new(url_regex, G_REGEX_CASELESS, 0, &err);
    if (url_gregex == NULL)
        fprintf(stderr, "url_regex: %s\n", err->message);
    else
    {
        vte_terminal_match_add_gregex(VTE_TERMINAL(term), url_gregex, 0);
        g_regex_unref(url_gregex);
    }

    /* Signals. */
    g_signal_connect(G_OBJECT(term), "bell",
                     G_CALLBACK(sig_bell), win);
    g_signal_connect(G_OBJECT(term), "button-press-event",
                     G_CALLBACK(sig_button_press), NULL);
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

gboolean
sig_button_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    GtkClipboard *clip = NULL;
    char *url = NULL;

    if (event->type == GDK_BUTTON_PRESS)
    {
        if (((GdkEventButton *)event)->button == 3)
        {
            clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            url = vte_terminal_match_check_event(VTE_TERMINAL(widget),
                                                 event, NULL);
            if (url != NULL)
            {
                if (clip != NULL)
                    gtk_clipboard_set_text(clip, url, -1);
                g_free(url);
            }
        }
    }

    return FALSE;
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
    GInputStream* s;
    gssize i, sz_read;
    gsize msg_size = 4096;
    GSList *args = NULL;
    struct term_options *to = NULL;
    guint args_i;
    char option;
    char *value;

    to = calloc(sizeof(struct term_options), 1);
    to->cwd = NULL;
    to->hold = FALSE;
    to->message = calloc(1, msg_size);
    to->title = __NAME__;
    to->wm_class = __NAME_CAPITALIZED__;
    to->wm_name = __NAME__;

    s = g_io_stream_get_input_stream(G_IO_STREAM(connection));
    sz_read = g_input_stream_read(s, to->message, msg_size, NULL, NULL);

    if (sz_read < 1)
        goto garbled;

    for (i = 0; i < sz_read; i++)
    {
        switch (to->message[i])
        {
            case 'A':
                /* After the 'A' follows a NUL terminated string. Add
                 * this string to our list of arguments. */
                i++;
                if (i >= sz_read)
                    goto garbled;
                args = g_slist_append(args, &to->message[i]);

                /* Jump to the NUL byte of the string, so that the next
                 * iteration of the outer for-loop will jump to the next
                 * byte after that NUL. */
                while (i < sz_read && to->message[i] != 0)
                    i++;
                if (i == sz_read)
                    goto garbled;

                break;

            case 'H':
                to->hold = TRUE;
                break;

            case 'O':
                /* Directly after the 'O' follows another byte that
                 * indicates which option to set. After that, a NUL
                 * terminated string follows. */
                i++;
                if (i >= sz_read)
                    goto garbled;
                option = to->message[i];

                i++;
                if (i >= sz_read)
                    goto garbled;
                value = &to->message[i];

                if (option == 'C' || option == 'c' || option == 'n' ||
                    option == 't')
                {
                    /* Jump to the NUL byte of the string, so that the
                     * next iteration of the outer for-loop will jump to
                     * the next byte after that NUL. */
                    while (i < sz_read && to->message[i] != 0)
                        i++;
                    if (i == sz_read)
                        goto garbled;

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
                    goto garbled;

                break;

            default:
                goto garbled;
        }
    }

    if (args != NULL)
    {
        to->argv = calloc(g_slist_length(args) + 1, sizeof(char *));
        for (args_i = 0; args_i < g_slist_length(args); args_i++)
            to->argv[args_i] = (char *)(g_slist_nth(args, args_i)->data);
    }
    g_slist_free(args);

    /* We're not on the main thread. */
    g_main_context_invoke(NULL, term_new, to);

    /* Other handlers must not be called. */
    return TRUE;

garbled:
    fprintf(stderr, __NAME__": Garbled message, aborting.\n");
    g_slist_free(args);
    free(to->message);
    free(to);
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
    if (!setup_term(win, term, to))
        gtk_widget_destroy(win);

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
