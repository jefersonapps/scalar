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
  pdf/          inspeção, intervalos e cache de renderização Qt PDF
  export/       PDF multipágina com validação e substituição atômica
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

O módulo export produz PDF multipágina; math contém o conversor local e validação SVG, e pdf contém a importação. Não há arquivos vazios para simular funcionalidades futuras.

## Documento e unidades

Project contém identidade, nome, datas e páginas. Cada página tem ID, largura/altura em milímetros, fundo RGBA e objetos com identidade e estilo próprios. As amostras têm posição em mm, pressão, tilt, rotação, timestamp, botões e origem. O editor seleciona a página atual; persistência e UI suportam várias páginas com tamanhos individuais e histórico por ID de página.

As coordenadas do documento não dependem de DPI ou viewport. `screen = world * (96/25.4) * zoom + pan`. Tela usa pixels lógicos do Qt. Device pixel ratio e troca de monitor pertencem ao backend Qt Quick; a geometria original não é escalada nem regravada. Alterar orientação normaliza o par largura/altura; não transforma traços existentes porque configuração de página é apenas de criação neste ciclo.

CanvasObject é uma variant tipada de StrokeObject, ShapeObject, ImageObject e TextObject. Page mantém coleções concretas; objectViews ordena referências por zIndex sem copiar amostras no render loop. ShapeObject é paramétrico: endpoints/vértices para linhas/polígonos, centro/raios/rotação para círculo/elipse. ImageObject tem quatro cantos e backing PNG imutável compartilhado; mover/duplicar não copia o bitmap. ObjectProperties tem locked, visible, zIndex e revisão transitória do cache. Transforms editam coordenadas físicas; undo guarda before/after. Não há matriz genérica persistida para strokes neste ciclo.

## Entrada

UI e entrada estão na main thread. CanvasItem tem uma máquina de estados com Drawing, PossibleHold, ShapePreview, CreatingShape, Erasing e estados de navegação/edição. CanvasSelection separa seleção/handles; CanvasScene separa os nós gráficos do input. InputManager produz PointerSample; o renderer e o documento não conhecem QTabletEvent. Eventos de tablet são interceptados na QQuickWindow e mapeados para a área local do item; a toolbar é excluída. Eventos sintéticos de mouse são consumidos para evitar traços duplicados. O caminho de tablet precisa ser validado nos drivers Windows/Linux.

Stylus desenha; mouse usa caneta/mão; toque navega. Middle drag ou Space drag faz pan. Wheel zoom ancora no ponteiro; pixel deltas do trackpad fazem pan, Ctrl + pixel delta faz zoom; ZoomNativeGesture trata pinch quando fornecido pelo sistema. Touch de dois dedos usa razão de distâncias. Durante stylus ativo, toque é ignorado: isso é uma proteção básica, não uma validação de palm rejection em hardware.

Pressão: minWidth + (maxWidth - minWidth) × pow(clamp(pressure × sensitivity), gamma). Filtro exponencial leve somente na pressão; posições não são atrasadas. Mouse tem pressão 1. Release de tablet preserva a última pressão para não produzir estreitamento artificial. Escape, perda de grab, janela desativada ou item desabilitado cancelam o preview sem comando parcial.

## Renderização

CanvasItem usa QQuickItem/Qt Quick Scene Graph. Cada stroke concluído gera QSGGeometryNode com triângulos em mm. Segmentos têm largura interpolada; discos aproximados por 12 segmentos fecham extremidades e junções. O stroke ativo possui nó separado. Traços e formas concluídos não são retesselados a cada pointer move; revisão/tipo invalidam apenas o nó alterado. Fill e borda de formas usam meshes separados. Imagens usam textura e transformação vetorial dos cantos; o bitmap é decodificado previamente em worker. Uma transformação de raiz implementa pan/zoom; clip limita o desenho à página.

Navegação preserva também a posição dos nós na árvore: só alterações reais de ordem modificam a lista de filhos, evitando invalidar os batches gráficos a cada passo de zoom. Contornos contínuos de círculos e elipses usam uma faixa fechada de 256 triângulos, sem discos sobrepostos nas 128 junções. Elipses muito estreitas em relação à espessura mantêm a tesselação geral para evitar inversão da borda interna; estilos tracejado/pontilhado preservam suas geometrias próprias.

MSAA 4x é solicitado no início, mas disponibilidade e antialiasing real dependem do backend. Não há QQuickPaintedItem no canvas. QPainter gera thumbnails e caches de texto em worker; o documento continua editável. Ainda não há culling, índice espacial, atualização incremental do mesh ativo ou batching explícito. A tesselação do traço ativo cresce linearmente com suas amostras e o sync percorre IDs de objetos existentes; benchmark será necessário antes de declarar 60 FPS ou baixa latência.

