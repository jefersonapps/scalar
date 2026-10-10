# Scalar

Quadro branco desktop offline para Windows e Linux, em C++20, Qt 6 e Qt Quick.

**Estado: Scalar 0.5 — Milestone 5, ferramentas geométricas.** Régua com snap, compasso e construções geométricas estão integrados ao canvas vetorial. Projetos multipágina, miniaturas, PDF, imagens, texto e LaTeX offline permanecem disponíveis. O quadro inteiro pode ser exportado em PDF multipágina. Exportação PNG e opções avançadas de exportação permanecem para um ciclo futuro.

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

Cada página mantém seu zoom e posição durante a sessão. Páginas novas herdam o zoom anterior e começam no topo; ao reiniciar o aplicativo, a vista volta ao ajuste padrão. A escrita livre usa curvas adaptativas já durante o desenho, com transições suaves de pressão, incluindo no PDF exportado. O zoom reutiliza a geometria da caneta e as imagens dos marcadores; o refinamento dos marcadores é preparado em segundo plano.

A função Apagar/Erase do Wacom Center e a ponta de borracha da caneta ativam temporariamente a borracha quando o driver informa o modo Eraser. Ao voltar à ponta normal ou retirar a caneta da proximidade do tablet, a ferramenta anterior é restaurada. O atalho de teclado E continua selecionando a borracha.

Clique em Mover quadro para selecionar a ferramenta, ou segure H fora dos campos de texto para usá-la temporariamente; ao soltar H, a ferramenta anterior volta. O cursor mostra uma mão aberta e se fecha durante o arraste.

Crie um quadro na Home; escolha página, orientação e fundo, ou use “Criar com padrão”. Seleção, caneta, borracha, imagens, formas e mão ficam na toolbar flutuante. Clique novamente na caneta para cores, espessura e curva de pressão. A rolagem move a página verticalmente; Ctrl + rolagem dá zoom. Espaço + arraste move a página. Touch move o quadro e dois dedos fazem pinch. Salvar como cria uma cópia em local escolhido; quadros novos já possuem arquivo gerenciado na pasta de dados do aplicativo.

Texto: selecione T e arraste para definir a caixa. Escreva diretamente no quadro; clique fora ou use Ctrl+Enter para concluir, e Esc para cancelar. Duplo clique permite editar textos existentes. O botão no canto superior esquerdo abre fonte, tamanho, cor, alinhamento, negrito e itálico; a fonte cursiva é o padrão e a lista inclui as fontes instaladas no sistema. Use $...$ ou $$...$$ para matemática LaTeX, convertida em geometria vetorial ao concluir a edição.

Atalhos: V seleção, P caneta, M marcador, E borracha, T texto, H mão; Ctrl+Z/Ctrl+Shift+Z, Ctrl+D duplicar, Delete excluir selecionados, Ctrl+V colar imagem ou texto, Ctrl+S, Ctrl+Shift+S, Ctrl+O e Ctrl+N. Escape cancela a operação ativa.

Ao criar, mover ou editar uma reta, o quadro mostra seu ângulo de 0° a 360°, medido no sentido anti-horário a partir da horizontal para a direita. Segure Ctrl ao criar, girar ou arrastar uma extremidade para encaixar em múltiplos de 15°. O menu de formas e as propriedades do objeto permitem ajustar o comprimento dos traços, o intervalo do tracejado e a distância entre os pontos, em milímetros. Ao selecionar um setor circular, marque “Mostrar ângulo” para exibir sua abertura no quadro; essa opção fica salva por objeto.

Desenhe e mantenha pressionado por 1 segundo no final para reconhecer linha, círculo, elipse, triângulo, retângulo, quadrado, paralelogramo, setores circulares e polígonos regulares de 5 a 12 lados. Arcos circulares reconhecidos fecham um setor com dois raios e preenchimento translúcido de 10%. O tempo é configurável. Segurar solicita sempre a melhor candidata, mesmo com tremor; Shift aplica tracejado. Linhas separadas com endpoints próximos (3 mm) fecham um polígono preenchido automaticamente; Ctrl+Z restaura as linhas. Uma linha reconhecida acompanha o ponteiro enquanto pressionado. Ao soltar, a conversão é confirmada; Ctrl+Z restaura o traço original. Em seleção, Shift alterna objetos; arrastar no vazio seleciona uma área. Handles editam endpoints, vértices de triângulo, centro/raio, escala proporcional e rotação.

A borracha preserva os segmentos fora da região apagada; cada gesto completo pode ser desfeito. Ela acompanha todo o caminho entre os eventos, incluindo movimentos rápidos, e preserva a pressão do pincel e a transparência do marcador. Para apagar formas, marque “Apagar formas também” ou segure Ctrl; Shift restaura os trechos apagados nesta sessão do quadro. Ao fechar o quadro ou o aplicativo, o histórico recuperável é descartado; o arquivo conserva apenas o resultado consolidado do apagamento. Imagens são incorporadas ao arquivo e não dependem de caminho externo. PNG/JPEG/BMP usam Qt; WebP usa plugin Qt ou libwebp, e SVG requer Qt SVG ou seu plugin de imagem.

O botão de páginas abre miniaturas, adiciona uma página ou duplica a atual. Cada miniatura tem um botão neutro de excluir no canto superior direito; excluir a última página cria uma página em branco. Ctrl+PgUp/PgDown navega entre páginas. Importe PDF pela Home, pelo painel de páginas ou arrastando o arquivo; escolha todas as páginas ou um intervalo como `1, 3-5`. Cada página mantém seu tamanho físico, o PDF original é incorporado e as anotações ficam acima dele. Undo/redo é separado por página.

