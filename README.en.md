# POORD2OID — Diablo II: Resurrected on Android

[Português](README.md) · [Source](https://github.com/rufl/POORD2OID) · [Licenses and credits](THIRD-PARTY-NOTICES.md)

An experimental Android port for running the Nintendo Switch version of **Diablo II: Resurrected** on compatible Android devices. It runs the game's ARM64 code through an NVN→Vulkan compatibility layer; it is not a conventional emulator.

> **Status:** experimental. Compatibility, stability, and performance vary by device, GPU, driver, and Android version. No specific compatibility or frame-rate target is guaranteed. See [driver status and compatibility](#drivers-and-compatibility).

## Requirements and game data

- An ARM64 Android device with working Vulkan support. Driver choices depend on hardware; see the table below.
- Game data and keys legally obtained by the user. **This repository and its APK do not include the game, ROM/NSP/XCI, firmware, keys, or protected content.** We do not provide instructions for obtaining or bypassing protections on those materials.
- Sufficient storage for imported data and caches.

The APK contains the launcher and port, not game data. Use the import flow supported by the app/build. Back up existing saves before changing app data.

## Install and run

1. Get an APK from [Releases](https://github.com/rufl/POORD2OID/releases), or build it using [`docs/BUILD_ON_WINDOWS.md`](docs/BUILD_ON_WINDOWS.md). Install only an APK whose source and signing identity you trust.
2. Open the launcher and import a data package prepared from your own game files.
3. Start with **System Vulkan**. Try alternative drivers one at a time and only when listed for your device.
4. Launch the game. For failures, collect logs as described in the build guide and include device model, Android version, GPU, selected driver, and relevant logs in an issue. Never attach keys or game files.

The launcher supports touch controls and Android/HID game controllers. The GameSir G8+ has not been specifically validated; operation depends on how Android exposes it as a compatible controller.

## Drivers and compatibility

The launcher lists only options matching detected hardware and packaged assets; visibility does not mean a driver can render the game correctly. Custom driver selections are strict: a startup failure is not a silent fallback. Explicitly select **System Vulkan** to recover.

| Option | Known hardware/status |
| --- | --- |
| **System Vulkan** | The device manufacturer's Vulkan driver; recommended baseline for diagnosis. |
| **MaliSX2 0.4.0** | Third-party package for Mali/kbase devices. Bundled with its license and checksums; upstream lists Mali-G77, but D2R compatibility and performance on Xiaomi 11T are not guaranteed. |
| **JimVulkan PanVK r9** | Experimental Mali/kbase driver. It may load and initialize without presenting frames or running the game; treat it as experimental. |
| **Mesa Turnip T30** | Qualcomm/Adreno KGSL build. Not an option for the Xiaomi 11T's Mali-G77. |

On Xiaomi 11T (Dimensity 1200, Mali-G77 MC9), the expected choices are System Vulkan, MaliSX2, and PanVK when their packages are available. Passing driver initialization does not prove stable gameplay. Previous user reports found MaliSX2 the most stable option in their tests, but this is not a reproducible benchmark or a guarantee.

### Display, frame pacing, and performance

Fresh settings request a 20:9 game view at 1280×576 and a 30 Hz/FPS presentation cap, according to the launcher's initial preferences. These are configuration requests, not a guarantee of sustained 30 FPS; demanding scenes, shader compilation, temperature, and driver limits can lower performance. Menus and loading screens may remain centered at 16:9. Android cutouts and system insets can change the usable area; verify HUD, map, dialogs, and controls on-device.

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
