# Native platform input

Original C++17 input state with a C ABI imported by Swift. Holds simultaneous buttons and resets on inactive/background/disappear lifecycle events. Atomic snapshots permit a future engine thread to read the current held state; this is not a queued event API. Ownership must outlive readers.

No X-Ray runtime, game data loader, gameplay, renderer port, or original APK source reconstruction is implemented. Engine availability remains false. The next substantial milestone is an engine implementation compatible with the game data; this adapter alone does not meet it.

Host validation: Tests/InputTests.cpp covers simultaneous presses, repeat presses, independent release, reset, invalid IDs and null handles. Compile with C++17 and Sources/Native/STInput.cpp. AddressSanitizer and UndefinedBehaviorSanitizer passed with leak detection disabled because the host runs under ptrace. iOS compilation is checked separately by GitHub Actions; physical-device behavior is not yet verified.
