
## Actions layout JSON format

- The layout file is saved as `~/.config/nemo/actions/actions-tree.json`.
- A missing or invalid layout will result in a flat list of all actions.
- Definition requirements may change...

#### Structure and node elements

##### The definition must have a 'toplevel' root object, which contains an array of objects (with these possibly having arrays of children also):
```json
{
  "toplevel": [
  ]
}
```

##### Children must adhere to the following definitions:
- `'uuid': string` - For valid 'spices'-based actions, this will be their UUID. For legacy/non-spice actions, it will be the action file's basename, suffixed by `@untracked`. For submenus, it will be the submenu's label.
- `'type': string` - `action`, `submenu` or `separator`
- `'position': integer` - The action or submenu's position at the current tree depth. This is currently for reference only, as the order is preserved when parsing or creating json files, and saved position is ignored.
- `'user-label': string` - can be `null` - The action or submenu's label. In the case of actions, this will be `null` initially, and the action's `Name` field will be used. It can be overridden, and the new value is kept here.
- `'user-icon': string` - can be `null` or empty - The action or submenu's icon. In the case of actions, this will be `null` initially, and the icon string will be drawn from the action's `Icon-Name` field. It can be overridden - the new value is kept here. The special value of `""` (empty string) will suppress any icon altogether.
- `'children': array` (submenu types only) - contains another level of actions and possibly submenus.

##### Example
```json
{
  "toplevel": [
    {
      "uuid": "sample@untracked",
      "type": "action",
      "position": 0,
      "user-label": null,
      "user-icon": null
    },
    {
      "uuid": "mint_dev_tool_make_thumbnail@untracked",
      "type": "action",
      "position": 1,
      "user-label": null,
      "user-icon": null
    },
    {
      "uuid": "92_show-expo@untracked",
      "type": "action",
      "position": 2,
      "user-label": "Manage workspaces",
      "user-icon": "xsi-address-book-new-symbolic"
    },
    {
      "uuid": "Test category",
      "type": "submenu",
      "position": 3,
      "user-label": "Test category",
      "user-icon": "face-smile",
      "children": [
        {
          "uuid": "change-background@untracked",
          "type": "action",
          "position": 0,
          "user-label": null,
          "user-icon": null
        },
        {
          "uuid": "Sub test category",
          "type": "submenu",
          "position": 1,
          "user-label": "Sub test category",
          "user-icon": null,
          "children": [
            {
              "uuid": "mint_dev_tool_show_file_metadata@untracked",
              "type": "action",
              "position": 0,
              "user-label": null,
              "user-icon": null
            }
          ]
        },
        {
          "uuid": "mint_dev_tool_add_shadow@untracked",
          "type": "action",
          "position": 2,
          "user-label": null,
          "user-icon": null
        }
      ]
    },
    {
      "uuid": "91_delete-workspace@untracked",
      "type": "action",
      "position": 4,
      "user-label": null,
      "user-icon": null
    }
  ]
}
```
