# Build e publicação Android

English: [ANDROID_BUILD.md](ANDROID_BUILD.md)  
中文：[ANDROID_BUILD.zh-CN.md](ANDROID_BUILD.zh-CN.md)

O APK genérico não contém jogo, chaves ou dados protegidos. O pacote de dados é produzido localmente com os arquivos do próprio usuário e importado no telefone.

## Build local

No Linux x86_64 ou WSL2:

```bash
python3 dm/android/tools/bootstrap.py
python3 dm/android/tools/build.py --game-run --version v0.7.1
python3 dm/android/tools/ci_checks.py
```

O resultado é `dm/android/build/D2R-Android-port.apk`. Para instalar:

```bash
adb -s DEVICE install -r dm/android/build/D2R-Android-port.apk
```

Atualizações exigem a mesma assinatura. Não desinstale antes de preservar os saves privados do aplicativo.

O build publicado atual é a [v0.7.1](https://github.com/rufl/POORD2ROID/releases/tag/v0.7.1), commit `4f0933a`, com `versionName=0.7.1-android-port`, `versionCode=701` e SHA-256 `fdc82bd5a33e9f081bf04c4cb8073358a50b9ebec5f48b85c0227323730c6312`. O build ARM64 passou; a medição A/B em aparelho continua pendente. Consulte o [changelog](../../CHANGELOG.md).

## Drivers

O APK inclui System Vulkan, MaliSX2 0.6.0, PanVK r10 e Turnip T30. As recomendações completas por GPU estão em [ANDROID_GPU_DRIVER.pt-BR.md](ANDROID_GPU_DRIVER.pt-BR.md). Turnip é exclusivo para Qualcomm Adreno; em Mali use MaliSX2, PanVK ou System Vulkan conforme a matriz.

## Verificação

```bash
python3 dm/android/tools/ci_checks.py
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

A disponibilidade no launcher não prova compatibilidade. Confirme o log `[vulkan-device] profile=` e a identidade final do driver.

## Publicação

O workflow publica automaticamente uma Release quando uma tag `v*` é enviada:

```bash
VERSION=vX.Y.Z
git tag -a "$VERSION" -m "D2R Android $VERSION" && git push origin "$VERSION"
```

A Release deve conter o APK genérico e o kit de empacotamento. Nunca publique jogo, keys, saves ou pacote de dados do usuário.
