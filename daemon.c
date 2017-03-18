#include <fcntl.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vte/vte.h>
#include "xiate.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "config.h"


struct exit_options
{
    gboolean hold;
    GtkWidget *win;
};

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


static gboolean setup_term(GtkWidget *, GtkWidget *, struct term_options *);
static void sig_bell(VteTerminal *, gpointer);
static gboolean sig_button_press(GtkWidget *, GdkEvent *, gpointer);
static void sig_child_exited(VteTerminal *, gint, gpointer);
static void sig_decrease_font_size(VteTerminal *, gpointer);
static void sig_increase_font_size(VteTerminal *, gpointer);
static gboolean sig_key_press(GtkWidget *, GdkEvent *, gpointer);
static void sig_window_resize(VteTerminal *, guint, guint, gpointer);
static void sig_window_title_changed(VteTerminal *, gpointer);
static gboolean sock_incoming(GSocketService *, GSocketConnection *, GObject *,
                              gpointer);
static void socket_listen(char *);
static gboolean term_new(gpointer);
static void term_set_font(GtkWidget *, VteTerminal *, size_t);
static void term_set_font_scale(GtkWidget *, VteTerminal *, gdouble);
static void term_set_size(GtkWidget *, VteTerminal *, glong, glong);


gboolean
setup_term(GtkWidget *win, GtkWidget *term, struct term_options *to)
{
    static char *args_default[] = { NULL, NULL, NULL };
    char **args_use;
    size_t i;
    GdkRGBA c_foreground_gdk;
    GdkRGBA c_background_gdk;
    GdkRGBA c_palette_gdk[16];
    GdkRGBA c_gdk;
#   if VTE_CHECK_VERSION(0,44,0)
    VteRegex *url_vregex = NULL;
    GError *err = NULL;
#   endif
    GSpawnFlags spawn_flags;
    struct exit_options *eo = NULL;

    if (to->argv != NULL)
    {
        args_use = to->argv;
        spawn_flags = G_SPAWN_SEARCH_PATH;
    }
    else
    {
        if (args_default[0] == NULL)
        {
            args_default[0] = vte_get_user_shell();
            if (args_default[0] == NULL)
                args_default[0] = "/bin/sh";
            if (login_shell)
                args_default[1] = g_strdup_printf("-%s", args_default[0]);
            else
                args_default[1] = args_default[0];
        }
        args_use = args_default;
        spawn_flags = G_SPAWN_SEARCH_PATH | G_SPAWN_FILE_AND_ARGV_ZERO;
    }

    /* Appearance. */
    term_set_font(NULL, VTE_TERMINAL(term), 0);
    gtk_widget_show_all(win);

    vte_terminal_set_allow_bold(VTE_TERMINAL(term), enable_bold);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(term), VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_geometry_hints_for_window(VTE_TERMINAL(term),
                                               GTK_WINDOW(win));
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(term), TRUE);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(term), scrollback_lines);

    gdk_rgba_parse(&c_foreground_gdk, c_foreground);
    gdk_rgba_parse(&c_background_gdk, c_background);
    for (i = 0; i < 16; i++)
        gdk_rgba_parse(&c_palette_gdk[i], c_palette[i]);
    vte_terminal_set_colors(VTE_TERMINAL(term), &c_foreground_gdk,
                            &c_background_gdk, c_palette_gdk, 16);

    if (c_bold != NULL)
    {
        gdk_rgba_parse(&c_gdk, c_bold);
        vte_terminal_set_color_bold(VTE_TERMINAL(term), &c_gdk);
    }

    if (c_cursor != NULL)
    {
        gdk_rgba_parse(&c_gdk, c_cursor);
        vte_terminal_set_color_cursor(VTE_TERMINAL(term), &c_gdk);
    }

#   if VTE_CHECK_VERSION(0,44,0)
    if (c_cursor_foreground != NULL)
    {
        gdk_rgba_parse(&c_gdk, c_cursor_foreground);
        vte_terminal_set_color_cursor_foreground(VTE_TERMINAL(term), &c_gdk);
    }
#   endif

#   if VTE_CHECK_VERSION(0,44,0)
    url_vregex = vte_regex_new_for_match(url_regex, strlen(url_regex),
                                         PCRE2_MULTILINE | PCRE2_CASELESS, &err);
    if (url_vregex == NULL)
        fprintf(stderr, "url_regex: %s\n",
                err == NULL ? "<err is NULL>" : err->message);
    else
    {
        vte_terminal_match_add_regex(VTE_TERMINAL(term), url_vregex, 0);
        vte_regex_unref(url_vregex);
    }
