# Arquitetura

## Árvore atual

```text
src/
  app/          entry point, AppController, ponte QML
  documents/    Project, Page, StrokeObject, PointerSample, unidades
  commands/     histórico de adição, conversão, transformação, estilo e exclusão
  input/        normalização de mouse e tablet, filtro de pressão
  canvas/       QQuickItem, máquina de estados, viewport, eventos
  rendering/    tesselação de traços/formas e CanvasScene (Scene Graph)
  geometry/     bounds, hit testing, transforms, views de objetos
  recognition/  resampling, RDP, fitting e confiança
  selection/    IDs selecionados e seleção por área
  tools/        borracha por cápsula com divisão de strokes
  clipboard/    importação/encoding de imagens em worker
  persistence/  ZIP, JSON, substituição atômica, SQLite, thumbnails
qml/
  theme/        singleton Theme e tokens
  components/   botões, campos, sliders, ícones, superfícies
  dialogs/      projeto novo, preferências
  pages/        Home, editor
  toolbars/     FloatingToolbar
  panels/       opções contextuais da caneta
scripts/        testes portáteis
 tests/         núcleo C++ e integração Qt Test
```

Os módulos pdf e export continuam planejados; math já contém o conversor local e validação SVG. Não há arquivos vazios para simular funcionalidades futuras.

## Documento e unidades

Project contém identidade, nome, datas e páginas. Cada página tem ID, largura/altura em milímetros, fundo RGBA e objetos StrokeObject com identidade e estilo próprios. As amostras têm posição em mm, pressão, tilt, rotação, timestamp, botões e origem. A UI do ciclo inicial edita somente a primeira página; persistência aceita várias páginas e tamanhos individuais.

As coordenadas do documento não dependem de DPI ou viewport. `screen = world * (96/25.4) * zoom + pan`. Tela usa pixels lógicos do Qt. Device pixel ratio e troca de monitor pertencem ao backend Qt Quick; a geometria original não é escalada nem regravada. Alterar orientação normaliza o par largura/altura; não transforma traços existentes porque configuração de página é apenas de criação neste ciclo.

CanvasObject é uma variant tipada de StrokeObject, ShapeObject, ImageObject e TextObject. Page mantém coleções concretas; objectViews ordena referências por zIndex sem copiar amostras no render loop. ShapeObject é paramétrico: endpoints/vértices para linhas/polígonos, centro/raios/rotação para círculo/elipse. ImageObject tem quatro cantos e backing PNG imutável compartilhado; mover/duplicar não copia o bitmap. ObjectProperties tem locked, visible, zIndex e revisão transitória do cache. Transforms editam coordenadas físicas; undo guarda before/after. Não há matriz genérica persistida para strokes neste ciclo.

## Entrada

UI e entrada estão na main thread. CanvasItem tem uma máquina de estados com Drawing, PossibleHold, ShapePreview, CreatingShape, Erasing e estados de navegação/edição. CanvasSelection separa seleção/handles; CanvasScene separa os nós gráficos do input. InputManager produz PointerSample; o renderer e o documento não conhecem QTabletEvent. Eventos de tablet são interceptados na QQuickWindow e mapeados para a área local do item; a toolbar é excluída. Eventos sintéticos de mouse são consumidos para evitar traços duplicados. O caminho de tablet precisa ser validado nos drivers Windows/Linux.

Stylus desenha; mouse usa caneta/mão; toque navega. Middle drag ou Space drag faz pan. Wheel zoom ancora no ponteiro; pixel deltas do trackpad fazem pan, Ctrl + pixel delta faz zoom; ZoomNativeGesture trata pinch quando fornecido pelo sistema. Touch de dois dedos usa razão de distâncias. Durante stylus ativo, toque é ignorado: isso é uma proteção básica, não uma validação de palm rejection em hardware.

Pressão: minWidth + (maxWidth - minWidth) × pow(clamp(pressure × sensitivity), gamma). Filtro exponencial leve somente na pressão; posições não são atrasadas. Mouse tem pressão 1. Release de tablet preserva a última pressão para não produzir estreitamento artificial. Escape, perda de grab, janela desativada ou item desabilitado cancelam o preview sem comando parcial.

## Renderização

CanvasItem usa QQuickItem/Qt Quick Scene Graph. Cada stroke concluído gera QSGGeometryNode com triângulos em mm. Segmentos têm largura interpolada; discos aproximados por 12 segmentos fecham extremidades e junções. O stroke ativo possui nó separado. Traços e formas concluídos não são retesselados a cada pointer move; revisão/tipo invalidam apenas o nó alterado. Fill e borda de formas usam meshes separados. Imagens usam textura e transformação vetorial dos cantos; o bitmap é decodificado previamente em worker. Uma transformação de raiz implementa pan/zoom; clip limita o desenho à página.

MSAA 4x é solicitado no início, mas disponibilidade e antialiasing real dependem do backend. Não há QQuickPaintedItem no canvas. QPainter gera thumbnails e caches de texto em worker; o documento continua editável. Ainda não há culling, índice espacial, atualização incremental do mesh ativo ou batching explícito. A tesselação do traço ativo cresce linearmente com suas amostras e o sync percorre IDs de objetos existentes; benchmark será necessário antes de declarar 60 FPS ou baixa latência.

## Comandos

