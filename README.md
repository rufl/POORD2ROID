# POORD2ROID — Diablo II: Resurrected no Android na Mali-G77 MC9 / Xiaomi 11T

- [English version](README.en.md) · [Versão mais recente v0.7.5](https://github.com/rufl/POORD2ROID/releases/tag/v0.7.5) · [Changelog](CHANGELOG.md) · [Matriz completa de drivers](docs/ANDROID_GPU_DRIVER.pt-BR.md) · [Código-fonte](https://github.com/rufl/POORD2ROID) · [Licenças e créditos](THIRD-PARTY-NOTICES.md)

Port Android experimental para executar a versão de NS de **Diablo II: Resurrected** em dispositivos Android compatíveis. O projeto usa código ARM64 do jogo e implementa uma camada NVN→Vulkan; não é um emulador convencional.

> **Estado:** projeto experimental. Compatibilidade, estabilidade e desempenho variam por dispositivo, GPU, driver e versão do Android. Não há garantia de que o jogo funcione ou alcance uma taxa de quadros específica. Consulte [limitações e estado dos drivers](#drivers-e-compatibilidade).

## Requisitos e conteúdo do jogo

- Um dispositivo Android ARM64 com Vulkan funcional. As opções de driver dependem do hardware; consulte a tabela abaixo.
- Dados do jogo e chaves obtidos legalmente pelo próprio usuário. **Este repositório e o APK não incluem jogo, ROM/NSP/XCI, firmware, chaves ou conteúdo protegido.** Não fornecemos instruções para obter ou contornar proteções desses materiais.
- Espaço de armazenamento suficiente para os dados importados e caches.

O APK contém o launcher e o port, não os dados do jogo. Use o fluxo de importação suportado pelo próprio aplicativo/build. Não copie saves existentes sobre os dados do app sem backup.

## Instalação e uso

1. Obtenha o APK atual na [v0.7.5](https://github.com/rufl/POORD2ROID/releases/tag/v0.7.5), consulte todas as [Releases](https://github.com/rufl/POORD2ROID/releases) ou construa-o conforme [`docs/BUILD_ON_WINDOWS.md`](docs/BUILD_ON_WINDOWS.md). Instale apenas um APK cuja origem e assinatura você confie.
2. Abra o launcher e importe o pacote de dados preparado a partir dos seus próprios arquivos do jogo.
3. Escolha **System Vulkan** primeiro. Teste drivers alternativos individualmente apenas se forem listados para o dispositivo.
4. Inicie o jogo. Para problemas, consulte os logs descritos no guia de build e abra uma issue com modelo do aparelho, Android, GPU, driver selecionado e logs relevantes — nunca inclua chaves ou arquivos do jogo.

A versão v0.7.5 corrige a seleção da orientação da superfície Android: o framebuffer nativo do jogo volta ao comportamento Vulkan anterior que prioriza `identity`, usando `currentTransform` apenas como fallback. O APK completo de package-only, launcher/atividade Java, `classes.dex`, perfis normal/large de touch-layout e os ajustes nativos recentes foram mantidos. Não havia dispositivo Android conectado para confirmação visual final; versão, tamanho e SHA-256 estão no [changelog](CHANGELOG.md).

O launcher oferece controles táteis e suporte a controles Android/HID. O GameSir G8+ não foi validado especificamente; seu funcionamento depende de como o Android o apresenta como controle compatível.

## Drivers e compatibilidade

O menu mostra apenas as opções compatíveis com a configuração reconhecida; a disponibilidade não significa que o driver execute o jogo corretamente. Seleções personalizadas são estritas: falha de inicialização não deve ser interpretada como fallback silencioso. Volte explicitamente a **System Vulkan** para recuperar.

| Opção | Hardware/estado conhecido |
| --- | --- |
| **System Vulkan** | Driver Vulkan do fabricante; baseline recomendado para diagnóstico e fallback. |
| **MaliSX2 0.5.1** | Mali kbase via `/dev/mali0`, Android API 29+. Preferido para G31/G51/G52/G57/G68/G76/G77/G78/G78AE/G310/G510/G610/G710/G615/G715. Experimental e sem MSAA. |
| **JimVulkan PanVK r10** | Mali kbase via `/dev/mali0`, Android API 31+. Preferido para G71/G72 e alternativa para Bifrost/G77-class. Experimental/não conformante. |
| **Mesa Turnip T30** | Qualcomm/Adreno via KGSL, Android API 30+. Nunca usar em Mali. |

No Xiaomi 11T (Dimensity 1200, Mali-G77 MC9), as opções esperadas são System Vulkan, MaliSX2 e PanVK, se os respectivos pacotes forem encontrados. Um teste de inicialização ou seleção não prova gameplay estável. Os relatos anteriores indicaram que MaliSX2 foi a opção mais estável nos testes do usuário, mas isso não constitui benchmark reproduzível nem garantia.

### Tela, taxa de quadros e desempenho

Em configurações novas, o port solicita mundo em 20:9 a 1280×576 e apresentação limitada a 30 Hz/FPS; isso não garante 30 FPS sustentados. No Android, o mundo mantém a área ampla e a HUD usa um canvas lógico 16:9 central, com os toques da interface convertidos para a mesma área segura. Menus e carregamentos continuam centralizados em 16:9. A mudança está coberta por testes de geometria e build Android; a validação visual final de HUD, mapa, diálogos e controles ainda depende do aparelho.

O modo de decodificação GPU de texturas BC é opcional e vem desativado por padrão: pode deslocar trabalho da CPU para a GPU, mas não é necessariamente mais rápido em cenas limitadas pela GPU. Compare tempos de quadro sustentados antes de habilitá-lo. O cache de pipelines e o aquecimento reduzem algumas compilações repetidas, não eliminam toda compilação durante o jogo.

## Compilar e diagnosticar

O build Android é feito em Linux/WSL; veja [`docs/BUILD_ON_WINDOWS.md`](docs/BUILD_ON_WINDOWS.md) para os passos e pré-requisitos. O diretório `upstream/` mantém a base importada e as alterações integradas; [`UPSTREAM.md`](UPSTREAM.md) descreve a proveniência e o estado do snapshot.

Para recolher logs no dispositivo:

```sh
adb logcat -s D2RAndroid
adb shell run-as org.d2r.android.port cat files/game.log
```

O logcat identifica escolha/preparação/falhas do driver. `files/game.log` contém a saída nativa depois que o host inicia. O segundo comando pode não existir ou não ter conteúdo se a falha ocorrer antes desse ponto.

## Créditos e licenças

Este fork deriva do trabalho de **jhaizhou** em [d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile) e do port Android de **sydeEvans** em [d2r-android](https://github.com/sydeEvans/d2r-android). Também integra ou redistribui componentes de terceiros, incluindo Mesa Turnip, MaliSX2, JimVulkan PanVK, AdrenoTools e bibliotecas vendorizadas. [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) registra fontes, licenças e limites de redistribuição; [UPSTREAM.md](UPSTREAM.md) detalha a base de código.

Eden, LASRecomp, AMD FSR e Qualcomm SGSR foram consultados apenas como referências de projeto/pesquisa, salvo indicação em contrário nos avisos; seus códigos não são incorporados como dependências deste port. Créditos e licenças devem ser preservados.

Diablo II: Resurrected e Nintendo Switch são marcas de seus respectivos titulares. Este projeto não é oficial nem endossado pela Blizzard ou Nintendo.
