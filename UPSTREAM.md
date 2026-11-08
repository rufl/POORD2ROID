# Upstream provenance

The `upstream/` directory is the imported Android port with this fork's changes integrated. It is no longer a byte-for-byte copy of the upstream repository.

| Project | Role | License / credit |
| --- | --- | --- |
| [jhaizhou-ops/d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile) | Original project and inspiration for the compatibility layer and game-host architecture. | MIT; copyright notices are retained in the imported source. |
| [sydeEvans/d2r-android](https://github.com/sydeEvans/d2r-android) | Direct Android-port source. The imported baseline is commit `5d8e32210b6d54bc0f8140865332a8616766779f`. | MIT; the upstream source identifies copyright to jhaizhou. |
| This repository | Device adaptation, Android input and frame pacing, Mali fixes, driver selection/packaging, diagnostics, and Android-specific changes. | See root `LICENSE` and `THIRD-PARTY-NOTICES.md`. |

## Fork changes

Changes are integrated directly into `upstream/`; cloning this repository does not require applying a patch. The tree includes, among other changes:

- Android launcher, touch/gamepad input, frame pacing, startup sequencing, and diagnostics.
- System Vulkan plus hardware-gated MaliSX2, JimVulkan PanVK, and Turnip driver options.
- Mali-related BC texture decode and shader workarounds, including local-size Z-fold handling and a regression check.
- Android display/resolution settings, pipeline-cache persistence/warm-up, and optional GPU BC texture decoding.

These features are experimental and should not be interpreted as universal device support. See the maintained Android build guide for current behavior and verification limits.

## Comparing to the direct upstream

```bash
git clone https://github.com/sydeEvans/d2r-android /tmp/d2r-android-upstream
diff -ru /tmp/d2r-android-upstream upstream/
```

The historical patch at `adapt/android-gamepad-and-frame-rate.patch` records an earlier set of Android changes. It is not the complete representation of all current fork changes; inspect the tree diff for the full change set.

## Rebuilding from an upstream baseline

The patch applies only to the baseline and scope recorded in its header. It does not recreate all current changes. For a full reconstruction, compare the source tree and review the history of this fork; do not assume the historical patch alone reproduces the current APK.
