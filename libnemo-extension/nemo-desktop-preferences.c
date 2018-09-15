#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include "nemo-desktop-preferences.h"

typedef struct
{
    GtkBuilder *builder;
    GSettings *desktop_settings;
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
                       "computer_toggle",
                       "computer-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "home_toggle",
                       "home-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "trash_toggle",
                       "trash-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "drives_toggle",
                       "volumes-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "network_toggle",
                       "network-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "orphan_switch",
                       "show-orphaned-desktop-icons");

    gtk_widget_show_all (GTK_WIDGET (preferences));
}

static void
nemo_desktop_preferences_dispose (GObject *object)
{
    NemoDesktopPreferences *preferences = NEMO_DESKTOP_PREFERENCES (object);

    g_clear_object (&preferences->priv->builder);
    g_clear_object (&preferences->priv->desktop_settings);

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
