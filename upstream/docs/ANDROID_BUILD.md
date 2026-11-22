# Android build and deployment

This page summarizes the Android build from a Linux x86_64 environment. On Windows, use WSL2. Portuguese (Brasil): [`ANDROID_BUILD.pt-BR.md`](ANDROID_BUILD.pt-BR.md). Driver matrix: [`ANDROID_GPU_DRIVER.md`](ANDROID_GPU_DRIVER.md) or [`ANDROID_GPU_DRIVER.pt-BR.md`](ANDROID_GPU_DRIVER.pt-BR.md). Release history: [`CHANGELOG.md`](../../CHANGELOG.md).

## Build types and game data

GitHub Actions builds a generic APK and a pack kit. The generic APK does not contain game data or locally derived game modules; the user imports a data package on the phone. A game-enabled APK can be built locally from the user's own prepared files, but those inputs and generated protected content must stay out of this repository and CI. No game data, firmware, or keys are distributed here.

## Toolchain

Run `python3 dm/android/tools/bootstrap.py` to install the pinned Android NDK (r28c), API 35, JDK, and build tools. The toolchain defaults to `~/dm-work/tools/android`; override it with `DM_ANDROID_TOOLS`. Install the Python dependencies listed by the build scripts in a virtual environment. Shader translation requires the .NET 10 SDK and a first-time NuGet restore needs network access. The scripts fetch pinned `libadrenotools` and `liblinkernsbypass` sources as documented in the GPU-driver notes.

Prepare the user's RomFS and rewritten `main`, `sdk`, and `subsdk0` modules, plus the matching SPIR-V cache. Tools include `extract_packages.py`, `patch_nso.py` with `DM_TARGET=android`, and `build_shaders.py`. Keep all source game packages, keys, and generated game payloads outside Git.

## Build commands

```bash
python3 dm/android/tools/bootstrap.py
python3 dm/android/tools/build.py --game-run --version v0.7.4 \
  --game-source /path/to/prepared/game \
  --shader-cache /path/to/prepared/shaders/spirv
python3 dm/android/tools/ci_checks.py
```

The output is `dm/android/build/D2R-Android-port.apk`. Install it using `adb -s DEVICE install -r`. APK updates require the same signing identity as the installed build; otherwise Android rejects the update. Back up app-private saves before uninstalling an app to change signing identity.

### Current published build

