#!/usr/bin/python3
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('XApp', '1.0')
from gi.repository import Gtk, Gdk, GLib, Gio, XApp, GdkPixbuf, Pango
import cairo
import json
import os
from pathlib import Path
import uuid
import gettext
import subprocess

import leconfig

#FIXME build config
gettext.install(leconfig.PACKAGE, leconfig.LOCALE_DIR)

JSON_FILE = Path(GLib.get_user_config_dir()).joinpath("nemo/actions-tree.json")
USER_ACTIONS_DIR = Path(GLib.get_user_data_dir()).joinpath("nemo/actions")
GLADE_FILE = Path(leconfig.PKG_DATADIR).joinpath("layout-editor/nemo-action-layout-editor.glade")

NON_SPICE_UUID_SUFFIX = "@untracked"

ROW_HASH, ROW_UUID, ROW_TYPE, ROW_POSITION, ROW_OBJ = range(5)

ROW_TYPE_ACTION = "action"
ROW_TYPE_SUBMENU = "submenu"
ROW_TYPE_SEPARATOR = "separator"

def new_hash():
    return uuid.uuid4().hex

class Row():
    def __init__(self, row_meta=None, keyfile=None, path=None, enabled=True):
        self.keyfile = keyfile
        self.row_meta = row_meta
        self.enabled = enabled
        self.path = path  # PosixPath

    def get_icon_string(self, original=False):
        icon_string = None

        if self.row_meta and not original:
            user_assigned_name = self.row_meta.get('user-icon', None)
            if user_assigned_name is not None:
                icon_string = user_assigned_name

        if icon_string is None:
            if self.keyfile is not None:
                try:
                    icon_string = self.keyfile.get_string('Nemo Action', 'Icon-Name')
                except GLib.Error:
                    pass

        return icon_string

    def get_path(self):
        return self.path

    def get_icon_type_and_data(self, original=False):
        icon_string = self.get_icon_string(original)

        if icon_string is None:
            return None

        if icon_string.startswith("/"):
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(icon_string, 16, 16)
            surface = Gdk.cairo_surface_create_from_pixbuf(pixbuf, self.main_window.get_scale_factor(), None)
            return ("surface", surface)

        return ("icon-name", icon_string)

    def get_label(self):
        if self.row_meta is not None:
            if self.row_meta.get("type") == ROW_TYPE_SEPARATOR:
                return "──────────────────────────────"

        label = None

        if self.row_meta is not None:
            user_assigned_label = self.row_meta.get('user-label', None)
            if user_assigned_label is not None:
                label = user_assigned_label

        if label is None:
            if self.keyfile is not None:
                try:
                    label = self.keyfile.get_string('Nemo Action', 'Name').replace("_", "")
                except GLib.Error as e:
                    print(e)
                    pass

        if label is None:
            return _("Unknown")

        return label

    def set_custom_label(self, label):
        if not self.row_meta:
            self.row_meta = {}

        self.row_meta['user-label'] = label

    def set_custom_icon(self, icon):
        if not self.row_meta:
            self.row_meta = {}

        self.row_meta['user-icon'] = icon

    def get_custom_label(self):
        if self.row_meta:
            return self.row_meta.get('user-label')
        return None

    def get_custom_icon(self):
        if self.row_meta:
            return self.row_meta.get('user-icon')
        return None

