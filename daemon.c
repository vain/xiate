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
    char *message;
    char *title;
    char *wm_class;
    char *wm_name;
};


static void setup_css(void);
static gboolean setup_term(GtkWidget *, GtkWidget *, struct term_options *);
static void setup_window(GtkWidget *, struct term_options *);
static void sig_child_exited(VteTerminal *, gint, gpointer);
static void sig_decrease_font_size(VteTerminal *, gpointer);
static void sig_icon_title_changed(VteTerminal *, gpointer);
static void sig_increase_font_size(VteTerminal *, gpointer);
static gboolean sock_incoming(GSocketService *, GSocketConnection *, GObject *,
                              gpointer);
static void socket_listen(char *);
static gboolean term_new(gpointer);


void
setup_css(void)
{
    /* Style provider for this screen. */
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    gtk_style_context_add_provider_for_screen(screen,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_css_provider_load_from_data(provider,
                                    "GtkWindow { background-color: " BG_COLOR "; }",
                                    -1, NULL);
    g_object_unref(provider);
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

    gdk_rgba_parse(&c_cursor_gdk, c_cursor);
    gdk_rgba_parse(&c_foreground_gdk, c_foreground);
    gdk_rgba_parse(&c_background_gdk, c_background);
    for (i = 0; i < 16; i++)
        gdk_rgba_parse(&c_palette_gdk[i], c_palette[i]);
    vte_terminal_set_colors(VTE_TERMINAL(term), &c_foreground_gdk, &c_background_gdk,
                            c_palette_gdk, 16);
    vte_terminal_set_color_cursor(VTE_TERMINAL(term), &c_cursor_gdk);
    g_signal_connect(G_OBJECT(term), "decrease-font-size",
                     G_CALLBACK(sig_decrease_font_size), NULL);
    g_signal_connect(G_OBJECT(term), "icon-title-changed",
                     G_CALLBACK(sig_icon_title_changed), win);
    g_signal_connect(G_OBJECT(term), "increase-font-size",
                     G_CALLBACK(sig_increase_font_size), NULL);

    /* Spawn child. */
    g_signal_connect(G_OBJECT(term), "child-exited",
                     G_CALLBACK(sig_child_exited), win);
    return vte_terminal_spawn_sync(VTE_TERMINAL(term), VTE_PTY_DEFAULT, NULL,
                                   args_use, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                   NULL, NULL, NULL);
}

void
setup_window(GtkWidget *win, struct term_options *to)
{
    gtk_window_set_title(GTK_WINDOW(win), to->title);
    gtk_window_set_wmclass(GTK_WINDOW(win), to->wm_name, to->wm_class);
    g_signal_connect(G_OBJECT(win), "delete-event",
                     G_CALLBACK(gtk_main_quit), NULL);
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
            case 'O':
                p++;
                option = *p;
                p++;
                value = p;
                if (option == 'c' || option == 'n' || option == 't')
                {
                    while (*p != 0 && (p - to->message) < sz_read)
                        p++;
                    if (*p != 0)
                        *p = 0;

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
            case 'A':
                /* After the 'A' follows a NUL terminated string. Add
                 * this string to our list of arguments. */
                p++;
                args = g_slist_append(args, p);
                /* Jump over the string. */
                while (*p != 0 && (p - to->message) < sz_read)
                    p++;
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
    setup_term(win, term, to);
    gtk_container_add(GTK_CONTAINER(win), term);
    gtk_widget_show_all(win);

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
    socket_listen("main");
    gtk_main();
}
