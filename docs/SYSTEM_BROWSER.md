# Navegador lateral e desempenho da escrita

O ícone de planeta na barra superior abre um painel à direita. A divisória entre o quadro e o painel pode ser arrastada para ajustar a largura. Fechar e reabrir o painel preserva a página e seu histórico durante a sessão do editor. A barra de endereço aceita URLs e pesquisas; os botões permitem voltar, avançar, recarregar e abrir a página no navegador padrão.

## Runtime do sistema

No Windows, o painel usa o **Microsoft Edge WebView2 Evergreen Runtime** instalado no computador. O Scalar liga apenas o pequeno loader estático do SDK; não inclui Chromium, Qt WebEngine, um runtime fixo do Edge nem um instalador de navegador. O runtime recebe suas atualizações pelo sistema. Quando ele não está disponível, o painel explica a situação e oferece abrir a página no navegador padrão. Não há instalação automática.

Na primeira configuração Windows, o CMake baixa do NuGet o SDK `Microsoft.Web.WebView2` 1.0.2903.40, com SHA-256 fixado. Esse SDK serve à compilação; o diretório do SDK não é instalado nem distribuído com o app. Para compilar sem acesso à rede, extraia esse pacote e configure `-DSCALAR_WEBVIEW2_SDK=<diretório extraído>`.

No Linux, o painel usa a biblioteca compartilhada **WebKitGTK 4.1** do sistema. O pacote Scalar não inclui Chromium, Qt WebEngine ou uma cópia privada do WebKit. Um pequeno executável `scalar-webkit-host` mantém o loop GTK separado do Qt Quick; ele usa o WebKit compartilhado e não contém um motor web. O APT resolve `libwebkit2gtk-4.1-0` como dependência; se ela não estiver instalada, haverá download e uso de disco adicionais. Para compilar, instale `libwebkit2gtk-4.1-dev`.

A integração de janelas GTK3 usa X11. Em desktops Wayland, o aplicativo seleciona XWayland automaticamente quando `DISPLAY` está disponível e `QT_QPA_PLATFORM` não foi definido. A dependência `xwayland` é declarada no pacote. Se o backend Qt for forçado para Wayland, o painel explica como iniciar com `QT_QPA_PLATFORM=xcb`. Backends sem X11 mantêm a opção de abrir externamente.

No Linux, o host sincroniza ativação e foco pelo protocolo XEmbed para que campos dos sites exibam o cursor de texto. Ao focar a barra de endereço, o quadro ou ocultar o painel, o foco do conteúdo web é liberado.

O perfil web (cookies/cache) fica em `AppLocalDataLocation/web-profile`, separado dos arquivos dos quadros. A visualização nativa é ocultada enquanto um diálogo/painel sobreposto precisa aparecer. Links que solicitam outra janela são abertos no mesmo painel. Recursos que dependem de janelas separadas, DRM ou políticas particulares do site podem ter limitações.

O host aplica uma configuração Fontconfig própria que ignora fontes locais WOFF/WOFF2, evitando o [travamento do Skia/WebKit 325185](https://bugs.webkit.org/show_bug.cgi?id=325185) em entradas sem família. As fontes do sistema e a configuração do quadro não são alteradas; fontes web carregadas pelos sites continuam disponíveis. Há um limite de 30 segundos para apresentar uma opção de tentar novamente em carregamentos sem resposta.

### Exceção: distribuição AppImage

O script `scripts/package-appimage.sh` inclui WebKitGTK e seus processos auxiliares no AppImage, junto com GTK, TLS/GIO e plugins de mídia. O painel integrado permanece disponível sem instalar WebKitGTK separadamente. O launcher ajusta os caminhos internos do navegador em uma cópia temporária privada e mantém o sandbox habilitado. Isso exige suporte a namespaces de usuário e X11/XWayland no destino. Atualizações do navegador chegam por uma nova geração do AppImage; o `.deb` continua usando o WebKit atualizado pelo sistema. Veja [geração e limites de compatibilidade](../BUILDING.md#appimage-linux-portátil-x86_64).

## Arrastar imagens

As imagens são inseridas na posição onde são soltas e podem ser desfeitas normalmente. Quando o navegador fornece os pixels no arraste, o Scalar os incorpora diretamente, sem baixar o endereço novamente. Quando fornece HTML/URL, o Scalar prioriza o `src` da imagem sobre o link que a envolve e baixa HTTP/HTTPS em uma tarefa de fundo. URLs `data:image/` também são aceitas.

Continuam valendo os limites de 8192 pixels por dimensão, 32 MP e 32 MiB por imagem. Downloads têm limite de tempo e de tamanho. Sites que não fornecem os pixels e exigem autenticação/cookies ou usam URLs `blob:` podem impedir a importação por URL; o importador de rede não compartilha cookies do navegador. A importação não executa scripts do HTML arrastado.

## Pincel

O pincel sólido opaco preserva a geometria dos trechos cujo filtro de suavização já está estabilizado. Ela é dividida em blocos de 128 amostras; a ponta mutável é reconstruída independentemente. Durante o mesmo traço, a cena também reaproveita os objetos da página quando documento e viewport não mudam. Refinamentos de resolução aguardam o término da escrita.

Ao soltar o pincel, apenas a janela final de suavização é recalculada, preservando o mesmo resultado da suavização completa. Salvamento automático e geração de miniaturas aguardam o fim do traço antes de copiar os dados do documento. Salvar explicitamente e encerrar continuam funcionando.

Isso remove trabalho que crescia com a duração do traço e reduz contenção na interface. Não garante latência zero diante de sobrecarga do sistema, drivers ou operações externas. `SCALAR_TABLET_TRACE` continua disponível para investigar pausas reais.

Os testes em `desktop_tests` cobrem reaproveitamento de geometria, igualdade da suavização final, redimensionamento, isolamento de atalhos, navegação local com o runtime Windows, importação HTTP em segundo plano e arraste de bitmap.

Validação em Windows/Qt 6.8.2: os testes específicos acima, a navegação nativa e os testes gráficos passaram. Os dez grupos do CTest fora de `desktop` também passaram. A execução ampla de `desktop` revelou oito falhas em testes existentes (fontes, temporização, configurações, borracha/camadas e WEBP); todas foram reproduzidas compilando a revisão anterior, sem estas alterações. Um teste que mantinha um ponteiro para o editor destruído ao voltar à biblioteca foi corrigido e revalidado.
