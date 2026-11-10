# Engine source candidates for STALKER Mobile

Research checkpoint: 2026-11-10. These are upstream source candidates, not iOS-ready binaries.

| Edition | Candidate | Current caveat |
|---|---|---|
| Shadow of Chernobyl | https://github.com/OpenXRay/xray | X-Ray 1.0 source expansion; iOS build and renderer portability not established |
| Clear Sky | https://github.com/OpenXRay/xray-15 | X-Ray 1.5 source expansion; iOS build not established |
| Call of Pripyat | https://github.com/OpenXRay/xray-16 | Main OpenXRay 1.6 codebase; ARM64 support does not imply iOS support |

The OpenXRay xray-16 README lists Call of Pripyat and Clear Sky support, but explicitly says Shadow of Chernobyl is not yet supported. Thus, do not wire all three launch buttons to xray-16 and call it a playable trilogy.

## Practical integration plan

1. Audit upstream source licenses, dependencies, and GSC Game World redistribution conditions before copying any engine source.
2. Identify iOS platform and SDL initialization in the user-supplied port *as interoperability evidence*, without redistributing the extracted binary.
3. Start with a small iOS cross-compilation probe for platform-neutral X-Ray components; do not claim a game engine is integrated.
4. Add an edition-specific engine interface that reports unsupported editions instead of silently attempting a different game.
5. Evaluate rendering, archive mounting, scripting, audio, filesystem, touch controls, and app lifecycle independently.
6. Preserve the requirement that players supply legitimately owned game data.

## Verified user-provided startup flags

`-soc -nointro -nosplash` for the observed iOS Shadow of Chernobyl port. Clear Sky and Call of Pripyat command-line switches remain unverified for the selected engine revision.

## References

- https://github.com/OpenXRay
- https://github.com/OpenXRay/xray-16/blob/dev/README.md