## Comandos

History armazena comandos com alterações before/after de objetos e fundos (Add, Delete, Transform, Convert, Style, Background). Undo/redo aplica o mesmo ID e geometria; operações de seleção/borracha agrupam suas alterações em um comando. Reconhecimento faz primeiro Add do traço e depois Convert: primeiro undo restaura o original, segundo o remove. Histórico não é persistido.

Reconhecimento roda em QtConcurrent após hold com tolerância 0,6 mm e tempo 250–1500 ms, padrão 500. Resampling para 512 pontos limita fitting/RDP; IDs e epochs descartam resultados após cancelamento/release. Linha aberta, círculos/ellipses por resíduos, e polígonos convexos com ângulos/lados validados. Linhas reconhecidas preservam a primeira amostra como origem; o fallback usa PCA para classificar, mas mantém os pontos reais de início e fim. Durante o ajuste após hold, apenas o endpoint acompanha a ponta. É heurístico, sem IA em nuvem; tolerâncias precisam de calibração com escrita real.

No editor, um candidato a setor circular só é aceito quando duas bordas próximas de formas ou traços retos existentes fornecem um vértice compatível com o arco inteiro. Uma curva aberta isolada, uma única borda ou lados paralelos não geram setor: ao segurar para reconhecer, o gesto é endireitado como segmento entre suas pontas originais. Gestos fechados sem contexto não viram segmentos degenerados. O ajuste e a validação de contexto ocorrem no worker sobre a mesma cópia da página; o resultado já contém o setor fechado no vértice. A API sem página permanece como ajuste geométrico de candidatos para testes e composição interna.

Borracha calcula interseções analíticas dos segmentos de stroke com uma cápsula entre posições do ponteiro. Limites novos interpolam pressão e demais amostras. Preview preserva as partes não apagadas; release registra exclusão de originais + adição dos fragmentos. Marcadores transparentes mantêm as amostras originais e usam as mesmas máscaras em blocos das formas; isso evita fragmentos com pontas sobrepostas que escurecem a tinta. Shift restaura a máscara local sem duplicar traços. Preenchimentos de região (`ImageObject::inkFill`) também recebem máscaras ordenadas; a borracha comum recorta esses preenchimentos sem Ctrl, e o mesmo cache em blocos compõe o PNG original na geometria do objeto para a tela e exportação. Tinta de caneta removida é preservada em `Page::erasedInk`; Shift restaura só o trecho atingido, e a reserva participa de undo/redo e salvamento.

Ctrl ou a opção “Apagar formas também” registra cápsulas ordenadas em `ShapeObject::erasedRegions`; Shift registra restauração local da geometria original. Na interface, `ShapeRasterCache` mantém blocos de 256 pixels com imagem original, máscara de visibilidade e resultado. Apenas blocos atingidos por novas cápsulas são compostos e enviados como textura ao Scene Graph. Blocos fora da área visível são descartados; mudanças de geometria/estilo e undo invalidam o cache. Durante zoom/pan, o Scene Graph reaproveita texturas e grade com margem de 25% da área visível. Resoluções de máscaras usam bandas de √2; uma pausa de 150 ms refina máscaras, texto e PDF na resolução final. Ao afastar, a resolução mantida é limitada a duas bandas acima do destino, evitando alocação excessiva; mudanças de banda recalculam a cobertura da área visível. Um pixel de margem permite interpolação sem emendas entre blocos. A quantidade de pixels por bloco não cresce com os recortes. Thumbnails e exportação usam a mesma composição; exportações grandes processam um bloco de cada vez. As formas continuam paramétricas no documento; essa máscara é apenas um cache de renderização, não uma conversão em imagem persistida. A tesselação recortada do core fica disponível para geometria e testes sem janela, fora do caminho de interação da UI.

## Persistência e threads

SQLite via Qt SQL fica na main thread para consultas pequenas de recentes e preferências. O arquivo do projeto usa JSON em ZIP STORE implementado com STL e sem APIs privadas. Serialization, escrita e thumbnails rodam no pool QtConcurrent. Abrir projeto também usa worker. Autosave agenda 1600 ms após alteração de documento, nunca por pointer move; um único writer por controlador preserva ordem. Se ocorrer uma nova alteração durante save, o snapshot antigo não limpa dirty state da revisão nova.

