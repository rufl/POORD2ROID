# Changelog

Release artifacts contain the Android port and launcher only. They do not contain game data, firmware, keys, saves, or protected game content.

## [v0.7.8] — 2026-11-26

### Android presentation, sharpening, and driver refresh

- Kept the v0.7.7 identity-preferred surface transform fix so the landscape framebuffer, touch overlay, and cursor remain upright.
- Added a persistent sharpening control to the Android menu: Off/Low/Medium/High presets plus custom strength adjustment, profile persistence, localized labels, and accessible +/- descriptions.
- The Vulkan sharpen pass maps the selected source crop to the destination rectangle, preserves cleared letterbox pixels, falls back to the linear blit on initialization/resource failure, and caches swapchain framebuffers and descriptor resources so enabled sharpening does not allocate Vulkan objects every frame.
- Refreshed bundled drivers to MaliSX2 0.6.1 and Mesa Turnip v27.0.0-20261126. PanVK r10 was already the latest pinned release and remains unchanged.
- Fixed Android startup parsing for the Java-persisted `handheld_`/`console_` sharpening keys. The previous build rejected `sharpen_preset` as an unknown launch setting before the game could start.
- Reduced the sharpening fragment pass to the direction whose result is actually used, removing discarded texture samples that amplified GPU stalls during camera movement while preserving the current output for every radius.
- Added a GPU-budgeted sharpening fallback with 30/24 ms hysteresis: when the previous GPU window is overloaded, the selected sharpening profile temporarily uses the existing linear blit to avoid movement stutters, then automatically resumes below budget.

### Packaging

- Complete `--game-run` package-only APK retained: Java launcher/game activity, `classes.dex`, normal/large touch-layout migration profiles, MaliSX2 0.6.1, PanVK r10, Turnip v27.0.0-20261126, and sharpening support.
- APK metadata is `versionName=0.7.8-android-port`, `versionCode=708`, size `42,123,820` bytes, SHA-256 `0cb772bdb0f28ed64daa9c5382ba484185d6cef31028d5f40655ddda6e22d031`. APK signing uses the v0.7.8 certificate `4832e0a9…`; v0.7.7 remains signed by `d4cbe1c8…`.

## [v0.7.7] — 2026-11-24

### Android surface orientation fix

- Restored `preTransform = cap.currentTransform` directly in `nvn_vulkan.c`, matching the shipped v0.7.3/v0.7.4 behavior and the non-Android path (`vulkan_front.c` already uses `capabilities.currentTransform`). v0.7.5/v0.7.6 preferred `identity`, which rotated the already-upright window-space blit a second time on `ROTATE_90` devices, pushing the cursor and options Display tab off the visible canvas.
- `[vulkan-surface-transform]` log line prints supported/current/selected transform masks each session; `selected` now equals `current`.

### Packaging

- Complete `--game-run` package-only APK retained: Java launcher/game activity, `classes.dex`, normal/large touch-layout migration profiles, MaliSX2 0.6.0, PanVK r10, Turnip T30, ADPF changes, Display routing.
- APK metadata is `versionName=0.7.7-android-port`, `versionCode=707`, size `43,020,844` bytes, SHA-256 `c01ced9d34b645bb975bf0089ca1eb2e095800feac97339cd9506e6015277ddc`. APK signing verification passed with v3; signer certificate SHA-256 `d4cbe1c8322c1a0491583888d674557c89cd1629118660a93ae9bd11d357baf8` matches v0.7.6, so installing over v0.7.6 keeps app data. No Android device was connected for final visual confirmation.



## [v0.7.6] — 2026-11-22

### Android driver update

- Bundled MaliSX2 updated from 0.5.1 to 0.6.0 (`62d6d62423`); packaging verifies the upstream ZIP SHA-256 `55b17293a6e3ab089e2d255cc6741f574def4c241265547d4a5aaa01cb4047f8` and library SHA-256 `c1c8738fabd6a3b5a8c2d3d0a732b1498a0569251d22cf7f05cd50c81db176fb`.
- Kept the complete `--game-run` package-only APK, Java launcher/game activity, normal/large touch-layout migration profiles, ADPF changes, PanVK r10, Turnip T30, Display routing, and recent native performance work.
- APK metadata is `versionName=0.7.6-android-port`, `versionCode=706`, size `43,020,844` bytes, SHA-256 `9e7a3be222a301da82364f456290afe68955a8593539b1e16799bcf3a955e7ea`. APK signing verification passed with v3. No Android device was connected for final visual confirmation.

## [v0.7.5] — 2026-11-22

### Android surface orientation correction

- Reverted the v0.7.4 Vulkan `currentTransform` selection after device evidence showed the native game framebuffer rotated while the Android touch overlay remained upright in landscape.
- Restored the earlier working identity-preferred `preTransform` behavior, with `currentTransform` only as the fallback when identity is unsupported.
- Retained the complete `--game-run` package-only APK, Java launcher/game activity, normal/large touch-layout profiles, ADPF changes, MaliSX2 0.5.1, PanVK r10, Display routing, and recent native performance work.
- APK metadata is `versionName=0.7.5-android-port`, `versionCode=705`, size `42,594,860` bytes, SHA-256 `2ef8b49537a43766877f0924d62d6cf247146b0d2c5f7c59373890a265423238`. APK signing and CI verification are required before publication; no Android device was connected for final visual confirmation.

