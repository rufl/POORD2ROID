# POORD2ROID — Diablo II: Resurrected on Android on Mali-G77 MC9 / Xiaomi 11T
- [Português](README.md) · [Latest release v0.7.5](https://github.com/rufl/POORD2ROID/releases/tag/v0.7.5) · [Changelog](CHANGELOG.md) · [Complete driver matrix](docs/ANDROID_GPU_DRIVER.md) · [Source](https://github.com/rufl/POORD2ROID) · [Licenses and credits](THIRD-PARTY-NOTICES.md)

An experimental Android port for running the Nintendo Switch version of **Diablo II: Resurrected** on compatible Android devices. It runs the game's ARM64 code through an NVN→Vulkan compatibility layer; it is not a conventional emulator.

> **Status:** experimental. Compatibility, stability, and performance vary by device, GPU, driver, and Android version. No specific compatibility or frame-rate target is guaranteed. See [driver status and compatibility](#drivers-and-compatibility).

## Requirements and game data

- An ARM64 Android device with working Vulkan support. Driver choices depend on hardware; see the table below.
- Game data and keys legally obtained by the user. **This repository and its APK do not include the game, ROM/NSP/XCI, firmware, keys, or protected content.** We do not provide instructions for obtaining or bypassing protections on those materials.
- Sufficient storage for imported data and caches.

The APK contains the launcher and port, not game data. Use the import flow supported by the app/build. Back up existing saves before changing app data.

## Install and run

1. Get the current APK from [v0.7.5](https://github.com/rufl/POORD2ROID/releases/tag/v0.7.5), browse all [Releases](https://github.com/rufl/POORD2ROID/releases), or build it using [`docs/BUILD_ON_WINDOWS.md`](docs/BUILD_ON_WINDOWS.md). Install only an APK whose source and signing identity you trust.
2. Open the launcher and import a data package prepared from your own game files.
3. Start with **System Vulkan**. Try alternative drivers one at a time and only when listed for your device.
4. Launch the game. For failures, collect logs as described in the build guide and include device model, Android version, GPU, selected driver, and relevant logs in an issue. Never attach keys or game files.

Release v0.7.5 corrects the Android surface orientation selection: the native game framebuffer now uses the earlier identity-preferred Vulkan `preTransform` behavior, with `currentTransform` only as fallback. It retains the complete package-only APK, Java launcher/game activity, `classes.dex`, normal/large touch-layout profiles, and recent native changes. No Android device was attached for final visual confirmation; version, size, and SHA-256 are recorded in the [changelog](CHANGELOG.md).

The launcher supports touch controls and Android/HID game controllers. The GameSir G8+ has not been specifically validated; operation depends on how Android exposes it as a compatible controller.

## Drivers and compatibility

The launcher lists only options matching detected hardware and packaged assets; visibility does not mean a driver can render the game correctly. Custom driver selections are strict: a startup failure is not a silent fallback. Explicitly select **System Vulkan** to recover.

| Option | Known hardware/status |
| --- | --- |
| **System Vulkan** | Manufacturer Vulkan driver; recommended baseline and recovery path. |
| **MaliSX2 0.5.1** | Mali kbase via `/dev/mali0`, Android API 29+. Preferred for G31/G51/G52/G57/G68/G76/G77/G78/G78AE/G310/G510/G610/G710/G615/G715. Experimental and no MSAA. |
| **JimVulkan PanVK r10** | Mali kbase via `/dev/mali0`, Android API 31+. Preferred for G71/G72 and alternate for Bifrost/G77-class devices. Experimental/non-conformant. |
| **Mesa Turnip T30** | Qualcomm/Adreno via KGSL, Android API 30+. Never use on Mali. |

On Xiaomi 11T (Dimensity 1200, Mali-G77 MC9), the expected choices are System Vulkan, MaliSX2, and PanVK when their packages are available. Passing driver initialization does not prove stable gameplay. Previous user reports found MaliSX2 the most stable option in their tests, but this is not a reproducible benchmark or a guarantee.

### Display, frame pacing, and performance

Fresh settings request a 20:9 world view at 1280×576 and cap presentation at 30 Hz/FPS; this does not guarantee sustained 30 FPS. On Android, the world keeps the wide view while the HUD uses a centered logical 16:9 canvas, with UI touches mapped to the same safe area. Menus and loading screens remain centered at 16:9. Geometry tests and the Android build cover the change; final visual validation of the HUD, map, dialogs, and controls still requires the device.

Optional GPU BC texture decoding is disabled by default. It can move work from the CPU to the GPU, but may not help GPU-bound scenes. Compare sustained frame times before enabling it. Pipeline caching and warm-up reduce some repeated compilation; they do not eliminate all in-game shader compilation.

## Build and diagnostics

The Android build runs on Linux/WSL; see [`docs/BUILD_ON_WINDOWS.md`](docs/BUILD_ON_WINDOWS.md) for prerequisites and steps. `upstream/` contains the imported base and integrated changes; [`UPSTREAM.md`](UPSTREAM.md) documents provenance and snapshot boundaries.

Collect device logs with:

```sh
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

Logcat records driver selection, preparation, and failures. `files/game.log` contains native output after the host starts. It may not exist or contain useful data if startup fails earlier.

## Credits and licenses

This fork builds on **jhaizhou's** [d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile) and **sydeEvans's** Android port [d2r-android](https://github.com/sydeEvans/d2r-android). It also integrates or redistributes third-party components, including Mesa Turnip, MaliSX2, JimVulkan PanVK, AdrenoTools, and vendored libraries. [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) records sources, licenses, and redistribution boundaries; [UPSTREAM.md](UPSTREAM.md) describes code provenance.

Eden, LASRecomp, AMD FSR, and Qualcomm SGSR were consulted only as design/research references unless the notices say otherwise; their code is not incorporated as a dependency of this port. Preserve all applicable credits and license terms.

Diablo II: Resurrected and Nintendo Switch are trademarks of their respective owners. This project is unofficial and is not endorsed by Blizzard or Nintendo.
