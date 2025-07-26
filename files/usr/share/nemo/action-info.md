# Nemo Actions
_This documentation is available online at https://github.com/linuxmint/nemo/wiki/Nemo-Actions_

### About
Nemo actions allow the user to add custom menu actions to apply to selected files. These can be one-shot commands or call their own scripts, and can be restricted to specific files by defining various conditions for the action.

Action files are just text files that end in `.nemo_action` and are stored in system and user data locations - `/usr/share/nemo/actions`, `~/.local/usr/share/nemo/actions` (all of your environment's `XDG_DATA_DIRS` are scanned, though typically only the aforementioned are used).

These files are structured as follows (this is a standard keyfile syntax also used in application .desktop and config files):

```
[Nemo Action]

Name=Test action on file %f
Comment=If you click this menu entry, %F will be acted upon!
Exec=notify-send "Hey there" "This is the result of an action you ran on the file '%F' - well done"
Selection=s
Extensions=any;
Terminal=true
Icon-Name=face-smile
```

If you add this to a newly created file `~/.local/share/nemo/actions/my-test-action.nemo_action`, and right-click on a single file in Nemo, this will appear in the menu. You should get the system notification immediately after you activate it.

---

### Troubleshooting
For debugging actions, you can run Nemo as follows:
```bash
# First, kill any existing instances
nemo --quit
# Then relaunch with debugging enabled
NEMO_DEBUG=Actions nemo --debug
```
> **Tip**: When debugging actions, it's usually a good idea to temporarily disable all but the action you're working with, to reduce the amount of logging output generated

With debugging enabled, you should get output that looks something like this when testing your action:
```bash
Nemo-DEBUG: nemo_action_update_display_state: ../libnemo-private/nemo-action.c:2011: Action 'new-sample' determined VISIBLE
Nemo-DEBUG: get_final_label: ../libnemo-private/nemo-action.c:1538: Action Label: Test action on file Documents
Nemo-DEBUG: get_final_tt: ../libnemo-private/nemo-action.c:1563: Action Tooltip: If you click this menu entry, /home/mtwebster/Documents will be acted upon!
Nemo-DEBUG: nemo_action_activate: ../libnemo-private/nemo-action.c:1462: Action Spawning: notify-send "Hey there" "This is the result of an action you ran on the file '/home/mtwebster/Documents' - well done"
```

---

### Reference
##### Field descriptions:

**Name** (mandatory): The label to display in the menu. This can include selection tokens and can be localized (see below).

`
Name=Test action on file %f
`

**Exec** (mandatory): The command to execute when the action is activated

`
Exec=notify-send "Hey there" "This is the result of an action you ran on the file '%F' - well done"
`

If you have a custom script to run, you can enclose it in `<...>` to have it executed from the action's directory. This is handy because it removes the need for the script to either be in the user's PATH or require an absolute path. Instead, the file or relative path within the brackets will be appended to the action's path. So, if your action is in `~/.local/share/actions/nemo/my-action.nemo_action` and you put `<my-action/my-action-script.sh>` as the Exec line, Nemo will assume that script is at `~/.local/share/actions/nemo/my-action/my-action-script.sh`.

**Selection** (mandatory): Provides a condition based on how many files are selected.

`
Selection=s
`

Displays the action if:

- `s`: one single file is selected.
- `m`: multiple files are selected.
- `any`: any number of files is selected (including no files).
- `notnone`: any number of files is selected (excluding no files).
- `none`: a click against the view's background.
- `42`: an integer specifying exactly how many files must be selected.

**Extensions** (mandatory if **Mimetypes** is not defined): What file extension(s) to match with.

`
Extensions=pdf;ps;
`

Displays the action if:

- `dir`: only directories are selected.
- `none`: only files with no extension are selected.
- `nodirs`: only files with no selection are selected, or if there is no selection.
- `any`: any file is selected.

or
- `pdf;txt;png;`: all files match a semicolon-separated list of file extensions.

**Mimetypes** (mandatory if **Extensions** is not defined): What file mimetypes to match with.

`
Mimetypes=text/plain;
`

Displays the action if all selected files have a mimetype included in this semicolon-separated list.

**Comment** (optional): The tooltip to display in Nemo's bottom status bar. This can include selection tokens and can be localized (see below).

`
Comment=If you click this menu entry, %F will be acted upon!
`

**Icon-Name** (optional): The name of the icon to display in Nemo's context menu next to the action entry. This must be an icon that is part of the GtkIconTheme, or an absolute path.  Symbolic icons are supported.

`
Icon-Name=folder
`

If you have a custom icon, you can enclose the filename in `<...>` to have it loaded from the action's directory. For instance, with `<my-action-icon.png>` nemo will look for an icon by that name in the same directory as the action file. If your action is a Spice and has its own subfolder, that can be included as a relative path (`my-action@me/my-action-icon.png`).

**Separator** (optional): Character(s) to separate multiple filenames if more than a single file is selected. By default a space is used.

`
Separator=,
`

**Quote** (optional): Quote type to use (if any) around individual selected files.

`
Quote=double
`

Can be one of:
- `single`: single quotations (' ')
- `double`: double quotations (" ")
- `backtick`: backticks       (` `)

**Dependencies** (optional): Semicolon-separated list of executable programs required for this action to display.

`
Dependencies=notify-send;zenity;
`

- If just a filename is provided, the PATH will be searched for it (/usr/bin, etc...).
- Absolute paths are valid.
- Prefixing a filename with ! will reverse the dependency - if the program exists, the dependency check will *fail*.

**UriScheme** (optional): a uri scheme (file, smb, sftp, ...) that the current location's scheme must match.

`
UriScheme=sftp
`

**Conditions** (optional): a semicolon-separated list of special visibility conditions, all of which must be met for the action to be visible.

`
Conditions=desktop;dbus org.Cinnamon;
`

Can be one or more of:
- `desktop`: The selection must be the desktop (not the 'Desktop' folder in Nemo's bookmarks, but *the* desktop).
- `removable`: The selection must be some sort of removable device.
- `dbus <name>`: <name> must be a well-known DBus name, and be owned.
- `gsettings <schema> <boolean key>`: The boolean key in the given schema must be true.
- `gsettings <schema> <key> <key-type> <[eq|ne|gt|lt]> <value>`: The value of `<key>` is [`equal|not equal|greater than|less than`] `<value>`. The `<key-types>` must match, but the comparisons are not clearly defined for non-numerical types.
- `exec <program>`: Run `<program>` (absolute path or PATH executable) and interpret its exit code as 0 for passing, and non-0 for failure.

**Terminal** (optional): Set to true to execute the Exec line in a spawned terminal window.
  
`
Terminal=false
`

- Generally you should not define **Quotes** when **Terminal** is true, as that should be handled during the conversion to terminal arguments.

**Files** (optional): A semicolon-separated list of globs, filenames and paths to be tested against the current *selection*.  All files in the selection must match *at least one* 'allowed' pattern, path or name for the action to be considered valid.

`
Files=.bash*;!.bashrc;
`

**Locations** (optional): A semicolon-separated list of globs, filenames and paths to be tested against the current *location*. The current location must match *at least one* 'allowed' pattern, path or name for the action to be considered valid.

`
Locations=.*;!.config;
`

##### Additional details about Locations and Files:
- Globs and paths can be relative or absolute.
- ~ (tilde) will be expanded at runtime to the user's home directory.
- Absolute paths will be tested against the file/location's full path. Otherwise it is tested against the filename only.
- A glob with a leading * will be matched against the full path, whether it contains additional path parts or is just a partial filename.
- If a condition is prefixed with a ! it will be considered an opposing condition (If the file passes this test, action is forbidden).
- Allowed patterns are considered before forbidden ones. This allows behaviors such as:
  - Allow any dot-file except .config: `Locations=.*;!.config`


##### Deprecated fields

**Stock-Id** (optional): A Gtk Stock ID to use for the icon. Note that if both **Icon-Name** and **Stock-Id** are defined, **Stock-Id** takes priority.

`
Stock-Id=gtk-ok
`

**Active** (optional): Whether or not the action is visible to Nemo. This really is never necessary, as actions can be enabled/disabled in preferences.

`
Active=true
`

---

##### Tokens used in the Name, Comment and Exec fields:
- `%U`: insert URI list of selection.
- `%F`: insert path list of selection.
- `%P`: insert path of parent (current) directory.
- `%f`: insert display name of first selected file.
- `%p`: insert display name of parent directory.
- `%D`: insert device path of file (i.e. /dev/sdb1).
- `%e`: insert display name of first selected file with the extension stripped.
- `%%`: insert a literal percent sign, don't treat the next character as a token.
- `%X`: insert the XID for the NemoWindow this action is being activated in.
- `%N`: (deprecated) - same as `%f`.
---
#### Cheat Sheet

```markdown
[Nemo Action]
# Mandatory fields
Name=Menu item label
Exec=program_to_run or <custom-script.sh>
Selection=s|m|any|notnone|none|42

Extensions=[dir|none|nodirs|any] | pdf;ps;
# and/or
Mimetypes=text/plain;

# Optional fields
Comment=If you click this menu entry, %F will be acted upon!
Icon-Name=folder-symbolic
Separator=,
Quote=single|double|backtick
Dependencies=notify-send;!zenity;
UriScheme=sftp
Conditions=desktop;dbus <name>;gsettings foo_schema foo_boolkey;removable;exec <program>;
Terminal=true|false
Files=.bash*;!.bashrc;
Locations=.*;!.config;
```
---