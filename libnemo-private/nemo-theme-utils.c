#include "nemo-theme-utils.h"
#include "nemo-global-preferences.h"

#include <eel/eel-string.h>

static GtkCssProvider *mandatory_css_provider = NULL;
static GtkCssProvider *desktop_css_provider = NULL;
static gboolean theme_has_desktop_support = FALSE;

static gboolean
css_provider_load_from_resource (GtkCssProvider *provider,
                                 const char     *resource_path,
                                 GError        **error)
{
    GBytes *data;
    gboolean retval;

    data = g_resources_lookup_data (resource_path, 0, error);
    if (!data)
        return FALSE;

    retval = gtk_css_provider_load_from_data (provider,
                                              g_bytes_get_data (data, NULL),
                                              g_bytes_get_size (data),
                                              error);
    g_bytes_unref (data);

    return retval;
}

static gchar *
load_file_contents_from_resource (const char *resource_path,
                                  GError    **error)
{
    GBytes *data;
    gchar *retval;

    data = g_resources_lookup_data (resource_path, 0, error);

    if (!data) {
        return NULL;
    }

    retval = g_strdup ((gchar *) g_bytes_get_data (data, NULL));

    g_bytes_unref (data);

    return retval;
}

static void
add_css_provider_at_priority (const gchar *rpath, guint priority)
{
    GtkCssProvider *provider;
    GError *error = NULL;

    provider = gtk_css_provider_new ();

    if (!css_provider_load_from_resource (provider, rpath, &error))
    {
        g_warning ("Failed to load css file '%s': %s", rpath, error->message);
        g_clear_error (&error);
        goto out;
    }

    gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                               GTK_STYLE_PROVIDER (provider),
                                               priority);

out:
    g_object_unref (provider);
}

static void
add_fallback_mandatory_css_provider (const gchar *theme_name)
{
    GtkCssProvider *current_provider;
    g_autofree gchar *css = NULL;
    g_autofree gchar *init_fallback_css = NULL;
    g_autofree gchar *final_fallback_css = NULL;
    GError *error = NULL;

    current_provider = gtk_css_provider_get_named (theme_name, NULL);

    css = gtk_css_provider_to_string (current_provider);

    if (!g_strstr_len (css, -1, "nemo")) {
        g_warning ("The theme appears to have no nemo support.  Adding some...");

        init_fallback_css = load_file_contents_from_resource ("/org/nemo/nemo-style-fallback-mandatory.css",
                                                              &error);

        if (!init_fallback_css) {
            g_warning ("Failed to load fallback mandatory css file: %s", error->message);
            g_clear_error (&error);

            goto out;
        }
    } else {
        goto out;
    }

    if (g_strstr_len (css, -1, "theme_selected_bg_color")) {
        final_fallback_css = g_strdup (init_fallback_css);

        goto apply;
    }

    /* Some themes don't prefix colors with theme_ - remove this from our fallback css */
    if (g_strstr_len (css, -1, "@define-color selected_bg_color")) {
        g_warning ("Replacing theme_selected_bg_color with selected_bg_color");
        final_fallback_css = eel_str_replace_substring (init_fallback_css,
                                                        "@theme_",
                                                        "@");
    } else {
        /* If we can find neither, just bail out */
        g_warning ("Theme lacks both theme_selected_bg_color and selected_bg_color - "
                   "cannot apply fallback CSS");
        goto out;
    }

apply:
    mandatory_css_provider = gtk_css_provider_new ();

    gtk_css_provider_load_from_data (mandatory_css_provider,
                                     final_fallback_css,
                                     -1,
                                     &error);

    if (error) {
        g_warning ("Failed to create a fallback provider: %s", error->message);
        g_clear_error (&error);
        g_clear_object (&mandatory_css_provider);
        goto out;
    }

    gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                               GTK_STYLE_PROVIDER (mandatory_css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
out:
    ;
}

static const char *supported_theme_hints[] = {
    "mint",
    "arc",
    "numix",
    "matcha"
};

static gboolean
is_known_supported_theme (const gchar *theme_name)
{
    gint i;
    gchar *name;
    gboolean ret;

    name = g_utf8_casefold (theme_name, -1);
    ret = FALSE;

    for (i = 0; i < G_N_ELEMENTS (supported_theme_hints); i++) {
        gchar *hint;

        hint = g_utf8_casefold (supported_theme_hints[i], -1);

        if (g_strstr_len (name, -1, hint)) {
            ret = TRUE;
        }

        g_free (hint);

        if (ret) {
            break;
        }
    }

    g_free (name);

    return ret;
}

static void
check_desktop_support (const gchar *theme_name)
{
    GtkCssProvider *provider;
    gchar *css;

    provider = gtk_css_provider_get_named (theme_name, NULL);
    css = gtk_css_provider_to_string (provider);

    theme_has_desktop_support = (g_strstr_len (css, -1, "nemo-desktop") != NULL);

    g_free (css);
}

static void
update_desktop_provider (void)
{
    gboolean use_theme;
    gboolean should_load;

    use_theme = g_settings_get_boolean (nemo_desktop_preferences,
                                        NEMO_PREFERENCES_DESKTOP_TEXT_SHADOW_USE_THEME);

    /* Load our desktop CSS unless the user opted into the theme's own
     * desktop styling AND the theme actually provides it. */
    should_load = !(use_theme && theme_has_desktop_support);

    if (should_load && desktop_css_provider == NULL) {
        GtkCssProvider *provider;
        GError *error = NULL;

        provider = gtk_css_provider_new ();

        if (!css_provider_load_from_resource (provider, "/org/nemo/nemo-style-desktop.css", &error)) {
            g_warning ("Failed to load desktop css: %s", error->message);
            g_clear_error (&error);
            g_object_unref (provider);
            return;
        }

        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        desktop_css_provider = provider;
    } else if (!should_load && desktop_css_provider != NULL) {
        gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                      GTK_STYLE_PROVIDER (desktop_css_provider));
        g_clear_object (&desktop_css_provider);
    }
}