Organização da biblioteca usa `folders(id,name,color)` e `project_folders(project_id,folder_id)`. Migração transacional para user_version 1 cria essas tabelas sem alterar projetos existentes. Um quadro pertence a uma pasta ou à raiz; nomes de pastas são únicos sem diferenciar maiúsculas/minúsculas. A Home filtra a lista completa de projetos por pasta, sem o antigo limite de 24 recentes. Mover entre pastas altera somente metadados. Trash oculta o quadro da contagem; restore preserva associação e exclusão permanente remove a associação. Pastas são locais ao banco e mantêm hierarquia em `parent_id` (migração 2), sem mudar o formato `.board`. A migração 3 adiciona `trash_folders`, `trashed_folders` e `trash_folder_members`: excluir uma pasta oculta toda a árvore e registra seus quadros na mesma transação. A lixeira apresenta um único item de pasta e move os arquivos em worker; restaurar recupera quadros e hierarquia juntos. Uma movimentação parcial mantém o registro para retomada pela manutenção. Expiração em 30 dias e exclusão definitiva removem os arquivos e os metadados da árvore. Pastas vazias também podem ser restauradas; se o pai original não existir ou estiver na lixeira, a pasta volta para a raiz.

Uma cópia do documento é obtida na main thread antes de lançar o worker. Esse custo ainda pode ser perceptível em documentos muito grandes; snapshots imutáveis compartilhados são a próxima otimização. Ao trocar de documento ou fechar, flush pode esperar o worker e precisa bloquear para garantir persistência; durante desenho normal o save não desabilita o canvas.

Arquivos novos ficam em AppDataLocation/projects. `.board.png` é thumbnail secundário; `library.sqlite` guarda metadados e settings. `session.board` e cleanShutdown detectam término anormal. Falha no flush impede fechamento normal pela janela e preserva recuperação; sucesso remove sessão temporária. Recuperação é oferecida em diálogo, e o quadro recuperado é salvo em arquivo gerenciado antes de sair.

## Limites de escopo

Não há exportação PNG. Qt PDF é dependência da importação e validação da exportação; MathJax e Node são incorporados ao pacote offline.

## Milestone 5 — ferramentas geométricas

GeometryTools concentra projeção sobre reta, transformações da régua, geometria do compasso, acumulação angular e construções, em C++ sem dependências externas. Os guias usam coordenadas físicas em mm; CanvasItem expõe somente sua transformação para QML. RulerGuide e CompassGuide são overlays passivos, separados dos objetos do documento. São ocultados ao abrir/trocar de página e não integram persistência, autosave ou exportação.

Estados explícitos controlam movimento, rotação, comprimento, abertura e desenho do compasso. Os eventos normalizados de mouse/stylus/touch passam pela mesma geometria. A régua reavalia a distância à borda e aos endpoints a cada amostra: projeta dentro da região de snap e desenha livremente fora dela, preservando pressão. O traço resultante continua sendo StrokeObject. A ponta do compasso desenha tanto no modo Compasso quanto Selecionar. Escape restaura o estado anterior do guia e descarta o desenho corrente.

CompassSweep acumula deltas angulares com correção de passagem por ±π. Amostras de arco são interpoladas com passo de até 2 graus. Uma revolução completa gera CircleObject; um arco parcial gera StrokeObject. Ambos usam os comandos de documento existentes para undo/redo. Paralelas, perpendiculares, mediatrizes e circunferências circunscritas geram ShapeObjects independentes: não há vínculos dinâmicos ou solver de restrições.

## Exportação PDF

PageRenderer recebe QPainter e Page em mm, independentemente da viewport, e atende tanto miniaturas quanto exportação. Traços e bordas usam paths do mesh, grades e preenchimentos usam paths vetoriais. TextRenderer compartilha seu layout entre canvas e exportação; texto comum usa drawText (fonte incorporada pelo Qt PDF) e matemática usa paths dos glyphs SVG, sem bitmap. Imagens e a base PDF usam raster com resolução limitada. O PDF importado não é mesclado vetorialmente por Qt PDF.

PdfExporter usa QPdfWriter a 300 dpi, margens zero e tamanho físico por página. Um arquivo temporário no diretório de destino é reaberto com Qt PDF para conferir contagem e medidas antes da cópia via QSaveFile e substituição atômica. O AppController envia um snapshot a um worker; editar e navegar não altera a exportação já iniciada nem bloqueia a entrada da caneta.

## Milestone 4

PdfPageObject é uma base opcional imutável da página, com bytes originais compartilhados, ID do asset e índice da página fonte. Cada worker cria seu próprio QPdfDocument. Inspeção converte pontos tipográficos em mm; páginas importadas mantêm papel branco. PdfRenderCache limita o cache a 96 MiB e cada bitmap a 4096 px por lado / 16 MP. Faixas de resolução acompanham zoom e DPI; durante atualização mantém-se a resolução anterior da mesma página. A base PDF precede grade e objetos no Scene Graph; as anotações continuam vetoriais.

Miniaturas são geradas em lotes de até 16 páginas em worker, com debounce de 700 ms e invalidação por revisão. Importação de até 32 imagens agrupa um único comando de undo, respeitando a página selecionada. IDs de projeto/página descartam resultados antigos. SVG usa Qt SVG quando disponível e é normalizado em PNG, como os demais ImageObjects.

