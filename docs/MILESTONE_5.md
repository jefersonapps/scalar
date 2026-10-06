# Milestone 5 — régua, snap, compasso e construções

Scalar 0.5.0, C++20 e Qt 6.4.2. Este ciclo segue a numeração revisada pelo usuário: ferramentas geométricas. A exportação PDF já existente permanece disponível; PNG e opções adicionais de exportação ficam para outro ciclo.

## Funcionalidades

- Régua temporária com divisões mm/cm, movimento, rotação e comprimento ajustável (20–500 mm).
- Snap configurável nas duas bordas: distância inicial de 3 mm, ajustável entre 0,5 e 10 mm. Cada amostra verifica proximidade à borda e aos endpoints; fora dessa região o traço volta ao desenho livre. Largura por pressão é preservada.
- Compasso com centro móvel, abertura ajustável (raio de 1–500 mm), hastes, ponta de desenho e indicação de raio/diâmetro.
- Arrastar a ponta ao redor do centro desenha um arco. Uma volta completa produz CircleObject; uma volta parcial mantém StrokeObject. O botão “Traçar circunferência” também cria um círculo exato.
- Construções no painel contextual: paralela, perpendicular, mediatriz de uma linha e circunferência circunscrita a um triângulo.
- Escape cancela gestos e restaura a posição anterior do guia. Desenhos/construções participam do undo/redo e salvamento existentes.
- Painéis claro/escuro, rolagem vertical em janela pequena e toolbar com rolagem horizontal quando necessário.

## Uso

Clique na régua na toolbar. Arraste seu corpo para mover; handle esquerdo gira e direito altera comprimento. Use a caneta perto de uma borda para desenhar com snap. Clique novamente no botão da régua para medidas, ângulo, snap e ocultação.

Clique no compasso. Arraste a dobradiça, a haste metálica ou a ponta seca para mover. Arraste a haste azul para abertura/orientação: a ponta do lápis acompanha o deslocamento, sem salto inicial, mantendo a ponta seca fixa. Arraste a ponta do lápis para traçar. Clique novamente na ferramenta para raio, centralização, ocultação ou circunferência imediata. O touch move/abre os guias; desenho de arco usa mouse/stylus.

O visual usa duas hastes metálicas, dobradiça, pega, ponta seca e lápis azul, sem triângulo preenchido ou círculo auxiliar permanente. As hastes mantêm comprimento físico de 40 mm nas aberturas usuais e se expandem para raios maiores que o instrumento. Testes adicionais verificam arraste diagonal, posição fixa do centro, ausência de salto ao agarrar, movimento pela dobradiça e restauração após Escape.

A região de captura da abertura cobre toda a haste do lápis, incluindo a peça azul. Clicar no vazio não reposiciona o compasso. Com Selecionar, clique/arraste no compasso ou régua para ajustar sem trocar de ferramenta; arrastar a ponta do lápis desenha arcos/circunferências também neste modo. O guia selecionado recebe destaque na borda. Delete oculta somente esse guia e não apaga objetos do documento. Os testes cobrem a extremidade azul em zoom alto, ajuste e desenho com Selecionar, saída do snap e ocultação individual dos dois guias.

Selecione uma LineObject ou TriangleObject para acessar construções. A paralela começa deslocada 10 mm; a perpendicular atravessa o primeiro endpoint; a mediatriz atravessa o ponto médio. Objetos resultantes podem ser movidos/editados normalmente. Triângulos colineares não produzem circunferência.

## Arquivos e arquitetura

`src/tools/GeometryTools.{h,cpp}` contém projeção, transformações, acumulação angular e construções. `src/canvas/CanvasGeometryTools.cpp` integra a máquina de estados ao input normalizado e aos comandos existentes. QML apresenta os guias em `RulerGuide.qml` e `CompassGuide.qml`; `GeometryOptions.qml` reutiliza o Design System.

Guias são temporários: movimento/configuração não altera o documento nem aciona autosave. Trocar de página ou abrir projeto oculta os guias. Somente os objetos desenhados são persistidos/exportados; não houve mudança no formato `.board`.

## Compilação e validação

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/scalar
```

Nenhuma dependência nova. `geometry_tools` verifica projeção, snap rotacionado, passagem angular por ±π, volta completa, retorno angular e construções matemáticas. `desktop` simula stylus com pressão, mouse e touch, exercita handles, zoom, snap desligado, arco/círculo, cancelamento, undo/redo, save/reopen e painéis em janela de 520 × 640. Os smokes abrem Home/editor no executável.

Resultado nesta máquina Linux Mint 22.3: compilação concluída; **11/11 entradas CTest passaram**, em aproximadamente 31 segundos. A suíte desktop passou sem avisos QML ou testes ignorados. Um aviso de binding circular ao redimensionar o painel foi corrigido antes da execução final da suíte.

Capturas geradas e revisadas:

- `build/screenshots/milestone5-guides-dark.png`
- `build/screenshots/milestone5-ruler-options-light.png`
- `build/screenshots/milestone5-compass-options-light.png`
- `build/screenshots/milestone5-ruler-options-small.png`

O backend offscreen software mostra os guias QML, mas não os meshes personalizados do canvas. Os testes verificam os objetos resultantes e suas coordenadas; as capturas não comprovam renderização GPU dos traços.

## Limitações

Construções são objetos independentes, sem vínculos dinâmicos quando a forma fonte muda. Arcos parciais são traços, sem ArcObject paramétrico. A circunferência completa usa espessura constante do estilo atual, enquanto o arco parcial preserva pressão. Guias não são salvos como preferências. Caneta física, desempenho GPU, HiDPI em múltiplos monitores e execução no Windows ainda exigem validação nos dispositivos correspondentes.

O painel de propriedades separa contorno, preenchimento, construções e camadas, com ações de duplicar/excluir fixas e rolagem em janelas menores. O estilo de objetos selecionados pode ser contínuo, tracejado ou pontilhado, com undo/redo. Triângulos oferecem circunferências inscrita (tangente aos lados) e circunscrita (pelos vértices); as construções seguem independentes da forma de origem.
