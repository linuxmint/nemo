#ifndef NEMO_PLACES_SIDEBAR_CUSTOM_ICONS_H
#define NEMO_PLACES_SIDEBAR_CUSTOM_ICONS_H

#include <glib.h>

G_BEGIN_DECLS

// Check if custom sidebar icons feature is enabled via GSettings
gboolean nemo_sidebar_custom_icons_enabled(void);

// Get a custom icon name for a given URI, or NULL if none set
gchar *nemo_sidebar_get_custom_icon_for_uri(const gchar *uri);

// Set a custom icon for a given URI
void nemo_sidebar_set_custom_icon_for_uri(const gchar *uri, const gchar *icon_name);

// Clear custom icon for a given URI
void nemo_sidebar_clear_custom_icon_for_uri(const gchar *uri);

// Called during Nemo startup to initialize icon mapping
void nemo_sidebar_custom_icons_init(void);

G_END_DECLS

#endif /* NEMO_PLACES_SIDEBAR_CUSTOM_ICONS_H */