[v0.7.4](https://github.com/rufl/POORD2ROID/releases/tag/v0.7.4) is the current published build. Its APK reports `versionName=0.7.4-android-port` and `versionCode=704`; SHA-256 is `47eb0f3aee346f8b904cbb02eb7a770f8df679263cfa70c4f702501cdb31481d`. A complete package-only APK requires `--game-run`; omitting it produces only the diagnostic NativeActivity probe. This build includes the Java launcher/game activity, `classes.dex`, normal/large touch-layout migration profiles, the Vulkan `currentTransform` fix, and recent native changes. The ARM64 build and v3 signing verification passed; final device visual validation remains outstanding.

## Android behavior and limits

The launcher supports touch, Android game controllers, keyboard input, native name entry, AAudio, and an optional performance overlay. Direct screen touches in menus and panels use the game's UI hit classification before injecting mouse clicks, so taps on empty/covered panel space do not activate controls. In gameplay, the touch zones align with the virtual HUD: tap a skill zone to activate its mapped skill, drag it to aim, tap a belt slot to drink that potion, and swipe a belt slot vertically to expand/collapse extra rows. Expanded belt rows can be tapped to use their potions. These zones use the shared iOS touch-control layout, but Android-specific visual placement and interaction still need device validation. The controller implementation is generic Android input handling; the GameSir G8+ has not been specifically validated. Disabling the stats overlay dismisses its popup instead of leaving a stale frame.
The launcher defaults fresh or legacy-empty settings to `1280×576` (20:9). The Xiaomi 11T display is 2400×1080 (20:9). In wide gameplay, the game may keep a 1280×720 NVN backing allocation after changing its active logical frame to 1280×576. The presenter treats a valid `WindowSetCrop` rectangle as authoritative and aspect-fits that 20:9 content to the display; logical dimensions are only a fallback when the crop is absent or invalid. It must not scale the unused 144-row allocation padding as if it were game content. Front-end and loading pages remain 16:9 and centered at native proportions (at 2400×1080, a 1920×1080 page leaves 240-pixel side bars). UI-page crops remain honored. This fixes the source-frame mismatch in geometry tests; it does not by itself validate that every game HUD widget is drawn or positioned correctly on-device. The imported data package also needs a `ctrl_touch` overlay marker to activate the custom touch layouts; packages without it use the game's original controller layout.
The Switch reference screenshots in the [Use a Potion gallery](https://www.useapotion.com/2021/09/diablo-ii-resurrected-nintendo-switch-review/) are 16:9 captures and are not identified as handheld or docked. [Digital Foundry's Switch analysis summary](https://www.nintenderos.com/2021/10/someten-a-test-el-apartado-tecnico-de-diablo-ii-resurrected-en-nintendo-switch/) reports roughly 900p docked and 720p handheld (with drops reported), which describes pixel count, not an ultrawide layout. Therefore those screenshots are reference for UI arrangement, not proof of mode-specific geometry.
The Android handheld profile inherits `ConsoleHD` rather than the low-resolution `ConsoleLV`; its previous base profile was the source of different panel geometry at handheld resolution. APK builds now carry complete normal and large Android overlays. The large variant scales the prior 1.5× profile another 15% (1.725× source font values) and scales numeric `pointSize` overrides as well; the generated bottom HUD is raised by 48 design pixels, with the matching belt, difficulty and skill touch targets raised by the same amount. At boot an imported overlay with `ctrl_touch`, or the known legacy overlay signature, is prepended by an app-private current Android overlay variant. The selected profile comes from `font_large` (`large` when absent), and APK updates detect the embedded archive digest before recopying; imported game data is not reimported. Host geometry and overlay generation do not prove every game HUD widget is visibly unclipped on the 11T; that still requires a render capture from the target configuration.

APK `0.6.84` adds a narrowly fingerprinted migration for the known v0.6.81 20:9 touch-layout data overlay that was imported without its `ctrl_touch` and `touch_strings.tsv` sidecars. With the matching `.build.config` and overlay archive present, it supplies those two small files from APK assets in app-private storage and prepends that directory to the existing overlay search path; it does not overwrite or reimport the multi-gigabyte game package. Font-only or other overlays do not match the migration signature. Runtime log proof is `[android-overlay-migration] repaired legacy touch overlay...`; an actual gameplay screenshot is still required to confirm the HUD is drawn.

APK `0.6.85` extends that exact-signature migration to a complete 7,009,616-byte Android overlay. Generate the HUD with `DM_ANDROID_HUD=1`: unlike iOS, Android has no UIKit `drawV4` layer, so this variant preserves the native HP/MP values, XP bar and skill widgets instead of hiding them. The build packages `.build.config` as `build.config` because AAPT drops dotfiles, verifies all migration entries in the final APK, and native restores the original filename in app-private storage. No full game-data reimport is required for the known legacy overlay. Driver preparation now hashes the actual copied library, not only its checksum marker. The 48-design-pixel Android HUD lift also moves the corresponding belt, difficulty and skill touch targets; `t_touch_controls` passes 47/47 cases and 1580 checks. These are offline validations; visible HUD restoration remains a device-test requirement.

APK `0.6.86` connects Android panel gestures to the existing host `dm_ui_drag_begin/move/end` path. Vertical drags on generic controls scroll lists; horizontal drags drive sliders. Item drag-and-drop, tabs, modal-blocked areas and outside-panel taps retain their separate handlers. `[android-ui-gesture] kind=scroll|slider` records the selected path. The gesture-classification regression and all 13 portable suites pass. This does not replace per-menu device validation.

APK `0.6.87` adds explicit docked/handheld touch-coordinate tests and `[android-touch-map]` logging with actual `dm_opmode`, raw surface coordinates and mapped game coordinates. Gameplay tests cover 1280x576 and 1020x459 full-screen 20:9 identity mapping. Menu tests cover 1280x720 and 1020x572 centered output, including edge and side-bar mapping. The handheld menu rounds to 1926x1080 at x=237 on a 2400x1080 surface; this is integer-pixel rounding, not stretched gameplay.

APK `0.6.88` adds a launcher `Gameplay aspect` toggle. Its initial fixed 1280-wide comparison bases were superseded because they did not produce native Switch handheld resolutions. The label is restored from the existing saved resolution, so there is no parallel settings file or native setting to diverge. `LaunchAspectTest` covers restoration, both selections and malformed legacy input; all 14 portable suites pass. This control is intended for same-scene screenshot comparison.

APK `0.6.89` corrects the toggle for both operation profiles. The stored value remains the docked base: 16:9 selects 1600x900, producing 1280x720 handheld; 20:9 selects 1600x720, producing 1280x576 handheld. Geometry tests cover both effective pairs and all 14 portable suites pass.

APK `0.6.90` initially sent the command-overlay font buttons through `OptionsPanelMessage:SetLowVisionMode`. APK `0.6.94` supersedes that route because generated Android profiles do not support the game's low-vision rebuild: the buttons now persist a complete normal / large overlay choice for the next launch.

APK `0.6.91` updates the bundled Mali drivers to their latest upstream releases: MaliSX2 `0.4.1` (commit `a833123`, SHA-256 `feb44fcb...`) and JimVulkan PanVK `r10` (commit `58f2c55`, SHA-256 `7e97db4d...`). PanVK r10 adds query wait spin reduction and stale render target fixes on Mali. Both driver archives and libraries are verified by checksum. All 14 portable suites pass.

APK `0.6.93` fixes the stale migration archive carried by `0.6.92`: Android preserves the native Display / Video route (`@OptionsMenuVideo` -> `VideoOptionsPanel`) and moves Custom controls to the original Controls / `KeyBindingMenu` route. The migration now embeds the regenerated overlay, so updating the APK replaces the legacy 20:9 overlay without reimporting the multi-gigabyte game package. VersionCode 693; SHA-256 `f79f906437d6c91efdc7841830094ab446e8cc0a56119c1af7c840fef4809de2`; Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.93-display-restored.apk`. Device visual confirmation remains required.

APK `0.6.94` removes the runtime `OptionsPanelMessage:SetLowVisionMode` request: generated Android profiles already contain the supported font layout, while the game’s low-vision rebuild is incompatible with them. The command-overlay − / + buttons now store the normal / large complete-overlay choice for the next launch, so neither mode can call that crash path. Fresh launcher settings use a 60 Hz present cap; an explicitly saved 30 Hz or display-following setting remains unchanged. The 3D quality selection now persists and native applies it before the guest starts, so Auto / High / Balanced / Performance / Ultra work consistently in both handheld and docked profiles. Sustained FPS remains device-, scene-, thermal- and driver-bound. VersionCode 694; SHA-256 `062cc469a976a30858f01cc53e5289fce5fad55a647baf012e77ee87007421e7`; Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.94-hud-font-fps.apk`.

APK `0.6.109` synchronizes Android's device-level controller IDs into the native input queue. A controller that exposes real axes is still filtered separately from Xiaomi's axisless `uinput-xiaomi`, but its key and motion events can now be recognized by device ID when Bluetooth firmware supplies an incomplete or incorrect event source mask. The normal Android mappings remain unchanged: left stick X/Y, right stick Z/RZ with RX/RY fallback, HAT D-pad, trigger axes, and standard `KEYCODE_BUTTON_*` buttons. This targets the GameSir G8 Plus without requiring the GameSir mapping app. The 14 portable checks, Java/native APK compilation, signing, and embedded-overlay audit pass; physical G8 Plus input still requires device validation.

The launcher exposes System Vulkan and hardware-gated MaliSX2, JimVulkan PanVK, and Turnip. MaliSX2 and PanVK require Mali/kbase; Turnip requires Qualcomm KGSL. The Mali path has no phone-model allowlist: any Mali device exposing `/dev/mali0` is capability-probed, and the native log classifies known Mali-G31/G51/G52/G71/G72/G76 Bifrost and G57/G68/G77/G78/G78AE/G310/G510/G610/G710/G615/G715 Valhall names. The bundled MaliSX2 0.5.1 and PanVK r10 remain experimental; driver availability does not prove that a driver can present or run D2R. Custom selection is strict; explicitly select System Vulkan to recover. See [`ANDROID_GPU_DRIVER.zh-CN.md`](ANDROID_GPU_DRIVER.zh-CN.md) and the root [`THIRD-PARTY-NOTICES.md`](../../THIRD-PARTY-NOTICES.md).

Optional GPU BC texture decoding can be enabled with `bc_decode_gpu 1` in app-private `files/temp/perf.conf`; default `0` uses CPU decoding. BC6H/BC7 and unsupported texture shapes remain on CPU, and setup failures fall back. This option can lose performance when GPU-bound; compare sustained frame times on the target device.

The app persists Vulkan pipeline cache and can warm pipelines from its manifest. Warm-up reduces some repeat stalls; combinations not seen before can still compile during play. Mali-specific behavior remains device/driver-dependent.

## Logs

```bash
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

Logcat reports launcher driver choice and native driver-loading/identity diagnostics. The app-private game log receives native output after host startup; it may be absent when failure occurs earlier. For a bug report, include device model, Android version, GPU, selected driver, app version, reproduction steps, and relevant logs. Do not include game files, keys, or saves.
