#include "xiate.h"
#include <gio/gunixsocketaddress.h>
#include <X11/Xresource.h>
#include <string.h>
#include <gdk/gdkx.h>

GSocketAddress* xiate_socket_address_impl(const char* suffix, int rm)
{
    char *name = g_strdup_printf("%s-%s", __NAME__, suffix ? suffix : "main");
    char *path = g_build_filename(g_get_user_runtime_dir(), name, NULL);
    GSocketAddress* address = g_unix_socket_address_new(path);

    if(rm) unlink(path);

    g_free(name);
    g_free(path);

    return address;
}

GSocketAddress* xiate_socket_address(const char* suffix) {
    return xiate_socket_address_impl(suffix, 0);
}

GSocketAddress* xiate_new_socket_address(const char* suffix) {
    return xiate_socket_address_impl(suffix, 1);
}

static XPointer get_prop(
    XrmDatabase *db, struct term_options *options, const char* name,
    char **type)
{
    gchar *resName = g_strconcat(options->wm_name, ".", name, NULL);
    gchar *resClass = g_strconcat(options->wm_class, ".", name, NULL);
    XrmValue value;

    XrmGetResource(*db, resName, resClass, type, &value);

    g_free(resName);
    g_free(resClass);

    return value.addr;
}

static void get_string(
    XrmDatabase *db, struct term_options *options, const char* name,
    gchar **dest, char *def)
{
    char *type;
    XPointer addr = get_prop(db, options, name, &type);
    *dest =
            addr ?
            g_strdup(addr) : def ?
            g_strdup(def) : def;
}

static gint64 get_int(
    XrmDatabase *db, struct term_options *options, const char* name,
    gint64 def)
{
    char *type;
    XPointer addr = get_prop(db, options, name, &type);
    return
            addr ?
            g_ascii_strtoll(addr, NULL, 10) :
            def;
}

static gboolean get_boolean(
    XrmDatabase *db, struct term_options *options, const char* name,
    gboolean def)
{
    char *type;
    XPointer addr = get_prop(db, options, name, &type);
    return
            addr ?
            strcasecmp(addr, "TRUE") == 0
            || strcasecmp(addr, "YES") == 0
            || strcasecmp(addr, "ON") == 0
            || strcasecmp(addr, "1") == 0 :
            def;
}

static gchar **xiate_fonts = NULL;

const gchar* xiate_get_font(int index)
{
    const gchar *result = *xiate_fonts;;
    while(*result && (result - *xiate_fonts) < index)
        ++result;
    return result;
}

void xiate_get_resources(
    GtkWidget *term,
    struct term_options *options)
{
    GdkDisplay *gdkDisplay = gtk_widget_get_display(term);
    Display *display = gdk_x11_display_get_xdisplay(gdkDisplay);
    char *resm = XResourceManagerString(display);

    if(!resm) return;

    XrmDatabase db = XrmGetStringDatabase(resm);

    options->loginShell = get_boolean(&db, options, "loginShell", FALSE);
    options->saveLines = get_int(&db, options, "saveLines", 64);

    /* The terminal colors */
    get_string(&db, options, "foreground", &options->foreground, "#AAAAAA");
    get_string(&db, options, "background", &options->background, "#000000");
    get_string(&db, options, "cursorColor", &options->cursorColor, "#00FF00");
    get_string(&db, options, "cursorColor2", &options->cursorColor2, "#000000");
    get_string(&db, options, "colorBD", &options->colorBD, "#FFFFFF");

    char **palette = options->palette;
    get_string(&db, options, "color0",  palette   , "#000000");
    get_string(&db, options, "color1",  palette+1 , "#AA0000");
    get_string(&db, options, "color2",  palette+2 , "#00AA00");
    get_string(&db, options, "color3",  palette+3 , "#AA5500");
    get_string(&db, options, "color4",  palette+4 , "#0000AA");
    get_string(&db, options, "color5",  palette+5 , "#AA00AA");
    get_string(&db, options, "color6",  palette+6 , "#00AAAA");
    get_string(&db, options, "color7",  palette+7 , "#AAAAAA");
    get_string(&db, options, "color8",  palette+8 , "#555555");
    get_string(&db, options, "color9",  palette+9 , "#FF5555");
    get_string(&db, options, "color10", palette+10, "#55FF55");
    get_string(&db, options, "color11", palette+11, "#FFFF55");
    get_string(&db, options, "color12", palette+12, "#5555FF");
    get_string(&db, options, "color13", palette+13, "#FF55FF");
    get_string(&db, options, "color14", palette+14, "#55FFFF");
    get_string(&db, options, "color15", palette+15, "#FFFFFF");

    /* Fonts. Stored in a global variable. */
    if(!xiate_fonts) {
        gchar *font_spec;
        get_string(&db, options, "font", &font_spec, "VGA 12");
        xiate_fonts = g_strsplit(font_spec, ",", -1);
    }

    XrmDestroyDatabase(db);
}

