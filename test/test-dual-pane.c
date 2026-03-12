/*
 * test-dual-pane.c
 *
 * GTest-based tests for Nemo dual-pane layout functionality.
 *
 * Covers:
 *   - Existing split-view on/off cycle (regression guard)
 *   - New preference: vertical split layout
 *   - New preference: separate sidebar per pane
 *   - New preference: separate nav bar per pane
 *   - Combinations of the above
 *   - Sidebar type change while per-pane mode is active (crash regression)
 *   - Hide/show sidebar while per-pane mode is active
 *   - Window destroy with per-pane mode active (crash regression)
 *   - Active-pane tracking through tear-down/rebuild cycles
 *   - pane->active_slot independence after wrappers are set up
 *
 * These tests require a running GLib main loop (gtk_init) and access to a
 * GSettings schema directory.  Run via:
 *
 *   GSETTINGS_SCHEMA_DIR=/tmp/nemo-schemas meson test --suite dual-pane
 *
 * or the project build script.
 *
 * NOTE: Because NemoWindow requires a GtkApplication, the tests use
 * NemoMainApplication as the singleton app context.  Many tests only
 * inspect widget hierarchy state (parent/child relationships) and do not
 * navigate to real filesystem paths, so no D-Bus or display compositor is
 * required beyond what Xvfb provides in CI.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include "src/nemo-main-application.h"
#include "src/nemo-application.h"
#include "src/nemo-window.h"
#include "src/nemo-window-private.h"
#include "src/nemo-window-pane.h"
#include "libnemo-private/nemo-global-preferences.h"

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Global app singleton shared across all tests in this process. */
static NemoApplication *test_app = NULL;

/*
 * create_test_window:
 * Create a fresh NemoWindow for a test.  The window is NOT shown;
 * widget hierarchies are still inspectable without a realised window.
 * The caller must gtk_widget_destroy() it when done.
 */
static NemoWindow *
create_test_window (void)
{
    NemoWindow *window;

    g_assert_nonnull (test_app);

    window = NEMO_APPLICATION_GET_CLASS (test_app)->create_window (
                 test_app,
                 gdk_screen_get_default ());

    /* Flush pending GTK/GLib events so construction signals settle */
    while (g_main_context_iteration (NULL, FALSE))
        ;

    return window;
}

/* Convenience: set a dual-pane bool preference and flush events. */
static void
set_pref_bool (const gchar *key, gboolean val)
{
    g_settings_set_boolean (nemo_preferences, key, val);
    while (g_main_context_iteration (NULL, FALSE))
        ;
}

/* Return the number of panes currently on a window. */
static guint
pane_count (NemoWindow *window)
{
    return g_list_length (window->details->panes);
}

/* Return the second pane or NULL. */
static NemoWindowPane *
get_pane2 (NemoWindow *window)
{
    GList *last = g_list_last (window->details->panes);
    if (!last || last->data == window->details->panes->data)
        return NULL;
    return NEMO_WINDOW_PANE (last->data);
}

