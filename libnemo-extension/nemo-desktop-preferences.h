#ifndef _NEMO_DESKTOP_PREFERENCES_H_
#define _NEMO_DESKTOP_PREFERENCES_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_DESKTOP_PREFERENCES (nemo_desktop_preferences_get_type ())

G_DECLARE_FINAL_TYPE (NemoDesktopPreferences, nemo_desktop_preferences, NEMO, DESKTOP_PREFERENCES, GtkBin)

NemoDesktopPreferences *nemo_desktop_preferences_new (void);

G_END_DECLS

#endif /* _NEMO_DESKTOP_PREFERENCES_H_ */