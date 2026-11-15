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

## User confirmation (v14)

The standalone `STArchiveImportView` now asks for explicit confirmation before beginning an import, including a warning that the previous imported archive set will be replaced. Import runs off the main thread, and the store performs a free-space preflight. The UI currently displays an indeterminate progress indicator, **not** byte-level progress. No successful device build or end-to-end import has been verified for this change.

Remaining blockers: identify the actual launcher entry view and connect this screen; verify XcodeGen and GitHub Actions build; add cancellation and incremental progress; implement X-Ray archive validation and native runtime separately.
