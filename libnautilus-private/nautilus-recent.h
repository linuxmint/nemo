

#ifndef __NAUTILUS_RECENT_H__
#define __NAUTILUS_RECENT_H__

#include <gtk/gtk.h>
#include <libnautilus-private/nautilus-file.h>
#include <gio/gio.h>

void nautilus_recent_add_file (NautilusFile *file,
			       GAppInfo *application);

#endif
