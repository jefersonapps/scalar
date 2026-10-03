# Scalar

Quadro branco desktop offline para Windows e Linux, em C++20, Qt 6 e Qt Quick.

**Estado: Scalar 0.4 — Milestone 4 e exportação PDF.** Projetos multipágina, miniaturas, importação de PDF por intervalo e imagens por arquivo, clipboard e drag/drop estão integrados. Texto, LaTeX offline, caneta e formas permanecem editáveis. O quadro inteiro pode ser exportado em PDF multipágina. Exportação PNG, opções avançadas de exportação, régua e compasso seguem nos próximos milestones.

## Compilar

Instale as dependências de [BUILDING.md](BUILDING.md), depois:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/scalar
```

Sem Qt/CMake, o núcleo pode ser verificado com o compilador C++20:

```sh
./scripts/test-core.sh
```

## Uso previsto

Crie um quadro na Home; escolha página, orientação e fundo, ou use “Criar com padrão”. Seleção, caneta, borracha, imagens, formas e mão ficam na toolbar flutuante. Clique novamente na caneta para cores, espessura e curva de pressão. A rolagem faz zoom; Espaço + arraste move a página. Touch move o quadro e dois dedos fazem pinch. Salvar como cria uma cópia em local escolhido; quadros novos já possuem arquivo gerenciado na pasta de dados do aplicativo.

Atalhos: V seleção, P caneta, E borracha, T texto, H mão; Ctrl+Z/Ctrl+Shift+Z, Ctrl+D duplicar, Delete excluir selecionados, Ctrl+V colar imagem ou texto, Ctrl+S, Ctrl+Shift+S, Ctrl+O e Ctrl+N. Escape cancela a operação ativa.

Desenhe e mantenha pressionado por 500 ms no final para reconhecer linha, círculo, elipse, triângulo, retângulo, quadrado e polígonos regulares de 5 a 12 lados. O tempo é configurável. Segurar solicita sempre a melhor candidata, mesmo com tremor; Shift aplica tracejado. Linhas separadas com endpoints próximos (3 mm) fecham um polígono preenchido automaticamente; Ctrl+Z restaura as linhas. Uma linha reconhecida acompanha o ponteiro enquanto pressionado. Ao soltar, a conversão é confirmada; Ctrl+Z restaura o traço original. Em seleção, Shift alterna objetos; arrastar no vazio seleciona uma área. Handles editam endpoints, vértices de triângulo, centro/raio, escala proporcional e rotação.

A borracha preserva os segmentos fora da região apagada; a operação completa pode ser desfeita. Imagens são incorporadas ao arquivo e não dependem de caminho externo. PNG/JPEG/BMP usam Qt; WebP usa plugin Qt ou libwebp, e SVG requer Qt SVG ou seu plugin de imagem.

O botão de páginas abre miniaturas, adiciona uma página ou duplica a atual. Ctrl+PgUp/PgDown navega entre páginas. Importe PDF pela Home, pelo painel de páginas ou arrastando o arquivo; escolha todas as páginas ou um intervalo como `1, 3-5`. Cada página mantém seu tamanho físico, o PDF original é incorporado e as anotações ficam acima dele. Undo/redo é separado por página.

Para navegar rapidamente, use as setas no topo do editor; o contador abre as miniaturas. O botão de exportação no topo (seta para cima) salva **todas as páginas em PDF**, em sua ordem e tamanho físico, incluindo fundos, grades e conteúdo. Traços, formas, texto e matemática são vetoriais; imagens e a base PDF importada permanecem raster. Veja [exportação PDF](docs/PDF_EXPORT.md).

## Documentação

- [Arquitetura](ARCHITECTURE.md)
- [Design System](DESIGN_SYSTEM.md)
- [Build e execução](BUILDING.md)
- [Formato do projeto](FILE_FORMAT.md)
- [Entrega inicial](docs/MILESTONE_1.md)
- [Milestone 2, borracha e imagens](docs/MILESTONE_2.md)
- [Milestone 3, lixeira e validação](docs/MILESTONE_3.md)
- [Milestone 4, PDF e páginas](docs/MILESTONE_4.md)

Fundo e grade: botão de grade no topo. A cor é independente do preset de grade; trocar a grade preserva as cores, opacidade e espessura ajustadas. Novos quadros começam brancos no tema claro e pretos no escuro; mudar o tema não recolore páginas salvas. Ajustes são aplicados em tempo real. Texto: T, clique na página e escreva; `$...$` insere matemática no texto e `$$...$$` em destaque. Seleção → “Editar texto / LaTeX” reabre a fonte.

Lixeira: botão de excluir no card pede confirmação e move o arquivo e thumbnail. Na lixeira é possível restaurar ou excluir permanentemente. A expiração após 30 dias é verificada na abertura e a cada hora com o app aberto; com o app fechado, a limpeza ocorre na próxima abertura.

Não há serviços em nuvem ou telemetria. O build prepara automaticamente o MathJax e Node locais. Para preparar explicitamente o executável de desenvolvimento: `cmake --build build --target math_runtime`. A instalação/CPack exige e incorpora o runtime completo; o usuário final não precisa instalar npm/Node e a conversão funciona offline.
