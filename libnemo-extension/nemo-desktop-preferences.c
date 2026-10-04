#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include <glib/gi18n.h>
#include "nemo-desktop-preferences.h"

#define NUM_SHADOW_OPTIONS 3

static const gchar *shadow_options[NUM_SHADOW_OPTIONS] = {
    "normal",
    "darker",
    "darkest",
};

static const gchar *preview_bg_css =
    ".shadow-preview-bg {"
    "  background-image: linear-gradient(to bottom, #7ba4d4, #d4e6f7);"
    "}";

typedef struct
{
    GtkBuilder *builder;
    GSettings *desktop_settings;
    GtkCssProvider *desktop_css_provider;
    GtkCssProvider *preview_bg_provider;
    GtkWidget *shadow_radios[NUM_SHADOW_OPTIONS];
    GtkWidget *shadow_labels[NUM_SHADOW_OPTIONS];
    gulong shadow_setting_handler;
    gulong font_changed_handler;
} NemoDesktopPreferencesPrivate;

struct _NemoDesktopPreferences
{
    GtkBin parent_object;

    NemoDesktopPreferencesPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoDesktopPreferences, nemo_desktop_preferences, GTK_TYPE_BIN)

/* copied from nemo-file-management-properties.c */
static void
bind_builder_bool (GtkBuilder *builder,
                   GSettings  *settings,
                   const char *widget_name,
                   const char *prefs)
{
    g_settings_bind (settings, prefs,
                     gtk_builder_get_object (builder, widget_name),
                     "active", G_SETTINGS_BIND_DEFAULT);
}

static void
bind_builder_string_combo (GtkBuilder *builder,
                           GSettings  *settings,
                           const char *widget_name,
                           const char *prefs)
{
    g_settings_bind (settings, prefs,
                     gtk_builder_get_object (builder, widget_name),
                     "active-id", G_SETTINGS_BIND_DEFAULT);
}

static void
update_preview_fonts (NemoDesktopPreferencesPrivate *priv)
{
    gchar *font_str;
    PangoFontDescription *font_desc;
    gint i;

    font_str = g_settings_get_string (priv->desktop_settings, "font");
    font_desc = pango_font_description_from_string (font_str);
    g_free (font_str);

    for (i = 0; i < NUM_SHADOW_OPTIONS; i++) {
        PangoAttrList *attrs = pango_attr_list_new ();
        pango_attr_list_insert (attrs, pango_attr_font_desc_new (font_desc));
        gtk_label_set_attributes (GTK_LABEL (priv->shadow_labels[i]), attrs);
        pango_attr_list_unref (attrs);
    }

    pango_font_description_free (font_desc);
}

static void
on_shadow_button_toggled (GtkToggleButton *button, gpointer user_data)
{
    NemoDesktopPreferencesPrivate *priv = user_data;
    const gchar *id;

    if (!gtk_toggle_button_get_active (button))
        return;

    id = g_object_get_data (G_OBJECT (button), "shadow-id");
    g_settings_set_string (priv->desktop_settings, "desktop-text-shadow", id);
}

static void
on_shadow_setting_changed (GSettings *settings,
                           const gchar *key,
                           gpointer user_data)
{
    NemoDesktopPreferences *preferences = NEMO_DESKTOP_PREFERENCES (user_data);
    NemoDesktopPreferencesPrivate *priv = preferences->priv;
    gchar *current;
    gint i;

    current = g_settings_get_string (priv->desktop_settings, "desktop-text-shadow");

    for (i = 0; i < NUM_SHADOW_OPTIONS; i++) {
        if (g_strcmp0 (current, shadow_options[i]) == 0) {
            g_signal_handlers_block_by_func (priv->shadow_radios[i],
                                             on_shadow_button_toggled, priv);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->shadow_radios[i]), TRUE);
            g_signal_handlers_unblock_by_func (priv->shadow_radios[i],
                                               on_shadow_button_toggled, priv);
            break;
        }
    }

    g_free (current);
}

static void
on_desktop_font_changed (GSettings *settings,
                         const gchar *key,
                         gpointer user_data)
{
    NemoDesktopPreferences *preferences = NEMO_DESKTOP_PREFERENCES (user_data);
    update_preview_fonts (preferences->priv);
}

