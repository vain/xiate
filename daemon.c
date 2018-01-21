#include <fcntl.h>
#include <gio/gunixsocketaddress.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vte/vte.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "config.h"


struct Client
{
    char **argv;
    char *cwd;
    gboolean hold;
    char *message;
    char *title;
    char *wm_class;
    char *wm_name;
    GtkWidget *win;
    GIOStream *sock_stream;
    gboolean has_child_exit_status;
    gint child_exit_status;
};


static void cb_spawn_async(VteTerminal *, GPid, GError *, gpointer);
static void handle_history(VteTerminal *);
static void setup_term(GtkWidget *, struct Client *);
static void sig_bell(VteTerminal *, gpointer);
static gboolean sig_button_press(GtkWidget *, GdkEvent *, gpointer);
static void sig_child_exited(VteTerminal *, gint, gpointer);
static void sig_decrease_font_size(VteTerminal *, gpointer);
static void sig_increase_font_size(VteTerminal *, gpointer);
static gboolean sig_key_press(GtkWidget *, GdkEvent *, gpointer);
static void sig_window_destroy(GtkWidget *, gpointer);
static void sig_window_resize(VteTerminal *, guint, guint, gpointer);
static void sig_window_title_changed(VteTerminal *, gpointer);
static gboolean sock_incoming(GSocketService *, GSocketConnection *, GObject *,
                              gpointer);
static void socket_listen(char *);
static gboolean term_new(gpointer);
static void term_set_font(GtkWidget *, VteTerminal *, size_t);
static void term_set_font_scale(GtkWidget *, VteTerminal *, gdouble);
static void term_set_size(GtkWidget *, VteTerminal *, glong, glong);


void
cb_spawn_async(VteTerminal *term, GPid pid, GError *err, gpointer data)
{
    GtkWidget *win = (GtkWidget *)data;

    (void)term;

    if (pid == -1 && err != NULL)
    {
        fprintf(stderr, __NAME__": Spawning child failed: %s\n", err->message);
        gtk_widget_destroy(win);
    }
}

void
handle_history(VteTerminal *term)
{
    GFile *tmpfile = NULL;
    GFileIOStream *io_stream = NULL;
    GOutputStream *out_stream = NULL;
    GError *err = NULL;
    char *argv[] = { history_handler, NULL, NULL };

    if (history_handler == NULL)
        return;

    tmpfile = g_file_new_tmp(NULL, &io_stream, &err);
    if (tmpfile == NULL)
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n", err->message);
        goto free_and_out;
    }

    out_stream = g_io_stream_get_output_stream(G_IO_STREAM(io_stream));
    if (!vte_terminal_write_contents_sync(term, out_stream, VTE_WRITE_DEFAULT,
                                          NULL, &err))
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n", err->message);
        goto free_and_out;
    }

    if (!g_io_stream_close(G_IO_STREAM(io_stream), NULL, NULL))
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n", err->message);
        goto free_and_out;
    }

    argv[1] = g_file_get_path(tmpfile);
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, &err))
    {
        fprintf(stderr, __NAME__": Could not launch history handler: %s\n",
                err->message);
    }

free_and_out:
    if (argv[1] != NULL)
        g_free(argv[1]);
    if (io_stream != NULL)
        g_object_unref(io_stream);
    if (tmpfile != NULL)
        g_object_unref(tmpfile);
    if (err != NULL)
        g_error_free(err);
}

