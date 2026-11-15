# Persistent archive import (v13)

`STArchiveStore.importArchives(from:)` copies `gamedata.dbN` files from a user-selected folder into the app's Documents/ImportedGame directory.

- Run this on a background queue, **not** on the UI thread: files can be several GB.
- The source folder is never modified.
- A temporary staging folder prevents partial imports from replacing a previous import.
- Only filenames and copied byte counts are validated; **this does not validate X-Ray archives**.
- The destination is in Documents and remains available after the file picker closes.
- Do not call this from a UI until progress, cancellation, storage quota, and error handling have been reviewed.
- Never distribute game archives with the IPA.
- Tests exist but have not yet been run in iOS CI.

Future work: free-space preflight, stream hashing, cancellation/progress, secure file coordination, and explicit UI confirmation before replacing existing game data. The OpenXRay runtime is still not present.
