# Third-party notices and attribution

This file records third-party code and binary components used, modified, or redistributed by this repository, plus projects consulted only as references. Preserve the applicable license and copyright notices. The repository's own code is licensed under the root [`LICENSE`](LICENSE), except where a file or component states otherwise.

## Source projects

### d2r-mobile

- Source: <https://github.com/jhaizhou-ops/d2r-mobile>
- License: MIT; copyright notices are retained in the imported source.
- Role: original project and source of the compatibility-layer lineage and host architecture. See `upstream/LICENSE`.

### d2r-android

- Source: <https://github.com/sydeEvans/d2r-android>
- Imported baseline: `5d8e32210b6d54bc0f8140865332a8616766779f`
- License: MIT; imported source identifies copyright to jhaizhou.
- Role: direct Android port used as the base for this fork. See [`UPSTREAM.md`](UPSTREAM.md).

## Bundled Vulkan drivers and loader components

### MaliSX2 0.4.0

- Project/release: <https://github.com/bmdhacks/malisx2/releases/tag/v0.4.0>
- Pinned source reference: <https://github.com/bmdhacks/armsx2-turnip/tree/f59c211100b174191d9df540451e94b2d5cb2b26>
- Package: `malisx2-0.4.0-63e307e63a.zip`; SHA-256 `835f980506ed46fa51925fd279b256d23b0ddf481ffd1ab39be2fee275cf8bf8`.
- Library: `libvulkan_malisx2.so`; SHA-256 `0148e1ea6263653ef376007274eefdf1d58fb63eadf66e2290c6cc49219c0365`.
- License: MIT, copyright 2026 ARMSX2 and bmdhacks. The binary includes Mesa-derived components; consult the upstream vendor-license inventory and APK notice before redistribution.
- The build verifies the pinned archive and library hashes. Upstream's stated Mali support is not proof of D2R or Xiaomi 11T compatibility. The release notes identify MSAA as not implemented.

### JimVulkan PanVK r9 (Mesa 27.0.0-devel)

- Release/source: <https://github.com/JimVulkan/mali-panvk/releases/tag/v1.5.0>
- Pinned package: `upstream/dm/android/prebuilt/mali-panvk-27.0.0-devel-r9.zip`; SHA-256 `04d82e2f35e4e80138f9a407ec4222e40908b32196eb7484355993e6c5fdc376`.
- Bundled library `libvulkan_panfrost.so`; SHA-256 `6baf04f6075fe57d0dc1738039cf91d8db6ce98c9203dd9dc3d638f42c49e62a`.
- License: MIT for PanVK changes; the package's `NOTICE.txt` contains Mesa and third-party notices and is included in the APK as `PanVK-JimVulkan-r9.txt`.
- Status: experimental and non-conformant. Hardware support or successful device initialization does not demonstrate Android presentation or working D2R gameplay. Treat as an experimental option.

### Mesa Turnip T30

- Mesa project: <https://gitlab.freedesktop.org/mesa/mesa>
- Build metadata: `upstream/dm/android/prebuilt/turnip/meta.json`; build name `Turnip Adreno Driver T30 (@Mr_Purple_666)`, package version `T30`.
- License: Mesa license and notices shipped with the driver package. The metadata credits @Mr_Purple_666 and thanks DISKDVD.
- Hardware: Qualcomm Adreno/KGSL; not applicable to Mali devices such as Xiaomi 11T.

### AdrenoTools

- Project: <https://github.com/obfusk/adrenotools>
- License: MIT.
- Role: Android Vulkan driver loading/hook support used by the build. The build fetches pinned source; preserve its upstream license and notices.

## Vendored libraries

The source tree contains third-party components under `upstream/dm/third_party/`, including:

| Component | Upstream | License / note |
| --- | --- | --- |
| Ryujinx components | <https://github.com/Ryujinx/Ryujinx> | MIT; retain included notices. |
| libdeflate | <https://github.com/ebiggers/libdeflate> | MIT. |
| bcdec | <https://github.com/iOrange/bcdec> | MIT; some decoder techniques reference <https://gist.github.com/rygorous/2144712>. |
| FLIP | <https://github.com/Flipboard/FLIP> | MIT; bundled stb_image, tinyexr, and miniz retain their own notices. |

`liblinkernsbypass` is a pinned build dependency fetched from <https://github.com/libxdp/liblinkernsbypass>; its applicable Apache-2.0/MIT notices must be retained with the fetched source and build output.

`adapt/zipalign.py` and `adapt/vk_shim3.c` are fork-authored tools, not third-party imports; see the root license.


## References only; code not incorporated as a dependency

- [Eden](https://git.eden-emu.dev/eden-emu/eden), GPL-3.0: design reference only. Eden's shader translation code is not incorporated.
- [LASRecomp](https://github.com/GPUOpen-Effects/LASRecomp), Apache-2.0: research reference; no code imported.
- [AMD FidelityFX FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR), MIT, and [Qualcomm SGSR](https://github.com/SnapdragonGameStudios/snapdragon-gsr), BSD-3-Clause: upscaling research references; no code imported.

## Not distributed by this repository

- Game files, RomFS, NSP/NSZ/XCI images, firmware, or `prod.keys`/`title.keys`. Users are responsible for lawful possession and preparation of their own data.
- Proprietary Qualcomm Adreno 840 libraries. See [`drivers/Qualcomm_840/README.md`](drivers/Qualcomm_840/README.md); the repository contains no such binaries and grants no redistribution rights.
- APKs incorporating those proprietary libraries.

## Trademarks

Diablo II: Resurrected and Nintendo Switch are marks of their respective owners. This is an unofficial, independent project with no affiliation with or endorsement by Blizzard Entertainment or Nintendo.
