# Arquitetura

## Árvore do Milestone 1

```text
src/
  app/          entry point, AppController, ponte QML
  documents/    Project, Page, StrokeObject, PointerSample, unidades
  commands/     histórico de AddObjectCommand
  input/        normalização de mouse e tablet, filtro de pressão
  canvas/       QQuickItem, máquina de estados, viewport, eventos
  rendering/    tesselação portátil de traços
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

Os módulos geometry, recognition, tools, selection, pdf, export, math e clipboard serão acrescentados quando houver comportamento implementado. Não há arquivos vazios para simular funcionalidades futuras.

## Documento e unidades

Project contém identidade, nome, datas e páginas. Cada página tem ID, largura/altura em milímetros, fundo RGBA e objetos StrokeObject com identidade e estilo próprios. As amostras têm posição em mm, pressão, tilt, rotação, timestamp, botões e origem. A UI do ciclo inicial edita somente a primeira página; persistência aceita várias páginas e tamanhos individuais.

As coordenadas do documento não dependem de DPI ou viewport. `screen = world * (96/25.4) * zoom + pan`. Tela usa pixels lógicos do Qt. Device pixel ratio e troca de monitor pertencem ao backend Qt Quick; a geometria original não é escalada nem regravada. Alterar orientação normaliza o par largura/altura; não transforma traços existentes porque configuração de página é apenas de criação neste ciclo.

Não há CanvasObject polimórfico nem transforms de objeto no ciclo 1. Quando seleção/formas forem introduzidas, a coleção de objetos migrará para uma variant tipada com propriedades comuns e versões de serialização explícitas. Isso evita uma hierarquia fictícia antes da implementação real.

## Entrada

UI e entrada estão na main thread. CanvasItem tem estados enum Idle, Drawing, Panning e TouchGesture. InputManager produz PointerSample; o renderer e o documento não conhecem QTabletEvent. Eventos de tablet são interceptados na QQuickWindow e mapeados para a área local do item; a toolbar é excluída. Eventos sintéticos de mouse são consumidos para evitar traços duplicados. O caminho de tablet precisa ser validado nos drivers Windows/Linux.

Stylus desenha; mouse usa caneta/mão; toque navega. Middle drag ou Space drag faz pan. Wheel zoom ancora no ponteiro; pixel deltas do trackpad fazem pan, Ctrl + pixel delta faz zoom; ZoomNativeGesture trata pinch quando fornecido pelo sistema. Touch de dois dedos usa razão de distâncias. Durante stylus ativo, toque é ignorado: isso é uma proteção básica, não uma validação de palm rejection em hardware.

Pressão: minWidth + (maxWidth - minWidth) × pow(clamp(pressure × sensitivity), gamma). Filtro exponencial leve somente na pressão; posições não são atrasadas. Mouse tem pressão 1. Release de tablet preserva a última pressão para não produzir estreitamento artificial. Escape, perda de grab, janela desativada ou item desabilitado cancelam o preview sem comando parcial.

## Renderização

CanvasItem usa QQuickItem/Qt Quick Scene Graph. Cada stroke concluído gera QSGGeometryNode com triângulos em mm. Segmentos têm largura interpolada; discos aproximados por 12 segmentos fecham extremidades e junções. O stroke ativo possui nó separado. Traços concluídos não são retesselados a cada pointer move. Uma transformação de raiz implementa pan/zoom; clip limita o desenho à página.

MSAA 4x é solicitado no início, mas disponibilidade e antialiasing real dependem do backend. Não há QQuickPaintedItem no canvas. QPainter é usado apenas para thumbnails de 640 × 400 em worker. Ainda não há culling, índice espacial, atualização incremental do mesh ativo ou batching explícito. A tesselação do traço ativo cresce linearmente com suas amostras e o sync percorre IDs de objetos existentes; benchmark será necessário antes de declarar 60 FPS ou baixa latência.

## Comandos

History mantém AddObjectCommand e cursor. Undo remove o objeto por ID; redo recoloca o mesmo objeto e as mesmas amostras. Um comando novo após undo remove o ramo redo. O histórico não é persistido. Comandos de transformação, reconhecimento e mudança de estilo pertencem ao milestone 2.

## Persistência e threads

SQLite via Qt SQL fica na main thread para consultas pequenas de recentes e preferências. O arquivo do projeto usa JSON em ZIP STORE implementado com STL e sem APIs privadas. Serialization, escrita e thumbnails rodam no pool QtConcurrent. Abrir projeto também usa worker. Autosave agenda 1600 ms após alteração de documento, nunca por pointer move; um único writer por controlador preserva ordem. Se ocorrer uma nova alteração durante save, o snapshot antigo não limpa dirty state da revisão nova.

Uma cópia do documento é obtida na main thread antes de lançar o worker. Esse custo ainda pode ser perceptível em documentos muito grandes; snapshots imutáveis compartilhados são a próxima otimização. Ao trocar de documento ou fechar, flush pode esperar o worker e precisa bloquear para garantir persistência; durante desenho normal o save não desabilita o canvas.

Arquivos novos ficam em AppDataLocation/projects. `.board.png` é thumbnail secundário; `library.sqlite` guarda metadados e settings. `session.board` e cleanShutdown detectam término anormal. Falha no flush impede fechamento normal pela janela e preserva recuperação; sucesso remove sessão temporária. Recuperação é oferecida em diálogo, e o quadro recuperado é salvo em arquivo gerenciado antes de sair.

## Limites de escopo

Não há renderização/exportação PDF, formas, seleção, texto, LaTeX, imagens de documento, grades, régua ou compasso. Futuro pipeline de exportação deverá consumir o mesmo modelo em mm e separar renderização de página da viewport. Qt PDF será dependência do módulo de importação; MathJax ficará empacotado offline quando matemática for implementada.
