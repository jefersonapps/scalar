# Entrega do ciclo inicial

O Milestone 1 foi **compilado com Qt 6.4.2; cinco testes passaram**, incluindo UI e persistência. O aceite de stylus, renderer acelerado e desempenho continua pendente de sessão gráfica real.

## Estrutura, arquitetura e design

A árvore e os contratos estão em ARCHITECTURE.md; os tokens e componentes em DESIGN_SYSTEM.md; o formato em FILE_FORMAT.md. A base separa núcleo STL portátil, adapters Qt, canvas Scene Graph, persistência e UI QML. Um documento pode persistir várias páginas com tamanhos próprios; UI inicial usa uma página.

## Telas executadas em offscreen

Home: marca Scalar, título “Grandes ideias. Um quadro em branco.”, botões de criação/abertura e cards com thumbnails reais do documento salvos em worker. Criação: nome, A4/Carta/custom, retrato/paisagem, fundo branco/preto/verde. Editor: canvas amplo, cabeçalho compacto, caneta/mão/undo/redo em toolbar inferior translúcida. Popover da caneta mostra cores, largura em mm e gamma. Preferências têm Light/Dark/System, efeitos reduzidos e defaults de criação. Há diálogo de recuperação de sessão.

Screenshots estão em `build/screenshots/`. Home, criação, editor light/dark, opções da caneta e configurações dark foram executados sem warnings QML. Corrigidos caminho qrc, sinal deprecated de palette, alinhamento do cabeçalho e cor do texto primário. As capturas usam software/offscreen: os meshes de strokes não apareceram nelas; não constituem aprovação do renderer GPU.

## Verificado

`./scripts/test-core.sh`: compilação com g++ C++20, `-Wall -Wextra -Wpedantic -Werror`, execução bem-sucedida.

Testes executados:

- A4 210 × 297 mm, landscape 297 × 210 mm, Carta e orientação.
- Rejeição de página inválida (zero/NaN).
- Round-trip world/screen em vários fatores de zoom e preservação da âncora.
- Curva de pressão monotônica e limites.
- Mesh com números finitos, round cap de tap, pontos repetidos.
- Undo/redo e descarte do ramo redo após novo comando.
- Zoom sem alteração da geometria do documento.
- 10.000 IDs sem colisões na amostra do teste.
- ZIP round-trip, todos os prefixos truncados, CRC inválido, tamanho adulterado e compressão não suportada.
- ZIP produzido pelo código C++ aberto e CRC conferido por Python zipfile; project.json lido e interpretado.

O mesmo núcleo passou com AddressSanitizer e UndefinedBehaviorSanitizer, usando `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer não pôde executar sob o ptrace deste ambiente; não há validação de vazamentos.

## Integração Qt verificada

- CMake, Qt entry point e carga do recurso qrc em smoke tests de Home/editor.
- Criação pela UI, mouse press/move/release produzindo amostras, undo/redo e zoom sem mutação. Stylus/touch/trackpad físicos permanecem pendentes.
- Tema dark preserva fundo branco; botões primários usam texto claro; diálogos e popovers abrem sem warnings QML.
- Save/load via AppController e ProjectStore; dados persistidos e reabertos. Autosave roda durante o teste, mas impacto de latência e recuperação de crash ainda precisam de aceite.
- Qt Test: round-trip completo com amostras stylus e páginas diferentes; tentativa inválida preserva arquivo existente; esquema desconhecido/corrupção; settings/recents após reabrir SQLite.

## Limitações materiais

Binário Linux disponível em build/scalar. Cinco testes CTest passaram. O sandbox não acessa display :0/GPU; execução real foi feita apenas offscreen/software, sem validação visual dos strokes GPU. Windows, stylus física, palm rejection, 60 FPS, latência, antialiasing real, DPI e multimonitor precisam de validação. Não há instalador. Canvas ainda não faz culling/spatial index/batching explícito; mesh ativo é retesselado; snapshot copia documento antes do worker. Blur não implementado. Não há seleção/borracha/texto/imagens/PDF/LaTeX/régua/compasso nem benchmarks gráficos. Os critérios de exportação A4/MediaBox pertencem ao milestone 5 e não foram testados aqui.

## Próximos passos

Executar build/scalar no terminal da sessão gráfica e verificar os traços com renderer acelerado. Validar caneta física, DPI e performance em Windows/Linux. Somente após aceite do M1: iniciar M2 com seleção, handles, reconhecimento local e hold-to-line, mantendo Ctrl+Z capaz de restaurar o stroke original.
