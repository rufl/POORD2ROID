# Build on Windows with WSL

The Android build uses Linux tools. On Windows, build inside WSL2 (Ubuntu or another supported Linux distribution) rather than using the obsolete native-Windows wrapper described in earlier revisions. Current release: [v0.7.5](https://github.com/rufl/POORD2ROID/releases/tag/v0.7.5) · [Changelog](../CHANGELOG.md) · Driver matrix: [`ANDROID_GPU_DRIVER.md`](ANDROID_GPU_DRIVER.md) · [`ANDROID_GPU_DRIVER.pt-BR.md`](ANDROID_GPU_DRIVER.pt-BR.md).

## Prerequisites

- WSL2 with a Linux x86_64 distribution and network access.
- Git and the packages required by the upstream bootstrap/build scripts.
- The Android NDK/API/JDK and Python dependencies installed by `upstream/dm/android/tools/bootstrap.py` and the build guide in `upstream/docs/ANDROID_BUILD.md`.
- .NET SDK 10 and the game files required to generate the local game modules and shader cache. These files are not provided by this repository.

## Build

From the WSL shell, clone this repository into a WSL-accessible path (recommended), then follow the bootstrap and build steps in [`upstream/docs/ANDROID_BUILD.md`](../upstream/docs/ANDROID_BUILD.md). Avoid the obsolete native-Windows wrapper and its old `build-win.py` / `build-tools-win` instructions.

The Android build script is `upstream/dm/android/tools/build.py`. A complete package-only APK requires `--game-run`; without it, the script intentionally builds only the diagnostic NativeActivity probe. Keep any personal game data, keys, and generated protected content outside version control.

Build output is under `upstream/dm/android/build/`. The signing key determines whether Android accepts an APK as an in-place update: use the same signing identity as the installed app or uninstall the old app first (uninstalling can remove app-private data, so back it up).

The current release APK reports `versionName=0.7.5-android-port` and `versionCode=705`; SHA-256 is `2ef8b49537a43766877f0924d62d6cf247146b0d2c5f7c59373890a265423238`. Build with `--game-run --version v0.7.5` to reproduce that manifest identity. Compilation and v3 signing verification passed; final device visual validation remains outstanding.

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
