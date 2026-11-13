# OpenXRay resource-aware entry: upstream investigation

Status: **not implemented or validated on iOS**.

The upstream `src/xrEngine/x_ray.cpp` parses a `-fsltx <path>` option before
`Core.Initialize("OpenXRay", commandLine, true, ...)`. It currently reads
the path with `sscanf(..., "%[^ ] ", fsgame)`, so a filesystem path
containing spaces cannot safely be passed using this interface. The
upstream `src/xr_3da/entry_point.cpp` currently exports only the
experimental `st_xray_embedded_run(const char*)` in our diagnostic workflow.

A correct `st_xray_embedded_run_with_resources(args, root)` needs to:

1. Resolve the actual game-specific `fsgame.ltx` / filesystem config
   location inside the imported root, not assume the root is itself the
   config file. Verify the actual layout for all three games.
2. Pass a validated config path to `Core.Initialize` without relying on
   whitespace-sensitive `-fsltx` parsing. Prefer an explicit upstream
   initialization API over modifying process-wide working directory.
3. Preserve the iOS sandbox, avoid process-wide `chdir`, and avoid
   silently falling back to the legacy entry.
4. Adapt platform lifecycle, renderer, input and audio before describing
   an archive as playable. An exported symbol alone is insufficient.

The archive importer and optional iOS linker intentionally require the
resource-aware export, so the current experimental upstream archive is
rejected. Do not fabricate this export merely to pass symbol validation.

Upstream source references:
- https://github.com/OpenXRay/xray-16/blob/dev/src/xrEngine/x_ray.cpp
- https://github.com/OpenXRay/xray-16/blob/dev/src/xr_3da/entry_point.cpp
