### Search Helpers

Nemo's file and content search utilizes a plugin-type system to allow content search for additional file types.

The only requirements for a helper is the ability to extract text from a given file type and print it to stdout. A definition file provides details necessary to use the helper.

##### Example definition file:

```
[Nemo Search Cat Helper]
TryExec=pdftotext
Exec=pdftotext %s -
MimeType=application/pdf;
Priority=200

```
The `Nemo Search Cat Helper` group name is mandatory.

- The filename must end in `.nemo_search_helper`.
- `TryExec` should be set to the name of the executable (without any arguments). When the helpers are loaded, Nemo will
  check that the program a) exists and b) is executable. If these checks fail, the helper will be skipped. If only a
  program name is provided, it must exist in the user's path. This can also be an absolute path. 
- `Exec` should provide the full command line necessary to extract the text from the file. The `%s` argument will be replaced by
  the file name being processed during content search. Note, uris are not supported, only paths (local files).
- `MimeType`is a semicolon (`;`)-separated list of mimetypes that this search helper should be used with. It must be
  semicolon-terminated (even if there's only a single item).
- `Priority` is a value used to break a tie when multiple helpers support the same mimetype. The higher value wins. In the
  event of a tie, the last helper processed is used (the order is currently undefined). If the `Priority` entry is missing,
  the value is assumed to be 100.
- The `TryExec`, `Exec` and `MimeType` keys are mandatory.

These definition files can be placed in `<datadir>/nemo/search-helpers` where `<datadir>` can be some directory in XDG_DATA_DIRS or under the user's data directory (`~/.local/share/namo/search-helpers`). The user directory is processed last, and takes precedence over the system dirs.

##### Debugging:
If something doesn't seem to be working, you can run nemo with debugging enabled:
```
NEMO_DEBUG=Search nemo --debug
```
This will print out a bit of extra information related to searches.