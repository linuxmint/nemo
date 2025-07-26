#!/usr/bin/python3
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('XApp', '1.0')
gi.require_version('Xmlb', '2.0')
from gi.repository import Gtk, Gdk, GLib, Gio, XApp, GdkPixbuf, Pango, Xmlb
import cairo
import json
from pathlib import Path
import uuid
import gettext
import locale
import subprocess
import os

import leconfig

locale.bindtextdomain("nemo", leconfig.LOCALE_DIR)
gettext.bindtextdomain("nemo", leconfig.LOCALE_DIR)
gettext.textdomain("nemo")
_ = gettext.gettext

gresources = Gio.Resource.load(os.path.join(leconfig.PKG_DATADIR, "nemo-action-layout-editor-resources.gresource"))
gresources._register()

JSON_FILE = Path(GLib.get_user_config_dir()).joinpath("nemo/actions-tree.json")
USER_ACTIONS_DIR = Path(GLib.get_user_data_dir()).joinpath("nemo/actions")

NON_SPICE_UUID_SUFFIX = "@untracked"

ROW_HASH, ROW_UUID, ROW_TYPE, ROW_OBJ = range(4)

ROW_TYPE_ACTION = "action"
ROW_TYPE_SUBMENU = "submenu"
ROW_TYPE_SEPARATOR = "separator"

def new_hash():
    return uuid.uuid4().hex

class BuiltinShortcut():
    def __init__(self, label, accel_string):
        self.key, self.mods = Gtk.accelerator_parse(accel_string)

        if self.key == 0 and self.mods == 0:
            self.label = "invalid (%s)" % accel_string

        self.label = _(label)

