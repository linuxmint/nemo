#ifndef NEMO_ICON_INFO_H
#define NEMO_ICON_INFO_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Names for Nemo's different zoom levels, from tiniest items to largest items */
typedef enum {
	NEMO_ZOOM_LEVEL_SMALLEST = 0,
	NEMO_ZOOM_LEVEL_SMALLER,
	NEMO_ZOOM_LEVEL_SMALL,
	NEMO_ZOOM_LEVEL_STANDARD,
	NEMO_ZOOM_LEVEL_LARGE,
	NEMO_ZOOM_LEVEL_LARGER,
	NEMO_ZOOM_LEVEL_LARGEST
} NemoZoomLevel;

#define NEMO_ZOOM_LEVEL_N_ENTRIES (NEMO_ZOOM_LEVEL_LARGEST + 1)

/* Nominal icon sizes for each Nemo zoom level.
 * This scheme assumes that icons are designed to
 * fit in a square space, though each image needn't
 * be square. Since individual icons can be stretched,
 * each icon is not constrained to this nominal size.
 */
#define NEMO_ICON_SIZE_SMALLEST	16
#define NEMO_ICON_SIZE_SMALLER	24
#define NEMO_ICON_SIZE_SMALL	32
#define NEMO_ICON_SIZE_STANDARD	48
#define NEMO_ICON_SIZE_LARGE	72
#define NEMO_ICON_SIZE_LARGER	96
#define NEMO_ICON_SIZE_LARGEST     192

/* Maximum size of an icon that the icon factory will ever produce */
#define NEMO_ICON_MAXIMUM_SIZE     320

typedef struct _NemoIconInfo      NemoIconInfo;
typedef struct _NemoIconInfoClass NemoIconInfoClass;


#define NEMO_TYPE_ICON_INFO                 (nemo_icon_info_get_type ())
#define NEMO_ICON_INFO(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_ICON_INFO, NemoIconInfo))
#define NEMO_ICON_INFO_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_ICON_INFO, NemoIconInfoClass))
#define NEMO_IS_ICON_INFO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_ICON_INFO))
#define NEMO_IS_ICON_INFO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_ICON_INFO))
#define NEMO_ICON_INFO_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_ICON_INFO, NemoIconInfoClass))


GType    nemo_icon_info_get_type (void) G_GNUC_CONST;

NemoIconInfo *    nemo_icon_info_new_for_pixbuf               (GdkPixbuf         *pixbuf);
NemoIconInfo *    nemo_icon_info_lookup                       (GIcon             *icon,
								       int                size);
NemoIconInfo *    nemo_icon_info_lookup_from_name             (const char        *name,
								       int                size);
NemoIconInfo *    nemo_icon_info_lookup_from_path             (const char        *path,
								       int                size);
gboolean              nemo_icon_info_is_fallback                  (NemoIconInfo  *icon);
GdkPixbuf *           nemo_icon_info_get_pixbuf                   (NemoIconInfo  *icon);
GdkPixbuf *           nemo_icon_info_get_pixbuf_nodefault         (NemoIconInfo  *icon);
GdkPixbuf *           nemo_icon_info_get_pixbuf_nodefault_at_size (NemoIconInfo  *icon,
								       gsize              forced_size);
GdkPixbuf *           nemo_icon_info_get_pixbuf_at_size           (NemoIconInfo  *icon,
								       gsize              forced_size);
gboolean              nemo_icon_info_get_embedded_rect            (NemoIconInfo  *icon,
								       GdkRectangle      *rectangle);
gboolean              nemo_icon_info_get_attach_points            (NemoIconInfo  *icon,
								       GdkPoint         **points,
								       gint              *n_points);
const char *          nemo_icon_info_get_display_name             (NemoIconInfo  *icon);
const char *          nemo_icon_info_get_used_name                (NemoIconInfo  *icon);

void                  nemo_icon_info_clear_caches                 (void);

/* Relationship between zoom levels and icons sizes. */
guint nemo_get_icon_size_for_zoom_level          (NemoZoomLevel  zoom_level);
guint nemo_get_list_icon_size_for_zoom_level     (NemoZoomLevel  zoom_level);

gint  nemo_get_icon_size_for_stock_size          (GtkIconSize        size);
guint nemo_icon_get_emblem_size_for_icon_size    (guint              size);

gboolean nemo_icon_theme_can_render              (GThemedIcon *icon);
GIcon * nemo_user_special_directory_get_gicon (GUserDirectory directory);


G_END_DECLS

#endif /* NEMO_ICON_INFO_H */

