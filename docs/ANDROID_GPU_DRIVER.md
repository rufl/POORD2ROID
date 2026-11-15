# Android Mali driver matrix

Português (Brasil): [ANDROID_GPU_DRIVER.pt-BR.md](ANDROID_GPU_DRIVER.pt-BR.md)

This is the canonical GPU-family matrix for the POORD2ROID Android APK. GPU family is authoritative; phone marketing names are not. The same phone name can ship different SoCs.

## Driver selection

| Driver | Path | Minimum Android | Recommendation |
|---|---|---:|---|
| **System Vulkan** | Vendor Vulkan | Any | Baseline, recovery, and only bundled choice for legacy/unknown GPUs |
| **MaliSX2 0.5.0** | Mali kbase `/dev/mali0` | API 29 | First custom driver for the listed MaliSX2 families; preferred for G57/G68/G77 |
| **PanVK r10** | Mali kbase `/dev/mali0` | API 31 | Preferred for G71/G72; alternate for Bifrost/G77-class devices |
| **Turnip T30** | Qualcomm KGSL `/dev/kgsl-3d0` | API 30 | Adreno only; never select on Mali |

Custom drivers are experimental. Availability in the launcher does not prove device creation, presentation, or gameplay compatibility. The launcher has no phone-model allowlist; native diagnostics report `mali-bifrost`, `mali-valhall`, or `mali-unknown`.

## Complete bundled Mali matrix

| GPU family | Architecture | First driver | Alternate | Notes |
|---|---|---|---|---|
| G31 | Bifrost | MaliSX2 0.5.0 | PanVK r10 API 31+; System | Low-end bandwidth/memory headroom |
| G51 / G52 | Bifrost | MaliSX2 0.5.0 | PanVK r10 API 31+; System | Check vendor feature exposure |
| G71 / G72 | Bifrost | PanVK r10 API 31+ | System | Not listed by MaliSX2 0.5.0 |
| G76 | Bifrost | MaliSX2 0.5.0 | PanVK r10 API 31+; System | Compare drivers on device firmware |
| G57 / G68 | Valhall v9 | MaliSX2 0.5.0 | PanVK r10 when recognized; System | API 29+ MaliSX2, API 31+ PanVK |
| G77 | Valhall v9 | MaliSX2 0.5.0 | PanVK r10 API 31+; System | Xiaomi 11T G77 MC9 reference device |
| G78 / G78AE | Valhall | MaliSX2 0.5.0 | PanVK r10 when recognized; System | Do not assume G77 results transfer |
| G310 / G510 | Valhall v10 | MaliSX2 0.5.0 | System | Watch sustained thermals and memory |
| G610 / G710 | Valhall v10 | MaliSX2 0.5.0 | System | Experimental custom path |
| G615 / G715 | Valhall v11 | MaliSX2 0.5.0 | System; PanVK only if explicitly recognized | Immortalis/ray-tracing features are not promised |

## System-only families in this APK

Mali-400/450/470, T604/T628/T720/T760/T820/T830/T860/T880, G620, G725, and any family not listed by the pinned MaliSX2/PanVK releases use System Vulkan only. The system driver may still work if it exposes the required Vulkan queue, formats, memory, and shader features.

## Recommended test order

1. System Vulkan baseline.
2. MaliSX2 on API 29–30 Mali devices.
3. MaliSX2 versus PanVK r10 on API 31+ Bifrost/G77-class devices.
4. System Vulkan for unsupported or unstable custom paths.

```bash
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

Confirm `[vulkan-device] profile=`, `[vulkan-subgroup]`, and final driver identity. A launcher label alone is not proof.

## Pinned checksums

- MaliSX2 archive: `71c1e3dc68ef1396d469f245019d42ab73980e48034095453b7be776195c3641`
- MaliSX2 library: `b80af8f6ed1d88a1e19d0538564e90de8f7e46f4e32f8daec7cb7a772e292721`
- PanVK archive: `7e97db4d79d81ec9461d1e0676b62a865561cb56e21c151693aa25ffbe749f1c`
- PanVK library: `80acd461bdf07f5fb99af5c7f516e272103340b6775c22dcd347a6d418c7ac0d`

Sources: [MaliSX2 v0.5.0](https://github.com/bmdhacks/malisx2/releases/tag/v0.5.0) and [JimVulkan PanVK releases](https://github.com/JimVulkan/mali-panvk/releases).