class Row():
    def __init__(self, row_meta=None, keyfile=None, path=None, enabled=True, scale_factor=1):
        self.keyfile = keyfile
        self.row_meta = row_meta
        self.enabled = enabled
        self.scale_factor = scale_factor
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

        if icon_string.startswith("<") and icon_string.endswith(">"):
            real_string = icon_string[1:-1]
            icon_string = str(self.path.parent / real_string)

        if GLib.path_is_absolute(icon_string):
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(icon_string, 16, 16)
            surface = Gdk.cairo_surface_create_from_pixbuf(pixbuf, self.scale_factor, None)
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
                    label = self.keyfile.get_locale_string('Nemo Action', 'Name', None).replace("_", "")
                except GLib.Error as e:
                    print(e)
                    pass

        if label is None:
            return _("Unknown")

        return label

    def get_accelerator_string(self):
        if self.row_meta is not None:
            try:
                accel_string = self.row_meta['accelerator']
                if accel_string is not None:
                    return accel_string
            except KeyError:
                pass

        return None

    def set_custom_label(self, label):
        if not self.row_meta:
            self.row_meta = {}

        self.row_meta['user-label'] = label

    def set_custom_icon(self, icon):
        if not self.row_meta:
            self.row_meta = {}

        self.row_meta['user-icon'] = icon

    def set_accelerator_string(self, accel_string):
        if not self.row_meta:
            self.row_meta = {}

        self.row_meta['accelerator'] = accel_string

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
            self.builder = Gtk.Builder.new_from_resource("/org/nemo/action-layout-editor/nemo-action-layout-editor.glade")
        else:
            self.builder = builder

        self.builtin_shortcuts = []
        self.load_nemo_shortcuts()

        self.main_window = window
        self.layout_editor_box = self.builder.get_object("layout_editor_box")
        self.add(self.layout_editor_box)
        self.treeview_holder = self.builder.get_object("treeview_holder")
        self.save_button = self.builder.get_object("save_button")
        self.discard_changes_button = self.builder.get_object("discard_changes_button")
        self.default_layout_button = self.builder.get_object("default_layout_button")
        self.name_entry = self.builder.get_object("name_entry")
        self.new_row_button = self.builder.get_object("new_row_button")
        self.row_controls_box = self.builder.get_object("row_controls_box")
        self.remove_submenu_button = self.builder.get_object("remove_submenu_button")
        self.clear_icon_button = self.builder.get_object("clear_icon_button")
        self.icon_selector_menu_button = self.builder.get_object("icon_selector_menu_button")
        self.icon_selector_image = self.builder.get_object("icon_selector_image")
        self.action_enabled_switch = self.builder.get_object("action_enabled_switch")
        self.selected_item_widgets_group = XApp.VisibilityGroup.new(True, True, [
            self.icon_selector_menu_button,
            self.name_entry
        ])

        self.up_button = self.builder.get_object("up_button")
        self.up_button.connect("clicked", self.up_button_clicked)
        self.down_button = self.builder.get_object("down_button")
        self.down_button.connect("clicked", self.down_button_clicked)

        self.nemo_plugin_settings = Gio.Settings(schema_id="org.nemo.plugins")
        # Disabled/Enabled may be toggled in nemo preferences directly, keep us in sync.
        self.nemo_plugin_settings.connect("changed", self.on_disabled_settings_list_changed)

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

        self.path_map = []

        self.model = Gtk.TreeStore(str, str, str, object)  # (hash, uuid, type, Row)

        self.treeview = Gtk.TreeView(
            model=self.model,
            enable_tree_lines=True,
            headers_visible=False,
            visible=True
        )

        # Icon and label
        column = Gtk.TreeViewColumn()
        self.treeview.append_column(column)
        column.set_expand(True)

        cell = Gtk.CellRendererPixbuf()
        column.pack_start(cell, False)
        column.set_cell_data_func(cell, self.menu_icon_render_func)
        cell = Gtk.CellRendererText()
        column.pack_start(cell, False)
        column.set_cell_data_func(cell, self.menu_label_render_func)

        # Accelerators
        column = Gtk.TreeViewColumn()
        column.set_sizing(Gtk.TreeViewColumnSizing.AUTOSIZE)
        column.set_expand(True)
        self.treeview.append_column(column)

        cell = Gtk.CellRendererAccel()
        cell.set_property("editable", True)
        cell.set_property("xalign", 0)
        column.pack_end(cell, False)
        column.set_cell_data_func(cell, self.accel_render_func)

        layout = self.treeview.create_pango_layout(_("Click to add a shortcut"))
        w, h = layout.get_pixel_size()
        column.set_min_width(w + 20)

        cell.connect("editing-started", self.on_accel_edit_started)
        cell.connect("accel-edited", self.on_accel_edited)
        cell.connect("accel-cleared", self.on_accel_cleared)
        self.editing_accel = False

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

        self.updating_model = False
        self.updating_row_edit_fields = False
        self.dnd_autoscroll_timeout_id = 0

        self.monitors = []
        self.monitor_action_dirs()

        self.needs_saved = False
        self.reload_model()
        self.update_treeview_state()
        self.update_arrow_button_states()
        self.set_needs_saved(False)

    def load_nemo_shortcuts(self):
        source = Xmlb.BuilderSource()
        try:
            xml = Gio.resources_lookup_data("/org/nemo/action-layout-editor/nemo-shortcuts.ui", Gio.ResourceLookupFlags.NONE)
            ret = source.load_bytes(xml, Xmlb.BuilderSourceFlags.NONE)
            builder = Xmlb.Builder()
            builder.import_source(source)
            silo = builder.compile(Xmlb.BuilderCompileFlags.NONE, None)
        except GLib.Error as e:
            print("Could not load nemo-shortcuts.ui from resource file - we won't be able to detect built-in shortcut collisions: %s" % e.message)
            return

        root = silo.query_first("interface")
        for child in root.query(f"object/child", 0):
            for section in child.query("object[@class='GtkShortcutsSection']", 0):
                for group in section.query("child/object[@class='GtkShortcutsGroup']", 0):
                    for shortcut in group.query("child/object[@class='GtkShortcutsShortcut']", 0):
                        label = shortcut.query_text("property[@name='title']")
                        accel = shortcut.query_text("property[@name='accelerator']")
                        self.builtin_shortcuts.append(BuiltinShortcut(label, accel))

    def reload_model(self, flat=False):
        self.updating_model = True
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
            except (FileNotFoundError, ValueError, KeyError, json.decoder.JSONDecodeError):
                self.data = {
                    'toplevel': []
                }

        installed_actions = self.load_installed_actions()
        self.fill_model(self.model, None, self.data['toplevel'], installed_actions)

        start_path = Gtk.TreePath.new_first()
        self.treeview.get_selection().select_path(start_path)
        self.treeview.scroll_to_cell(start_path, None, True, 0, 0)
        self.update_row_controls()

        self.updating_model = False

    def monitor_action_dirs (self):
        data_dirs = GLib.get_system_data_dirs() + [GLib.get_user_data_dir()]

        for d in data_dirs:
            full = os.path.join(d, "nemo", "actions")
            file = Gio.File.new_for_path(full)
            try:
                if not file.query_exists(None):
                    continue
                monitor = file.monitor_directory(Gio.FileMonitorFlags.WATCH_MOVES | Gio.FileMonitorFlags.SEND_MOVED, None)
                monitor.connect("changed", self.actions_folder_changed)
                self.monitors.append(monitor)
            except GLib.Error as e:
                print("Error monitoring action directory '%s'" % full)

    def actions_folder_changed(self, monitor, file, other, event_type, data=None):
        if not file.get_basename().endswith(".nemo_action"):
            return

        self.reload_model()
        self.update_treeview_state()
        self.set_needs_saved(False)

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

        # Check the node has a valid accelerator
        try:
            accel_str = node['accelerator']

            if accel_str not in ("", None):
                key, mods = Gtk.accelerator_parse(accel_str)
                if key == 0 and mods == 0:
                    raise ValueError("%s: Invalid accelerator string '%s'" % (uuid, accel_str))
        except KeyError:
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
                            print("Error loading action file '%s': %s" % (str(file), e.message))
                            continue

        return actions

    def fill_model(self, model, parent, items, installed_actions):
        disabled_actions = self.nemo_plugin_settings.get_strv("disabled-actions")
        scale_factor = self.main_window.get_scale_factor()

        for item in items:
            row_type = item.get("type")
            uuid = item.get('uuid')

            if row_type == ROW_TYPE_ACTION:
                try:
                    kf = installed_actions[uuid][1]  # (path, kf) tuple
                    path = Path(installed_actions[uuid][0])
                except KeyError:
                    print("Ignoring missing installed action %s" % uuid)
                    continue

                iter = model.append(parent, [new_hash(), uuid, row_type, Row(item, kf, path, path.name not in disabled_actions, scale_factor)])

                del installed_actions[uuid]
            elif row_type == ROW_TYPE_SEPARATOR:
                iter = model.append(parent, [new_hash(), "separator", ROW_TYPE_SEPARATOR, Row(item, None, None, True)])
            else:
                iter = model.append(parent, [new_hash(), uuid, row_type, Row(item, None, None, True)])

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
            model.append(parent, [new_hash(), uuid, ROW_TYPE_ACTION, Row(None, kf, path, enabled, scale_factor)])

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

    def on_disabled_settings_list_changed(self, settings, key, data=None):
        disabled_actions = self.nemo_plugin_settings.get_strv("disabled-actions")

        def update_disabled(model, path, iter, data=None):
            row = model.get_value(iter, ROW_OBJ)
            row_uuid = model.get_value(iter, ROW_UUID)
            old_enabled = row.enabled
            row.enabled = (row_uuid not in disabled_actions)
            if old_enabled != row.enabled:
                self.model.row_changed(path, iter)
            return False

        self.updating_model = True
        self.model.foreach(update_disabled)
        self.updating_model = False
        self.queue_draw()

    def serialize_model(self, parent, model):
        used_uuids = {}
        result = []

        iter = model.iter_children(parent)
        while iter:
            row_type = model.get_value(iter, ROW_TYPE)
            row = model.get_value(iter, ROW_OBJ)
            raw_uuid = model.get_value(iter, ROW_UUID)

            uuid = raw_uuid
            if raw_uuid in used_uuids:
                uuid = raw_uuid + str(used_uuids[raw_uuid])
                used_uuids[raw_uuid] += 1
            else:
                used_uuids[raw_uuid] = 0

            item = {
                'uuid': uuid,
                'type': row_type,
                'user-label': row.get_custom_label(),
                'user-icon': row.get_custom_icon(),
                'accelerator': row.get_accelerator_string()
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

    def set_selection(self, iter):
        selection = self.treeview.get_selection()
        selection.select_iter(iter)
        path, niter = self.get_selected_row_path_iter()

    def get_selected_row_field(self, field):
        path, iter = self.get_selected_row_path_iter()
        return self.model.get_value(iter, field)

    def selected_row_changed(self, needs_saved=True):
        if self.updating_model:
            return

        path, iter = self.get_selected_row_path_iter()
        if iter is not None:
            self.model.row_changed(path, iter)

        self.update_row_controls()

        if needs_saved:
            self.set_needs_saved(True)

    def on_treeview_position_changed(self, selection):
        if self.updating_model:
            return

        self.update_row_controls()
        self.update_arrow_button_states()

    def update_row_controls(self):
        self.updating_row_edit_fields = True

        try:
            row = self.get_selected_row_field(ROW_OBJ)
        except TypeError:
            self.row_controls_box.set_sensitive(False)
            self.name_entry.set_icon_from_icon_name(Gtk.EntryIconPosition.SECONDARY, None)
            self.icon_selector_image.clear()
            self.name_entry.set_text("")
            return

        self.row_controls_box.set_sensitive(True)

        if row is not None:
            row_type = self.get_selected_row_field(ROW_TYPE)

            self.name_entry.set_text(row.get_label())

            self.set_icon_button(row)
            self.original_icon_menu_item.set_visible(row_type == ROW_TYPE_ACTION)
            orig_icon = row.get_icon_string(original=True)
            self.original_icon_menu_item.set_sensitive(orig_icon is not None and orig_icon != row.get_icon_string())
            self.selected_item_widgets_group.set_sensitive(row.enabled and row_type != ROW_TYPE_SEPARATOR)
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

    def update_arrow_button_states(self):
        can_up = True
        can_down = True

        path, for_iter = self.get_selected_row_path_iter()
        first_iter = self.model.get_iter_first()

        if self.same_iter(for_iter, first_iter):
            can_up = False
        else:
            last_iter = None
            while first_iter:
                last_iter = first_iter
                first_iter = self.model.iter_next(first_iter)

            if self.same_iter(for_iter, last_iter):
                can_down = False

        self.up_button.set_sensitive(can_up)
        self.down_button.set_sensitive(can_down)

    # Button signal handlers

    def up_button_clicked(self, button):
        self.move_selection_up_one()

    def down_button_clicked(self, button):
        self.move_selection_down_one()

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
            Row({"uuid": "New Submenu"}, None, None, True)])

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

        self.updating_model = True
        if row_type == ROW_TYPE_SUBMENU:
            parent_iter = self.model.iter_parent(selection_iter)
            self.move_tree(self.model, selection_iter, parent_iter)

        self.remove_row_by_hash(self.model, row_hash)
        self.updating_model = False

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
            self.save_disabled_list()

    def on_accel_edited(self, accel, path, key, mods, kc, data=None):
        if not self.validate_accelerator(key, mods):
            return

        row = self.get_selected_row_field(ROW_OBJ)
        if row is not None:
            row.set_accelerator_string(Gtk.accelerator_name(key, mods))
            self.selected_row_changed()

    def on_accel_cleared(self, accel, path, data=None):
        row = self.get_selected_row_field(ROW_OBJ)
        if row is not None:
            row.set_accelerator_string(None)
            self.selected_row_changed()

    def on_accel_edit_started(self, cell, editable, path, data=None):
        self.editing_accel = True
        editable.connect("editing-done", self.accel_editing_done)

    def accel_editing_done(self, editable, data=None):
        self.editing_accel = False
        editable.disconnect_by_func(self.accel_editing_done)

    def validate_accelerator(self, key, mods):
        # Check nemo's built-ins (copy, paste, etc...)
        for shortcut in self.builtin_shortcuts:
            if shortcut.key == key and shortcut.mods == mods:
                label = f"<b>{shortcut.label}</b>"
                dialog = Gtk.MessageDialog(
                    transient_for=self.main_window,
                    modal=True,
                    message_type=Gtk.MessageType.ERROR,
                    buttons=Gtk.ButtonsType.OK,
                    text=_("This key combination is already in use by Nemo (%s). It cannot be changed.") % label,
                    use_markup=True
                )
                dialog.run()
                dialog.destroy()
                return False

        conflict = False

        def check_for_action_conflict(iter):
            foreach_iter = self.model.iter_children(iter)

            nonlocal conflict

            while not conflict and foreach_iter is not None:
                row = self.model.get_value(foreach_iter, ROW_OBJ)
                accel_string = row.get_accelerator_string()

                if accel_string is not None:
                    row_key, row_mod = Gtk.accelerator_parse(accel_string)
                    if row_key == key and row_mod == mods:
                        label = f"\n\n<b>{row.get_label()}</b>\n\n"
                        dialog = Gtk.MessageDialog(
                            transient_for=self.main_window,
                            modal=True,
                            message_type=Gtk.MessageType.WARNING,
                            buttons=Gtk.ButtonsType.YES_NO,
                            text=_("This key combination is already in use by another action:") + 
                                   label + _("Do you want to replace it?"),
                            use_markup=True
                        )
                        resp = dialog.run()
                        dialog.destroy()

                        # nonlocal conflict

                        if resp == Gtk.ResponseType.YES:
                            row.set_accelerator_string(None)
                            conflict = False
                        else:
                            conflict = True
                        break

                foreach_type = self.model.get_value(foreach_iter, ROW_TYPE)
                if foreach_type == ROW_TYPE_SUBMENU:
                    check_for_action_conflict(foreach_iter)

                foreach_iter = self.model.iter_next(foreach_iter)

        check_for_action_conflict(None)
        return not conflict

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

    def accel_render_func(self, column, cell, model, iter, data):
        row_type = model.get_value(iter, ROW_TYPE)
        if row_type in (ROW_TYPE_SUBMENU, ROW_TYPE_SEPARATOR):
            cell.set_property("visible", False)
            return

        row = model.get_value(iter, ROW_OBJ)

        accel_string = row.get_accelerator_string() or ""
        key, mods = Gtk.accelerator_parse(accel_string)
        cell.set_property("visible", True)
        cell.set_property("accel-key", key)
        cell.set_property("accel-mods", mods)

        if accel_string == "":
            spath, siter = self.get_selected_row_path_iter()
            current_path = model.get_path(iter)
            if current_path is not None and current_path.compare(spath) == 0:
                if not self.editing_accel:
                    cell.set_property("text", _("Click to add a shortcut"))
                else:
                    cell.set_property("text", None)
            else:
                cell.set_property("text", " ")

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
            new_iter = self.model.insert(target_iter, 0, [new_hash(), source_uuid, source_type, row])
        else:
            if position == Gtk.TreeViewDropPosition.BEFORE:
                new_iter = self.model.insert_before(parent, target_iter, [new_hash(), source_uuid, source_type, row])
            elif position == Gtk.TreeViewDropPosition.AFTER:
                new_iter = self.model.insert_after(parent, target_iter, [new_hash(), source_uuid, source_type, row])

        # we have to recreate all children to the new menu location.
        if new_iter is not None:
            if source_type == ROW_TYPE_SUBMENU:
                self.move_tree(self.model, source_iter, new_iter)
            self.remove_row_by_hash(self.model, source_hash)

        return True

    # Up/Down button handling
    def get_new_row_data(self, source_iter):
        source_hash = self.model.get_value(source_iter, ROW_HASH)
        source_uuid = self.model.get_value(source_iter, ROW_UUID)
        source_type = self.model.get_value(source_iter, ROW_TYPE)
        source_object = self.model.get_value(source_iter, ROW_OBJ)
        target_hash = new_hash()

        return (
            source_hash,
            target_hash,
            source_type,
            [
                target_hash,
                source_uuid,
                source_type,
                source_object
            ]
        )

    def get_last_at_level(self, model, iter):
        if model.iter_has_child(iter):
            foreach_iter = model.iter_children(iter)
            while foreach_iter is not None:
                last_iter = foreach_iter
                foreach_iter = model.iter_next(foreach_iter)
                continue

            return self.get_last_at_level(model, last_iter)
        return iter

    def same_iter(self, iter1, iter2):
        if iter1 is None and iter2 is None:
            return True
        elif iter1 is None and iter2 is not None:
            return False
        elif iter2 is None and iter1 is not None:
            return False

        path1 = self.model.get_path(iter1)
        path2 = self.model.get_path(iter2)
        return path1.compare(path2) == 0

    def path_is_valid(self, path):
        try:
            test_iter = self.model.get_iter(path)
        except ValueError:
            return False
        return True

    def next_path_validated(self, path):
        path.next()
        return self.path_is_valid(path)

    """
    The move_selection_up_one and _down_one methods are complicated because there are only up/down
    arrows (to keep things simple to the user). This navigates the treeview as if it was fully
    expanded, but movement is by row, *not* by level, so it's possible to reach every node in the
    tree.
    """

    def move_selection_up_one(self):
        path, iter = self.get_selected_row_path_iter()
        if iter is None:
            return

        parent = self.model.iter_parent(iter)

        target_path = path
        target_iter = None
        target_parent = None

        source_hash, target_hash, row_type, inserted_row = self.get_new_row_data(iter)
        inserted_iter = None

        if target_path.prev():
            target_iter = self.get_last_at_level(self.model, self.model.get_iter(target_path))
            target_iter_type = self.model.get_value(target_iter, ROW_TYPE)

            if target_iter_type == ROW_TYPE_SUBMENU:
                inserted_iter = self.model.prepend(target_iter, inserted_row)
                target_parent = target_iter
            else:
                target_parent = self.model.iter_parent(target_iter)

                if self.same_iter(parent, target_parent):
                    inserted_iter = self.model.insert_before(target_parent, target_iter, inserted_row)
                else:
                    inserted_iter = self.model.insert_after(target_parent, target_iter, inserted_row)
        elif target_path.up():
            if target_path.prev():
                target_iter = self.get_last_at_level(self.model, self.model.get_iter(target_path))
                target_parent = self.model.iter_parent(target_iter)
                inserted_iter = self.model.insert_after(target_parent, target_iter, inserted_row)
            else:
                # We're at the top?
                top_iter = self.model.get_iter_first()
                if not self.same_iter(iter, top_iter):
                    inserted_iter = self.model.insert_before(None, top_iter, inserted_row)

        source_was_expanded = False

        if inserted_iter is not None:
            self.updating_model = True

            if row_type == ROW_TYPE_SUBMENU:
                if self.treeview.row_expanded(self.model.get_path(iter)):
                    source_was_expanded = True
                self.move_tree(self.model, iter, inserted_iter)

            if target_parent is not None:
                self.treeview.expand_row(self.model.get_path(target_parent), True)
            elif source_was_expanded:
                self.treeview.expand_row(self.model.get_path(inserted_iter), True)

            self.remove_row_by_hash(self.model, source_hash)
            self.updating_model = False

            self.select_row_by_hash(self.model, target_hash)

            self.treeview.scroll_to_cell(self.model.get_path(inserted_iter), None, False, 0, 0)
            self.set_needs_saved(True)

    def move_selection_down_one(self):
        path, iter = self.get_selected_row_path_iter()

        if iter is None:
            return

        target_path = path
        target_iter = None
        target_parent = None

        source_hash, target_hash, row_type, inserted_row = self.get_new_row_data(iter)
        inserted_iter = None

        if self.next_path_validated(target_path):
            # is it a menu? Add it as its first child
            maybe_submenu_iter = self.model.get_iter(target_path)
            maybe_submenu_type = self.model.get_value(maybe_submenu_iter, ROW_TYPE)
            if maybe_submenu_type == ROW_TYPE_SUBMENU:
                inserted_iter = self.model.prepend(maybe_submenu_iter, inserted_row)
                target_parent = maybe_submenu_iter
            else:
                # or else add after the test row
                target_iter = self.model.get_iter(target_path)
                target_parent = self.model.iter_parent(target_iter)
                inserted_iter = self.model.insert_after(target_parent, target_iter, inserted_row)
        else:
            # path_next_validated modifies target_path directly, reset it to the origin
            target_path = path
            if target_path.get_depth() > 1 and target_path.up() and self.path_is_valid(target_path):
                target_iter = self.model.get_iter(target_path)
                target_parent = self.model.iter_parent(target_iter)
                inserted_iter = self.model.insert_after(target_parent, target_iter, inserted_row)

        source_was_expanded = False

        if inserted_iter is not None:
            self.updating_model = True

            if row_type == ROW_TYPE_SUBMENU:
                if self.treeview.row_expanded(self.model.get_path(iter)):
                    source_was_expanded = True
                self.move_tree(self.model, iter, inserted_iter)

            if target_parent is not None:
                self.treeview.expand_row(self.model.get_path(target_parent), True)
            elif source_was_expanded:
                self.treeview.expand_row(self.model.get_path(inserted_iter), True)

            self.remove_row_by_hash(self.model, source_hash)
            self.updating_model = False

            self.select_row_by_hash(self.model, target_hash)

            self.treeview.scroll_to_cell(self.model.get_path(inserted_iter), None, False, 0, 0)
            self.set_needs_saved(True)

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

    def remove_row_by_hash(self, model, old_hash):
        iter = self.lookup_iter_by_hash(model, old_hash)

        if iter is not None:
            model.remove(iter)

    def select_row_by_hash(self, model, hash):
        iter = self.lookup_iter_by_hash(model, hash)

        if iter is not None:
            self.set_selection(iter)

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

        for monitor in self.monitors:
            monitor.cancel()

        return True

class EditorWindow():
    def __init__(self):
        self.builder = Gtk.Builder.new_from_resource("/org/nemo/action-layout-editor/nemo-action-layout-editor.glade")
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