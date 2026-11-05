# Navegador lateral e desempenho da escrita

O ícone de planeta na barra superior abre um painel à direita. A divisória entre o quadro e o painel pode ser arrastada para ajustar a largura. Fechar e reabrir o painel preserva a página e seu histórico durante a sessão do editor. A barra de endereço aceita URLs e pesquisas; os botões permitem voltar, avançar, recarregar e abrir a página no navegador padrão.

## Runtime do sistema

No Windows, o painel usa o **Microsoft Edge WebView2 Evergreen Runtime** instalado no computador. O Scalar liga apenas o pequeno loader estático do SDK; não inclui Chromium, Qt WebEngine, um runtime fixo do Edge nem um instalador de navegador. O runtime recebe suas atualizações pelo sistema. Quando ele não está disponível, o painel explica a situação e oferece abrir a página no navegador padrão. Não há instalação automática.

Na primeira configuração Windows, o CMake baixa do NuGet o SDK `Microsoft.Web.WebView2` 1.0.2903.40, com SHA-256 fixado. Esse SDK serve à compilação; o diretório do SDK não é instalado nem distribuído com o app. Para compilar sem acesso à rede, extraia esse pacote e configure `-DSCALAR_WEBVIEW2_SDK=<diretório extraído>`.

No Linux, esta implementação oferece abrir o navegador padrão externamente. Ela não adiciona um motor web ao pacote Linux. O quadro continua aceitando imagens arrastadas do navegador externo.

O perfil web (cookies/cache) fica em `AppLocalDataLocation/web-profile`, separado dos arquivos dos quadros. A visualização nativa é ocultada enquanto um diálogo/painel sobreposto precisa aparecer. Links que solicitam outra janela são abertos no mesmo painel. Recursos que dependem de janelas separadas, DRM ou políticas particulares do site podem ter limitações.

## Arrastar imagens

As imagens são inseridas na posição onde são soltas e podem ser desfeitas normalmente. Quando o navegador fornece os pixels no arraste, o Scalar os incorpora diretamente, sem baixar o endereço novamente. Quando fornece HTML/URL, o Scalar prioriza o `src` da imagem sobre o link que a envolve e baixa HTTP/HTTPS em uma tarefa de fundo. URLs `data:image/` também são aceitas.

Continuam valendo os limites de 8192 pixels por dimensão, 32 MP e 32 MiB por imagem. Downloads têm limite de tempo e de tamanho. Sites que não fornecem os pixels e exigem autenticação/cookies ou usam URLs `blob:` podem impedir a importação por URL; o importador de rede não compartilha cookies do navegador. A importação não executa scripts do HTML arrastado.

## Pincel

O pincel sólido opaco preserva a geometria dos trechos cujo filtro de suavização já está estabilizado. Ela é dividida em blocos de 128 amostras; a ponta mutável é reconstruída independentemente. Durante o mesmo traço, a cena também reaproveita os objetos da página quando documento e viewport não mudam. Refinamentos de resolução aguardam o término da escrita.

Ao soltar o pincel, apenas a janela final de suavização é recalculada, preservando o mesmo resultado da suavização completa. Salvamento automático e geração de miniaturas aguardam o fim do traço antes de copiar os dados do documento. Salvar explicitamente e encerrar continuam funcionando.

Isso remove trabalho que crescia com a duração do traço e reduz contenção na interface. Não garante latência zero diante de sobrecarga do sistema, drivers ou operações externas. `SCALAR_TABLET_TRACE` continua disponível para investigar pausas reais.

Os testes em `desktop_tests` cobrem reaproveitamento de geometria, igualdade da suavização final, redimensionamento, isolamento de atalhos, navegação local com o runtime Windows, importação HTTP em segundo plano e arraste de bitmap.

Validação em Windows/Qt 6.8.2: os testes específicos acima, a navegação nativa e os testes gráficos passaram. Os dez grupos do CTest fora de `desktop` também passaram. A execução ampla de `desktop` revelou oito falhas em testes existentes (fontes, temporização, configurações, borracha/camadas e WEBP); todas foram reproduzidas compilando a revisão anterior, sem estas alterações. Um teste que mantinha um ponteiro para o editor destruído ao voltar à biblioteca foi corrigido e revalidado.