#   endif

    /* Signals. */
    eo = calloc(1, sizeof (struct exit_options));
    if (!eo)
    {
        perror(__NAME__": calloc for 'eo'");
        fprintf(stderr, __NAME__": Did not connect signal handler for "
                        "'child-exited'\n");
    }
    else
    {
        eo->hold = to->hold;
        eo->win = win;
        g_signal_connect(G_OBJECT(term), "child-exited",
                         G_CALLBACK(sig_child_exited), eo);
    }
    g_signal_connect(G_OBJECT(term), "bell",
                     G_CALLBACK(sig_bell), win);
    g_signal_connect(G_OBJECT(term), "button-press-event",
                     G_CALLBACK(sig_button_press), NULL);
    g_signal_connect(G_OBJECT(term), "decrease-font-size",
                     G_CALLBACK(sig_decrease_font_size), win);
    g_signal_connect(G_OBJECT(term), "increase-font-size",
                     G_CALLBACK(sig_increase_font_size), win);
    g_signal_connect(G_OBJECT(term), "key-press-event",
                     G_CALLBACK(sig_key_press), win);
    g_signal_connect(G_OBJECT(term), "resize-window",
                     G_CALLBACK(sig_window_resize), win);
    g_signal_connect(G_OBJECT(term), "window-title-changed",
                     G_CALLBACK(sig_window_title_changed), win);

    /* Spawn child. */
    return vte_terminal_spawn_sync(VTE_TERMINAL(term), VTE_PTY_DEFAULT, to->cwd,
                                   args_use, NULL, spawn_flags,
                                   NULL, NULL, NULL, NULL, NULL);
}

void
sig_bell(VteTerminal *term, gpointer data)
{
    GtkWidget *win = (GtkWidget *)data;

    (void)term;

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

    (void)data;

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
    struct exit_options *eo = (struct exit_options *)data;
    GdkRGBA c_background_gdk;

    (void)status;

    if (eo->hold)
    {
        gdk_rgba_parse(&c_background_gdk, c_background);
        vte_terminal_set_color_cursor(term, &c_background_gdk);
        gtk_window_set_title(GTK_WINDOW(eo->win), __NAME__" - CHILD HAS QUIT");
    }
    else
        gtk_widget_destroy(eo->win);

    free(eo);
}

void
sig_decrease_font_size(VteTerminal *term, gpointer data)
{
    term_set_font_scale((GtkWidget *)data, term, 1.0 / 1.1);
}

void
sig_increase_font_size(VteTerminal *term, gpointer data)
{
    term_set_font_scale((GtkWidget *)data, term, 1.1);
}

gboolean
sig_key_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    VteTerminal *term = VTE_TERMINAL(widget);
    GtkWidget *win = (GtkWidget *)data;

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
            case GDK_KEY_plus:
            case GDK_KEY_equal:
                sig_increase_font_size(term, data);
                return TRUE;
            case GDK_KEY_minus:
                sig_decrease_font_size(term, data);
                return TRUE;
            case GDK_KEY_KP_0:
                vte_terminal_set_font_scale(term, 1);
                vte_terminal_set_geometry_hints_for_window(VTE_TERMINAL(term),
                                                           GTK_WINDOW(win));
                return TRUE;

            case GDK_KEY_KP_1: term_set_font(win, term, 0); return TRUE;
            case GDK_KEY_KP_2: term_set_font(win, term, 1); return TRUE;
            case GDK_KEY_KP_3: term_set_font(win, term, 2); return TRUE;
            case GDK_KEY_KP_4: term_set_font(win, term, 3); return TRUE;
            case GDK_KEY_KP_5: term_set_font(win, term, 4); return TRUE;
            case GDK_KEY_KP_6: term_set_font(win, term, 5); return TRUE;
            case GDK_KEY_KP_7: term_set_font(win, term, 6); return TRUE;
            case GDK_KEY_KP_8: term_set_font(win, term, 7); return TRUE;
            case GDK_KEY_KP_9: term_set_font(win, term, 8); return TRUE;
        }
    }

    return FALSE;
}

void
sig_window_resize(VteTerminal *term, guint width, guint height, gpointer data)
{
    term_set_size((GtkWidget *)data, term, width, height);
}

void
sig_window_title_changed(VteTerminal *term, gpointer data)
{
    GtkWidget *win = (GtkWidget *)data;

    gtk_window_set_title(GTK_WINDOW(win), vte_terminal_get_window_title(term));
}

