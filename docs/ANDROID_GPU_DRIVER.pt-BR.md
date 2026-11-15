# Matriz de drivers Mali no Android

English: [ANDROID_GPU_DRIVER.md](ANDROID_GPU_DRIVER.md)

Esta é a matriz canônica por família de GPU do APK Android do POORD2ROID. A GPU real é a referência; o nome comercial do telefone não é. O mesmo modelo pode usar SoCs diferentes.

## Seleção de driver

| Driver | Caminho | Android mínimo | Recomendação |
|---|---|---:|---|
| **System Vulkan** | Vulkan do fabricante | Qualquer | Baseline, recuperação e única opção incluída para GPUs antigas/desconhecidas |
| **MaliSX2 0.5.0** | Mali kbase `/dev/mali0` | API 29 | Primeiro customizado nas famílias listadas; preferido para G57/G68/G77 |
| **PanVK r10** | Mali kbase `/dev/mali0` | API 31 | Preferido para G71/G72; alternativa para Bifrost/G77-class |
| **Turnip T30** | Qualcomm KGSL `/dev/kgsl-3d0` | API 30 | Somente Adreno; nunca usar em Mali |

Drivers customizados são experimentais. A opção aparecer no launcher não prova criação do device, apresentação ou gameplay. Não existe whitelist de telefones; o diagnóstico nativo informa `mali-bifrost`, `mali-valhall` ou `mali-unknown`.

## Matriz completa das GPUs Mali incluídas

| Família GPU | Arquitetura | Primeiro driver | Alternativa | Observações |
|---|---|---|---|---|
| G31 | Bifrost | MaliSX2 0.5.0 | PanVK r10 API 31+; System | Pouca margem de memória/largura de banda |
| G51 / G52 | Bifrost | MaliSX2 0.5.0 | PanVK r10 API 31+; System | Verifique recursos expostos pelo fabricante |
| G71 / G72 | Bifrost | PanVK r10 API 31+ | System | Não listadas pelo MaliSX2 0.5.0 |
| G76 | Bifrost | MaliSX2 0.5.0 | PanVK r10 API 31+; System | Compare no firmware real |
| G57 / G68 | Valhall v9 | MaliSX2 0.5.0 | PanVK r10 quando reconhecer; System | MaliSX2 API 29+, PanVK API 31+ |
| G77 | Valhall v9 | MaliSX2 0.5.0 | PanVK r10 API 31+; System | Xiaomi 11T G77 MC9 é referência |
| G78 / G78AE | Valhall | MaliSX2 0.5.0 | PanVK r10 quando reconhecer; System | Não transfira automaticamente resultados do G77 |
| G310 / G510 | Valhall v10 | MaliSX2 0.5.0 | System | Observe temperatura e memória sustentadas |
| G610 / G710 | Valhall v10 | MaliSX2 0.5.0 | System | Caminho customizado experimental |
| G615 / G715 | Valhall v11 | MaliSX2 0.5.0 | System; PanVK se reconhecer explicitamente | Recursos Immortalis/ray tracing não são garantidos |

## Famílias somente System Vulkan neste APK

Mali-400/450/470, T604/T628/T720/T760/T820/T830/T860/T880, G620, G725 e qualquer família ausente nas releases fixadas do MaliSX2/PanVK usam somente System Vulkan. O driver do fabricante ainda pode funcionar se expuser fila, formatos, memória e shaders necessários.

## Ordem de teste

1. Baseline System Vulkan.
2. MaliSX2 em Mali API 29–30.
3. Compare MaliSX2 e PanVK r10 em Bifrost/G77-class API 31+.
4. Use System Vulkan em caminhos customizados instáveis ou não suportados.

```bash
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

Confirme `[vulkan-device] profile=`, `[vulkan-subgroup]` e a identidade final do driver. O texto do launcher sozinho não prova o carregamento.

## Checksums fixados

- MaliSX2 archive: `71c1e3dc68ef1396d469f245019d42ab73980e48034095453b7be776195c3641`
- MaliSX2 library: `b80af8f6ed1d88a1e19d0538564e90de8f7e46f4e32f8daec7cb7a772e292721`
- PanVK archive: `7e97db4d79d81ec9461d1e0676b62a865561cb56e21c151693aa25ffbe749f1c`
- PanVK library: `80acd461bdf07f5fb99af5c7f516e272103340b6775c22dcd347a6d418c7ac0d`

Fontes: [MaliSX2 v0.5.0](https://github.com/bmdhacks/malisx2/releases/tag/v0.5.0) e [releases do JimVulkan PanVK](https://github.com/JimVulkan/mali-panvk/releases).
