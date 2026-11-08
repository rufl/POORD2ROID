# POORD2OID — Diablo II: Resurrected no Android

[English version](README.en.md) · [Código-fonte](https://github.com/rufl/POORD2OID) · [Licenças e créditos](THIRD-PARTY-NOTICES.md)

Port Android experimental para executar a versão de Nintendo Switch de **Diablo II: Resurrected** em dispositivos Android compatíveis. O projeto usa código ARM64 do jogo e implementa uma camada NVN→Vulkan; não é um emulador convencional.

> **Estado:** projeto experimental. Compatibilidade, estabilidade e desempenho variam por dispositivo, GPU, driver e versão do Android. Não há garantia de que o jogo funcione ou alcance uma taxa de quadros específica. Consulte [limitações e estado dos drivers](#drivers-e-compatibilidade).

## Requisitos e conteúdo do jogo

- Um dispositivo Android ARM64 com Vulkan funcional. As opções de driver dependem do hardware; consulte a tabela abaixo.
- Dados do jogo e chaves obtidos legalmente pelo próprio usuário. **Este repositório e o APK não incluem jogo, ROM/NSP/XCI, firmware, chaves ou conteúdo protegido.** Não fornecemos instruções para obter ou contornar proteções desses materiais.
- Espaço de armazenamento suficiente para os dados importados e caches.

O APK contém o launcher e o port, não os dados do jogo. Use o fluxo de importação suportado pelo próprio aplicativo/build. Não copie saves existentes sobre os dados do app sem backup.

## Instalação e uso

1. Obtenha um APK em [Releases](https://github.com/rufl/POORD2OID/releases) ou construa-o conforme [`docs/BUILD_ON_WINDOWS.md`](docs/BUILD_ON_WINDOWS.md). Instale apenas um APK cuja origem e assinatura você confie.
2. Abra o launcher e importe o pacote de dados preparado a partir dos seus próprios arquivos do jogo.
3. Escolha **System Vulkan** primeiro. Teste drivers alternativos individualmente apenas se forem listados para o dispositivo.
4. Inicie o jogo. Para problemas, consulte os logs descritos no guia de build e abra uma issue com modelo do aparelho, Android, GPU, driver selecionado e logs relevantes — nunca inclua chaves ou arquivos do jogo.

O launcher oferece controles táteis e suporte a controles Android/HID. O GameSir G8+ não foi validado especificamente; seu funcionamento depende de como o Android o apresenta como controle compatível.

## Drivers e compatibilidade

O menu mostra apenas as opções compatíveis com a configuração reconhecida; a disponibilidade não significa que o driver execute o jogo corretamente. Seleções personalizadas são estritas: falha de inicialização não deve ser interpretada como fallback silencioso. Volte explicitamente a **System Vulkan** para recuperar.

| Opção | Hardware/estado conhecido |
| --- | --- |
| **System Vulkan** | Driver Vulkan fornecido pelo fabricante do dispositivo; referência recomendada para diagnóstico. |
| **MaliSX2 0.4.0** | Pacote de terceiros para dispositivos Mali/kbase. Está incluído com licença e checksums; o upstream lista Mali-G77, mas desempenho e compatibilidade com D2R no Xiaomi 11T não estão garantidos. |
| **JimVulkan PanVK r9** | Driver experimental para Mali/kbase. Pode carregar e inicializar sem conseguir apresentar frames ou executar o jogo; trate como experimental. |
| **Mesa Turnip T30** | Build para dispositivos Qualcomm/Adreno com KGSL. Não é uma opção para o Mali-G77 do Xiaomi 11T. |

No Xiaomi 11T (Dimensity 1200, Mali-G77 MC9), as opções esperadas são System Vulkan, MaliSX2 e PanVK, se os respectivos pacotes forem encontrados. Um teste de inicialização ou seleção não prova gameplay estável. Os relatos anteriores indicaram que MaliSX2 foi a opção mais estável nos testes do usuário, mas isso não constitui benchmark reproduzível nem garantia.

### Tela, taxa de quadros e desempenho

Em configurações novas, o port solicita uma área de jogo 20:9 em 1280×576 e limita a apresentação a 30 Hz/FPS, conforme as preferências iniciais do launcher. Isso é uma solicitação de configuração, não uma garantia de 30 FPS sustentados; cenas pesadas, compilação de shaders, temperatura e limites do driver podem reduzir o desempenho. Menus e telas de carregamento podem manter apresentação central 16:9. A área útil pode diferir por causa de recortes/barras do Android; confira HUD, mapa, diálogos e controles no aparelho.

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