class NemoActionsOrganizer(Gtk.Box):
    def __init__(self, window, builder=None):
        Gtk.Box.__init__(self, orientation=Gtk.Orientation.VERTICAL)

        if builder is None:
            self.builder = Gtk.Builder.new_from_file(str(GLADE_FILE))
        else:
            self.builder = builder

        self.main_window = window
        self.layout_editor_box = self.builder.get_object("layout_editor_box")
        self.add(self.layout_editor_box)
        self.treeview_holder = self.builder.get_object("treeview_holder")
        self.save_button = self.builder.get_object("save_button")
        self.discard_changes_button = self.builder.get_object("discard_changes_button")
        self.default_layout_button = self.builder.get_object("default_layout_button")
        self.name_entry = self.builder.get_object("name_entry")
        self.new_row_button = self.builder.get_object("new_row_button")
        self.remove_submenu_button = self.builder.get_object("remove_submenu_button")
        self.clear_icon_button = self.builder.get_object("clear_icon_button")
        self.icon_selector_menu_button = self.builder.get_object("icon_selector_menu_button")
        self.icon_selector_image = self.builder.get_object("icon_selector_image")
        self.action_enabled_switch = self.builder.get_object("action_enabled_switch")

        self.nemo_plugin_settings = Gio.Settings(schema_id="org.nemo.plugins")

        # Icon MenuButton
        menu = Gtk.Menu()

        self.blank_icon_menu_item = Gtk.ImageMenuItem(label=_("No icon"), image=Gtk.Image(icon_name="checkbox-symbolic"))
        self.blank_icon_menu_item.connect("activate", self.on_clear_icon_clicked)
        menu.add(self.blank_icon_menu_item)

        self.original_icon_menu_image = Gtk.Image()
        self.original_icon_menu_item = Gtk.ImageMenuItem(label=_("Use the original icon (if there is one)"), image=self.original_icon_menu_image)
        self.original_icon_menu_item.connect("activate", self.on_original_icon_clicked)
        menu.add(self.original_icon_menu_item)

        item = Gtk.MenuItem(label=_("Choose..."))
        item.connect("activate", self.on_choose_icon_clicked)
        menu.add(item)

        menu.show_all()
        self.icon_selector_menu_button.set_popup(menu)

        # New row MenuButton

        menu = Gtk.Menu()

        item = Gtk.ImageMenuItem(label=_("New submenu"), image=Gtk.Image(icon_name="pan-end-symbolic"))
        item.connect("activate", self.on_new_submenu_clicked)
        menu.add(item)

        item = Gtk.ImageMenuItem(label=_("New separator"), image=Gtk.Image(icon_name="list-remove-symbolic"))
        item.connect("activate", self.on_new_separator_clicked)
        menu.add(item)

        menu.show_all()
        self.new_row_button.set_popup(menu)

        # Tree/model

        self.model = Gtk.TreeStore(str, str, str, int, object)  # (hash, uuid, type, position, Row)

        self.treeview = Gtk.TreeView(
            model=self.model,
            enable_tree_lines=True,
            headers_visible=False,
            visible=True
        )

        column = Gtk.TreeViewColumn()
        self.treeview.append_column(column)

        cell = Gtk.CellRendererPixbuf()
        column.pack_start(cell, False)
        column.set_cell_data_func(cell, self.menu_icon_render_func)
        cell = Gtk.CellRendererText()
        column.pack_start(cell, False)
        column.set_cell_data_func(cell, self.menu_label_render_func)

        self.treeview_holder.add(self.treeview)

        self.save_button.connect("clicked", self.on_save_clicked)
        self.discard_changes_button.connect("clicked", self.on_discard_changes_clicked)
        self.default_layout_button.connect("clicked", self.on_default_layout_clicked)
        self.treeview.get_selection().connect("changed", self.on_treeview_position_changed)
        self.name_entry.connect("changed", self.on_name_entry_changed)
        self.name_entry.connect("icon-press", self.on_name_entry_icon_clicked)
        self.remove_submenu_button.connect("clicked", self.on_remove_submenu_clicked)
        self.action_enabled_switch.connect("notify::active", self.on_action_enabled_switch_notify)
        self.treeview.connect("row-activated", self.on_row_activated)

        # DND
        self.treeview.drag_source_set(
            Gdk.ModifierType.BUTTON1_MASK,
            None,
            Gdk.DragAction.MOVE,
        )
        self.treeview.drag_dest_set(
            Gtk.DestDefaults.ALL,
            None,
            Gdk.DragAction.MOVE,
        )

        self.treeview.drag_source_add_text_targets()
        self.treeview.drag_dest_add_text_targets()
        self.treeview.connect("drag-begin", self.on_drag_begin)
        self.treeview.connect("drag-end", self.on_drag_begin)
        self.treeview.connect("drag-motion", self.on_drag_motion)
        self.treeview.connect("drag-data-get", self.on_drag_data_get)
        self.treeview.connect("drag-data-received", self.on_drag_data_received)

        self.reloading_model = False
        self.updating_row_edit_fields = False
        self.dnd_autoscroll_timeout_id = 0

        self.needs_saved = False
        self.reload_model()

        self.update_treeview_state()
        self.set_needs_saved(False)

    def reload_model(self, flat=False):
        self.reloading_model = True
        self.model.clear()

        if flat:
            self.data = {
                'toplevel': []
            }
        else:
            try:
                with open(JSON_FILE, 'r') as file:
                    try:
                        self.data = json.load(file)
                    except json.decoder.JSONDecodeError as e:
                        print("Could not process json file: %s" % e)
                        raise

                    try:
                        self.validate_tree(self.data)
                    except (ValueError, KeyError) as e:
                        print("Schema validation failed, ignoring saved layout: %s" % e)
                        raise
            except (FileNotFoundError, ValueError, KeyError, json.decoder.JSONDecodeError) as e:
                self.data = {
                    'toplevel': []
                }

        installed_actions = self.load_installed_actions()
        self.fill_model(self.model, None, self.data['toplevel'], installed_actions)

        start_path = Gtk.TreePath.new_first()
        self.treeview.get_selection().select_path(start_path)
        self.treeview.scroll_to_cell(start_path, None, True, 0, 0)
        self.update_row_controls()

        self.reloading_model = False

    def save_model(self):
        # Save the modified model back to the JSON file
        self.data["toplevel"] = self.serialize_model(None, self.model)

        with open(JSON_FILE, 'w') as file:
            json.dump(self.data, file, indent=2)

    def validate_tree(self, data):
        # Iterate thru every node in the json tree and validate it
        for node in data['toplevel']:
            self.validate_node(node)

    def validate_node(self, node):
        # Check that the node has a valid type
        keys = node.keys()
        if not ("uuid" in keys and "type" in keys):
            raise KeyError("Missing required keys: uuid, type")

        # Mandatory keys

        # Check that the node has a valid UUID
        uuid = node['uuid']
        if (not isinstance(uuid, str)) or uuid in (None, ""):
            raise ValueError("Invalid or empty UUID '%s' (must not be a non-null, non-empty string)" % str(uuid))

        # Check that the node has a valid type
        type = node['type']
        if (not isinstance(type, str)) or type not in (ROW_TYPE_ACTION, ROW_TYPE_SUBMENU, ROW_TYPE_SEPARATOR):
            raise ValueError("%s: Invalid type '%s' (must be a string, either 'action' or 'submenu')" % (uuid, str(node['type'])))

        # Optional keys

        # Check that the node has a valid label
        try:
            label = node['user-label']
            if (label is not None and (not isinstance(label, str))) or label == "":
                raise ValueError("%s: Invalid label '%s' (must be null or a non-zero-length string)" % (uuid, str(label)))
        except KeyError:
            # not mandatory
            pass

        # Check that the node has a valid icon
        try:
            icon = node['user-icon']
            if icon is not None and (not isinstance(icon, str)):
                raise ValueError("%s: Invalid icon '%s' (must be an any-length string or null)" % (uuid, icon))
        except KeyError:
            # not mandatory
            pass

        # Check that the node has a valid children list
        try:
            children = node['children']
            if node["type"] in (ROW_TYPE_ACTION, ROW_TYPE_SEPARATOR):
                print("%s: Action or separator node has children, ignoring them" % uuid)
            else:
                if not isinstance(children, list):
                    raise ValueError("%s: Invalid 'children' (must be a list)")

            # Check that the node's children are valid
            for child in children:
                self.validate_node(child)
        except KeyError:
            # not mandatory
            pass

    def load_installed_actions(self):
        # Load installed actions from the system
        actions = {}

        data_dirs = GLib.get_system_data_dirs() + [GLib.get_user_data_dir()]

        for data_dir in data_dirs:
            actions_dir = Path(data_dir).joinpath("nemo/actions")
            if actions_dir.is_dir():
                for path in actions_dir.iterdir():
                    file = Path(path)
                    if file.suffix == ".nemo_action":
                        uuid = file.name

                        try:
                            kf = GLib.KeyFile()
                            kf.load_from_file(str(file), GLib.KeyFileFlags.NONE)

                            actions[uuid] = (file, kf)
                        except GLib.Error as e:
                            print("Error loading action file '%s': %s" % (action_file, e.message))
                            continue

        return actions

    def fill_model(self, model, parent, items, installed_actions):
        disabled_actions = self.nemo_plugin_settings.get_strv("disabled-actions")

        for item in items:
            row_type = item.get("type")
            uuid = item.get('uuid')
            position = item.get('position')

            if row_type == ROW_TYPE_ACTION:
                try:
                    kf = installed_actions[uuid][1]  # (path, kf) tuple
                    path = Path(installed_actions[uuid][0])
                except KeyError:
                    print("Ignoring missing installed action %s" % uuid)
                    continue

                iter = model.append(parent, [new_hash(), uuid, row_type, position, Row(item, kf, path, path.name not in disabled_actions)])

                del installed_actions[uuid]
            elif row_type == ROW_TYPE_SEPARATOR:
                iter = model.append(parent, [new_hash(), "separator", ROW_TYPE_SEPARATOR, 0, Row(item, None, None, True)])
            else:
                iter = model.append(parent, [new_hash(), uuid, row_type, position, Row(item, None, None, True)])

                if 'children' in item:
                    self.fill_model(model, iter, item['children'], installed_actions)

        # Don't run the following code during recursion, only add untracked actions to the root node
        if parent is not None:
            return

        def push_disabled(key):
            path, kf = installed_actions[key]
            return path.name in disabled_actions

        sorted_actions = {uuid: installed_actions[uuid] for uuid in sorted(installed_actions, key=push_disabled)}

        for uuid, (path, kf) in sorted_actions.items():
            enabled = path.name not in disabled_actions
            model.append(parent, [new_hash(), uuid, ROW_TYPE_ACTION, 0, Row(None, kf, path, enabled)])

    def save_disabled_list(self):
        disabled = []

        def get_disabled(model, path, iter, data=None):
            row = model.get_value(iter, ROW_OBJ)
            row_type = model.get_value(iter, ROW_TYPE)
            if row_type == ROW_TYPE_ACTION:
                if not row.enabled:
                    nonlocal disabled
                    disabled.append(row.get_path().name)

            return False

        self.model.foreach(get_disabled)

        self.nemo_plugin_settings.set_strv("disabled-actions", disabled)

    def serialize_model(self, parent, model):
        result = []

        iter = model.iter_children(parent)
        while iter:
            row_type = model.get_value(iter, ROW_TYPE)
            row = model.get_value(iter, ROW_OBJ)

            item = {
                'uuid': model.get_value(iter, ROW_UUID),
                'type': row_type,
                'position': model.get_value(iter, ROW_POSITION),
                'user-label': row.get_custom_label(),
                'user-icon': row.get_custom_icon()
            }

            if row_type == ROW_TYPE_SUBMENU:
                item['children'] = self.serialize_model(iter, model)

            result.append(item)
            iter = model.iter_next(iter)
        return result

    def flatten_model(self):
        self.reload_model(flat=True)
        self.update_treeview_state()
        self.set_needs_saved(True)

    def update_treeview_state(self):
        self.treeview.expand_all()

    def get_selected_row_path_iter(self):
        selection = self.treeview.get_selection()
        model, paths = selection.get_selected_rows()
        if paths:
            path = paths[0]
            iter = model.get_iter(path)
            return (path, iter)

        return (None, None)

    def get_selected_row_field(self, field):
        path, iter = self.get_selected_row_path_iter()
        return self.model.get_value(iter, field)

    def selected_row_changed(self, needs_saved=True):
        if self.reloading_model:
            return

        path, iter = self.get_selected_row_path_iter()
        if iter is not None:
            self.model.row_changed(path, iter)

        self.update_row_controls()

        if needs_saved:
            self.set_needs_saved(True)

    def on_treeview_position_changed(self, selection):
        if self.reloading_model:
            return

        self.update_row_controls()

    def update_row_controls(self):
        row = self.get_selected_row_field(ROW_OBJ)

        if row is not None:
            self.updating_row_edit_fields = True

            row_type = self.get_selected_row_field(ROW_TYPE)
            row_uuid = self.get_selected_row_field(ROW_UUID)

            self.name_entry.set_text(row.get_label())

            self.set_icon_button(row)
            self.original_icon_menu_item.set_visible(row_type == ROW_TYPE_ACTION)
            orig_icon = row.get_icon_string(original=True)
            self.original_icon_menu_item.set_sensitive(orig_icon is not None and orig_icon != row.get_icon_string())
            self.icon_selector_menu_button.set_sensitive(row.enabled and row_type != ROW_TYPE_SEPARATOR)
            self.name_entry.set_sensitive(row.enabled and row_type != ROW_TYPE_SEPARATOR)
            self.action_enabled_switch.set_active(row.enabled)
            self.action_enabled_switch.set_sensitive(row_type == ROW_TYPE_ACTION)
            self.remove_submenu_button.set_sensitive(row_type in (ROW_TYPE_SUBMENU, ROW_TYPE_SEPARATOR))

            if row_type == ROW_TYPE_ACTION and row.get_custom_label() is not None:
                self.name_entry.set_icon_from_icon_name(Gtk.EntryIconPosition.SECONDARY, "edit-delete-symbolic")
                self.name_entry.set_icon_sensitive(Gtk.EntryIconPosition.SECONDARY, True)
            else:
                self.name_entry.set_icon_from_icon_name(Gtk.EntryIconPosition.SECONDARY, None)
                self.name_entry.set_icon_sensitive(Gtk.EntryIconPosition.SECONDARY, False)

            self.updating_row_edit_fields = False

    def on_row_activated(self, path, column, data=None):
        row_type = self.get_selected_row_field(ROW_TYPE)
        if row_type != ROW_TYPE_ACTION:
            return

        self.action_enabled_switch.set_active(not self.action_enabled_switch.get_active())

    def set_icon_button(self, row):
        for image, use_orig in ([self.icon_selector_image, False], [self.original_icon_menu_image, True]):

            try:
                cur_attr, cur_name_or_surface = row.get_icon_type_and_data(original=use_orig)

                if cur_attr == "surface":
                    image.set_from_surface(cur_name_or_surface)
                else:
                    image.set_from_icon_name(cur_name_or_surface, Gtk.IconSize.BUTTON)
            except TypeError:
                image.props.icon_name = None
                image.props.surface = None

    def set_needs_saved(self, needs_saved):
        if needs_saved:
            self.save_button.set_sensitive(True)
            self.discard_changes_button.set_sensitive(True)
        else:
            self.save_button.set_sensitive(False)
            self.discard_changes_button.set_sensitive(False)

        self.needs_saved = needs_saved

    # Button signal handlers

    def on_save_clicked(self, button):
        self.save_model()
        self.save_disabled_list()
        self.set_needs_saved(False)

    def on_discard_changes_clicked(self, button):
        self.set_needs_saved(False)
        self.reload_model()
        self.update_treeview_state()

    def on_default_layout_clicked(self, button):
        self.flatten_model()

    def on_clear_icon_clicked(self, menuitem):
        row = self.get_selected_row_field(ROW_OBJ)
        if row is not None:
            row.set_custom_icon("")
            self.selected_row_changed()

    def on_original_icon_clicked(self, menuitem):
        row = self.get_selected_row_field(ROW_OBJ)
        if row is not None:
            row.set_custom_icon(None)
            self.selected_row_changed()

    def on_choose_icon_clicked(self, menuitem):
        chooser = XApp.IconChooserDialog()

        row = self.get_selected_row_field(ROW_OBJ)
        if row is not None:
            icon_name = row.get_icon_string()

            if icon_name is not None:
                response = chooser.run_with_icon(icon_name)
            else:
                response = chooser.run()

        if response == Gtk.ResponseType.OK:
            row.set_custom_icon(chooser.get_icon_string())
            self.selected_row_changed()

        chooser.hide()
        chooser.destroy()

    def on_new_submenu_clicked(self, menuitem):
        # Add on same level as current selection
        path, selection_iter = self.get_selected_row_path_iter()
        row_type = self.get_selected_row_field(ROW_TYPE)

        if row_type == ROW_TYPE_ACTION:
            parent = self.model.iter_parent(selection_iter)
        else:
            parent = selection_iter

        new_iter = self.model.insert_after(parent, selection_iter, [
            new_hash(),
            _("New submenu"),
            ROW_TYPE_SUBMENU,
            0,
            Row({"uuid": "New Submenu"}, None, None, True)])

        # new_path = self.model.get_path(new_iter)
        # self.treeview.scroll_to_cell(new_path, None, True, 0.5, 0.5)

        selection = self.treeview.get_selection()
        selection.select_iter(new_iter)

        self.selected_row_changed()
        self.name_entry.grab_focus()

    def on_new_separator_clicked(self, menuitem):
        # Add on same level as current selection
        path, selection_iter = self.get_selected_row_path_iter()
        row_type = self.get_selected_row_field(ROW_TYPE)

        if row_type == ROW_TYPE_ACTION:
            parent = self.model.iter_parent(selection_iter)
        else:
            parent = selection_iter

        new_iter = self.model.insert_after(parent, selection_iter, [
            new_hash(),
            "separator",
            ROW_TYPE_SEPARATOR,
            0,
            Row({"uuid": "separator", "type": "separator"}, None, None, True)])

        selection = self.treeview.get_selection()
        selection.select_iter(new_iter)

        self.selected_row_changed()

    def on_remove_submenu_clicked(self, button):
        path, selection_iter = self.get_selected_row_path_iter()
        row_type = self.model.get_value(selection_iter, ROW_TYPE)
        row_hash = self.model.get_value(selection_iter, ROW_HASH)

        if row_type == ROW_TYPE_ACTION:
            return

        if row_type == ROW_TYPE_SUBMENU:
            parent_iter = self.model.iter_parent(selection_iter)
            self.move_tree(self.model, selection_iter, parent_iter)

        self.remove_source_row_by_hash(self.model, row_hash)
        self.selected_row_changed()

    def on_name_entry_changed(self, entry):
        if self.updating_row_edit_fields:
            return

        row = self.get_selected_row_field(ROW_OBJ)
        if row is not None:
            row.set_custom_label(entry.get_text())

            # A submenu's UUID matches its label. Update it when the label is changed.
            row_type = self.get_selected_row_field(ROW_TYPE)
            if row_type == ROW_TYPE_SUBMENU:
                path, iter = self.get_selected_row_path_iter()
                if iter is not None:
                    self.model.set_value(iter, ROW_UUID, entry.get_text())

            self.selected_row_changed()

    def on_name_entry_icon_clicked(self, entry, icon_pos, event, data=None):
        if icon_pos != Gtk.EntryIconPosition.SECONDARY:
            return

        row = self.get_selected_row_field(ROW_OBJ)
        if row is not None:
            row.set_custom_label(None)
            self.selected_row_changed()

    def on_action_enabled_switch_notify(self, switch, pspec):
        if self.updating_row_edit_fields:
            return

        row = self.get_selected_row_field(ROW_OBJ)
        if row is not None:
            row.enabled = switch.get_active()
            # The layout file does not track active/inactive actions,
            # so this shouldn't prompt a layout save.
            self.selected_row_changed(needs_saved=False)

    # Cell render functions

    def menu_icon_render_func(self, column, cell, model, iter, data):
        row = model.get_value(iter, ROW_OBJ)

        try:
            attr, name_or_surface = row.get_icon_type_and_data()
            cell.set_property(attr, name_or_surface)
        except TypeError:
            cell.set_property("icon-name", None)
            cell.set_property("surface", None)

    def menu_label_render_func(self, column, cell, model, iter, data):
        row_type = model.get_value(iter, ROW_TYPE)
        row = model.get_value(iter, ROW_OBJ)

        if row_type == ROW_TYPE_SUBMENU:
            cell.set_property("markup", "<b>%s</b>" % row.get_label())
            cell.set_property("weight", Pango.Weight.BOLD)
        else:
            cell.set_property("markup", row.get_label())
            cell.set_property("weight", Pango.Weight.NORMAL if row.enabled else Pango.Weight.ULTRALIGHT)

    # DND

    def on_drag_begin(self, widget, context):
        source_path, source_iter = self.get_selected_row_path_iter()
        width = 0
        height = 0

        def gather_row_surfaces(current_root_iter, surfaces):
            foreach_iter = self.model.iter_children(current_root_iter)

            while foreach_iter is not None:
                foreach_path = self.model.get_path(foreach_iter)
                surface = self.treeview.create_row_drag_icon(foreach_path)
                row_surfaces.append(surface)
                nonlocal width
                nonlocal height
                width = max(width, surface.get_width())
                height += surface.get_height() - 1

                foreach_type = self.model.get_value(foreach_iter, ROW_TYPE)
                if foreach_type == ROW_TYPE_SUBMENU:
                    gather_row_surfaces(foreach_iter, surfaces)

                foreach_iter = self.model.iter_next(foreach_iter)

        source_row_surface = self.treeview.create_row_drag_icon(source_path)
        width = source_row_surface.get_width()
        height = source_row_surface.get_height() - 1
        row_surfaces = [source_row_surface]

        gather_row_surfaces(source_iter, row_surfaces)

        final_surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, width + 4, height + 4)
        scale = self.main_window.get_scale_factor()
        final_surface.set_device_scale(scale, scale)

        def sc(v):
            return v * scale

        def usc(v):
            return v / scale

        cr = cairo.Context(final_surface)

        y = 2
        first = True
        for s in row_surfaces:
            cr.save()
            cr.set_source_surface(s, 2, y)
            cr.paint()
            cr.restore()
            if not first:
                cr.save()
                cr.set_source_rgb(1, 1, 1)
                cr.rectangle(sc(1), y - 2, usc(width) - 2, 4)
                cr.fill()
                cr.restore()
            first = False

            y += usc(s.get_height()) - 1

        cr.show_page()

        Gtk.drag_set_icon_surface(context, final_surface)

    def on_drag_end(self, context, data=None):
        self.dnd_autoscroll_cancel()

    def dnd_autoscroll(self):
        AUTO_SCROLL_MARGIN = 20

        window = self.treeview.get_bin_window()
        vadjust = self.treeview_holder.get_vadjustment()
        seat = Gdk.Display.get_default().get_default_seat()
        pointer = seat.get_pointer()

        window, x, y, mask = window.get_device_position(pointer)
        y += vadjust.get_value()
        rect = self.treeview.get_visible_rect()

        offset = y - (rect.y + 2 * AUTO_SCROLL_MARGIN)
        if offset > 0:
            offset = y - (rect.y + rect.height - 2 * AUTO_SCROLL_MARGIN);
            if offset < 0:
                return

        value = max(0.0, min(vadjust.get_value() + offset, vadjust.get_upper() - vadjust.get_page_size()))
        vadjust.set_value(value)

        self.dnd_autoscroll_start()

    def dnd_autoscroll_timeout(self, data=None):
        self.dnd_autoscroll_timeout_id = 0
        self.dnd_autoscroll()

        return GLib.SOURCE_REMOVE

    def dnd_autoscroll_cancel(self):
        if self.dnd_autoscroll_timeout_id > 0:
            GLib.source_remove(self.dnd_autoscroll_timeout_id)
            self.dnd_autoscroll_timeout_id = 0

    def dnd_autoscroll_start(self):
        if self.dnd_autoscroll_timeout_id > 0:
            GLib.source_remove(self.dnd_autoscroll_timeout_id)
        self.dnd_autoscroll_timeout_id = GLib.timeout_add(50, self.dnd_autoscroll_timeout)

    def on_drag_motion(self, widget, context, x, y, etime):
        target_row = self.treeview.get_dest_row_at_pos(x, y)
        if not target_row:
            Gdk.drag_status(context, 0, etime)
            return False

        model = self.treeview.get_model()

        path, position = target_row
        i = model.get_iter(path)
        target_row_type = model.get_value(i, ROW_TYPE)
        target_row = model.get_value(i, ROW_OBJ)
        source_path, source_iter = self.get_selected_row_path_iter()
        source_row = model.get_value(source_iter, ROW_OBJ)
        source_row_type = model.get_value(source_iter, ROW_TYPE)

        if source_path.compare(path) == 0 or source_path.is_ancestor(path) and source_row_type == ROW_TYPE_SUBMENU:
            Gdk.drag_status(context, 0, etime)
            return False

        if target_row_type != ROW_TYPE_SUBMENU and position in (Gtk.TreeViewDropPosition.INTO_OR_BEFORE,
                                                                Gtk.TreeViewDropPosition.INTO_OR_AFTER):
            Gdk.drag_status(context, 0, etime)
            return False

        self.treeview.set_drag_dest_row(path, position)
        action = Gdk.DragAction.MOVE

        self.dnd_autoscroll_start()

        Gdk.drag_status(context, action, etime)
        return True

    def on_drag_data_get(self, widget, context, selection_data, info, etime):
        target_atom = selection_data.get_target()
        target = target_atom.name()

        if target == "UTF8_STRING":
            selection = self.treeview.get_selection()
            model, paths = selection.get_selected_rows()
            if paths:
                path = paths[0]
                iter = model.get_iter(path)
                item_data = {
                    "hash": model.get_value(iter, ROW_HASH),
                    "uuid": model.get_value(iter, ROW_UUID),
                    'type': model.get_value(iter, ROW_TYPE)
                }

                selection_data.set_text(json.dumps(item_data), -1)

    def on_drag_data_received(self, widget, context, x, y, selection_data, info, etime):
        drop_info = self.treeview.get_dest_row_at_pos(x, y)
        if not drop_info:
            Gdk.drag_status(context, 0, etime)
            return

        path, position = drop_info

        if selection_data:
            dropped_data = selection_data.get_text()

            if path:
                iter = self.model.get_iter(path)
                parent = self.model.iter_parent(iter)
            else:
                iter = None
                parent = None

            if not self.reorder_items(iter, parent, dropped_data, position):
                Gdk.drag_status(context, 0, etime)
                return
        Gtk.drag_finish(context, True, True, etime)

        self.set_needs_saved(True)
        self.update_treeview_state()

    def reorder_items(self, target_iter, parent, dropped_data, position):
        source_data = json.loads(dropped_data)
        source_hash = source_data['hash']
        source_uuid = source_data['uuid']
        source_type = source_data['type']
        source_iter = self.lookup_iter_by_hash(self.model, source_hash)

        if source_iter is None:
            print("no source row found, cancelling drop")
            return False

        target_row_type = self.model.get_value(target_iter, ROW_TYPE)
        if target_row_type == ROW_TYPE_ACTION and \
                position in (Gtk.TreeViewDropPosition.INTO_OR_BEFORE,
                             Gtk.TreeViewDropPosition.INTO_OR_AFTER):
            return False

        new_iter = None
        row = self.model.get_value(source_iter, ROW_OBJ)

        if target_row_type == ROW_TYPE_SUBMENU and \
                position in (Gtk.TreeViewDropPosition.INTO_OR_BEFORE,
                             Gtk.TreeViewDropPosition.INTO_OR_AFTER,
                             Gtk.TreeViewDropPosition.AFTER):
            new_iter = self.model.insert(target_iter, 0, [new_hash(), source_uuid, source_type, 0, row])
        else:
            if position == Gtk.TreeViewDropPosition.BEFORE:
                new_iter = self.model.insert_before(parent, target_iter, [new_hash(), source_uuid, source_type, 0, row])
            elif position == Gtk.TreeViewDropPosition.AFTER:
                new_iter = self.model.insert_after(parent, target_iter, [new_hash(), source_uuid, source_type, 0, row])

        # we have to recreate all children to the new menu location.
        if new_iter is not None:
            if source_type == ROW_TYPE_SUBMENU:
                self.move_tree(self.model, source_iter, new_iter)
            self.remove_source_row_by_hash(self.model, source_hash)

        self.update_positions(parent)
        return True

    def move_tree(self, model, source_iter, new_iter):
        foreach_iter = self.model.iter_children(source_iter)

        while foreach_iter is not None:
            row_hash = model.get_value(foreach_iter, ROW_HASH)
            row_uuid = model.get_value(foreach_iter, ROW_UUID)
            row_type = model.get_value(foreach_iter, ROW_TYPE)
            row = model.get_value(foreach_iter, ROW_OBJ)

            if row is None:
                print("During prune/paste, could not find row for %s with hash %s" % (row_uuid, row_hash))
                continue
            inserted_iter = self.model.insert(new_iter, -1, [
                new_hash(),
                row_uuid,
                row_type,
                0,
                row
            ])

            if row_type == ROW_TYPE_SUBMENU:
                self.move_tree(model, foreach_iter, inserted_iter)

            foreach_iter = self.model.iter_next(foreach_iter)

    def lookup_iter_by_hash(self, model, hash):
        result = None

        def compare(model, path, iter, data):
            current = model.get_value(iter, ROW_HASH)
            if current == hash:
                nonlocal result
                result = iter
                return True
            return False

        model.foreach(compare, hash)
        return result

    def remove_source_row_by_hash(self, model, old_hash):
        iter = self.lookup_iter_by_hash(model, old_hash)

        if iter is not None:
            model.remove(iter)

    def update_positions(self, parent):
        if parent:
            iter = self.model.iter_children(parent)
        else:
            iter = self.model.get_iter_first()

        position = 0
        while iter:
            self.model.set_value(iter, ROW_POSITION, position)
            position += 1
            if self.model.iter_has_child(iter):
                self.update_positions(iter)
            iter = self.model.iter_next(iter)

    def quit(self, *args, **kwargs):
        if self.needs_saved:
            dialog = Gtk.MessageDialog(
                transient_for=self.main_window,
                modal=True,
                message_type=Gtk.MessageType.OTHER,
                buttons=Gtk.ButtonsType.YES_NO,
                text=_("The layout has changed. Save it?")
            )

            dialog.set_title(_("Unsaved changes"))
            response = dialog.run()
            dialog.destroy()

            if response == Gtk.ResponseType.DELETE_EVENT:
                return False

            if response == Gtk.ResponseType.YES:
                self.save_model()
                self.save_disabled_list()

        return True