void
setup_term(GtkWidget *term, struct Client *c)
{
    static char *args_default[] = { NULL, NULL, NULL };
    char **args_use;
    size_t i;
    GdkRGBA c_foreground_gdk;
    GdkRGBA c_background_gdk;
    GdkRGBA c_palette_gdk[16];
    GdkRGBA c_gdk;
    VteRegex *url_vregex = NULL;
    GError *err = NULL;
    GSpawnFlags spawn_flags;

    if (c->argv != NULL)
    {
        args_use = c->argv;
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
    gtk_widget_show_all(c->win);

    vte_terminal_set_allow_bold(VTE_TERMINAL(term), enable_bold);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(term), VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_geometry_hints_for_window(VTE_TERMINAL(term),
                                               GTK_WINDOW(c->win));
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(term), TRUE);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(term), scrollback_lines);

    if (hyperlink_handler != NULL)
        vte_terminal_set_allow_hyperlink(VTE_TERMINAL(term), TRUE);

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

    if (c_cursor_foreground != NULL)
    {
        gdk_rgba_parse(&c_gdk, c_cursor_foreground);
        vte_terminal_set_color_cursor_foreground(VTE_TERMINAL(term), &c_gdk);
    }

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

    /* Signals. */
    g_signal_connect(G_OBJECT(term), "bell",
                     G_CALLBACK(sig_bell), c->win);
    g_signal_connect(G_OBJECT(term), "button-press-event",
                     G_CALLBACK(sig_button_press), NULL);
    g_signal_connect(G_OBJECT(term), "child-exited",
                     G_CALLBACK(sig_child_exited), c);
    g_signal_connect(G_OBJECT(term), "decrease-font-size",
                     G_CALLBACK(sig_decrease_font_size), c->win);
    g_signal_connect(G_OBJECT(term), "increase-font-size",
                     G_CALLBACK(sig_increase_font_size), c->win);
    g_signal_connect(G_OBJECT(term), "key-press-event",
                     G_CALLBACK(sig_key_press), c->win);
    g_signal_connect(G_OBJECT(term), "resize-window",
                     G_CALLBACK(sig_window_resize), c->win);
    g_signal_connect(G_OBJECT(term), "window-title-changed",
                     G_CALLBACK(sig_window_title_changed), c->win);

    /* Spawn child. */
    vte_terminal_spawn_async(VTE_TERMINAL(term), VTE_PTY_DEFAULT, c->cwd,
                             args_use, NULL, spawn_flags, NULL, NULL, NULL, 60,
                             NULL, cb_spawn_async, c->win);
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
    char *argv[] = { hyperlink_handler, NULL, NULL };
    GError *err = NULL;

    (void)data;

    if (event->type == GDK_BUTTON_PRESS)
    {
        if (((GdkEventButton *)event)->button == 3)
        {
            /* Explicit hyperlinks take precedence over potential URL
             * matches. */
            if (hyperlink_handler != NULL)
            {
                url = vte_terminal_hyperlink_check_event(VTE_TERMINAL(widget), event);
                if (url != NULL)
                {
                    argv[1] = url;
                    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL,
                                       NULL, NULL, &err))
                    {
                        fprintf(stderr, __NAME__": Could not spawn hyperlink "
                                "handler: %s\n", err->message);
                        g_error_free(err);
                    }
                    g_free(url);
                    return FALSE;
                }
            }

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
    struct Client *c = (struct Client *)data;
    GdkRGBA c_background_gdk;

    c->has_child_exit_status = TRUE;
    c->child_exit_status = status;

    if (c->hold)
    {
        gdk_rgba_parse(&c_background_gdk, c_background);
        vte_terminal_set_color_cursor(term, &c_background_gdk);
        gtk_window_set_title(GTK_WINDOW(c->win), __NAME__" - CHILD HAS QUIT");
    }
    else
        gtk_widget_destroy(c->win);
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
            case GDK_KEY_F:
                handle_history(term);
                return TRUE;
            case GDK_KEY_C:
                vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
                return TRUE;
            case GDK_KEY_V:
                vte_terminal_paste_clipboard(term);
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
sig_window_destroy(GtkWidget *widget, gpointer data)
{
    struct Client *c = (struct Client *)data;
    GOutputStream *out_stream;
    unsigned char exit_code_buffer[1];

    (void)widget;

    /* Figure out exit code of our child. We deal with the full status
     * code as returned by wait(2) here, but there's no point in sending
     * the full integer to the client, since we can't/won't try to fake
     * stuff like "the child had a segfault". Just use the least
     * significant 8 bits to get a "normal" exit code in the range of
     * [0, 255]. This also makes socket handling easier, since we only
     * ever send one byte. */
    if (c->has_child_exit_status)
    {
        if (WIFEXITED(c->child_exit_status))
            exit_code_buffer[0] = WEXITSTATUS(c->child_exit_status);
        else if (WIFSIGNALED(c->child_exit_status))
            exit_code_buffer[0] = WTERMSIG(c->child_exit_status);
        else
        {
            fprintf(stderr, __NAME__": Child exited but neither WIFEXITED nor "
                                    "WIFSIGNALED: %d\n", c->child_exit_status);
            exit_code_buffer[0] = 1;
        }
    }
    else
        /* If there is no child exit status, it means the user has
         * forcibly closed the terminal window. We interpret this as
         * "ABANDON MISSION!!1!", so we won't return an exit code of 0
         * in this case.
         *
         * This will also happen if we fail to start the child in the
         * first place. */
        exit_code_buffer[0] = 1;

    /* Send and then close this client's socket to signal the client
     * program that the window has been closed. */
    out_stream = g_io_stream_get_output_stream(c->sock_stream);
    g_output_stream_write(out_stream, exit_code_buffer, 1, NULL, NULL);
    g_io_stream_close(c->sock_stream, NULL, NULL);
    g_object_unref(c->sock_stream);

    free(c->argv);
    free(c->message);
    free(c);
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
    GInputStream *in_stream;
    gssize read_now;
    gsize msg_size = 4096, i, sz_read = 0;
    GSList *args = NULL;
    struct Client *c = NULL;
    guint args_i;
    char option;
    char *value;

    (void)data;
    (void)service;
    (void)source_object;

    c = calloc(1, sizeof (struct Client));
    if (c == NULL)
    {
        perror(__NAME__": calloc for 'c'");
        goto garbled;
    }
    c->message = calloc(1, msg_size);
    if (c->message == NULL)
    {
        perror(__NAME__": calloc for 'c->message'");
        goto garbled;
    }
    c->title = __NAME__;
    c->wm_class = __NAME_CAPITALIZED__;
    c->wm_name = __NAME__;

    in_stream = g_io_stream_get_input_stream(G_IO_STREAM(connection));

    /* We'll keep the socket open until the window has been closed. */
    c->sock_stream = G_IO_STREAM(connection);
    g_object_ref(c->sock_stream);

    /* The client is expected to send at least "S\000F\000", so we read
     * until we see a "\000F\000". In between, there can be any number
     * of options, each of which must be terminated by a NUL byte. */
    do
    {
        read_now = g_input_stream_read(in_stream,
                                       c->message + sz_read,
                                       msg_size - sz_read,
                                       NULL, NULL);
        if (read_now == 0 || read_now == -1)
            goto garbled;

        sz_read += read_now;
    } while (sz_read < 3 ||
             c->message[(sz_read - 1) - 2] != 0   ||
             c->message[(sz_read - 1) - 1] != 'F' ||
             c->message[(sz_read - 1)    ] != 0);

    for (i = 0; i < sz_read; i++)
    {
        switch (c->message[i])
        {
            case 'S':
            case 'F':
                /* This signals the start or end of the client's
                 * message. We just check here if the message format is
                 * okay. */
                i++;
                if (i >= sz_read || c->message[i] != 0)
                    goto garbled;
                break;

            case 'A':
                /* After the 'A' follows a NUL terminated string. Add
                 * this string to our list of arguments. */
                i++;
                if (i >= sz_read)
                    goto garbled;
                args = g_slist_append(args, &c->message[i]);

                /* Jump to the NUL byte of the string, so that the next
                 * iteration of the outer for-loop will jump to the next
                 * byte after that NUL. */
                while (i < sz_read && c->message[i] != 0)
                    i++;
                if (i == sz_read)
                    goto garbled;

                break;

            case 'H':
                c->hold = TRUE;
                i++;
                if (i >= sz_read || c->message[i] != 0)
                    goto garbled;
                break;

            case 'O':
                /* Directly after the 'O' follows another byte that
                 * indicates which option to set. After that, a NUL
                 * terminated string follows. */
                i++;
                if (i >= sz_read)
                    goto garbled;
                option = c->message[i];

                i++;
                if (i >= sz_read)
                    goto garbled;
                value = &c->message[i];

                if (option == 'C' || option == 'c' || option == 'n' ||
                    option == 't')
                {
                    /* Jump to the NUL byte of the string, so that the
                     * next iteration of the outer for-loop will jump to
                     * the next byte after that NUL. */
                    while (i < sz_read && c->message[i] != 0)
                        i++;
                    if (i == sz_read)
                        goto garbled;

                    if (option == 'C')
                        c->cwd = value;
                    if (option == 'c')
                        c->wm_class = value;
                    if (option == 'n')
                        c->wm_name = value;
                    if (option == 't')
                        c->title = value;
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
        c->argv = calloc(sizeof (char *), g_slist_length(args) + 1);
        if (c->argv == NULL)
            perror(__NAME__": calloc for 'c->argv'");
        else
        {
            for (args_i = 0; args_i < g_slist_length(args); args_i++)
                c->argv[args_i] = (char *)(g_slist_nth(args, args_i)->data);
        }
        g_slist_free(args);
    }

    /* We're not on the main thread. */
    g_main_context_invoke(NULL, term_new, c);

    /* Other handlers must not be called. */
    return TRUE;

garbled:
    fprintf(stderr, __NAME__": Garbled message or memory error, aborting.\n");
    if (c != NULL && c->sock_stream != NULL)
        g_object_unref(c->sock_stream);
    if (c != NULL)
        free(c->message);
    free(c);
    if (args != NULL)
        g_slist_free(args);
    return TRUE;
}

void
socket_listen(char *suffix)
{
    GSocketService *sock;
    GSocketAddress *sa;
    GError *err = NULL;
    char *path;

    path = g_strdup_printf("/tmp/%s-%d", __NAME__, getuid());
    mkdir(path, S_IRWXU);
    g_free(path);

    path = g_strdup_printf("/tmp/%s-%d/%s", __NAME__, getuid(), suffix);
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
term_new(gpointer data)
{
    struct Client *c = (struct Client *)data;
    GtkWidget *term;

    c->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(c->win), c->title);
    gtk_window_set_wmclass(GTK_WINDOW(c->win), c->wm_name, c->wm_class);
    g_signal_connect(G_OBJECT(c->win), "destroy", G_CALLBACK(sig_window_destroy), c);

    term = vte_terminal_new();
    gtk_container_add(GTK_CONTAINER(c->win), term);
    setup_term(term, c);

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
    if (argc < 1 || argc > 2)
    {
        fprintf(stderr, __NAME__": Invalid arguments, check manpage\n");
        return 1;
    }

    gtk_init(&argc, &argv);
    if (argc == 2)
        socket_listen(argv[1]);
    else
        socket_listen("main");
    gtk_main();
}