static void
process_theme (GtkSettings *gtk_settings)
{
    gchar *theme_name;

    if (mandatory_css_provider != NULL) {
        gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                      GTK_STYLE_PROVIDER (mandatory_css_provider));
        g_clear_object (&mandatory_css_provider);
    }

    g_object_get (gtk_settings,
                  "gtk-theme-name", &theme_name,
                  NULL);

    if (!is_known_supported_theme (theme_name)) {
        g_warning ("Current gtk theme is not known to have nemo support (%s) - checking...", theme_name);
        add_fallback_mandatory_css_provider (theme_name);
    }

    check_desktop_support (theme_name);
    update_desktop_provider ();

    gtk_style_context_reset_widgets (gdk_screen_get_default ());
    g_free (theme_name);
}

static void
on_use_theme_setting_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
    update_desktop_provider ();
    gtk_style_context_reset_widgets (gdk_screen_get_default ());
}

void
nemo_theme_utils_init_styles (void)
{
    static gboolean initialized = FALSE;
    GtkSettings *gtk_settings;

    if (initialized)
        return;

    initialized = TRUE;

    add_css_provider_at_priority ("/org/nemo/nemo-style-fallback.css",
                                  GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

    add_css_provider_at_priority ("/org/nemo/nemo-style-application.css",
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_settings = gtk_settings_get_default ();

    g_signal_connect_swapped (gtk_settings, "notify::gtk-theme-name",
                              G_CALLBACK (process_theme), gtk_settings);

    g_signal_connect (nemo_desktop_preferences,
                      "changed::" NEMO_PREFERENCES_DESKTOP_TEXT_SHADOW_USE_THEME,
                      G_CALLBACK (on_use_theme_setting_changed), NULL);

    process_theme (gtk_settings);
}
