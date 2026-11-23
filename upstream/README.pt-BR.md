# D2R Android

Executa a versão ARM64 original de Diablo II: Resurrected nativamente no Android. O repositório contém o port e as ferramentas, não contém jogo, chaves ou dados protegidos.

English: [README.md](README.md) · 中文: [README.zh-CN.md](README.zh-CN.md) · [Changelog](../CHANGELOG.md)

## Download

Baixe a versão atual [v0.7.5](https://github.com/rufl/POORD2ROID/releases/tag/v0.7.5) ou consulte todas as [Releases](https://github.com/rufl/POORD2ROID/releases):

1. `D2R-Android-port-v0.7.5.apk` — aplicativo Android atual.
2. `d2r-package-kit-<versão>.zip` — quando anexado, contém as ferramentas para gerar o pacote a partir dos seus próprios arquivos; caso contrário, use esta árvore de código.

O APK não contém jogo. Use o kit com um agente de código para preparar os dados localmente e instalar por ADB.

O APK v0.7.6 usa `versionName=0.7.6-android-port`, `versionCode=706` e SHA-256 `9e7a3be222a301da82364f456290afe68955a8593539b1e16799bcf3a955e7ea`. É o package-only completo: contém launcher Java, atividade de jogo, `classes.dex` e os perfis normal/large de touch-layout. A seleção de `preTransform` Vulkan agora usa `currentTransform` para alinhar o framebuffer ao overlay de toque e manter o cursor visível; `identity` é fallback apenas quando `currentTransform` não é suportado. A confirmação visual final em dispositivo ainda é necessária.

## Escolha do driver por GPU Mali

| GPU | Driver inicial | Alternativa |
|---|---|---|
| G31/G51/G52/G76 | MaliSX2 0.6.0 | PanVK r10 no API 31+ |
| G71/G72 | PanVK r10 no API 31+ | System Vulkan |
| G57/G68/G77/G78/G78AE | MaliSX2 0.6.0 | PanVK r10 no API 31+ |
| G310/G510/G610/G710/G615/G715/G620/G720 | MaliSX2 0.6.0 | System Vulkan ou PanVK r10 quando reconhecido |
| T604/T628/T720/T760/T820/T830/T860/T880, G725 | System Vulkan | — |

A matriz completa, limitações e checksums está em [docs/ANDROID_GPU_DRIVER.pt-BR.md](docs/ANDROID_GPU_DRIVER.pt-BR.md). A matriz usa a GPU real, não o nome comercial do telefone. O launcher não bloqueia modelos Mali por whitelist; a confirmação final vem do log Vulkan.

## Build

```bash
python3 dm/android/tools/bootstrap.py
python3 dm/android/tools/build.py --game-run --version v0.7.1
python3 dm/android/tools/ci_checks.py
```

Veja [docs/ANDROID_BUILD.pt-BR.md](docs/ANDROID_BUILD.pt-BR.md) para build, instalação e publicação. O código do jogo e o pacote de dados continuam fora do repositório.

## Estado

MaliSX2 e PanVK são drivers experimentais. Compatibilidade, FPS, consumo e estabilidade variam por SoC, firmware, temperatura e cena. Turnip é somente para Qualcomm Adreno; nunca selecione Turnip em um aparelho Mali.