## [v0.7.4] — 2026-11-22

### Complete Android package rebuild

- Rebuilt the release as a package-only game APK with `--game-run`; v0.7.2 and v0.7.3 were accidentally published as the diagnostic NativeActivity probe because that flag was omitted.
- Restored the complete launcher/game Java surface (`LauncherActivity`, `D2RActivity`, `classes.dex`) and both normal/large touch-layout migration profiles from the previously working v0.6.94 build.
- Kept the current Vulkan `currentTransform` fix, ADPF changes, MaliSX2 0.5.1, PanVK r10, Display routing, and recent native performance work.
- APK metadata is `versionName=0.7.4-android-port`, `versionCode=704`, size `42,594,860` bytes, SHA-256 `47eb0f3aee346f8b904cbb02eb7a770f8df679263cfa70c4f702501cdb31481d`. APK signing verification passed with v3. No Android device was connected for final visual confirmation.

## [v0.7.3] — 2026-11-21

### Android display orientation hotfix

- Vulkan swapchains now use the Android surface `currentTransform` for `preTransform`, preventing the presentation engine from rotating an already-upright blit on devices reporting `ROTATE_90`.

- WSL Ubuntu built the generic ARM64 APK successfully; APK metadata is `versionName=0.7.3-android-port`, `versionCode=703`, size `22,883,679` bytes, SHA-256 `a70e57f38eb3abdae9899420d4ee144cee56bf6a8e3d87b0273a05e6261c2315`. APK signing verification passed with v3. No Android device was connected for final visual confirmation.

## [v0.7.2] — 2026-11-20

### Android Vulkan follow-up

- ADPF frame sessions now bind the guest and recording thread IDs together, use `APerformanceHint_setThreads` when available, recreate sessions after thread-group or target changes, and retry failed creation/reporting with bounded backoff.
- Bundled MaliSX2 is pinned to upstream `0.5.1+308fe121c7`; the archive and library SHA-256 values are verified during packaging.
- Android Display-tab routing remains native (`VideoOptionsPanel`); the focused regression suite passes.
- WSL Ubuntu built the generic ARM64 APK successfully; APK metadata is `versionName=0.7.2-android-port`, `versionCode=702`, size `22,883,679` bytes, SHA-256 `7ed7453adbf6bf03b0920c201991171ff45b0ec4555d7e381deb740c1cfe8758`. APK signing verification passed with v3. No Android device was connected, so device A/B and runtime Display-tab visibility remain pending.

## [v0.7.1] — 2026-11-20

### Android Vulkan performance

- Replaced per-operation `calloc`/`free` churn for deferred `post_op` metadata with a queue-serialized freelist backed by 64-entry chunks. GPU resources still retire at their real fence, FIFO guest-memory publication is unchanged, and the chunks are released during async shutdown.
- Expanded the conservative async-copy page Bloom filters from 512 to 8,192 bits and the representable per-range limit from 512 KiB to 8 MiB. The saved frame-1080 log reported 9,305.67 checks/frame and 9,304.67 exact FIFO-chain scans/frame before this change.
- Bloom-filter negatives only reject impossible overlap. Every possible hit, overflow, and range larger than 8 MiB still uses the original exact FIFO scan.

### Verification and artifact

- Commit: [`4f0933a`](https://github.com/rufl/POORD2ROID/commit/4f0933a4abe6fc461d9e8722809e0401fa8c457a).
- ARM64 game-host and release APK build completed successfully.
- APK metadata: `versionName=0.7.1-android-port`, `versionCode=701`.
- Artifact: [`D2R-Android-port-v0.7.1-4f0933a.apk`](https://github.com/rufl/POORD2ROID/releases/download/v0.7.1/D2R-Android-port-v0.7.1-4f0933a.apk), 28,700,056 bytes.
- SHA-256: `fdc82bd5a33e9f081bf04c4cb8073358a50b9ebec5f48b85c0227323730c6312`.
- No Android device was attached for a matched A/B run. This release makes no measured FPS claim.

## [v0.7.0] — 2026-11-19

- Recover from `APerformanceHintSession_report` failures by closing the failed session and permitting recreation with the existing 60→3,600-cycle backoff.
- Retry ADPF session creation indefinitely rather than disabling it after three transient failures.
- Validate acquire/present cycles across the guest and recording threads using the window and wall clock; repeated acquire keeps the earliest start.

## [v0.6.99] — 2026-11-19

- Retry ADPF hint-session creation indefinitely with bounded backoff.
- Preserve the earliest repeated acquire and use the acquire index only for correlation.

## [v0.6.98] — 2026-11-19

- Fix ADPF cycles being rejected solely because acquire and present run on different threads.
- Keep the earliest acquire timestamp within a frame.

[v0.7.5]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.5
[v0.7.8]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.8
[v0.7.7]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.7
[v0.7.6]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.6
[v0.7.4]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.4
[v0.7.2]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.2
[v0.7.1]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.1
[v0.7.0]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.0
[v0.6.99]: https://github.com/rufl/POORD2ROID/releases/tag/v0.6.99
[v0.6.98]: https://github.com/rufl/POORD2ROID/releases/tag/v0.6.98
