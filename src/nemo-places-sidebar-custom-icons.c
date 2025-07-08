#include "nemo-places-sidebar-custom-icons.h"
#include <gio/gio.h>

#define SETTINGS_SCHEMA "org.nemo.sidebar-panels.custom-icons"
#define KEY_ENABLED     "use-custom-sidebar-icons"
#define KEY_ICON_MAP    "custom-sidebar-icons"

static GSettings *settings = NULL;

void nemo_sidebar_custom_icons_init(void) {
    if (!settings)
        settings = g_settings_new(SETTINGS_SCHEMA);
}

gboolean nemo_sidebar_custom_icons_enabled(void) {
    if (!settings)
        nemo_sidebar_custom_icons_init();
    return g_settings_get_boolean(settings, KEY_ENABLED);
}

gchar *nemo_sidebar_get_custom_icon_for_uri(const gchar *uri) {
    if (!settings)
        nemo_sidebar_custom_icons_init();

    GVariant *map = g_settings_get_value(settings, KEY_ICON_MAP);
    gchar *icon_name = NULL;

    if (g_variant_lookup(map, uri, "s", &icon_name)) {
        g_variant_unref(map);
        return icon_name;
    }

    g_variant_unref(map);
    return NULL;
}

void nemo_sidebar_set_custom_icon_for_uri(const gchar *uri, const gchar *icon_name) {
    if (!settings)
        nemo_sidebar_custom_icons_init();

    GVariant *map = g_settings_get_value(settings, KEY_ICON_MAP);
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{ss}"));

    // Rebuild the dict with updated key
    GVariantIter iter;
    gchar *key, *value;
    g_variant_iter_init(&iter, map);
    while (g_variant_iter_next(&iter, "{ss}", &key, &value)) {
        if (g_strcmp0(key, uri) != 0)
            g_variant_builder_add(&builder, "{ss}", key, value);
        g_free(key);
        g_free(value);
    }
    g_variant_unref(map);

    // Add or update this entry
    g_variant_builder_add(&builder, "{ss}", uri, icon_name);
    g_settings_set_value(settings, KEY_ICON_MAP, g_variant_builder_end(&builder));
}

void nemo_sidebar_clear_custom_icon_for_uri(const gchar *uri) {
    if (!settings)
        nemo_sidebar_custom_icons_init();

    GVariant *map = g_settings_get_value(settings, KEY_ICON_MAP);
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{ss}"));

    // Copy everything except the URI to remove
    GVariantIter iter;
    gchar *key, *value;
    g_variant_iter_init(&iter, map);
    while (g_variant_iter_next(&iter, "{ss}", &key, &value)) {
        if (g_strcmp0(key, uri) != 0)
            g_variant_builder_add(&builder, "{ss}", key, value);
        g_free(key);
        g_free(value);
    }
    g_variant_unref(map);

    g_settings_set_value(settings, KEY_ICON_MAP, g_variant_builder_end(&builder));
}