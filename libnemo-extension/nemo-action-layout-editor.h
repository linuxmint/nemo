/* nemo-action-layout-editor.h */

/*  A widget that allows organizing nemo actions into a tree structure
 *  with submenus and separators, and assigning keyboard shortcuts.
 */

#ifndef __NEMO_ACTION_LAYOUT_EDITOR_H__
#define __NEMO_ACTION_LAYOUT_EDITOR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_ACTION_LAYOUT_EDITOR (nemo_action_layout_editor_get_type())

G_DECLARE_FINAL_TYPE (NemoActionLayoutEditor, nemo_action_layout_editor, NEMO, ACTION_LAYOUT_EDITOR, GtkBox)

GtkWidget *nemo_action_layout_editor_new (void);

G_END_DECLS

#endif /* __NEMO_ACTION_LAYOUT_EDITOR_H__ */
