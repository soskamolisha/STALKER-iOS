# On-device archive header inspection (prototype)

Select **Архив .db** in the running iOS prototype and choose a file from Files. The app calls a small C interface around the existing `scanArchive` C++ reader and reports the number of top-level chunks and high-bit compressed flags. This scan is read-only: it does not extract files, decompress data, detect game identity, or run the engine. It cannot confirm that an arbitrary file is a genuine X-Ray `.db` archive.

CI compiles and runs `Tests/ArchiveProbeTests.cpp` using synthetic local files. No proprietary game resources belong in this repository. The IPA remains unsigned and must be signed/provisioned before installation.
