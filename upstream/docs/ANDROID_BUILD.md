# Android build and deployment

This page summarizes the Android build from a Linux x86_64 environment. On Windows, use WSL2 and see the fork's [`docs/BUILD_ON_WINDOWS.md`](../../docs/BUILD_ON_WINDOWS.md). The upstream source tree retains additional language-specific documentation; this page is the English build reference.

## Build types and game data

GitHub Actions builds a generic APK and a pack kit. The generic APK does not contain game data or locally derived game modules; the user imports a data package on the phone. A game-enabled APK can be built locally from the user's own prepared files, but those inputs and generated protected content must stay out of this repository and CI. No game data, firmware, or keys are distributed here.

## Toolchain

Run `python3 dm/android/tools/bootstrap.py` to install the pinned Android NDK (r28c), API 35, JDK, and build tools. The toolchain defaults to `~/dm-work/tools/android`; override it with `DM_ANDROID_TOOLS`. Install the Python dependencies listed by the build scripts in a virtual environment. Shader translation requires the .NET 10 SDK and a first-time NuGet restore needs network access. The scripts fetch pinned `libadrenotools` and `liblinkernsbypass` sources as documented in the GPU-driver notes.

Prepare the user's RomFS and rewritten `main`, `sdk`, and `subsdk0` modules, plus the matching SPIR-V cache. Tools include `extract_packages.py`, `patch_nso.py` with `DM_TARGET=android`, and `build_shaders.py`. Keep all source game packages, keys, and generated game payloads outside Git.

## Build commands

```bash
python3 dm/android/tools/bootstrap.py
python3 dm/android/tools/build.py --game-run \
  --game-source /path/to/prepared/game \
  --shader-cache /path/to/prepared/shaders/spirv
python3 dm/android/tools/ci_checks.py
```

The output is `dm/android/build/D2R-Android-port.apk`. Install it using `adb -s DEVICE install -r`. APK updates require the same signing identity as the installed build; otherwise Android rejects the update. Back up app-private saves before uninstalling an app to change signing identity.

## Android behavior and limits

The launcher supports touch, Android game controllers, keyboard input, native name entry, AAudio, and an optional performance overlay. Direct screen touches in menus and panels use the game's UI hit classification before injecting mouse clicks, so taps on empty/covered panel space do not activate controls. In gameplay, the touch zones align with the virtual HUD: tap a skill zone to activate its mapped skill, drag it to aim, tap a belt slot to drink that potion, and swipe a belt slot vertically to expand/collapse extra rows. Expanded belt rows can be tapped to use their potions. These zones use the shared iOS touch-control layout, but Android-specific visual placement and interaction still need device validation. The controller implementation is generic Android input handling; the GameSir G8+ has not been specifically validated. Disabling the stats overlay dismisses its popup instead of leaving a stale frame.
The launcher exposes in-world 20:9 render sizes and defaults fresh or legacy-empty settings to `1280×576`. Menus and loading retain the game's 16:9 output. During gameplay, UI globals, HUD layout, touch mapping, Vulkan viewport, and scissor stay on the same full wide render dimensions. The previous Android path reduced game UI dimensions to a centered 16:9 canvas and separately restricted the Vulkan viewport/scissor; that disagreed with the Switch HUD layout's full-screen anchors and clipped gameplay panels/HUD. That path is removed. For the 11T's 20:9 profile, the expected gameplay render target is wide and its full viewport/scissor cover all source pixels (for example, `1920×864` at the handheld scale). This mapping is covered by host geometry tests; actual device rendering must still be confirmed on-device.

The launcher exposes System Vulkan and hardware-gated MaliSX2, JimVulkan PanVK, and Turnip. MaliSX2 and PanVK require Mali/kbase; Turnip requires Qualcomm KGSL. Driver availability does not prove that the driver can present or run D2R. Custom selection is strict; explicitly select System Vulkan to recover. MaliSX2 0.4.0 is checksum-pinned and its license is bundled. Upstream lists Mali-G77 support, but D2R presentation, Xiaomi 11T compatibility, and performance are not guaranteed. PanVK remains experimental. See [`ANDROID_GPU_DRIVER.zh-CN.md`](ANDROID_GPU_DRIVER.zh-CN.md) and the root [`THIRD-PARTY-NOTICES.md`](../../THIRD-PARTY-NOTICES.md).

Optional GPU BC texture decoding can be enabled with `bc_decode_gpu 1` in app-private `files/temp/perf.conf`; default `0` uses CPU decoding. BC6H/BC7 and unsupported texture shapes remain on CPU, and setup failures fall back. This option can lose performance when GPU-bound; compare sustained frame times on the target device.

The app persists Vulkan pipeline cache and can warm pipelines from its manifest. Warm-up reduces some repeat stalls; combinations not seen before can still compile during play. Mali-specific behavior remains device/driver-dependent.

## Logs

```bash
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

Logcat reports launcher driver choice and native driver-loading/identity diagnostics. The app-private game log receives native output after host startup; it may be absent when failure occurs earlier. For a bug report, include device model, Android version, GPU, selected driver, app version, reproduction steps, and relevant logs. Do not include game files, keys, or saves.