History armazena comandos com alterações before/after de objetos e fundos (Add, Delete, Transform, Convert, Style, Background). Undo/redo aplica o mesmo ID e geometria; operações de seleção/borracha agrupam suas alterações em um comando. Reconhecimento faz primeiro Add do traço e depois Convert: primeiro undo restaura o original, segundo o remove. Histórico não é persistido.

Reconhecimento roda em QtConcurrent após hold com tolerância 0,6 mm e tempo 250–1500 ms, padrão 500. Resampling para 512 pontos limita fitting/RDP; IDs e epochs descartam resultados após cancelamento/release. Linha aberta, círculos/ellipses por resíduos, e polígonos convexos com ângulos/lados validados. É heurístico, sem IA em nuvem; tolerâncias precisam de calibração com escrita real.

Borracha calcula interseções analíticas dos segmentos de stroke com uma cápsula entre posições do ponteiro. Limites novos interpolam pressão e demais amostras. Preview preserva as partes não apagadas; release registra exclusão de originais + adição dos fragmentos. Shapes/images não são rasterizados nem apagados pela borracha por trecho; seleção + Delete os remove.

## Persistência e threads

SQLite via Qt SQL fica na main thread para consultas pequenas de recentes e preferências. O arquivo do projeto usa JSON em ZIP STORE implementado com STL e sem APIs privadas. Serialization, escrita e thumbnails rodam no pool QtConcurrent. Abrir projeto também usa worker. Autosave agenda 1600 ms após alteração de documento, nunca por pointer move; um único writer por controlador preserva ordem. Se ocorrer uma nova alteração durante save, o snapshot antigo não limpa dirty state da revisão nova.

Uma cópia do documento é obtida na main thread antes de lançar o worker. Esse custo ainda pode ser perceptível em documentos muito grandes; snapshots imutáveis compartilhados são a próxima otimização. Ao trocar de documento ou fechar, flush pode esperar o worker e precisa bloquear para garantir persistência; durante desenho normal o save não desabilita o canvas.

Arquivos novos ficam em AppDataLocation/projects. `.board.png` é thumbnail secundário; `library.sqlite` guarda metadados e settings. `session.board` e cleanShutdown detectam término anormal. Falha no flush impede fechamento normal pela janela e preserva recuperação; sucesso remove sessão temporária. Recuperação é oferecida em diálogo, e o quadro recuperado é salvo em arquivo gerenciado antes de sair.

## Limites de escopo

Não há importação/exportação PDF, régua ou compasso. Futuro pipeline de exportação deverá consumir o mesmo modelo em mm e separar renderização de página da viewport. Qt PDF será dependência do módulo de importação; MathJax e Node são incorporados ao pacote offline.

## Milestone 3

BackgroundStyle define gridType, gridColor, opacity, thicknessMm e spacingX/Y; a cor base preserva o campo legado Page.background. BackgroundMesh gera um único batch de triângulos para a região visível. Zoom distante omite subdivisões e amostra grades em múltiplos do espaçamento para limitar custo. Mudanças na caneta não regeneram a grade. Presets customizados ficam na tabela SQLite background_presets.

TextObject contém fonte UTF-8, família do sistema, tamanho em pontos, bold/italic/alinhamento, RGBA, quatro cantos e MathFragments. Cada fragmento retém offset/comprimento em bytes, LaTeX, display, SVG e dimensões em em. MathJax 3.2.2 roda em um subprocesso Node privado ao pacote, lançado em worker; não há navegador nem conexão de rede. Erros preservam o conteúdo do diálogo e o objeto anterior.

MathMesh lê os paths independentes do SVG (fontCache=none), compõe transforms, curva quadrática/cúbica e reflexões, triangula por faixas com winding para preservar furos e gera geometria em mm. O Scene Graph posiciona os meshes dentro do objeto: zoom, resize e rotação não ampliam bitmaps de equação. Unicode de fallback usa outlines da fonte do sistema. Fórmulas têm geometria em cache; sync/input não executam MathJax ou tesselação. Texto comum usa cache de textura por faixa de resolução; thumbnails são raster por definição. O SVG original permite futura exportação vetorial.

Hold deixa de aceitar polígonos arbitrários: triângulo/retângulo/quadrado e regulares de 5–12 lados são ajustados. Círculos e elipses preservam prioridade para curvas suaves. Se nenhum fitting confiante vence, hold explícito aplica linha por PCA ou curva fechada por eixos principais, sem rejeição silenciosa. Um worker separado procura ciclos de 3–12 LineObjects por endpoints próximos. O fechamento guarda remoção das linhas e adição do polígono em um comando; undo restaura todos os segmentos. Polígonos antigos e polígonos fechados por linhas continuam editáveis, inclusive côncavos.

## Lixeira

SQLite mantém uma entrada durável antes de mover o arquivo para AppDataLocation/trash/<uuid>.board, conservando nome, ID, caminho original e deletedAt UTC. Recentes excluem IDs da lixeira. Movimentação, restore e limpeza ocorrem em worker; rename ou cópia via QSaveFile atende discos diferentes. Thumbnail acompanha o arquivo. Restore não sobrescreve destinos existentes. Manutenção retoma moves interrompidos e remove itens vencidos após 30 dias; roda ao iniciar e a cada hora. Exclusão permanente remove arquivos antes dos metadados. Operações concluídas atualizam a Home e status.
