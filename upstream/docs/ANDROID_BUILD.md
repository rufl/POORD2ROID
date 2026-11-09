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
The launcher defaults fresh or legacy-empty settings to `1280×576` (20:9). The Xiaomi 11T display is 2400×1080 (20:9). In wide gameplay, the game may keep a 1280×720 NVN backing allocation after changing its active logical frame to 1280×576. The presenter treats a valid `WindowSetCrop` rectangle as authoritative and aspect-fits that 20:9 content to the display; logical dimensions are only a fallback when the crop is absent or invalid. It must not scale the unused 144-row allocation padding as if it were game content. Front-end and loading pages remain 16:9 and centered at native proportions (at 2400×1080, a 1920×1080 page leaves 240-pixel side bars). UI-page crops remain honored. This fixes the source-frame mismatch in geometry tests; it does not by itself validate that every game HUD widget is drawn or positioned correctly on-device. The imported data package also needs a `ctrl_touch` overlay marker to activate the custom touch layouts; packages without it use the game's original controller layout.
The Switch reference screenshots in the [Use a Potion gallery](https://www.useapotion.com/2021/09/diablo-ii-resurrected-nintendo-switch-review/) are 16:9 captures and are not identified as handheld or docked. [Digital Foundry's Switch analysis summary](https://www.nintenderos.com/2021/10/someten-a-test-el-apartado-tecnico-de-diablo-ii-resurrected-en-nintendo-switch/) reports roughly 900p docked and 720p handheld (with drops reported), which describes pixel count, not an ultrawide layout. Therefore those screenshots are reference for UI arrangement, not proof of mode-specific geometry.
The Android handheld profile now inherits `ConsoleHD` rather than the low-resolution `ConsoleLV`; its previous base profile was the source of different panel geometry at handheld resolution. The current large-font overlay scales the prior 1.5× profile another 15% (1.725× source font values) and scales numeric `pointSize` overrides as well. The generated bottom HUD is raised by 48 design pixels to clear the lower system-gesture region without resizing its art or hitboxes. These changes are source/data-overlay changes: rebuilding the game-data overlay is required; the generic APK alone does not update imported game data. Host geometry and overlay generation pass, but a successful build does not prove every in-game panel/HUD is visibly unclipped on the 11T. That requires a render capture from the target configuration.

APK `0.6.84` adds a narrowly fingerprinted migration for the known v0.6.81 20:9 touch-layout data overlay that was imported without its `ctrl_touch` and `touch_strings.tsv` sidecars. With the matching `.build.config` and overlay archive present, it supplies those two small files from APK assets in app-private storage and prepends that directory to the existing overlay search path; it does not overwrite or reimport the multi-gigabyte game package. Font-only or other overlays do not match the migration signature. Runtime log proof is `[android-overlay-migration] repaired legacy touch overlay...`; an actual gameplay screenshot is still required to confirm the HUD is drawn.

APK `0.6.85` extends that exact-signature migration to a complete 7,009,616-byte Android overlay. Generate the HUD with `DM_ANDROID_HUD=1`: unlike iOS, Android has no UIKit `drawV4` layer, so this variant preserves the native HP/MP values, XP bar and skill widgets instead of hiding them. The build packages `.build.config` as `build.config` because AAPT drops dotfiles, verifies all migration entries in the final APK, and native restores the original filename in app-private storage. No full game-data reimport is required for the known legacy overlay. Driver preparation now hashes the actual copied library, not only its checksum marker. The 48-design-pixel Android HUD lift also moves the corresponding belt, difficulty and skill touch targets; `t_touch_controls` passes 47/47 cases and 1580 checks. These are offline validations; visible HUD restoration remains a device-test requirement.

APK `0.6.86` connects Android panel gestures to the existing host `dm_ui_drag_begin/move/end` path. Vertical drags on generic controls scroll lists; horizontal drags drive sliders. Item drag-and-drop, tabs, modal-blocked areas and outside-panel taps retain their separate handlers. `[android-ui-gesture] kind=scroll|slider` records the selected path. The gesture-classification regression and all 13 portable suites pass. This does not replace per-menu device validation.

APK `0.6.87` adds explicit docked/handheld touch-coordinate tests and `[android-touch-map]` logging with actual `dm_opmode`, raw surface coordinates and mapped game coordinates. Gameplay tests cover 1280x576 and 1020x459 full-screen 20:9 identity mapping. Menu tests cover 1280x720 and 1020x572 centered output, including edge and side-bar mapping. The handheld menu rounds to 1926x1080 at x=237 on a 2400x1080 surface; this is integer-pixel rounding, not stretched gameplay.

APK `0.6.88` adds a launcher `Gameplay aspect` toggle. Its initial fixed 1280-wide comparison bases were superseded because they did not produce native Switch handheld resolutions. The label is restored from the existing saved resolution, so there is no parallel settings file or native setting to diverge. `LaunchAspectTest` covers restoration, both selections and malformed legacy input; all 14 portable suites pass. This control is intended for same-scene screenshot comparison.

APK `0.6.89` corrects the toggle for both operation profiles. The stored value remains the docked base: 16:9 selects 1600x900, producing 1280x720 handheld; 20:9 selects 1600x720, producing 1280x576 handheld. Geometry tests cover both effective pairs and all 14 portable suites pass.

APK `0.6.90` adds two native font-level buttons to the Android command overlay: − requests the normal font and + requests the game's low-vision large font. The request is queued to the game main thread and delivered to the UI root via `OptionsPanelMessage:SetLowVisionMode`; it is not a write to original widget objects. `font_large=0/1` is persisted in launch settings, validated and published as `DM_FONT_LARGE`. The current implementation has two levels, matching the game's native accessibility mode; visual confirmation on the device remains required.

APK `0.6.91` updates the bundled Mali drivers to their latest upstream releases: MaliSX2 `0.4.1` (commit `a833123`, SHA-256 `feb44fcb...`) and JimVulkan PanVK `r10` (commit `58f2c55`, SHA-256 `7e97db4d...`). PanVK r10 adds query wait spin reduction and stale render target fixes on Mali. Both driver archives and libraries are verified by checksum. All 14 portable suites pass.

The launcher exposes System Vulkan and hardware-gated MaliSX2, JimVulkan PanVK, and Turnip. MaliSX2 and PanVK require Mali/kbase; Turnip requires Qualcomm KGSL. Driver availability does not prove that the driver can present or run D2R. Custom selection is strict; explicitly select System Vulkan to recover. MaliSX2 0.4.1 and JimVulkan PanVK r10 are checksum-pinned and their notices/licenses are bundled. Upstream lists Mali-G77 support, but D2R presentation, Xiaomi 11T compatibility, and performance are not guaranteed. PanVK remains experimental. See [`ANDROID_GPU_DRIVER.zh-CN.md`](ANDROID_GPU_DRIVER.zh-CN.md) and the root [`THIRD-PARTY-NOTICES.md`](../../THIRD-PARTY-NOTICES.md).

Optional GPU BC texture decoding can be enabled with `bc_decode_gpu 1` in app-private `files/temp/perf.conf`; default `0` uses CPU decoding. BC6H/BC7 and unsupported texture shapes remain on CPU, and setup failures fall back. This option can lose performance when GPU-bound; compare sustained frame times on the target device.

The app persists Vulkan pipeline cache and can warm pipelines from its manifest. Warm-up reduces some repeat stalls; combinations not seen before can still compile during play. Mali-specific behavior remains device/driver-dependent.

## Logs

```bash
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

Logcat reports launcher driver choice and native driver-loading/identity diagnostics. The app-private game log receives native output after host startup; it may be absent when failure occurs earlier. For a bug report, include device model, Android version, GPU, selected driver, app version, reproduction steps, and relevant logs. Do not include game files, keys, or saves.