class EditorWindow():
    def __init__(self):
        self.builder = Gtk.Builder.new_from_file(str(GLADE_FILE))
        self.main_window = self.builder.get_object("main_window")
        self.hamburger_button = self.builder.get_object("hamburger_button")
        self.editor = NemoActionsOrganizer(self.main_window, self.builder)
        self.main_window.add(self.editor)

        # Hamburger menu
        menu = Gtk.Menu()

        item = Gtk.ImageMenuItem(label=_("Open user actions folder"), image=Gtk.Image(icon_name="folder-symbolic", icon_size=Gtk.IconSize.MENU))
        item.connect("activate", self.open_actions_folder_clicked)
        menu.add(item)

        item = Gtk.MenuItem(label=_("Quit"))
        item.connect("activate", self.quit)
        menu.add(item)

        menu.show_all()
        self.hamburger_button.set_popup(menu)

        self.main_window.connect("delete-event", self.window_delete)

        self.main_window.show_all()
        self.main_window.present_with_time(0)

    def quit(self, button):
        if self.editor.quit():
            Gtk.main_quit()
            return True
        return False

    def window_delete(self, window, event, data=None):
        if self.editor.quit():
            Gtk.main_quit()
        return Gdk.EVENT_STOP

    def open_actions_folder_clicked(self, button):
        subprocess.Popen(["xdg-open", USER_ACTIONS_DIR])

if __name__ == "__main__":
    import sys

    EditorWindow()
    Gtk.main()

    sys.exit(0)