Presets de grade e cor de fundo são independentes na interface. Selecionar grade altera tipo e espaçamento, preservando cores, opacidade e espessura. O tema define somente a cor inicial de um novo quadro; o fundo armazenado nunca é recalculado ao trocar o tema.

## Milestone 3

BackgroundStyle define gridType, gridColor, opacity, thicknessMm e spacingX/Y; a cor base preserva o campo legado Page.background. BackgroundMesh gera um único batch de triângulos para a região visível. Zoom distante omite subdivisões e amostra grades em múltiplos do espaçamento para limitar custo. Mudanças na caneta não regeneram a grade. Presets customizados ficam na tabela SQLite background_presets.

TextObject contém fonte UTF-8, família do sistema, tamanho em pontos, bold/italic/alinhamento, RGBA, quatro cantos e MathFragments. Cada fragmento retém offset/comprimento em bytes, LaTeX, display, SVG e dimensões em em. MathJax 3.2.2 roda em um subprocesso Node privado ao pacote, lançado em worker; não há navegador nem conexão de rede. InlineTextEditor mantém o rascunho no canvas; erros preservam esse rascunho e o objeto anterior. CreatingText trata o arraste em coordenadas do documento e emite o retângulo da caixa. O objeto original é ocultado apenas no Scene Graph durante a edição. TextObject guarda boxWidthMm/boxHeightMm opcionais: o layout quebra linhas na largura definida e cresce verticalmente quando necessário. A conclusão prepara o texto em worker e aplica um comando, incluindo undo/redo.

MathMesh lê os paths independentes do SVG (fontCache=none), compõe transforms, curva quadrática/cúbica e reflexões, triangula por faixas com winding para preservar furos e gera geometria em mm. O Scene Graph posiciona os meshes dentro do objeto: zoom, resize e rotação não ampliam bitmaps de equação. Unicode de fallback usa outlines da fonte do sistema. Fórmulas têm geometria em cache; sync/input não executam MathJax ou tesselação. Texto comum usa cache de textura por faixa de resolução; thumbnails são raster por definição. O SVG original permite futura exportação vetorial.

Hold deixa de aceitar polígonos arbitrários: triângulo/retângulo/quadrado, paralelogramos e regulares de 5–12 lados são ajustados. Paralelogramos exigem quatro cantos e pares de lados opostos com direções e comprimentos compatíveis; o ajuste preserva a inclinação e torna os lados opostos paralelos. Esses cantos impedem a conversão para elipse mesmo com lados levemente curvos. Círculos e elipses preservam prioridade para curvas suaves. Se nenhum fitting confiante vence, hold explícito aplica linha por PCA ou curva fechada por eixos principais, sem rejeição silenciosa. Um worker separado procura ciclos de 3–12 LineObjects por endpoints próximos. O fechamento guarda remoção das linhas e adição do polígono em um comando; undo restaura todos os segmentos. Polígonos antigos e polígonos fechados por linhas continuam editáveis, inclusive côncavos.

## Lixeira

Renomear um quadro aberto altera o modelo e usa o autosave existente. Renomear pela biblioteca lê e salva o projeto atomicamente em worker; a atualização dos metadados SQLite ocorre após o arquivo ter sido salvo. O ID, as páginas e a associação à pasta são preservados. A busca de nomes fica no AppController e normaliza caixa e marcas de acentuação. QML apresenta resultados, filtros e navegação da hierarquia de pastas.

SQLite mantém uma entrada durável antes de mover o arquivo para AppDataLocation/trash/<uuid>.board, conservando nome, ID, caminho original e deletedAt UTC. Recentes excluem IDs da lixeira. Movimentação, restore e limpeza ocorrem em worker; rename ou cópia via QSaveFile atende discos diferentes. Thumbnail acompanha o arquivo. Restore não sobrescreve destinos existentes. Manutenção retoma moves interrompidos e remove itens vencidos após 30 dias; roda ao iniciar e a cada hora. Exclusão permanente remove arquivos antes dos metadados. Operações concluídas atualizam a Home e status.

TextFormatting usa o QTextDocument do editor para aplicar negrito/itálico à seleção e extrair segmentos UTF-16. Esses segmentos acompanham TextObject, comandos, serialização, layout e exportação. Lobster Two (regular, italic, bold) é registrada a partir de recursos locais, com licença incluída. A criação de QFont é centralizada em documentFont; no Linux, NoFontMerging evita a falha nativa em charsets de fallback observada no Qt/fontconfig desta instalação. Fontes que não contêm um caractere exibem o glyph ausente em vez de acionar esse fallback defeituoso; o padrão embutido cobre os acentos portugueses.
