
## Actions layout JSON format

- The layout file is saved as `~/.config/nemo/actions-tree.json` (`$XDG_CONFIG_HOME/nemo/actions-tree.json`).
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
- `'uuid': string` - For actions, this is the action file's name, including the `.nemo_action` extension (for example `sample.nemo_action`). This is also true for 'spices'-based actions (for example `name@author.nemo_action`). For submenus, it will be the submenu's label. Separators need a `uuid` too (the layout editor writes `separator`).
- `'type': string` - `action`, `submenu` or `separator`
- `'user-label': string` - can be `null` - The action or submenu's label. In the case of actions, this will be `null` initially, and the action's `Name` field will be used. It can be overridden, and the new value is kept here.
- `'user-icon': string` - can be `null` or empty - The action or submenu's icon. In the case of actions, this will be `null` initially, and the icon string will be drawn from the action's `Icon-Name` field. It can be overridden - the new value is kept here. The special value of `""` (empty string) will suppress any icon altogether.
- `'accelerator': string` - can be `null` or empty - A keyboard shortcut for the action, in GTK accelerator format (for example `<Primary>Return` or `<Shift>F4`). `null` or `""` means no shortcut. It is only used for actions.
- `'children': array` (submenu types only) - contains another level of actions and possibly submenus. It is required: a submenu without `children` makes the whole layout invalid.

##### Example
```json
{
  "toplevel": [
    {
      "uuid": "sample.nemo_action",
      "type": "action",
      "user-label": null,
      "user-icon": null,
      "accelerator": "<Primary>Return"
    },
    {
      "uuid": "mint_dev_tool_make_thumbnail.nemo_action",
      "type": "action",
      "user-label": null,
      "user-icon": null,
      "accelerator": null
    },
    {
      "uuid": "92_show-expo.nemo_action",
      "type": "action",
      "user-label": "Manage workspaces",
      "user-icon": "xsi-address-book-new-symbolic",
      "accelerator": null
    },
    {
      "uuid": "Test category",
      "type": "submenu",
      "user-label": "Test category",
      "user-icon": "face-smile",
      "accelerator": null,
      "children": [
        {
          "uuid": "change-background.nemo_action",
          "type": "action",
          "user-label": null,
          "user-icon": null,
          "accelerator": null
        },
        {
          "uuid": "Sub test category",
          "type": "submenu",
          "user-label": "Sub test category",
          "user-icon": null,
          "accelerator": null,
          "children": [
            {
              "uuid": "mint_dev_tool_show_file_metadata.nemo_action",
              "type": "action",
              "user-label": null,
              "user-icon": null,
              "accelerator": null
            }
          ]
        },
        {
          "uuid": "mint_dev_tool_add_shadow.nemo_action",
          "type": "action",
          "user-label": null,
          "user-icon": null,
          "accelerator": null
        }
      ]
    },
    {
      "uuid": "91_delete-workspace.nemo_action",
      "type": "action",
      "user-label": null,
      "user-icon": null,
      "accelerator": null
    }
  ]
}
```
