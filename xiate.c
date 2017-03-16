#include "xiate.h"
#include <gio/gunixsocketaddress.h>

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
