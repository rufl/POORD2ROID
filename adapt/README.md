# Adaptation tools

This directory contains APK adaptation, driver-shim, and verification utilities. Most Android runtime changes are now integrated directly into `upstream/`; the historical `android-gamepad-and-frame-rate.patch` is not a complete description of the current fork. See [`UPSTREAM.md`](../UPSTREAM.md).

## Files

| File | Purpose |
| --- | --- |
| `vk_shim3.c` | Fork-authored Vulkan forwarding shim used by the optional Qualcomm adaptation flow. |
| `build-vkshim3.sh` | Builds the shim in the documented Linux/container toolchain. |
| `prebuilt/libd2rvlk.so` | Prebuilt shim artifact; rebuild and verify against the current source before relying on it. |
| `patch-apk.py` | Legacy APK repackaging tool for a separately supplied Qualcomm Adreno 840 driver set. It is not the normal build or driver-selection path. |
| `zipalign.py` | Fork-authored ZIP alignment utility. |
| `elfsym.py` | ELF dynamic-symbol inspection utility. |
| `install-and-verify.sh`, `verify-runtime.sh`, `print-state.sh` | Device installation/runtime diagnostics for the optional legacy adaptation flow. Review scripts before running them on a device. |
| `remove-magisk-driver-module.sh`, `magisk-d2r-bind/` | Legacy Magisk-related utilities. Root/Magisk is not required for the normal Android port; do not install or remove modules without understanding the device changes. |
| `android-gamepad-and-frame-rate.patch` | Historical Android patch against the baseline noted in its header; not the full current fork diff. |

## Qualcomm Adreno 840 files

The proprietary driver injection path is optional and separate from the normal APK. The repository does not redistribute the Qualcomm binaries. See [`drivers/Qualcomm_840/README.md`](../drivers/Qualcomm_840/README.md) for licensing and scope. This path does not apply to the Xiaomi 11T's Mali-G77.

Do not infer that a driver is compatible merely because it can be loaded. Confirm the Vulkan device identity, surface creation, presentation, and actual game behavior. System Vulkan remains the recovery baseline.