Para navegar rapidamente, use as setas no topo do editor; o contador abre as miniaturas. O botão de exportação no topo (seta para cima) salva **todas as páginas em PDF**, em sua ordem e tamanho físico, incluindo fundos, grades e conteúdo. Traços, formas, texto e matemática são vetoriais; imagens e a base PDF importada permanecem raster. Veja [exportação PDF](docs/PDF_EXPORT.md).

Uma seleção pode ser movida arrastando qualquer ponto dentro da sua caixa. O painel de propriedades e o menu do botão direito permitem exportar apenas os objetos selecionados como PDF ou SVG, recortados ao conteúdo e sem o fundo do quadro. O SVG mantém transparência; partes apagadas usam imagens com alpha, enquanto os demais traços e formas permanecem vetoriais.

Régua: arraste o corpo para mover, o handle esquerdo para girar e o direito para alterar o comprimento. Com a régua visível, desenhe perto de uma borda com a caneta: os pontos são projetados sobre a reta, preservando pressão. Compasso: arraste a dobradiça ou a haste metálica para mover; arraste a haste azul para ajustar abertura e orientação com a ponta seca fixa. Arraste a ponta do lápis para desenhar. Uma volta completa cria uma circunferência vetorial; uma volta parcial mantém o arco como traço. Clique novamente no botão da ferramenta para medidas em mm, snap, centralização e ocultação. Escape cancela o gesto; Ctrl+Z desfaz o desenho.

Selecione uma linha para criar uma paralela, perpendicular ou mediatriz pelo painel contextual. Selecione um triângulo para criar sua circunferência circunscrita. As construções são objetos editáveis independentes; régua e compasso são guias temporários e não aparecem na exportação.

## Documentação

- [Arquitetura](ARCHITECTURE.md)
- [Design System](DESIGN_SYSTEM.md)
- [Build e execução](BUILDING.md)
- [Formato do projeto](FILE_FORMAT.md)
- [Entrega inicial](docs/MILESTONE_1.md)
- [Milestone 2, borracha e imagens](docs/MILESTONE_2.md)
- [Milestone 3, lixeira e validação](docs/MILESTONE_3.md)
- [Milestone 4, PDF e páginas](docs/MILESTONE_4.md)
- [Milestone 5, régua e compasso](docs/MILESTONE_5.md)

Fundo e grade: botão de grade no topo. A cor é independente do preset de grade; trocar a grade preserva as cores, opacidade e espessura ajustadas. Novos quadros começam brancos no tema claro e pretos no escuro; mudar o tema não recolore páginas salvas. Ajustes são aplicados em tempo real. Texto: T, clique na página e escreva; `$...$` insere matemática no texto e `$$...$$` em destaque. Seleção → “Editar texto / LaTeX” reabre a fonte.

Nomes: o botão de ajustes no card abre a edição do quadro. No quadro aberto, clique no título para renomear; Enter ou um clique fora salva o nome. A busca da biblioteca encontra quadros em todas as pastas e ignora maiúsculas e acentos. “Recentes” mostra os 12 quadros alterados mais recentemente; “Todos os quadros” inclui toda a biblioteca.

Lixeira: o botão de excluir fica no modal de edição do quadro, pede confirmação e move o arquivo e thumbnail. Na lixeira é possível restaurar ou excluir permanentemente. A expiração após 30 dias é verificada na abertura e a cada hora com o app aberto; com o app fechado, a limpeza ocorre na próxima abertura.

Pastas: “Nova pasta” cria uma pasta no nível atual, com nome e cinco cores (azul, verde, ocre, roxo e rosa). A navegação lateral permite abrir e expandir subpastas. Arraste um card para uma pasta ou para Voltar para subir um nível. No ícone de ajustes, altere nome/cor ou exclua a pasta; o conteúdo volta ao nível anterior. Ao sair de um quadro, a navegação retorna à pasta de origem. Organização é local no SQLite, sem mover arquivos; salvamento e restauração da lixeira preservam a pasta. A interface usa azul primário nos temas claro e escuro.

Não há serviços em nuvem ou telemetria. O build prepara automaticamente o MathJax e Node locais. Para preparar explicitamente o executável de desenvolvimento: `cmake --build build --target math_runtime`. A instalação/CPack exige e incorpora o runtime completo; o usuário final não precisa instalar npm/Node e a conversão funciona offline.

Texto: a fonte manuscrita padrão Lobster Two acompanha o aplicativo e suporta os acentos do português. O seletor pesquisa as fontes instaladas e mostra seus nomes na própria fonte. Acima da caixa, **B** e *I* formatam o trecho selecionado (Ctrl+B/Ctrl+I); a formatação é preservada ao salvar e reabrir. A licença da fonte está em `assets/fonts/LobsterTwo-OFL.txt`.

Marcador (M): desenhe com cor, largura e opacidade próprias, começando em 10%. Clique novamente na ferramenta para ajustar a cor, a largura e a opacidade. Ctrl + clique tenta preencher uma região delimitada por traços ou contornos de formas, inclusive duas linhas e um arco. Pontas com aberturas de até 2 mm podem ser conectadas. Regiões que alcançam a borda da página são recusadas; grade, imagens e traços do marcador não formam barreiras. Preenchimentos preservam vazios internos, podem ser movidos ou excluídos pela seleção e suportam desfazer/refazer e salvar/reabrir.
