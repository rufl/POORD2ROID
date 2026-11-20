# Changelog

Release artifacts contain the Android port and launcher only. They do not contain game data, firmware, keys, saves, or protected game content.

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

[v0.7.2]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.2
[v0.7.1]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.1
[v0.7.0]: https://github.com/rufl/POORD2ROID/releases/tag/v0.7.0
[v0.6.99]: https://github.com/rufl/POORD2ROID/releases/tag/v0.6.99
[v0.6.98]: https://github.com/rufl/POORD2ROID/releases/tag/v0.6.98
