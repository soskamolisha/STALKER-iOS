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

## Concrete launcher-to-engine path contract

The launcher currently imports each edition under
`Application Support/ImportedGames/{shadow,clearSky,pripyat}` and passes
that **directory** as `resource_root` to
`st_openxray_runtime_run_with_resources`. The existing native
`XRayBootstrap::attachResources` recognizes a nonempty `gamedata/`
directory or `gamedata.db*` archives at that root; it does **not**
locate or validate an `fsgame.ltx` filesystem configuration. Thus
successful resource preflight cannot imply that upstream
`Core.Initialize` can mount the data.

Before enabling the runtime entry, the engine adapter must resolve a
game-specific filesystem config from the imported layout, validate its
paths against the iOS sandbox, and pass that resolved configuration to
upstream initialization. The archive ABI currently carries only
`(command_line, resource_root)`; the adapter owns the config lookup.
Do not synthesize a generic `fsgame.ltx` without confirming the
required aliases and per-edition resource layout.