gboolean
sock_incoming(GSocketService *service, GSocketConnection *connection,
              GObject *source_object, gpointer data)
{
    GInputStream* s;
    gssize i, sz_read;
    gsize msg_size = 4096;
    GSList *args = NULL;
    struct term_options *to = NULL;
    guint args_i;
    char option;
    char *value;

    (void)data;
    (void)service;
    (void)source_object;

    to = calloc(1, sizeof (struct term_options));
    if (!to)
    {
        perror(__NAME__": calloc for 'to'");
        goto garbled;
    }
    to->cwd = NULL;
    to->hold = FALSE;
    to->message = calloc(1, msg_size);
    if (!to->message)
    {
        perror(__NAME__": calloc for 'to->message'");
        goto garbled;
    }
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

    if (args)
    {
        to->argv = calloc(sizeof (char *), g_slist_length(args) + 1);
        if (!to->argv)
            perror(__NAME__": calloc for 'to->argv'");
        else
        {
            for (args_i = 0; args_i < g_slist_length(args); args_i++)
                to->argv[args_i] = (char *)(g_slist_nth(args, args_i)->data);
        }
        g_slist_free(args);
    }

    /* We're not on the main thread. */
    g_main_context_invoke(NULL, term_new, to);

    /* Other handlers must not be called. */
    return TRUE;

garbled:
    fprintf(stderr, __NAME__": Garbled message or memory error, aborting.\n");
    if (args)
        g_slist_free(args);
    if (to && to->message)
        free(to->message);
    if (to)
        free(to);
    return TRUE;
}

void
socket_listen(char *suffix)
{
    GSocketService *sock;
    GSocketAddress *sa;
    GError *err = NULL;

    sa = xiate_new_socket_address(suffix);

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
term_new(gpointer data)
{
    GtkWidget *term, *win;
    struct term_options *to = (struct term_options *)data;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), to->title);
    gtk_window_set_wmclass(GTK_WINDOW(win), to->wm_name, to->wm_class);

    term = vte_terminal_new();
    gtk_container_add(GTK_CONTAINER(win), term);
    if (!setup_term(win, term, to))
        gtk_widget_destroy(win);

    if (to->argv)
        free(to->argv);
    free(to->message);
    free(to);

    /* Remove this source. */
    return FALSE;
}

void
term_set_font(GtkWidget *win, VteTerminal *term, size_t index)
{
    PangoFontDescription *font_desc = NULL;
    glong width, height;

    if (index >= sizeof fonts / sizeof fonts[0])
    {
        fprintf(stderr, __NAME__": Warning: Invalid font index\n");
        return;
    }

    width = vte_terminal_get_column_count(term);
    height = vte_terminal_get_row_count(term);

    font_desc = pango_font_description_from_string(fonts[index]);
    vte_terminal_set_font(term, font_desc);
    pango_font_description_free(font_desc);
    vte_terminal_set_font_scale(term, 1);

    term_set_size(win, term, width, height);

    if (win != NULL)
        vte_terminal_set_geometry_hints_for_window(VTE_TERMINAL(term),
                                                   GTK_WINDOW(win));
}

void
term_set_font_scale(GtkWidget *win, VteTerminal *term, gdouble mult)
{
    gdouble s;
    glong width, height;

    width = vte_terminal_get_column_count(term);
    height = vte_terminal_get_row_count(term);

    s = vte_terminal_get_font_scale(term);
    s *= mult;
    vte_terminal_set_font_scale(term, s);
    term_set_size(win, term, width, height);
    vte_terminal_set_geometry_hints_for_window(VTE_TERMINAL(term),
                                               GTK_WINDOW(win));
}

void
term_set_size(GtkWidget *win, VteTerminal *term, glong width, glong height)
{
    GtkRequisition natural;

    /* This resizes the window to the exact size of the child widget.
     * This works even if the child uses padding or other cosmetic
     * attributes, and we don't need to know anything about it. */
    if (width > 0 && height > 0)
    {
        vte_terminal_set_size(term, width, height);

        /* win might be NULL when called from term_set_font(). */
        if (win != NULL)
        {
            gtk_widget_get_preferred_size(GTK_WIDGET(term), NULL, &natural);
            gtk_window_resize(GTK_WINDOW(win), natural.width, natural.height);
        }
    }
}

int
main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    if (argc == 2)
        socket_listen(argv[1]);
    else
        socket_listen("main");
    gtk_main();
}
