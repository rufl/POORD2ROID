# Build on Windows with WSL

The Android build uses Linux tools. On Windows, build inside WSL2 (Ubuntu or another supported Linux distribution) rather than using the obsolete native-Windows wrapper described in earlier revisions.

## Prerequisites

- WSL2 with a Linux x86_64 distribution and network access.
- Git and the packages required by the upstream bootstrap/build scripts.
- The Android NDK/API/JDK and Python dependencies installed by `upstream/dm/android/tools/bootstrap.py` and the build guide in `upstream/docs/ANDROID_BUILD.md`.
- .NET SDK 10 and the game files required to generate the local game modules and shader cache. These files are not provided by this repository.

## Build

From the WSL shell, clone this repository into a WSL-accessible path (recommended), then follow the bootstrap and build steps in [`upstream/docs/ANDROID_BUILD.md`](../upstream/docs/ANDROID_BUILD.md). Avoid the obsolete native-Windows wrapper and its old `build-win.py` / `build-tools-win` instructions.

The Android build script is `upstream/dm/android/tools/build.py`. A game-enabled build requires locally prepared game sources and shader data; a generic launcher APK does not contain game assets. Keep any personal game data, keys, and generated protected content outside version control.

Build output is under `upstream/dm/android/build/`. The signing key determines whether Android accepts an APK as an in-place update: use the same signing identity as the installed app or uninstall the old app first (uninstalling can remove app-private data, so back it up).

## Device diagnostics

For driver selection and Android startup:

```sh
adb logcat -s D2RAndroid
```

After the native host has started, collect its log with:

```sh
adb shell run-as org.d2r.android.port cat files/game.log
```

The native log may be absent if startup fails before host initialization. Do not share game assets, keys, or personal save data when reporting issues.

## Limits

Successful compilation does not prove that a particular GPU driver can present frames or run the game. Use System Vulkan as the baseline, then test eligible alternative drivers separately. Device-specific performance, display safe areas, and controller behavior require on-device validation. See the root README for the current compatibility summary and the third-party notices for binary provenance and licensing.
