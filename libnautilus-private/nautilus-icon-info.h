#ifndef NAUTILUS_ICON_INFO_H
#define NAUTILUS_ICON_INFO_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Names for Nautilus's different zoom levels, from tiniest items to largest items */
typedef enum {
	NAUTILUS_ZOOM_LEVEL_SMALLEST,
	NAUTILUS_ZOOM_LEVEL_SMALLER,
	NAUTILUS_ZOOM_LEVEL_SMALL,
	NAUTILUS_ZOOM_LEVEL_STANDARD,
	NAUTILUS_ZOOM_LEVEL_LARGE,
	NAUTILUS_ZOOM_LEVEL_LARGER,
	NAUTILUS_ZOOM_LEVEL_LARGEST
} NautilusZoomLevel;

#define NAUTILUS_ZOOM_LEVEL_N_ENTRIES (NAUTILUS_ZOOM_LEVEL_LARGEST + 1)

/* Nominal icon sizes for each Nautilus zoom level.
 * This scheme assumes that icons are designed to
 * fit in a square space, though each image needn't
 * be square. Since individual icons can be stretched,
 * each icon is not constrained to this nominal size.
 */
#define NAUTILUS_ICON_SIZE_SMALLEST	16
#define NAUTILUS_ICON_SIZE_SMALLER	24
#define NAUTILUS_ICON_SIZE_SMALL	32
#define NAUTILUS_ICON_SIZE_STANDARD	48
#define NAUTILUS_ICON_SIZE_LARGE	72
#define NAUTILUS_ICON_SIZE_LARGER	96
#define NAUTILUS_ICON_SIZE_LARGEST     192

/* Maximum size of an icon that the icon factory will ever produce */
#define NAUTILUS_ICON_MAXIMUM_SIZE     320

typedef struct _NautilusIconInfo      NautilusIconInfo;
typedef struct _NautilusIconInfoClass NautilusIconInfoClass;


#define NAUTILUS_TYPE_ICON_INFO                 (nautilus_icon_info_get_type ())
#define NAUTILUS_ICON_INFO(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ICON_INFO, NautilusIconInfo))
#define NAUTILUS_ICON_INFO_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICON_INFO, NautilusIconInfoClass))
#define NAUTILUS_IS_ICON_INFO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ICON_INFO))
#define NAUTILUS_IS_ICON_INFO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ICON_INFO))
#define NAUTILUS_ICON_INFO_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_ICON_INFO, NautilusIconInfoClass))


GType    nautilus_icon_info_get_type (void) G_GNUC_CONST;

NautilusIconInfo *    nautilus_icon_info_new_for_pixbuf               (GdkPixbuf         *pixbuf);
NautilusIconInfo *    nautilus_icon_info_lookup                       (GIcon             *icon,
								       int                size);
NautilusIconInfo *    nautilus_icon_info_lookup_from_name             (const char        *name,
								       int                size);
NautilusIconInfo *    nautilus_icon_info_lookup_from_path             (const char        *path,
								       int                size);
gboolean              nautilus_icon_info_is_fallback                  (NautilusIconInfo  *icon);
GdkPixbuf *           nautilus_icon_info_get_pixbuf                   (NautilusIconInfo  *icon);
GdkPixbuf *           nautilus_icon_info_get_pixbuf_nodefault         (NautilusIconInfo  *icon);
GdkPixbuf *           nautilus_icon_info_get_pixbuf_nodefault_at_size (NautilusIconInfo  *icon,
								       gsize              forced_size);
GdkPixbuf *           nautilus_icon_info_get_pixbuf_at_size           (NautilusIconInfo  *icon,
								       gsize              forced_size);
gboolean              nautilus_icon_info_get_embedded_rect            (NautilusIconInfo  *icon,
								       GdkRectangle      *rectangle);
gboolean              nautilus_icon_info_get_attach_points            (NautilusIconInfo  *icon,
								       GdkPoint         **points,
								       gint              *n_points);
const char *          nautilus_icon_info_get_display_name             (NautilusIconInfo  *icon);
const char *          nautilus_icon_info_get_used_name                (NautilusIconInfo  *icon);

void                  nautilus_icon_info_clear_caches                 (void);

/* Relationship between zoom levels and icons sizes. */
guint nautilus_get_icon_size_for_zoom_level          (NautilusZoomLevel  zoom_level);

gint  nautilus_get_icon_size_for_stock_size          (GtkIconSize        size);
guint nautilus_icon_get_emblem_size_for_icon_size    (guint              size);

gboolean nautilus_icon_theme_can_render              (GThemedIcon *icon);
GIcon * nautilus_user_special_directory_get_gicon (GUserDirectory directory);


G_END_DECLS

#endif /* NAUTILUS_ICON_INFO_H */

