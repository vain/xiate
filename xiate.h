#ifndef __XIATE_H__
#define __XIATE_H__

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

struct term_options
{
    char **argv;
    char *cwd;
    gboolean hold;
    char *message;
    char *title;
    char *wm_class;
    char *wm_name;

    /* Background color of text under the cursor. There's a special mode:
     * "If NULL, text under the cursor will be drawn with foreground and
     * background colors reversed." */
    gchar *cursorColor;

    /* Foreground color of text under the cursor. Just like the background
     * color, NULL reverses foreground and background. */
    gchar *cursorColor2;

    /* Quoting from the VTE reference: "Sets the color used to draw bold
     * text in the default foreground color. If [...] NULL then the default
     * color is used." */
    gchar *colorBD;

    gchar *foreground;
    gchar *background;
    gchar *palette[16];

    gboolean loginShell;

    gint64 saveLines;
};

GSocketAddress* xiate_socket_address(const char* suffix);
GSocketAddress* xiate_new_socket_address(const char* suffix);

void xiate_get_resources(GtkWidget *term, struct term_options *options);

const gchar* xiate_get_font(int index);

#endif
