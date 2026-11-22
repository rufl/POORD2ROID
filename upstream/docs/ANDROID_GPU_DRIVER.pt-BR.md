# Matriz de drivers Mali no Android

English: [ANDROID_GPU_DRIVER.md](ANDROID_GPU_DRIVER.md)  
中文：[ANDROID_GPU_DRIVER.zh-CN.md](ANDROID_GPU_DRIVER.zh-CN.md)

Esta matriz usa a família da GPU. Ela não garante que todo modelo de telefone funcione: kernel do fabricante, memória, extensões Vulkan, firmware, temperatura e apresentação ainda importam. O nome comercial do telefone não identifica a GPU com segurança; o mesmo produto pode usar SoCs diferentes.

## Drivers incluídos

| Driver | Caminho GPU/kernel | Android mínimo | Melhor uso | Limitações |
|---|---|---:|---|---|
| **MaliSX2 0.6.0** | Mali Arm via `/dev/mali0` / kbase | API 29 | Primeiro driver customizado para as famílias listadas, especialmente Valhall G57/G68/G77/G78 | Experimental; o upstream não implementa MSAA; não há garantia para toda GPU listada |
| **PanVK r10** | Mesa PanVK via `/dev/mali0` / kbase | API 31 | Alternativa para Bifrost e GPUs da classe G77 quando MaliSX2 não estiver disponível ou regredir | Experimental e não conformante; suporte e testes são mais restritos |
| **System Vulkan** | Vulkan do fabricante | nenhum | Recuperação e única opção para GPUs antigas/não listadas | Recursos e desempenho dependem do fabricante |
| **Turnip T30** | Qualcomm KGSL `/dev/kgsl-3d0` | API 30 | Apenas Adreno | Nunca usar em Mali |

O launcher não possui lista branca de modelos de telefone. Um aparelho Mali com `/dev/mali0` recebe as opções Mali; depois o probe Vulkan identifica o dispositivo real. O log usa `mali-bifrost`, `mali-valhall` ou `mali-unknown`.

## Matriz completa das GPUs Mali cobertas

“Preferido” significa o primeiro driver customizado a testar, não uma garantia. Se ele falhar ao criar o device, apresentar a imagem ou executar o jogo, use a alternativa ou System Vulkan e capture o log.

| Família GPU | Arquitetura | Driver preferido | Alternativa | Observações |
|---|---|---|---|---|
| **Mali-G31** | Bifrost | MaliSX2 0.6.0 | PanVK r10 no API 31+; System Vulkan | Baixo custo; pouca memória e largura de banda |
| **Mali-G51** | Bifrost | MaliSX2 0.6.0 | PanVK r10 no API 31+; System Vulkan | Verifique formatos/recursos Vulkan do fabricante |
| **Mali-G52** | Bifrost | MaliSX2 0.6.0 | PanVK r10 no API 31+; System Vulkan | Configuração comum de entrada/intermediária; térmica varia muito |
| **Mali-G71 / G72** | Bifrost | PanVK r10 no API 31+ | System Vulkan | Não listadas pelo MaliSX2 0.6.0; use System no API 29/30 |
| **Mali-G76** | Bifrost | MaliSX2 0.6.0 | PanVK r10 no API 31+; System Vulkan | Use o nome real da GPU, não o marketing do telefone |
| **Mali-G57** | Valhall v9 | MaliSX2 0.6.0 | PanVK r10 quando reconhecer o dispositivo; System Vulkan | MaliSX2 API 29+; PanVK API 31+ |
| **Mali-G68** | Valhall v9 | MaliSX2 0.6.0 | PanVK r10 quando reconhecer o dispositivo; System Vulkan | Valhall de entrada; reduza resolução/qualidade se houver pressão de memória |
| **Mali-G77** | Valhall v9 | MaliSX2 0.6.0 | PanVK r10 no API 31+; System Vulkan | Xiaomi 11T G77 MC9 é o aparelho de referência; MaliSX2 é o caminho customizado estável relatado pelo usuário |
| **Mali-G78 / G78AE** | Valhall v9 | MaliSX2 0.6.0 | PanVK r10 quando reconhecer; System Vulkan | Compare os dois drivers por firmware; não transfira resultados do G77 automaticamente |
| **Mali-G310 / G510** | Valhall v10 | MaliSX2 0.6.0 | System Vulkan | Observe pressão de memória e temperatura sustentada |
| **Mali-G610 / G710** | Valhall v10 | MaliSX2 0.6.0 | PanVK r10 quando reconhecer; System Vulkan | Suporte customizado ainda experimental |
| **Mali-G615 / G715** | Valhall v11 | MaliSX2 0.6.0 | PanVK r10 quando reconhecer; System Vulkan | Ray tracing não é garantido |
| **Mali-G620 / G720** | Valhall v12 | MaliSX2 0.6.0 | PanVK r10 quando reconhecer; System Vulkan | Novas famílias listadas no MaliSX2 0.6.0; valide no firmware real |

## GPUs fora da lista customizada

Neste APK, estas famílias usam **somente System Vulkan**. O telefone ainda pode executar o port se o driver do fabricante expuser fila, formatos, memória e shaders necessários; os drivers Mali customizados não adicionam suporte a elas.

- Mali-400 / Mali-450 / Mali-470
- Mali-T604 / T628 / T720 / T760 / T820 / T830 / T860 / T880
- Mali-G725 e outras famílias ausentes nas releases fixadas do MaliSX2/PanVK
- GPUs sem caminho kbase ou sem `/dev/mali0`

## Procedimento recomendado

1. Comece com **System Vulkan** e guarde o baseline.
2. Em Mali API 29–30, teste **MaliSX2 0.6.0**.
3. Em Bifrost/G77 API 31+, compare **MaliSX2** e **PanVK r10**; mantenha o que entra no jogo e permanece estável em uma cena longa.
4. Em GPU antiga/não listada, mantenha **System Vulkan**.
5. Nunca use Turnip em Mali.

```bash
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

Procure `[driver]`, `[vulkan-device] profile=`, `[vulkan-subgroup]` e a identidade final do driver. O texto do launcher sozinho não prova que o ICD foi carregado.

## Artefatos fixados

- MaliSX2 `malisx2-0.6.0-62d6d62423.zip`
  - SHA-256 do arquivo: `55b17293a6e3ab089e2d255cc6741f574def4c241265547d4a5aaa01cb4047f8`
  - SHA-256 da biblioteca: `c1c8738fabd6a3b5a8c2d3d0a732b1498a0569251d22cf7f05cd50c81db176fb`
- PanVK `mali-panvk-27.0.0-devel-r10.zip`
  - SHA-256 do arquivo: `7e97db4d79d81ec9461d1e0676b62a865561cb56e21c151693aa25ffbe749f1c`
  - SHA-256 da biblioteca: `80acd461bdf07f5fb99af5c7f516e272103340b6775c22dcd347a6d418c7ac0d`

Fontes: [MaliSX2 v0.6.0](https://github.com/bmdhacks/malisx2/releases/tag/v0.6.0), [releases do JimVulkan PanVK](https://github.com/JimVulkan/mali-panvk/releases) e os checksums fixados em `dm/android/tools/build.py`.
