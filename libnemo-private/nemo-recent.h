

#ifndef __NEMO_RECENT_H__
#define __NEMO_RECENT_H__

#include <gtk/gtk.h>
#include <libnemo-private/nemo-file.h>
#include <gio/gio.h>

void nemo_recent_add_file (NemoFile *file,
			       GAppInfo *application);

#endif
