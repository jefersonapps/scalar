# Milestone 4 — páginas, imagens e importação de PDF

Implementado em Scalar 0.4, com Qt 6.4.2 e C++20. A exportação PDF foi adicionada após este ciclo; veja [PDF_EXPORT.md](PDF_EXPORT.md). Exportação PNG e opções avançadas permanecem no Milestone 5.

## Entrega

- Painel de páginas com miniaturas, seleção, nova página e duplicação. Ctrl+PgUp/PgDown navega; a viewport ajusta a página selecionada. Objetos, fundo e undo/redo pertencem à página atual.
- Importação Qt PDF pela Home, painel ou drag/drop. Diálogo permite todas as páginas ou intervalo (`1, 3-5`), com validação e correção sem reiniciar a importação.
- Cada página importada conserva sua medida física. PDF original incorporado, compartilhado entre páginas e salvo uma única vez; anotações vetoriais sobre uma base PDF protegida da borracha.
- Cache PDF assíncrono por faixa de zoom/DPI, com atualização de resolução. Miniaturas em worker com debounce e lotes limitados.
- Importação de múltiplas imagens pelo diálogo, clipboard de arquivos e drag/drop. O lote forma um comando de undo. Clipboard de bitmap e texto continuam disponíveis, na página selecionada.
- Cor do fundo separada do preset de grade, com ajustes imediatos. Trocar grade preserva cores, opacidade e espessura; novos quadros começam brancos no tema claro e pretos no escuro. Trocar o tema não recolore documentos existentes.
- Formato `.board` v4 com leitura de v1–v3, preservando PDF, páginas e imagens sem depender dos arquivos de origem.

Código novo: `src/pdf/PdfService`, `PdfRenderCache`, `src/app/AppPages`, `qml/panels/PagesPanel`, `qml/components/PageThumbnail`, `qml/dialogs/ImportPdfDialog` e `tests/pdf_tests.cpp`. Arquitetura, formato, design e dependências estão documentados nos arquivos da raiz.

## Build e validação executados

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./scripts/test-core.sh
./build/scalar
```

Build concluído em Linux Mint 22.3 / GCC 13.3 / Qt 6.4.2. **10/10 entradas CTest passaram**, em 26,70 s, sem skips nas suítes Qt Test; o teste portátil de núcleo também passou. A suíte desktop teve 20 verificações e a suíte PDF, 6 (incluindo setup/cleanup).

PDFs reais temporários verificaram A4 retrato, A4 paisagem e Carta; leitura de intervalos; rasterização e cores; limites do cache; troca rápida de página; anotações e histórico separado; save/load e reabertura após remoção do PDF original. Testes desktop cobriram diálogo, drop de PDF, miniaturas, duplicação, nova página, teclado, clipboard de múltiplos arquivos, PNG/JPEG/BMP, WebP por fallback, texto na segunda página e independência fundo/grade. Os fluxos existentes de mouse, seleção, borracha, matemática e persistência continuaram passando. Não houve warnings QML nos cenários verificados.

Capturas produzidas ao executar a UI:

- `build/screenshots/milestone4-pdf-dialog.png`
- `build/screenshots/milestone4-pages.png`
- `build/screenshots/milestone4-pages-small.png` — janela de 520 × 640, painel sem overflow.

O backend offscreen exibe texturas PDF/imagens e miniaturas, mas não demonstra a renderização acelerada dos meshes de traços, grades ou equações. Windows, GPU, stylus física, DPI de múltiplos monitores e desempenho precisam de validação em hardware real.

## Limites

Qt PDF foi instalado e testado. Qt SVG e os plugins extras de imagem não estão instalados neste ambiente: o caminho opcional de importação SVG foi implementado, mas **não foi compilado/testado aqui**. A distribuição completa deve incluir Qt SVG; SVG importado como imagem é normalizado para PNG, enquanto equações continuam vetoriais.

PDFs protegidos por senha não possuem diálogo de desbloqueio. Limites atuais: 64 MiB por PDF, 1000 páginas por projeto, 128 MiB por `.board` incluindo base64; cache PDF de 96 MiB, renderização até 4096 px por lado / 16 MP. Zoom extremo fica limitado por esse teto. Não há remoção/reordenação de páginas neste ciclo; adicionar/duplicar páginas não entra no histórico de undo dos objetos. Novas páginas vazias herdam tamanho e fundo da página atual. Importação gera papel branco para conservar a aparência do PDF.

O pacote atual ainda requer deploy das bibliotecas Qt conforme BUILDING.md; não é um instalador nativo completo. Próxima etapa: completar as opções de exportação e adicionar PNG.
