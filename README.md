# STALKER Mobile — iOS prototype

**Goal:** explore a native iOS port for iPhone 15 Pro Max. **This is not the S.T.A.L.K.E.R. game and is not an installable IPA.**

## Implemented
- SwiftUI landscape-oriented prototype with touch input press/release events
- MetalKit drawable with a small GPU-rendered triangle (proof of concept only)
- A stub `GameEngine` interface that explicitly reports that X-Ray is unavailable
- XcodeGen `project.yml` for generating an Xcode project
- GitHub Actions macOS build workflow for simulator and unsigned device compilation

## Build from a Mac
Install Xcode and XcodeGen, then:
```sh
xcodegen generate
xcodebuild -project STALKERiOSStarter.xcodeproj -scheme STALKERiOSStarter -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build
```

Or visit **Actions → Build iOS starter → Run workflow**. Check the job logs for actual build success or failure. An uploaded unsigned device app ZIP cannot be installed on an iPhone without code signing. The workflow is not a signed IPA exporter.

## Critical blockers
1. Identify and obtain compatible, appropriately licensed X-Ray engine source. Android ARM64 ELF libraries from the APK are not compatible with iOS Mach-O.
2. Port the actual X-Ray renderer, platform APIs, audio, filesystem, and lifecycle.
3. Verify the engine variant supports *Shadow of Chernobyl*.
4. Supply legally obtained original game data; do not commit it to this public repository.
5. Configure Apple signing and provisioning for a device-installable IPA.

See `APK_AUDIT.md` and `PORTING.md` for details.

**No successful CI run has been verified in this project documentation.**