/* Return TRUE if the widget is an ancestor of the given container. */
static gboolean
widget_is_inside (GtkWidget *widget, GtkWidget *container)
{
    GtkWidget *p = widget;
    while (p != NULL) {
        if (p == container) return TRUE;
        p = gtk_widget_get_parent (p);
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/* Test suite setup / teardown                                          */
/* ------------------------------------------------------------------ */

static void
suite_setup (void)
{
    /* Ensure all dual-pane prefs are at defaults before each test */
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,  FALSE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, FALSE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, FALSE);
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* GROUP 1: Existing split-view behaviour (all new prefs OFF)           */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/*
 * test_split_view_on_off:
 * Basic split-view cycle: single pane → split → single.
 * Verifies pane count and that the window doesn't crash.
 */
static void
test_split_view_on_off (void)
{
    NemoWindow *w = create_test_window ();

    g_assert_cmpuint (pane_count (w), ==, 1);

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_cmpuint (pane_count (w), ==, 2);
    g_assert_nonnull (get_pane2 (w));

    nemo_window_split_view_off (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_cmpuint (pane_count (w), ==, 1);
    g_assert_null (get_pane2 (w));

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_split_view_active_pane_survives:
 * After split-view-off, the surviving pane is the one that was active.
 */
static void
test_split_view_active_pane_survives (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    pane1 = w->details->panes->data;
    nemo_window_set_active_pane (w, pane1);
    while (g_main_context_iteration (NULL, FALSE)) ;

    nemo_window_split_view_off (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    /* pane1 must be the survivor */
    g_assert_cmpuint (pane_count (w), ==, 1);
    g_assert_true (w->details->panes->data == pane1);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_split_view_secondary_pane_active_survives:
 * If pane2 is active when split-view-off fires, pane2 should survive.
 */
static void
test_split_view_secondary_pane_active_survives (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane2;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    pane2 = get_pane2 (w);
    g_assert_nonnull (pane2);
    nemo_window_set_active_pane (w, pane2);
    while (g_main_context_iteration (NULL, FALSE)) ;

    nemo_window_split_view_off (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_cmpuint (pane_count (w), ==, 1);
    g_assert_true (w->details->panes->data == pane2);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_split_view_toolbar_returns_to_holder:
 * After split-view-off with all new prefs OFF, both toolbars should be
 * back in toolbar_holder, not inside the pane widget.
 */
static void
test_split_view_toolbar_returns_to_holder (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    nemo_window_split_view_off (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    pane1 = w->details->panes->data;
    /* toolbar's parent should be toolbar_holder */
    g_assert_true (
        gtk_widget_get_parent (pane1->tool_bar) ==
        w->details->toolbar_holder);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_sidebar_hide_show_no_split:
 * Hide and show sidebar without any split view; no crash, sidebar
 * pointer follows.
 */
static void
test_sidebar_hide_show_no_split (void)
{
    NemoWindow *w = create_test_window ();

    g_assert_nonnull (w->details->sidebar);

    nemo_window_hide_sidebar (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    g_assert_null (w->details->sidebar);

    nemo_window_show_sidebar (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    g_assert_nonnull (w->details->sidebar);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_sidebar_hide_show_with_split_prefs_off:
 * Sidebar hide/show works correctly with split view on but new prefs off.
 */
static void
test_sidebar_hide_show_with_split_prefs_off (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    nemo_window_hide_sidebar (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    g_assert_null (w->details->sidebar);
    /* Wrappers must not be active without per-pane prefs */
    g_assert_null (w->details->primary_pane_content_paned);
    g_assert_null (w->details->secondary_pane_content_paned);

    nemo_window_show_sidebar (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    g_assert_nonnull (w->details->sidebar);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* GROUP 2: Vertical split orientation pref                             */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/*
 * test_vertical_split_orientation:
 * Enabling vertical-split should change split_view_hpane orientation to
 * GTK_ORIENTATION_VERTICAL.
 */
static void
test_vertical_split_orientation (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    /* Default: horizontal */
    g_assert_cmpint (
        gtk_orientable_get_orientation (
            GTK_ORIENTABLE (w->details->split_view_hpane)),
        ==, GTK_ORIENTATION_HORIZONTAL);

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, TRUE);

    g_assert_cmpint (
        gtk_orientable_get_orientation (
            GTK_ORIENTABLE (w->details->split_view_hpane)),
        ==, GTK_ORIENTATION_VERTICAL);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_vertical_split_orientation_toggle:
 * Toggling the pref multiple times changes orientation and doesn't crash.
 */
static void
test_vertical_split_orientation_toggle (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, TRUE);
    g_assert_cmpint (gtk_orientable_get_orientation (
        GTK_ORIENTABLE (w->details->split_view_hpane)),
        ==, GTK_ORIENTATION_VERTICAL);

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, FALSE);
    g_assert_cmpint (gtk_orientable_get_orientation (
        GTK_ORIENTABLE (w->details->split_view_hpane)),
        ==, GTK_ORIENTATION_HORIZONTAL);

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, TRUE);
    g_assert_cmpint (gtk_orientable_get_orientation (
        GTK_ORIENTABLE (w->details->split_view_hpane)),
        ==, GTK_ORIENTATION_VERTICAL);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_vertical_split_pane_count_preserved:
 * Changing orientation must not alter the pane count.
 */
static void
test_vertical_split_pane_count_preserved (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    g_assert_cmpuint (pane_count (w), ==, 2);

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, TRUE);
    g_assert_cmpuint (pane_count (w), ==, 2);

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, FALSE);
    g_assert_cmpuint (pane_count (w), ==, 2);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_vertical_split_off_then_split_view_off:
 * Disabling split view after a vertical-split session must leave a
 * single pane with no dangling wrappers.
 */
static void
test_vertical_split_off_then_split_view_off (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, TRUE);

    nemo_window_split_view_off (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_cmpuint (pane_count (w), ==, 1);
    g_assert_null (w->details->primary_pane_content_paned);
    g_assert_null (w->details->secondary_pane_content_paned);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* GROUP 3: Separate sidebar per pane                                   */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/*
 * test_separate_sidebar_creates_sidebar2:
 * Enabling separate-sidebar in vertical+split mode must create sidebar2.
 */
static void
test_separate_sidebar_creates_sidebar2 (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    g_assert_nonnull (w->details->sidebar2);
    g_assert_nonnull (w->details->secondary_pane_content_paned);
    g_assert_nonnull (w->details->primary_pane_content_paned);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_disabled_removes_sidebar2:
 * Disabling separate-sidebar must remove sidebar2 cleanly.
 */
static void
test_separate_sidebar_disabled_removes_sidebar2 (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    g_assert_nonnull (w->details->sidebar2);

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, FALSE);

    g_assert_null (w->details->sidebar2);
    g_assert_null (w->details->secondary_pane_content_paned);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_pane2_inside_sec_paned:
 * When sidebar2 is active, pane2 must be a descendant of
 * secondary_pane_content_paned.
 */
static void
test_separate_sidebar_pane2_inside_sec_paned (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane2;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    pane2 = get_pane2 (w);
    g_assert_nonnull (pane2);
    g_assert_true (widget_is_inside (GTK_WIDGET (pane2),
                                     w->details->secondary_pane_content_paned));

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_pane1_inside_pri_paned:
 * When pane1 wrapper is active, pane1 must be inside
 * primary_pane_content_paned.
 */
static void
test_separate_sidebar_pane1_inside_pri_paned (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    pane1 = w->details->panes->data;
    g_assert_true (widget_is_inside (GTK_WIDGET (pane1),
                                     w->details->primary_pane_content_paned));

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_not_active_without_vertical_split:
 * sidebar2 must NOT be created if vertical-split is off, even if
 * separate-sidebar is on, because per-pane mode requires vertical.
 */
static void
test_separate_sidebar_not_active_without_vertical_split (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    /* horizontal split + separate-sidebar: should NOT create sidebar2 */
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);
    /* vertical is still FALSE */

    g_assert_null (w->details->sidebar2);
    g_assert_null (w->details->secondary_pane_content_paned);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_sidebar_hide_show_cycle:
 * Hide and show sidebar while separate-sidebar is active; no crash and
 * sidebar2 comes back.
 */
static void
test_separate_sidebar_hide_show_cycle (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    g_assert_nonnull (w->details->sidebar2);

    nemo_window_hide_sidebar (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_null (w->details->sidebar);
    g_assert_null (w->details->sidebar2);
    g_assert_null (w->details->primary_pane_content_paned);
    g_assert_null (w->details->secondary_pane_content_paned);

    nemo_window_show_sidebar (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_nonnull (w->details->sidebar);
    g_assert_nonnull (w->details->sidebar2);
    g_assert_nonnull (w->details->primary_pane_content_paned);
    g_assert_nonnull (w->details->secondary_pane_content_paned);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_split_view_off_cleans_up:
 * Turning split view off while separate-sidebar is active must leave no
 * orphaned wrapper pointers.
 */
static void
test_separate_sidebar_split_view_off_cleans_up (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    nemo_window_split_view_off (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_cmpuint (pane_count (w), ==, 1);
    g_assert_null (w->details->sidebar2);
    g_assert_null (w->details->primary_pane_content_paned);
    g_assert_null (w->details->secondary_pane_content_paned);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_type_change_places_to_tree:
 * CRASH REGRESSION: Changing sidebar type from Places→Tree while
 * separate-sidebar mode is active must not crash.
 * (The bug was pri_paned still occupying split_hpane child1 during teardown.)
 */
static void
test_separate_sidebar_type_change_places_to_tree (void)
{
    NemoWindow *w = create_test_window ();

    /* Ensure we start with places sidebar */
    nemo_window_set_sidebar_id (w, NEMO_WINDOW_SIDEBAR_PLACES);
    while (g_main_context_iteration (NULL, FALSE)) ;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    g_assert_nonnull (w->details->sidebar2);

    /* This used to crash with pane->slots == NULL assertion */
    nemo_window_set_sidebar_id (w, NEMO_WINDOW_SIDEBAR_TREE);
    while (g_main_context_iteration (NULL, FALSE)) ;

    /* Wrappers should be rebuilt correctly after the type change */
    g_assert_nonnull (w->details->sidebar);
    g_assert_nonnull (w->details->sidebar2);
    g_assert_nonnull (w->details->primary_pane_content_paned);
    g_assert_nonnull (w->details->secondary_pane_content_paned);
    g_assert_cmpuint (pane_count (w), ==, 2);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_type_change_tree_to_places:
 * Same crash regression in the other direction.
 */
static void
test_separate_sidebar_type_change_tree_to_places (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_set_sidebar_id (w, NEMO_WINDOW_SIDEBAR_TREE);
    while (g_main_context_iteration (NULL, FALSE)) ;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    nemo_window_set_sidebar_id (w, NEMO_WINDOW_SIDEBAR_PLACES);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_nonnull (w->details->sidebar);
    g_assert_nonnull (w->details->sidebar2);
    g_assert_cmpuint (pane_count (w), ==, 2);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_type_change_multiple_cycles:
 * Multiple sidebar-type changes in a row must not accumulate damage.
 */
static void
test_separate_sidebar_type_change_multiple_cycles (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    for (int i = 0; i < 4; i++) {
        const gchar *id = (i % 2 == 0) ? NEMO_WINDOW_SIDEBAR_TREE
                                        : NEMO_WINDOW_SIDEBAR_PLACES;
        nemo_window_set_sidebar_id (w, id);
        while (g_main_context_iteration (NULL, FALSE)) ;

        g_assert_nonnull (w->details->sidebar);
        g_assert_nonnull (w->details->sidebar2);
        g_assert_cmpuint (pane_count (w), ==, 2);
    }

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_sidebar_window_destroy:
 * CRASH REGRESSION: Destroying the window while separate-sidebar is
 * active must not crash (nemo_window_destroy lacked teardown calls).
 */
static void
test_separate_sidebar_window_destroy (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    g_assert_nonnull (w->details->sidebar2);

    /* Must not crash */
    gtk_widget_destroy (GTK_WIDGET (w));
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* GROUP 4: Separate nav bar per pane                                   */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/*
 * test_separate_nav_bar_embeds_toolbars:
 * With separate-nav-bar + vertical + split, both pane toolbars must be
 * inside their respective pane widgets (not in toolbar_holder).
 */
static void
test_separate_nav_bar_embeds_toolbars (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1, *pane2;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,  TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    pane1 = w->details->panes->data;
    pane2 = get_pane2 (w);
    g_assert_nonnull (pane2);

    /* Toolbars must be inside the pane, not toolbar_holder */
    g_assert_true (widget_is_inside (pane1->tool_bar, GTK_WIDGET (pane1)));
    g_assert_true (widget_is_inside (pane2->tool_bar, GTK_WIDGET (pane2)));

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_nav_bar_detaches_on_disable:
 * Disabling separate-nav-bar must move toolbars back to toolbar_holder.
 */
static void
test_separate_nav_bar_detaches_on_disable (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1, *pane2;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,  TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    pane1 = w->details->panes->data;
    pane2 = get_pane2 (w);

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, FALSE);

    g_assert_true (
        gtk_widget_get_parent (pane1->tool_bar) == w->details->toolbar_holder);
    g_assert_true (
        gtk_widget_get_parent (pane2->tool_bar) == w->details->toolbar_holder);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_nav_bar_not_active_without_vertical_split:
 * Toolbars must remain in toolbar_holder when vertical-split is off,
 * even if separate-nav-bar is on.
 */
static void
test_separate_nav_bar_not_active_without_vertical_split (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1, *pane2;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);
    /* vertical is still FALSE */

    pane1 = w->details->panes->data;
    pane2 = get_pane2 (w);

    g_assert_true (
        gtk_widget_get_parent (pane1->tool_bar) == w->details->toolbar_holder);
    g_assert_true (
        gtk_widget_get_parent (pane2->tool_bar) == w->details->toolbar_holder);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_nav_bar_split_view_off_restores_toolbar:
 * After split-view-off with separate-nav-bar active, remaining pane's
 * toolbar must be back in toolbar_holder.
 */
static void
test_separate_nav_bar_split_view_off_restores_toolbar (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,  TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    nemo_window_split_view_off (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    pane1 = w->details->panes->data;
    g_assert_true (
        gtk_widget_get_parent (pane1->tool_bar) == w->details->toolbar_holder);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_separate_nav_bar_window_destroy:
 * Destroying the window while separate-nav-bar is active must not crash.
 */
static void
test_separate_nav_bar_window_destroy (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,  TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    /* Must not crash */
    gtk_widget_destroy (GTK_WIDGET (w));
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* GROUP 5: All three new prefs on simultaneously                       */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/*
 * test_all_prefs_on_structure:
 * With all three new prefs on, verify full expected structure:
 * - split_hpane has two children
 * - child1 = pri_paned (sidebar1 + pane1 inside)
 * - child2 = sec_paned (sidebar2 + pane2 inside)
 * - pane1 toolbar is embedded in pane1
 * - pane2 toolbar is embedded in pane2
 */
static void
test_all_prefs_on_structure (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1, *pane2;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    pane1 = w->details->panes->data;
    pane2 = get_pane2 (w);
    g_assert_nonnull (pane2);

    /* Pane count intact */
    g_assert_cmpuint (pane_count (w), ==, 2);

    /* Wrapper pointers set */
    g_assert_nonnull (w->details->primary_pane_content_paned);
    g_assert_nonnull (w->details->secondary_pane_content_paned);
    g_assert_nonnull (w->details->sidebar2);

    /* Panes inside their wrappers */
    g_assert_true (widget_is_inside (GTK_WIDGET (pane1),
                                     w->details->primary_pane_content_paned));
    g_assert_true (widget_is_inside (GTK_WIDGET (pane2),
                                     w->details->secondary_pane_content_paned));

    /* Toolbars embedded */
    g_assert_true (widget_is_inside (pane1->tool_bar, GTK_WIDGET (pane1)));
    g_assert_true (widget_is_inside (pane2->tool_bar, GTK_WIDGET (pane2)));

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_all_prefs_on_then_split_view_off:
 * Full teardown with all prefs on should leave exactly 1 pane and no
 * dangling pointers.
 */
static void
test_all_prefs_on_then_split_view_off (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    nemo_window_split_view_off (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_cmpuint (pane_count (w), ==, 1);
    g_assert_null (w->details->sidebar2);
    g_assert_null (w->details->primary_pane_content_paned);
    g_assert_null (w->details->secondary_pane_content_paned);

    NemoWindowPane *pane1 = w->details->panes->data;
    g_assert_true (
        gtk_widget_get_parent (pane1->tool_bar) == w->details->toolbar_holder);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_all_prefs_on_sidebar_type_change:
 * Changing sidebar type with all prefs on must not crash and must
 * rebuild both sidebars.
 */
static void
test_all_prefs_on_sidebar_type_change (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_set_sidebar_id (w, NEMO_WINDOW_SIDEBAR_PLACES);
    while (g_main_context_iteration (NULL, FALSE)) ;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    nemo_window_set_sidebar_id (w, NEMO_WINDOW_SIDEBAR_TREE);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_nonnull (w->details->sidebar);
    g_assert_nonnull (w->details->sidebar2);
    g_assert_cmpuint (pane_count (w), ==, 2);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_all_prefs_on_hide_show_sidebar:
 * Hide/show sidebar with all prefs on must rebuild the full per-pane
 * layout after show.
 */
static void
test_all_prefs_on_hide_show_sidebar (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    nemo_window_hide_sidebar (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_null (w->details->sidebar);
    g_assert_null (w->details->sidebar2);
    g_assert_null (w->details->primary_pane_content_paned);
    g_assert_null (w->details->secondary_pane_content_paned);

    nemo_window_show_sidebar (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_nonnull (w->details->sidebar);
    g_assert_nonnull (w->details->sidebar2);
    g_assert_nonnull (w->details->primary_pane_content_paned);
    g_assert_nonnull (w->details->secondary_pane_content_paned);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_all_prefs_on_window_destroy:
 * Window destroy with all prefs on must not crash.
 */
static void
test_all_prefs_on_window_destroy (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    /* Must not crash */
    gtk_widget_destroy (GTK_WIDGET (w));
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* GROUP 6: Active pane tracking                                        */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/*
 * test_active_pane_pointer_valid_after_split_on:
 * After split-view-on, the active pane pointer must still be valid.
 */
static void
test_active_pane_pointer_valid_after_split_on (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1 = w->details->panes->data;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    /* active pane should be set to something valid */
    g_assert_nonnull (w->details->active_pane);
    g_assert_true (g_list_find (w->details->panes, w->details->active_pane) != NULL);

    gtk_widget_destroy (GTK_WIDGET (w));
    (void) pane1;
}

/*
 * test_active_pane_pointer_valid_after_orientation_change:
 * Changing orientation must not invalidate the active pane pointer.
 */
static void
test_active_pane_pointer_valid_after_orientation_change (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    NemoWindowPane *active_before = w->details->active_pane;

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, TRUE);

    g_assert_true (w->details->active_pane == active_before);
    g_assert_true (g_list_find (w->details->panes, w->details->active_pane) != NULL);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_active_pane_set_to_pane2:
 * Setting active pane to pane2 and then doing an orientation change
 * must leave pane2 as the active pane.
 */
static void
test_active_pane_set_to_pane2 (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    NemoWindowPane *pane2 = get_pane2 (w);
    nemo_window_set_active_pane (w, pane2);
    while (g_main_context_iteration (NULL, FALSE)) ;

    g_assert_true (w->details->active_pane == pane2);

    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT, TRUE);

    /* pane2 still the active pane after orientation change */
    g_assert_true (w->details->active_pane == pane2);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_active_slot_per_pane_independent:
 * Each pane's active_slot must be non-NULL and independent objects.
 */
static void
test_active_slot_per_pane_independent (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;

    NemoWindowPane *pane1 = w->details->panes->data;
    NemoWindowPane *pane2 = get_pane2 (w);

    g_assert_nonnull (pane1->active_slot);
    g_assert_nonnull (pane2->active_slot);
    g_assert_true (pane1->active_slot != pane2->active_slot);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* GROUP 7: Pref idempotency                                            */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/*
 * test_pref_idempotent_separate_sidebar:
 * Setting separate-sidebar to the same value twice must not change
 * structure or crash.
 */
static void
test_pref_idempotent_separate_sidebar (void)
{
    NemoWindow *w = create_test_window ();

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    GtkWidget *sec_paned_first = w->details->secondary_pane_content_paned;
    g_assert_nonnull (sec_paned_first);

    /* Setting to TRUE again should be a no-op */
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, TRUE);

    g_assert_true (w->details->secondary_pane_content_paned == sec_paned_first);
    g_assert_cmpuint (pane_count (w), ==, 2);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/*
 * test_pref_idempotent_separate_nav_bar:
 * Setting separate-nav-bar to the same value twice must not crash or
 * double-embed toolbars.
 */
static void
test_pref_idempotent_separate_nav_bar (void)
{
    NemoWindow *w = create_test_window ();
    NemoWindowPane *pane1, *pane2;

    nemo_window_split_view_on (w);
    while (g_main_context_iteration (NULL, FALSE)) ;
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,  TRUE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    pane1 = w->details->panes->data;
    pane2 = get_pane2 (w);

    /* Set again — embed_toolbar guard must prevent double-parenting */
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, TRUE);

    g_assert_true (widget_is_inside (pane1->tool_bar, GTK_WIDGET (pane1)));
    g_assert_true (widget_is_inside (pane2->tool_bar, GTK_WIDGET (pane2)));
    g_assert_cmpuint (pane_count (w), ==, 2);

    gtk_widget_destroy (GTK_WIDGET (w));
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    gtk_init (&argc, &argv);
    nemo_global_preferences_init ();

    /* Create and register the application singleton that NemoWindow requires.
     * G_APPLICATION_NON_UNIQUE prevents D-Bus primary-instance negotiation
     * so tests work without a session bus and run safely in parallel.
     * g_application_register() must be called before gtk_window_set_application
     * (called from nemo_window_constructed) will accept the window. */
    test_app = nemo_application_initialize_singleton (
                   NEMO_TYPE_MAIN_APPLICATION,
                   "application-id", "org.Nemo.Test",
                   "flags", G_APPLICATION_NON_UNIQUE,
                   NULL);
    /* g_application_register emits the startup signal, which calls
     * continue_startup, which registers all file views in the factory.
     * Do NOT register views manually after this - factory asserts on duplicates. */
    g_application_register (G_APPLICATION (test_app), NULL, NULL);

    suite_setup ();

    /* GROUP 1: Existing split-view behaviour */
    g_test_add_func ("/dual-pane/split-view/on-off",
                     test_split_view_on_off);
    g_test_add_func ("/dual-pane/split-view/active-pane-survives",
                     test_split_view_active_pane_survives);
    g_test_add_func ("/dual-pane/split-view/secondary-active-survives",
                     test_split_view_secondary_pane_active_survives);
    g_test_add_func ("/dual-pane/split-view/toolbar-returns-to-holder",
                     test_split_view_toolbar_returns_to_holder);
    g_test_add_func ("/dual-pane/split-view/sidebar-hide-show-no-split",
                     test_sidebar_hide_show_no_split);
    g_test_add_func ("/dual-pane/split-view/sidebar-hide-show-with-split-prefs-off",
                     test_sidebar_hide_show_with_split_prefs_off);

    /* GROUP 2: Vertical split orientation */
    g_test_add_func ("/dual-pane/vertical-split/orientation-changes",
                     test_vertical_split_orientation);
    g_test_add_func ("/dual-pane/vertical-split/orientation-toggle",
                     test_vertical_split_orientation_toggle);
    g_test_add_func ("/dual-pane/vertical-split/pane-count-preserved",
                     test_vertical_split_pane_count_preserved);
    g_test_add_func ("/dual-pane/vertical-split/split-view-off-cleanup",
                     test_vertical_split_off_then_split_view_off);

    /* GROUP 3: Separate sidebar per pane */
    g_test_add_func ("/dual-pane/separate-sidebar/creates-sidebar2",
                     test_separate_sidebar_creates_sidebar2);
    g_test_add_func ("/dual-pane/separate-sidebar/disable-removes-sidebar2",
                     test_separate_sidebar_disabled_removes_sidebar2);
    g_test_add_func ("/dual-pane/separate-sidebar/pane2-inside-sec-paned",
                     test_separate_sidebar_pane2_inside_sec_paned);
    g_test_add_func ("/dual-pane/separate-sidebar/pane1-inside-pri-paned",
                     test_separate_sidebar_pane1_inside_pri_paned);
    g_test_add_func ("/dual-pane/separate-sidebar/not-without-vertical-split",
                     test_separate_sidebar_not_active_without_vertical_split);
    g_test_add_func ("/dual-pane/separate-sidebar/hide-show-sidebar",
                     test_separate_sidebar_hide_show_cycle);
    g_test_add_func ("/dual-pane/separate-sidebar/split-view-off-cleanup",
                     test_separate_sidebar_split_view_off_cleans_up);
    g_test_add_func ("/dual-pane/separate-sidebar/sidebar-type-places-to-tree",
                     test_separate_sidebar_type_change_places_to_tree);
    g_test_add_func ("/dual-pane/separate-sidebar/sidebar-type-tree-to-places",
                     test_separate_sidebar_type_change_tree_to_places);
    g_test_add_func ("/dual-pane/separate-sidebar/sidebar-type-multiple-cycles",
                     test_separate_sidebar_type_change_multiple_cycles);
    g_test_add_func ("/dual-pane/separate-sidebar/window-destroy",
                     test_separate_sidebar_window_destroy);

    /* GROUP 4: Separate nav bar per pane */
    g_test_add_func ("/dual-pane/separate-nav-bar/embeds-toolbars",
                     test_separate_nav_bar_embeds_toolbars);
    g_test_add_func ("/dual-pane/separate-nav-bar/detaches-on-disable",
                     test_separate_nav_bar_detaches_on_disable);
    g_test_add_func ("/dual-pane/separate-nav-bar/not-without-vertical-split",
                     test_separate_nav_bar_not_active_without_vertical_split);
    g_test_add_func ("/dual-pane/separate-nav-bar/split-view-off-restores-toolbar",
                     test_separate_nav_bar_split_view_off_restores_toolbar);
    g_test_add_func ("/dual-pane/separate-nav-bar/window-destroy",
                     test_separate_nav_bar_window_destroy);

    /* GROUP 5: All three prefs on simultaneously */
    g_test_add_func ("/dual-pane/all-prefs-on/structure",
                     test_all_prefs_on_structure);
    g_test_add_func ("/dual-pane/all-prefs-on/split-view-off",
                     test_all_prefs_on_then_split_view_off);
    g_test_add_func ("/dual-pane/all-prefs-on/sidebar-type-change",
                     test_all_prefs_on_sidebar_type_change);
    g_test_add_func ("/dual-pane/all-prefs-on/hide-show-sidebar",
                     test_all_prefs_on_hide_show_sidebar);
    g_test_add_func ("/dual-pane/all-prefs-on/window-destroy",
                     test_all_prefs_on_window_destroy);

    /* GROUP 6: Active pane tracking */
    g_test_add_func ("/dual-pane/active-pane/valid-after-split-on",
                     test_active_pane_pointer_valid_after_split_on);
    g_test_add_func ("/dual-pane/active-pane/valid-after-orientation-change",
                     test_active_pane_pointer_valid_after_orientation_change);
    g_test_add_func ("/dual-pane/active-pane/set-to-pane2-survives-orientation",
                     test_active_pane_set_to_pane2);
    g_test_add_func ("/dual-pane/active-pane/slots-are-independent",
                     test_active_slot_per_pane_independent);

    /* GROUP 7: Pref idempotency */
    g_test_add_func ("/dual-pane/idempotency/separate-sidebar",
                     test_pref_idempotent_separate_sidebar);
    g_test_add_func ("/dual-pane/idempotency/separate-nav-bar",
                     test_pref_idempotent_separate_nav_bar);

    int result = g_test_run ();

    /* Reset prefs to defaults before exit */
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,   FALSE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR, FALSE);
    set_pref_bool (NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR, FALSE);

    return result;
}
