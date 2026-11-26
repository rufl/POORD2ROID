# Android Mali driver matrix

Português (Brasil): [ANDROID_GPU_DRIVER.pt-BR.md](ANDROID_GPU_DRIVER.pt-BR.md)  
中文：[ANDROID_GPU_DRIVER.zh-CN.md](ANDROID_GPU_DRIVER.zh-CN.md)

This matrix is GPU-family based. It is not a promise that every phone model works: Android vendor kernels, memory, Vulkan extensions, firmware, thermals, and game presentation still matter. A phone name is not a reliable GPU identifier because the same product name can ship with different SoCs.

## Bundled drivers

| Driver | GPU/kernel path | Minimum Android | Best use | Hard limits |
|---|---|---:|---|---|
| **MaliSX2 0.6.1** | Arm Mali through `/dev/mali0` / kbase | API 29 | Default custom driver for the listed Mali families, especially Valhall G57/G68/G77/G78 | Experimental; upstream does not implement MSAA; no guarantee for every listed GPU |
| **PanVK r10** | Mesa PanVK through `/dev/mali0` / kbase | API 31 | Alternate for Bifrost and G77-class devices when MaliSX2 is unavailable or regresses | Non-conformant experimental driver; release support and testing are narrower than the full family table |
| **System Vulkan** | Vendor Android Vulkan | none | Recovery path and the only choice for unsupported/legacy GPUs | Features and performance depend entirely on the phone vendor |
| **Turnip v27.0.0-20261126** | Qualcomm KGSL `/dev/kgsl-3d0` | API 30 | Adreno only | Never select for Mali; A8xx build is experimental |

The launcher does not maintain a phone-model allowlist. A Mali device with `/dev/mali0` is offered the Mali drivers, then native Vulkan capability probing reports the actual device. The log profile is `mali-bifrost`, `mali-valhall`, or `mali-unknown`.

## Complete bundled Mali GPU matrix

“Preferred” means the first custom driver to test, not a compatibility guarantee. If it fails to create a device, present, or run the game, switch to the listed alternate or System Vulkan and capture the driver log.

| GPU family | Architecture | Preferred driver | Alternate | Android/API notes |
|---|---|---|---|---|
| **Mali-G31** | Bifrost | MaliSX2 0.6.1 | PanVK r10 on API 31+; System Vulkan | Low-end; expect lower memory and bandwidth headroom |
| **Mali-G51** | Bifrost | MaliSX2 0.6.1 | PanVK r10 on API 31+; System Vulkan | Check vendor Vulkan format/feature exposure |
| **Mali-G52** | Bifrost | MaliSX2 0.6.1 | PanVK r10 on API 31+; System Vulkan | Common low/mid-range configuration; thermal limits vary widely |
| **Mali-G71 / G72** | Bifrost | PanVK r10 on API 31+ | System Vulkan | Not listed by MaliSX2 0.6.1; use System on API 29/30 |
| **Mali-G76** | Bifrost | MaliSX2 0.6.1 | PanVK r10 on API 31+; System Vulkan | Use the actual GPU name, not the phone marketing name |
| **Mali-G57** | Valhall v9 | MaliSX2 0.6.1 | PanVK r10 where the release recognizes the device; System Vulkan | API 29+ for MaliSX2; API 31+ for PanVK |
| **Mali-G68** | Valhall v9 | MaliSX2 0.6.1 | PanVK r10 where the release recognizes the device; System Vulkan | Low-end Valhall; reduce resolution/quality if memory pressure is visible |
| **Mali-G77** | Valhall v9 | MaliSX2 0.6.1 | PanVK r10 on API 31+; System Vulkan | Xiaomi 11T G77 MC9 is the current reference device; compare both custom paths |
| **Mali-G78 / G78AE** | Valhall v9 | MaliSX2 0.6.1 | PanVK r10 where the release recognizes the device; System Vulkan | Test both custom drivers per firmware; do not assume G77 results transfer |
| **Mali-G310 / G510** | Valhall v10 | MaliSX2 0.6.1 | System Vulkan | Check memory pressure and sustained thermals |
| **Mali-G610 / G710** | Valhall v10 | MaliSX2 0.6.1 | PanVK r10 where the release recognizes the device; System Vulkan | Custom support is still experimental |
| **Mali-G615 / G715** | Valhall v11 | MaliSX2 0.6.1 | PanVK r10 where the release recognizes the device; System Vulkan | Ray-tracing behavior is not promised |
| **Mali-G620 / G720** | Valhall v12 | MaliSX2 0.6.1 | PanVK r10 where the release recognizes the device; System Vulkan | Validate on actual firmware |

## GPUs outside the bundled custom-driver list

These families are **System Vulkan only** in this APK. A phone may still run the port if its vendor Vulkan driver exposes the required queue, formats, memory, and shader features; the custom Mali drivers do not add support for them.

- Mali-400 / Mali-450 / Mali-470
- Mali-T604 / T628 / T720 / T760 / T820 / T830 / T860 / T880
- Mali-G725 and other families not listed in the pinned MaliSX2 or PanVK release notes
- Any GPU exposed through a non-kbase path or without `/dev/mali0`

## Selection procedure

1. Start with **System Vulkan** and record the baseline.
2. On API 29–30 Mali devices, test **MaliSX2 0.6.1**.
3. On API 31+ Bifrost/G77-class devices, compare **MaliSX2** and **PanVK r10**; keep the driver that reaches the game and remains stable in a sustained scene.
4. On supported Adreno devices, compare Turnip v27.0.0-20261126 against System Vulkan; retain the one with lower sustained frame time and no rendering regressions.
5. On unsupported or legacy GPUs, keep **System Vulkan**.
6. Never use Turnip on Mali.

Collect diagnostics:

```bash
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

Useful lines include `[driver]`, `[vulkan-device] profile=`, `[vulkan-subgroup]`, and the final driver identity. A launcher label alone is not proof that the ICD loaded.

## Pinned artifacts

- MaliSX2 archive: `malisx2-0.6.1-9fbc7da290.zip`
  - Archive SHA-256: `1fbdb5dee1217d0f97190a60844f83c4504567c8c6bc6399b5a8843840e99206`
  - Library SHA-256: `c19badcb876fa863afe215fc20b1b87f3e78e964f961f03cc5925ab77381ac2f`
- PanVK archive: `mali-panvk-27.0.0-devel-r10.zip`
  - Archive SHA-256: `7e97db4d79d81ec9461d1e0676b62a865561cb56e21c151693aa25ffbe749f1c`
  - Library SHA-256: `80acd461bdf07f5fb99af5c7f516e272103340b6775c22dcd347a6d418c7ac0d`
- Turnip package: `Turnip-v27.0.0-20261126.zip`
  - Package SHA-256: `82ef4c9ac6a17d3df59b60ef1616c4ba6fbe244c51911c3b3cc0a99baab9955f`
  - Library SHA-256: `d2cab6b3dd347a1f4132a4b3cc74f696614b2fe0a67702b095268ca829e35290`

Sources: [MaliSX2 v0.6.1](https://github.com/bmdhacks/malisx2/releases/tag/v0.6.1), [JimVulkan PanVK releases](https://github.com/JimVulkan/mali-panvk/releases), [Turnip v27.0.0-20261126](https://github.com/The412Banner/Banners-Turnip/releases/tag/v27.0.0-20261126), and the pinned metadata/checksums in `dm/android/tools/build.py`.
