# Qualcomm Adreno 840 driver inputs — not distributed

This directory documents the optional legacy input location for `adapt/patch-apk.py`. The proprietary driver libraries are not included in this repository.

The historical package expected these six files:

| File | Purpose |
| --- | --- |
| `vulkan.adreno.so` | Vulkan ICD |
| `notllvm-qgl.so` | Qualcomm GL-to-Vulkan implementation |
| `notllvm-glnext.so` | Qualcomm graphics component |
| `notgsl.so` | Qualcomm graphics component |
| `notadreno_utils.so` | Qualcomm utility library |
| `libdmabufheap.so` | Android DMA-BUF heap dependency used by that package |

`meta.json` records package provenance when available. The upstream note identifies the binaries as proprietary files from a Meta Ray-Ban Display driver package, attributed there to Hugo. There is no redistribution authorization established by this repository; do not commit or publish those binaries or an APK containing them.

This injection flow is optional and is not part of the normal bundled-driver path. It targets Qualcomm/Adreno hardware and does not apply to Xiaomi 11T's Mali-G77. For the normal driver options and current limitations, see the root README and [`THIRD-PARTY-NOTICES.md`](../../THIRD-PARTY-NOTICES.md).

Before using any locally obtained driver package, verify that you are authorized to use it, inspect the adaptation scripts, and confirm driver identity from runtime logs. A matching vendor name alone is insufficient to establish that two driver builds are interchangeable.
