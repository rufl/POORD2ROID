# Durable project instructions

These notes persist the user's engineering expectations and the current Xiaomi 11T HUD incident across Paseo sessions. Read before continuing Android UI, rendering, packaging, or device-test work.

## User and device

- User values verified outcomes over assurances, speed, or a green checkbox. They have repeatedly had to retest broken builds and redownload large data packages; do not make them repeat avoidable work.
- Target: Xiaomi 11T, Android 14, 2400×1080 display (20:9), Mali-G77 MC9. User reported MaliSX2 as the only consistently stable driver so far; treat this as user-reported live evidence. Do not imply PanVK or fallback is stable without fresh evidence.
- Keep responses direct and candid. Never claim “fixed” based only on source inspection, a successful build, a geometry calculation, or a package link. Say exactly what was exercised and what remains unverified.

## Engineering/test rules learned from this history

- Separate the rendering path, game UI layout, touch/control overlay, and Android display surface. A correctly fitted Vulkan viewport does not establish that the game's HUD is drawn or correctly placed.
- For 20:9, preserve proportions and all expected game UI. No crop, zoom, stretch, or “fills screen” checkbox that hides HUD, loading art, dialogs, or menus. Inspect the game's actual layout JSON/anchors and coordinate transforms before changing geometry.
- Validate representative surfaces independently: in-game HUD/orbs/belt/skills, gameplay messages, inventory/character/skill panels, loading screen, options, and front-end character/menu. Screenshot or geometry evidence must match the actual logical/source rectangle and physical destination.
- Distinguish app/APK changes from imported game-data changes. Installing an APK does not update external RomFS or overlays. State whether a new APK, data package, both, or neither is required before sharing artifacts.
- Avoid repeat multi-gigabyte downloads. Reuse user-provided data and build a replacement package only when required; clearly state actual size and whether any game data changed. Validate package contents, manifest counts/bytes, ZIP CRC, signing identity/version, and HTTP availability before calling an artifact ready.
- For device claims, inspect fresh `game.log`/logcat and the actual selected driver/layout. A dropdown label is not proof of the backend loaded. A stopped ADB connection means the live state is unknown; do not infer success or failure beyond the screenshot and available server logs.
- Do not make the user perform repeated blind test cycles. A test request must include a precise expected log line or observable behavior and use existing imported data whenever possible.

## Current HUD incident (2026-11-09)

- User screenshot still shows no in-game HUD. This is unresolved; do not describe the gameplay HUD as fixed.
- Previous v0.6.83 device log reported `覆盖层没有 ctrl_touch 标记，照原版布局`. The touch overlay generator produced root-level `ctrl_touch` and `touch_strings.tsv`, but `dm/android/tools/build_package.py::collect_romfs_overlay` included only `.build.config` and one `.data` archive. This silently prevented `dm_padmode_boot` from switching to the generated touch layouts.
- The package builder now collects both sidecars as a pair; package-import regression test, builder assembly smoke test, and full corrected-ZIP CRC check passed. `docs/ANDROID_PACKAGE.zh-CN.md` documents the sidecar contract.
- A full corrected package was also produced and CRC-checked, but the Tailnet server recorded no phone GET; do not ask the user to import its 9.54 GB archive unless needed.
- A lower-bandwidth APK-side migration is now implemented for the exact legacy touch-overlay signature (`root`, `vfs-1`, size and target-platform fields) and exact archive file. It embeds the two sidecars, writes them under app-private storage, and prepends that directory to `DM_ROMFS_OVERLAY`; font-only overlays do not match.
- APK `v0.6.84` / versionCode 684 was rebuilt using the same signing keystore as v0.6.83. It is 28,454,464 bytes; SHA-256 `ae7939a8cc350587a52828ccf646a62ccdc806f92448617d4f7f116c50f3f968`. Tailnet URL: `http://100.86.44.23:8821/D2R-Android-port-v0.6.84-HUD-migration.apk` (HTTP 200; content length matches).
- Offline C fingerprint tests passed for the real touch overlay config and the font-only overlay config. APK build passed and contains both sidecars and the Android native library. The migration has not run on the phone; the HUD is still visually unverified.
- Wireless ADB endpoint last supplied, `100.106.114.77:34719`, could not be reached; `adb devices` and mDNS discovery returned no device. No fresh device log is available.

