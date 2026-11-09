# STALKER Mobile — iOS starter (not a game port)

Target: iPhone 15 Pro Max, iOS 27, arm64.

This repository contains a **SwiftUI placeholder interface only**. It does not contain a runnable X-Ray engine, original S.T.A.L.K.E.R. game data, or an IPA.

## What is needed
1. Obtain appropriately licensed, buildable X-Ray engine sources and confirm Shadow of Chernobyl support.
2. Port graphics, filesystem, audio, touch input and app lifecycle to iOS.
3. Integrate the engine with this UI.
4. Add legally obtained original game assets.
5. Create an Xcode iOS app target, configure Apple signing, build on macOS/cloud macOS CI, and test on-device.

## Current state
The file `Sources/StalkerStarterApp.swift` is a placeholder SwiftUI screen, **not a complete Xcode project**. GitHub Actions alone cannot convert an Android APK into an IPA.
