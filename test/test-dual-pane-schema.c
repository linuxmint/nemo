/*
 * test-dual-pane-schema.c
 *
 * Pure GSettings unit tests for the three new dual-pane schema keys.
 * No window, no display, no GtkApplication required.
 *
 * These tests verify:
 *   - Each key exists in the schema (schema lookup does not abort)
 *   - Each key defaults to FALSE in the schema (preserving existing behaviour)
 *   - Each key can be set and read back via dconf
 *   - start-with-dual-pane still exists (regression: we didn't remove it)
 *
 * "Default" tests use g_settings_get_default_value() which reads the schema
 * XML directly, bypassing any dconf overrides left by previous test runs.
 *
 * Run with:
 *   GSETTINGS_SCHEMA_DIR=<build>/test meson test --suite dual-pane
 */

#include <glib.h>
#include <gio/gio.h>

#define NEMO_SCHEMA "org.nemo.preferences"

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static GSettings *
open_nemo_prefs (void)
{
    GSettings *s = g_settings_new (NEMO_SCHEMA);
    g_assert_nonnull (s);
    return s;
}

/* Read the schema-declared default, bypassing any dconf override. */
static gboolean
schema_default_bool (GSettings *s, const gchar *key)
{
    GVariant *v = g_settings_get_default_value (s, key);
    g_assert_nonnull (v);
    gboolean result = g_variant_get_boolean (v);
    g_variant_unref (v);
    return result;
}

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

static void
test_schema_loads (void)
{
    GSettings *s = open_nemo_prefs ();
    /* If the schema didn't load, g_settings_new would have aborted. */
    g_object_unref (s);
}

static void
test_start_with_dual_pane_exists (void)
{
    GSettings *s = open_nemo_prefs ();
    /* Regression guard: moving the key to the Dual Pane section in the UI
     * must not remove it from the schema. */
    GVariant *v = g_settings_get_default_value (s, "start-with-dual-pane");
    g_assert_nonnull (v);
    g_assert_true (g_variant_is_of_type (v, G_VARIANT_TYPE_BOOLEAN));
    g_variant_unref (v);
    g_object_unref (s);
}

static void
test_vertical_split_default_false (void)
{
    GSettings *s = open_nemo_prefs ();
    g_assert_false (schema_default_bool (s, "dual-pane-vertical-split"));
    g_object_unref (s);
}

static void
test_separate_sidebar_default_false (void)
{
    GSettings *s = open_nemo_prefs ();
    g_assert_false (schema_default_bool (s, "dual-pane-separate-sidebar"));
    g_object_unref (s);
}

static void
test_separate_nav_bar_default_false (void)
{
    GSettings *s = open_nemo_prefs ();
    g_assert_false (schema_default_bool (s, "dual-pane-separate-nav-bar"));
    g_object_unref (s);
}

static void
test_vertical_split_roundtrip (void)
{
    GSettings *s = open_nemo_prefs ();
    g_settings_set_boolean (s, "dual-pane-vertical-split", TRUE);
    g_assert_true  (g_settings_get_boolean (s, "dual-pane-vertical-split"));
    g_settings_set_boolean (s, "dual-pane-vertical-split", FALSE);
    g_assert_false (g_settings_get_boolean (s, "dual-pane-vertical-split"));
    g_settings_reset (s, "dual-pane-vertical-split");
    g_object_unref (s);
}

static void
test_separate_sidebar_roundtrip (void)
{
    GSettings *s = open_nemo_prefs ();
    g_settings_set_boolean (s, "dual-pane-separate-sidebar", TRUE);
    g_assert_true  (g_settings_get_boolean (s, "dual-pane-separate-sidebar"));
    g_settings_set_boolean (s, "dual-pane-separate-sidebar", FALSE);
    g_assert_false (g_settings_get_boolean (s, "dual-pane-separate-sidebar"));
    g_settings_reset (s, "dual-pane-separate-sidebar");
    g_object_unref (s);
}

static void
test_separate_nav_bar_roundtrip (void)
{
    GSettings *s = open_nemo_prefs ();
    g_settings_set_boolean (s, "dual-pane-separate-nav-bar", TRUE);
    g_assert_true  (g_settings_get_boolean (s, "dual-pane-separate-nav-bar"));
    g_settings_set_boolean (s, "dual-pane-separate-nav-bar", FALSE);
    g_assert_false (g_settings_get_boolean (s, "dual-pane-separate-nav-bar"));
    g_settings_reset (s, "dual-pane-separate-nav-bar");
    g_object_unref (s);
}

static void
test_all_three_keys_independent (void)
{
    /* Setting one key must not affect the others. */
    GSettings *s = open_nemo_prefs ();

    g_settings_set_boolean (s, "dual-pane-vertical-split",   TRUE);
    g_settings_set_boolean (s, "dual-pane-separate-sidebar", FALSE);
    g_settings_set_boolean (s, "dual-pane-separate-nav-bar", FALSE);

    g_assert_true  (g_settings_get_boolean (s, "dual-pane-vertical-split"));
    g_assert_false (g_settings_get_boolean (s, "dual-pane-separate-sidebar"));
    g_assert_false (g_settings_get_boolean (s, "dual-pane-separate-nav-bar"));

    g_settings_set_boolean (s, "dual-pane-vertical-split",   FALSE);
    g_settings_set_boolean (s, "dual-pane-separate-sidebar", TRUE);
    g_settings_set_boolean (s, "dual-pane-separate-nav-bar", FALSE);

    g_assert_false (g_settings_get_boolean (s, "dual-pane-vertical-split"));
    g_assert_true  (g_settings_get_boolean (s, "dual-pane-separate-sidebar"));
    g_assert_false (g_settings_get_boolean (s, "dual-pane-separate-nav-bar"));

    /* Reset all to schema defaults so subsequent runs start clean. */
    g_settings_reset (s, "dual-pane-vertical-split");
    g_settings_reset (s, "dual-pane-separate-sidebar");
    g_settings_reset (s, "dual-pane-separate-nav-bar");

    g_object_unref (s);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/dual-pane/schema/loads",
                     test_schema_loads);
    g_test_add_func ("/dual-pane/schema/start-with-dual-pane-exists",
                     test_start_with_dual_pane_exists);
    g_test_add_func ("/dual-pane/schema/vertical-split-default-false",
                     test_vertical_split_default_false);
    g_test_add_func ("/dual-pane/schema/separate-sidebar-default-false",
                     test_separate_sidebar_default_false);
    g_test_add_func ("/dual-pane/schema/separate-nav-bar-default-false",
                     test_separate_nav_bar_default_false);
    g_test_add_func ("/dual-pane/schema/vertical-split-roundtrip",
                     test_vertical_split_roundtrip);
    g_test_add_func ("/dual-pane/schema/separate-sidebar-roundtrip",
                     test_separate_sidebar_roundtrip);
    g_test_add_func ("/dual-pane/schema/separate-nav-bar-roundtrip",
                     test_separate_nav_bar_roundtrip);
    g_test_add_func ("/dual-pane/schema/all-three-keys-independent",
                     test_all_three_keys_independent);

    return g_test_run ();
}
