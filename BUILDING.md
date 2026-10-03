# Compilar e validar

## Dependências

C++20, CMake >= 3.21, Qt >= 6.4 com Core, Gui, Quick, QuickControls2, Concurrent, Sql e Test. SQLite plugin QSQLITE precisa estar disponível. QML usa QtQuick, Controls, Layouts, Dialogs, Shapes, Templates, Window e QtQml.WorkerScript. Não precisa de Qt Widgets. CMake não baixa dependências.

### Ubuntu / Mint

Execute a instalação em uma máquina onde tenha permissão de administrar o sistema:

```sh
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
  libqt6sql6-sqlite qt6-image-formats-plugins qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-dialogs qml6-module-qtquick-shapes \
  qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml qml6-module-qtqml-workerscript
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/scalar
```

WEBP usa o plugin Qt de formatos de imagem. Se esse plugin não estiver disponível, CMake detecta opcionalmente `libwebp` (headers e biblioteca) e habilita um decoder alternativo offline. Em Ubuntu/Mint, `libwebp-dev` fornece essa alternativa. Ela é justificada para manter a importação WEBP funcional em instalações Qt mínimas, sem adicionar um subprocesso ou serviço. Em Windows, inclua o plugin WEBP no deploy do Qt ou disponibilize libwebp ao CMake.

Qt instalado fora do sistema: acrescente `-DCMAKE_PREFIX_PATH=/caminho/Qt/6.x/gcc_64`.

### Windows 10/11

Instale Qt 6 com kit MSVC x64 e Visual Studio Build Tools com C++20. No Developer PowerShell, ajustando o caminho do Qt:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
windeployqt --qmldir qml build/Release/scalar.exe
./build/Release/scalar.exe
```

O caminho é exemplo de kit; use sua instalação. O deploy precisa incluir QSQLITE, runtime QML e plataforma Windows. Não há instalador ou assinatura de binários neste ciclo.

## Núcleo sem Qt

Com CMake: `cmake -S . -B build-core -DSCALAR_BUILD_DESKTOP=OFF`, seguido de build e ctest. Sem CMake, em Linux com g++:

```sh
./scripts/test-core.sh
```

Esse comando usa C++20, warnings e `-Werror`. Testes de integração SQLite/JSON/QML precisam de Qt.

## Smoke e screenshots reais

Depois de compilar em ambiente gráfico:

```sh
./build/scalar --smoke-test
./build/scalar --smoke-test --screenshot /tmp/scalar-home.png
./build/scalar --editor --smoke-test --screenshot /tmp/scalar-editor.png
```

Para isolar dados e gerar imagens em Linux headless:

```sh
XDG_DATA_HOME=/tmp/scalar-smoke QT_QPA_PLATFORM=offscreen ./build/scalar --smoke-test
```

`--smoke-test` encerra após 1,8 s, verifica que não houve warnings QML e exige flush bem-sucedido. Screenshot usa `grabWindow`; offscreen pode não fornecer captura/renderização acelerada. Smoke não demonstra desempenho, qualidade visual ou stylus. No Windows, use ambiente gráfico e dados de teste separados.

## Roteiro manual de aceite

1. Home e criação nos temas light/dark/system. Criar A4 retrato, A4 paisagem, Carta e custom 180 × 240 mm. Verificar labels e tamanho persistido.
2. Desenhar taps, linhas rápidas, curvas e traços longos com mouse e stylus física. Variar pressão; confirmar diferenças de largura e ausência de traços duplicados.
3. Desenhar com autosave em andamento, monitorar pausas e latency. Verificar release fora da área, Escape, perda de foco e troca de ferramenta.
4. Wheel zoom ancorado, mão, Space drag, touch pan/pinch e trackpad. Fazer undo/redo antes e após zoom; geometria não deve mudar.
5. Salvar como, reabrir `.board`, conferir cor/espessura/amostras. Conferir recentes e thumbnail. Testar path sem permissão de escrita: status deve mostrar erro e fechamento não deve perder alterações.
6. Com um quadro dirty após salvar pelo menos uma vez, encerrar o processo de maneira anormal em ambiente de teste. Reabrir, recuperar e confirmar persistência após fechamento normal.
7. DPI 100%, 125%, 150%, 200%, dois monitores com DPI diferente, Windows e Ubuntu/Mint. Verificar flicker, recortes, labels, fontes, ícones e foco.

Logging usa categorias `scalar.app` e `scalar.persistence.sqlite`, níveis info/warning do Qt e timestamps do handler padrão. `QT_LOGGING_RULES='scalar.*.debug=true'` habilita debug das categorias. Ainda não há log estruturado JSON, profiler ou medição de FPS.

## Resultado neste ambiente

Linux Mint 22.3, GCC 13.3, Qt 6.4.2. Após a instalação das dependências pelo usuário, CMake configurou e o aplicativo compilou. O caminho dos recursos QML foi corrigido com BASE explícito; ApplicationPaletteChange substituiu o sinal deprecated paletteChanged.

`ctest --test-dir build --output-on-failure`: **7/7 passaram** — core, recognition, eraser, persistence, desktop, smoke_home e smoke_editor. O teste desktop abre Home, cria quadro pela UI, desenha com eventos de mouse, verifica undo/redo, zoom sem mutação, tema dark com página branca, save/load e ausência de warnings QML. Popovers e configurações também foram abertos. Cada teste desktop usa diretório temporário próprio, sem tocar nos projetos do usuário.

Screenshots reais de offscreen ficam em `build/screenshots/`: home-light.png, new-project.png, editor-light.png, editor-dark.png, pen-options.png e settings-dark.png. **Essas capturas usam o backend software, que neste ambiente não exibiu os meshes de traços; não comprovam renderização acelerada do canvas.** O teste valida os objetos/amostras desenhados, não a aparência do stroke na GPU.

A sessão gráfica `:0` não pode ser acessada pelo sandbox (`could not connect to display`); `/dev/dri` não está disponível. Inicie `./build/scalar` no seu terminal gráfico para validar traços visíveis, MSAA, tablet e desempenho. O build ainda pode emitir aviso não bloqueante do qmlimportscanner se qml6-module-qtqml estiver ausente; ele está incluído na lista de dependências acima.

## Validação 0.2

Desktop test também cobre hold-to-line, ajuste antes do release, undo para stroke, borracha parcial e undo/redo, inserção de círculo, seleção/handles, mover, escala/raio e rotação, estilo/fill, duplicar/excluir, Ctrl+V de bitmap, URLs no clipboard, importação por arquivo e drag/drop. Imagens e formas são salvas/reabertas. O teste de persistência verifica v1/v2, PNG incorporado, círculo e decode real; testes de núcleo verificam fitting de formas e cortes analíticos da borracha.

Captura do tema novo com imagens: `build/screenshots/milestone2-dark-images.png`. Veja `docs/MILESTONE_2.md` para uso e limites. SVG/WebP dependem de plugins de imagem Qt; o diálogo inclui extensões, mas importer retorna erro claro se o codec não estiver disponível. Nenhuma biblioteca de imagem externa foi adicionada.
