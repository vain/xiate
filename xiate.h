#ifndef __XIATE_H__
#define __XIATE_H__

#include <glib.h>
#include <gio/gio.h>

GSocketAddress* xiate_socket_address(const char* suffix);
GSocketAddress* xiate_new_socket_address(const char* suffix);

#endif