After updating to APK `0.6.84`, the user can keep the existing imported package; no 9.54 GB reimport is required for the known legacy overlay. The expected fresh startup log is `[android-overlay-migration] repaired legacy touch overlay in app-private storage; full game package was not reimported`, followed by the touch-layout activation log. Then verify actual gameplay HUD widgets in a screenshot. If the migration log is absent or the HUD still fails to render, inspect fresh logs and the loaded `HUDPanel` layout before touching viewport code. Report that live validation is outstanding until observed.

APK `0.6.85` supersedes that sidecar-only build. It makes valid `WindowSetCrop` authoritative (the v0.6.83 log selected 1280x720 despite a valid 1280x576 crop), and embeds a complete Android HUD overlay generated with `DM_ANDROID_HUD=1`. This preserves native HP/MP values, XP and SkillButton1–5, which the iOS-oriented generator previously hid even though Android has no UIKit replacement layer. AAPT omitted dotfiles; the APK now carries `build.config` and native restores `.build.config`. Final APK: 35,454,806 bytes, SHA-256 `3594a58cecdd54643955a5a9c4782970f54255f61e9814afb810dd92cd26824e`, versionCode 685, same signing certificate. Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.85-20x9-native-HUD.apk`. Offline suite 12/12 and focused geometry/migration/HUD-generation checks passed; live phone validation remains outstanding.

APK `0.6.86` adds the missing Android connection to `dm_ui_drag_begin/move/end`: vertical generic-control drags scroll lists and horizontal drags operate sliders. Item drag-and-drop and tab handlers remain separate. Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.86-touchscreen.apk`; SHA-256 `74351482698c986a89ce721d08e1379b63467ac3f9a12a155223948deb722788`; versionCode 686. All 13 portable suites pass. No claim that every menu works on the phone until exercised there.

APK `0.6.87` adds explicit docked/handheld coordinate tests and `[android-touch-map]` runtime logging. Gameplay 1280x576 and 1020x459 maps identically onto 2400x1080; centered 16:9 menu mappings and side bars are covered. Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.87-dual-touch.apk`; SHA-256 `7ba56743404e7482cc109a5e72616e940e59657ea7c0ad4a95b826afff8902a9`; versionCode 687. Physical on-device alignment remains unverified.

APK `0.6.88` adds a launcher gameplay-aspect toggle for screenshot A/B: 20:9 = 1280x576, 16:9 = 1280x720, persisted through the existing resolution setting. Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.88-aspect-toggle.apk`; SHA-256 `baba8058d232c6a8205ff38779332a3a2c06225a9004dc87998d7301690de510`; versionCode 688. All 14 portable suites pass. HUD remains unresolved pending comparative screenshots/logs.

APK `0.6.89` fixes native per-profile resolutions: 16:9 docked 1600x900 / handheld 1280x720; 20:9 docked 1600x720 / handheld 1280x576. Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.89-native-aspect.apk`; SHA-256 `b2e100edd304882261ebc2b0b0073c14291a52e263e614b91a4c3359c20f7d6a`; versionCode 689. Do not use v0.6.88 for the native-resolution comparison.

APK `0.6.90` adds −/+ native font controls using the game's LowVisionMode (normal/large), persisted as `font_large`, delivered on the game main thread as `OptionsPanelMessage:SetLowVisionMode`. Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.90-font-size.apk`; SHA-256 `c4601bed0703ff599b8a17347b52103dcdc759fed93086119efa9d6389892d26`; versionCode 690. Visual effect is not confirmed on device.

APK `0.6.91` updates the bundled Mali drivers to their newest upstream releases: MaliSX2 `0.4.1` (commit `a833123`) and PanVK `r10` (commit `58f2c55`), with archive/library SHA-256 validation. Qualcomm Turnip T30 was left untouched. Tailnet: `http://100.86.44.23:8821/D2R-Android-port-v0.6.91-mali-updated.apk`; SHA-256 `cf32c22fd4a075a097308031cc2c60703ddda82152884533f3f1df5bef9b68f3`; versionCode 691.
