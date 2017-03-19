#include "xiate.h"
#include <string.h>

struct client_options {
    gchar *wclass;
    gchar *title;
    gchar *name;
    gchar *suffix;
    gchar **command;
    gboolean hold;
};

struct client_state {
    GSocketConnection *conn;
    GSocketClient* gclient;
    GSocketAddress* addr;
    GOutputStream *ostream;
    GError *error;
};

static void parse_options(
    int *argc, char ***argv,
    struct client_options *options)
{
    GError *error = NULL;
    GOptionContext *context;

    memset(options, 0, sizeof(struct client_options));

    GOptionEntry entries[] = {
        { "class", 'c', 0, G_OPTION_ARG_STRING, &options->wclass,
          "The window class. If not specified, 'Xiate' will be used.",
          "CLASS" },
        { "title", 't', 0, G_OPTION_ARG_STRING, &options->title,
          "The window title.",
          "TITLE" },
        { "name", 'n', 0, G_OPTION_ARG_STRING, &options->name,
          "The window name.",
          "CLASS" },
        { "hold", 'h', 0, G_OPTION_ARG_NONE, &options->hold,
          "If specified, do not immediately exit.", NULL },
        { "suffix", 's', 0, G_OPTION_ARG_STRING, &options->suffix,
          "The Xiate identifier suffix. If not specified, 'main' will be used.",
          "SUFFIX" },
        { "", 0, 0, G_OPTION_ARG_STRING_ARRAY, &options->command,
          "The command to execute in the terminal", "[-- CMD ARG1 ARG2 ...]" },
        { NULL }
    };

    context = g_option_context_new(" - Start a Xiate terminal client");
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_help_enabled(context, TRUE);
    g_option_context_parse(context, argc, argv, &error);

    if(error) g_error(error->message);

    g_option_context_free(context);
}

static void connect(struct client_options *options, struct client_state *state)
{
    state->error = NULL;
    state->addr = xiate_socket_address(options->suffix);
    state->gclient = g_socket_client_new();
    state->conn = g_socket_client_connect(
        state->gclient,
        (GSocketConnectable*) state->addr,
        NULL, &(state->error));

    if(state->error) g_error(state->error->message);

    state->ostream = g_io_stream_get_output_stream(G_IO_STREAM(state->conn));
}

static void send(struct client_state *state, const char* buf, int size)
{
    g_output_stream_write(state->ostream, buf, size, NULL, &(state->error));
    if(state->error)
        g_error(state->error->message);
}

static void cmd(struct client_state* state,
                const char* cmd,
                const char* arg)
{
    send(state, cmd, strlen(cmd));
    send(state, arg, strlen(arg));
    send(state, "\000", 1);
}

static void init(struct client_options *options, struct client_state *state)
{
    gchar* pwd = g_get_current_dir();
    cmd(state, "OC", pwd);
    g_free(pwd);

    if(options->wclass) {
        cmd(state, "Oc", options->wclass);
        g_free(options->wclass);
    }

    if(options->title) {
        cmd(state, "Ot", options->title);
        g_free(options->title);
    }

    if(options->name) {
        cmd(state, "On", options->name);
        g_free(options->name);
    }

    if(options->hold) {
        send(state, "H", 1);
    }

    if(options->command) {
        for(gchar** arg=options->command; *arg; ++arg)
            cmd(state, "A", *arg);
        g_strfreev(options->command);
    }
}

int main(int argc, char **argv)
{
    static struct client_options options;
    static struct client_state state;

    parse_options(&argc, &argv, &options);
    connect(&options, &state);
    init(&options, &state);

    return 0;
}
