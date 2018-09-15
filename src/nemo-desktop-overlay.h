#ifndef _NEMO_DESKTOP_OVERLAY_H_
#define _NEMO_DESKTOP_OVERLAY_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_DESKTOP_OVERLAY (nemo_desktop_overlay_get_type ())

G_DECLARE_FINAL_TYPE (NemoDesktopOverlay, nemo_desktop_overlay, NEMO, DESKTOP_OVERLAY, GObject)

NemoDesktopOverlay *nemo_desktop_overlay_new (void);
void                nemo_desktop_overlay_show (NemoDesktopOverlay *overlay,
                                               gint                monitor);
void                nemo_desktop_overlay_update_in_place (NemoDesktopOverlay *overlay);
G_END_DECLS

#endif /* _NEMO_DESKTOP_OVERLAY_H_ */