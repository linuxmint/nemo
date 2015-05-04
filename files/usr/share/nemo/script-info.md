#### Scripts

When executed from a local folder, scripts will be passed 
the selected file names. When executed from a remote folder
(e.g. a folder showing web or ftp content), scripts will
be passed no parameters.

In all cases, the following environment variables will be
set by Nemo, which the scripts may use:

- NEMO_SCRIPT_SELECTED_FILE_PATHS: newline-delimited paths for selected files (only if local)
- NEMO_SCRIPT_SELECTED_URIS: newline-delimited URIs for selected files
- NEMO_SCRIPT_CURRENT_URI: URI for current location
- NEMO_SCRIPT_WINDOW_GEOMETRY: position and size of current window
- NEMO_SCRIPT_NEXT_PANE_SELECTED_FILE_PATHS: newline-delimited paths for selected files in the inactive pane of a split-view window (only if local)
- NEMO_SCRIPT_NEXT_PANE_SELECTED_URIS: newline-delimited URIs for selected files in the inactive pane of a split-view window
- NEMO_SCRIPT_NEXT_PANE_CURRENT_URI: URI for current location in the inactive pane of a split-view window