static void
add_provider_to_widget (GtkWidget *widget, GtkCssProvider *provider)
{
    gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                   GTK_STYLE_PROVIDER (provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
setup_shadow_preview_buttons (NemoDesktopPreferences *preferences)
{
    NemoDesktopPreferencesPrivate *priv = preferences->priv;
    gchar *current;
    gint i;

    priv->desktop_css_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (priv->desktop_css_provider,
                                         "/org/nemo/nemo-style-desktop.css");

    priv->preview_bg_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (priv->preview_bg_provider, preview_bg_css, -1, NULL);

    current = g_settings_get_string (priv->desktop_settings, "desktop-text-shadow");

    for (i = 0; i < NUM_SHADOW_OPTIONS; i++) {
        gchar *name;

        name = g_strdup_printf ("shadow_radio_%s", shadow_options[i]);
        priv->shadow_radios[i] = GTK_WIDGET (gtk_builder_get_object (priv->builder, name));
        g_free (name);

        name = g_strdup_printf ("shadow_frame_%s", shadow_options[i]);
        add_provider_to_widget (GTK_WIDGET (gtk_builder_get_object (priv->builder, name)),
                                priv->preview_bg_provider);
        g_free (name);

        name = g_strdup_printf ("shadow_label_%s", shadow_options[i]);
        priv->shadow_labels[i] = GTK_WIDGET (gtk_builder_get_object (priv->builder, name));
        g_free (name);

        add_provider_to_widget (priv->shadow_labels[i], priv->desktop_css_provider);

        g_object_set_data (G_OBJECT (priv->shadow_radios[i]), "shadow-id",
                           (gpointer) shadow_options[i]);

        g_signal_connect (priv->shadow_radios[i], "toggled",
                          G_CALLBACK (on_shadow_button_toggled), priv);

        if (g_strcmp0 (current, shadow_options[i]) == 0) {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->shadow_radios[i]), TRUE);
        }
    }

    g_free (current);

    update_preview_fonts (priv);

    priv->shadow_setting_handler =
        g_signal_connect (priv->desktop_settings, "changed::desktop-text-shadow",
                          G_CALLBACK (on_shadow_setting_changed), preferences);

    priv->font_changed_handler =
        g_signal_connect (priv->desktop_settings, "changed::font",
                          G_CALLBACK (on_desktop_font_changed), preferences);
}

static void
nemo_desktop_preferences_init (NemoDesktopPreferences *preferences)
{
    GtkWidget *widget;
    NemoDesktopPreferencesPrivate *priv = nemo_desktop_preferences_get_instance_private (preferences);

    preferences->priv = priv;

    priv->desktop_settings = g_settings_new ("org.nemo.desktop");

    priv->builder = gtk_builder_new ();
    gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
    gtk_builder_add_from_resource (priv->builder, "/org/nemo/nemo-desktop-preferences.glade", NULL);

    widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "prefs_box"));

    gtk_container_add (GTK_CONTAINER (preferences), widget);

    bind_builder_string_combo (priv->builder,
                               priv->desktop_settings,
                               "layout_combo",
                               "desktop-layout");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "computer_switch",
                       "computer-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "home_switch",
                       "home-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "trash_switch",
                       "trash-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "drives_switch",
                       "volumes-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "network_switch",
                       "network-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "orphan_switch",
                       "show-orphaned-desktop-icons");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "use_theme_switch",
                       "desktop-text-shadow-use-theme");

    setup_shadow_preview_buttons (preferences);

    gtk_widget_show_all (GTK_WIDGET (preferences));
}

static void
nemo_desktop_preferences_dispose (GObject *object)
{
    NemoDesktopPreferences *preferences = NEMO_DESKTOP_PREFERENCES (object);
    NemoDesktopPreferencesPrivate *priv = preferences->priv;

    if (priv->desktop_settings != NULL) {
        if (priv->shadow_setting_handler > 0) {
            g_signal_handler_disconnect (priv->desktop_settings, priv->shadow_setting_handler);
            priv->shadow_setting_handler = 0;
        }
        if (priv->font_changed_handler > 0) {
            g_signal_handler_disconnect (priv->desktop_settings, priv->font_changed_handler);
            priv->font_changed_handler = 0;
        }
    }

    g_clear_object (&priv->desktop_css_provider);
    g_clear_object (&priv->preview_bg_provider);
    g_clear_object (&priv->builder);
    g_clear_object (&priv->desktop_settings);

    G_OBJECT_CLASS (nemo_desktop_preferences_parent_class)->dispose (object);
}

static void
nemo_desktop_preferences_class_init (NemoDesktopPreferencesClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = nemo_desktop_preferences_dispose;
}

NemoDesktopPreferences *
nemo_desktop_preferences_new (void)
{
    return g_object_new (NEMO_TYPE_DESKTOP_PREFERENCES, NULL);
}
