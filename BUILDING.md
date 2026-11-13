# Compilar, testar e gerar instaladores

Execute os comandos abaixo na raiz do projeto (`scalar/`).

## Gerar o instalador Windows

O navegador lateral usa o WebView2 do Windows, sem empacotar um motor web. A compilação baixa apenas o SDK leve; veja [runtime, compilação offline e limitações](docs/SYSTEM_BROWSER.md).

No PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

Esse é o comando principal para recompilar o aplicativo e gerar o instalador. O script inicializa o ambiente MSVC, configura o CMake, compila e empacota com NSIS.

O ícone nativo `packaging/scalar.ico` é atualizado automaticamente a partir de `icon.png` antes da compilação. Alterações no `.ico` também recompilam o recurso do executável nas compilações incrementais. Não é necessário executar um comando separado para atualizar o ícone.

Ao instalar e criar o atalho, o instalador notifica o Shell do Windows para invalidar o cache de ícones e atualizar o executável e o atalho. Essa notificação evita que instalações atualizadas continuem exibindo o desenho anterior armazenado no cache.

**Instalador para distribuir:** `build-package\packages\Scalar-1.2.1-win64.exe` (o nome acompanha a versão do projeto). O executável `build-package\scalar.exe` é uma saída de compilação; distribua o instalador em `packages/`, que inclui as dependências.

### Ferramentas necessárias

- Visual Studio ou Build Tools com ferramentas C++ x64 e Windows SDK.
- CMake >= 3.21 e Ninja disponíveis no `PATH`.
- Qt >= 6.5, kit MSVC x64, com Qt Quick, Quick Controls, Quick Effects, SQL, Concurrent, PDF, SVG e `windeployqt`.
- Python >= 3.12 para preparar Node e MathJax durante o empacotamento.
- NSIS instalado.

O script está configurado para Qt em `C:\Qt\6.8.2\msvc2022_64`, CMake em `C:\Program Files\CMake\bin` e NSIS em `C:\NSIS\nsis-3.10`. Ajuste os caminhos em [package-windows.ps1](scripts/package-windows.ps1) se sua instalação for diferente. CMake e Ninja também podem ser encontrados pelo `PATH`.

O script reutiliza `build-package/` para recompilar somente os arquivos alterados. O CPack prepara uma área de empacotamento limpa. Ao terminar, execute o instalador para atualizar o app. A tela final oferece **Executar o Scalar** e **Criar atalho na área de trabalho**, ambas marcadas por padrão. Instalações silenciosas não executam essas ações.

O instalador usa compressão LZMA sólida. Inclui as DLLs do runtime Visual C++ junto ao app, sem duplicar o instalador desse runtime. Estilos Qt Quick Controls não usados e os bundles de navegador do MathJax são excluídos; Node, LaTeX offline, codecs e backends gráficos permanecem no pacote.

Para gerar também um pacote portátil, depois do script:

```powershell
cpack --config .\build-package\CPackConfig.cmake -G ZIP
```

O ZIP também fica em `build-package\packages\`. Extraia todo o conteúdo antes de executar o app.

## Gerar o instalador Linux

Em Debian, Ubuntu ou Linux Mint, com as dependências de desenvolvimento instaladas:

```sh
bash scripts/package-linux.sh
```

As saídas ficam em `build-package/packages/`:

- `scalar_*.deb`: pacote do aplicativo; o nome completo depende da versão e arquitetura.
- `install-scalar.sh`: assistente de instalação para distribuir junto do `.deb`.

O script valida e extrai o pacote antes de publicá-lo nessa pasta. Gere cada pacote no sistema de destino: Windows para `.exe`, Linux para `.deb`.

### Instalar no Linux

Para instalar e escolher as ações finais, como usuário normal:

```sh
bash ./build-package/packages/install-scalar.sh
```

O assistente solicita autorização para instalar pelo APT e, ao concluir, oferece executar o app e criar atalho na área de trabalho. Com Zenity e PolicyKit disponíveis na sessão gráfica, mostra caixas de seleção; caso contrário, pergunta no terminal. Alguns desktops ainda pedem permissão para executar o atalho.

Para instalar diretamente pelo APT:

```sh
sudo apt install ./build-package/packages/scalar_*.deb
```

A instalação direta usa a interface do gerenciador de pacotes, sem as opções do assistente. O comando instalado é `scalar-whiteboard`, para evitar conflito com o comando `scalar` fornecido pelo Git. No menu, o nome é Scalar.

## AppImage Linux portátil (x86_64)

Para gerar o AppImage com Qt/QML, Node/MathJax e **navegador integrado WebKitGTK**, instale Podman (ou use Docker já instalado) e execute:

```sh
sudo apt install podman
bash scripts/package-appimage.sh
```

O script compila em `python:3.12-bookworm` (Debian 12), sem instalar as dependências de desenvolvimento no sistema anfitrião. A primeira execução baixa a imagem, dependências e ferramentas de empacotamento; as próximas reutilizam os caches. Use `SCALAR_JOBS=2 bash scripts/package-appimage.sh` para limitar o consumo de memória. Docker deve estar acessível pelo usuário; não execute o script inteiro com `sudo`. O contêiner usa o UID do usuário e libera seus filtros seccomp/AppArmor para permitir o sandbox aninhado do WebKit durante o teste; não usa modo privilegiado. Restrições globais de namespaces impostas pelo anfitrião ainda podem impedir esse teste.

Saídas em `build-package/packages/`:

- `Scalar-1.2.1-x86_64.AppImage` (a versão acompanha o CMake).
- `Scalar-1.2.1-x86_64.AppImage.sha256`.

```sh
chmod +x build-package/packages/Scalar-1.2.1-x86_64.AppImage
./build-package/packages/Scalar-1.2.1-x86_64.AppImage
# Alternativa sem FUSE:
./build-package/packages/Scalar-1.2.1-x86_64.AppImage --appimage-extract-and-run
```

A base visa sistemas x86_64 com **glibc >= 2.36**, como Debian 12+, Ubuntu 24.04+ e Mint 22+. Ubuntu 22.04/Mint 21 e sistemas musl (Alpine) não são alvos desse pacote. A glibc, o servidor X11/XWayland e os drivers gráficos continuam sendo fornecidos pelo sistema. Isso amplia a compatibilidade, mas não substitui testes nas distribuições e GPUs de destino. O navegador conserva o sandbox do WebKit e exige que o sistema permita namespaces de usuário; o script não desativa essa proteção. O contêiner de compilação também precisa permitir esses namespaces para o teste do navegador.

Antes de publicar, o script verifica dependências ELF e a versão mínima de glibc, converte LaTeX em SVG, abre a interface com configurações temporárias e exige que o WebKit empacotado carregue uma página e execute JavaScript. A imagem final é extraída e conferida antes de substituir o artefato anterior. Esses testes não cobrem todos os sites, codecs, GPU ou stylus.

Para compatibilidade com drivers Mesa/LLVM recentes, o launcher prefere a `libstdc++` do sistema quando ela oferece a versão GLIBCXX exigida pela biblioteca incluída. Isso evita que a biblioteca da base Debian 12 impeça o carregamento de drivers mais novos. Em sistemas com runtime mais antigo, permanece a biblioteca do pacote.

O AppImage inclui uma cópia privada do WebKit, seus processos Web/Network/GPU quando disponíveis, GTK, módulos TLS/GIO e os plugins GStreamer instalados na base. Um launcher usa o Node já empacotado para ajustar os caminhos absolutos do WebKit em uma cópia temporária privada; a imagem permanece somente leitura. A cópia é removida quando o navegador termina normalmente. Como o WebKit é incluído, gere e redistribua o AppImage novamente para receber atualizações de segurança do navegador.

Para desenvolver o empacotamento usando as dependências locais:

```sh
# Além das dependências de desenvolvimento Linux listadas neste documento:
sudo apt install curl file patchelf desktop-file-utils imagemagick \
  qt6-declarative-dev-tools qt6-tools-dev-tools qmake6 \
  libgirepository1.0-dev librsvg2-dev glib-networking \
  bubblewrap xdg-dbus-proxy gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good xvfb xauth
bash scripts/package-appimage.sh --local
```

**`--local` herda a glibc e as bibliotecas da máquina atual**, portanto não tem a mesma base de compatibilidade. O relatório `usr/share/scalar/appimage-build.txt` dentro da imagem registra a glibc detectada. Os diretórios `build-package/appimage-local` e `build-package/appimage-container` separam os caches CMake. As ferramentas baixadas ficam em `build-package/appimage-tools`; remova esse cache quando quiser atualizar linuxdeploy e os plugins. A base do contêiner e os downloads `continuous` recebem atualizações: o processo não promete reprodução byte a byte.

Referências do empacotamento: [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy), [plugin Qt](https://github.com/linuxdeploy/linuxdeploy-plugin-qt) e [plugin GTK](https://github.com/linuxdeploy/linuxdeploy-plugin-gtk).

## Organização das pastas

| Pasta | Uso |
| --- | --- |
| `src/`, `qml/` | Código C++ e interface Qt Quick. |
| `assets/`, `icon.png` | Fontes e imagens do aplicativo. |
| `packaging/`, `cmake/` | Recursos e configuração dos pacotes. |
| `scripts/` | Comandos de empacotamento, preparação e testes. |
| `tests/`, `docs/` | Testes e documentação. |
| `third_party/mathjax/` | Dependências de matemática offline. |
| `build-package/` | Compilação para distribuição; instaladores em `packages/`. |
| `build/` | Compilação de desenvolvimento e testes; criada quando necessário. |
| `build-core/` | Compilação e testes do núcleo sem Qt; criada quando necessário. |

As três pastas `build*` acima contêm saídas geradas e são ignoradas pelo Git. Não use `dist/` nem pastas `build-windows-*` para novas compilações. Capturas de validação, quando necessárias, devem ficar em `build/screenshots/`.

## Desenvolvimento e testes

Use `build/` para desenvolver e testar, mantendo `build-package/` para os instaladores.

### Windows

Para abrir a compilação de desenvolvimento que já foi gerada, sem instalar:

```powershell
.\build\Release\scalar.exe
```

Depois de alterar o código, recompile antes de abrir:

```powershell
cmake --build build --config Release --parallel
.\build\Release\scalar.exe
```

No Developer PowerShell do Visual Studio, ajuste o caminho do Qt:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" `
  -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
& "C:\Qt\6.8.2\msvc2022_64\bin\windeployqt.exe" --qmldir qml build/Release/scalar.exe
ctest --test-dir build -C Release --output-on-failure
.\build\Release\scalar.exe
```

Além dos módulos do aplicativo, os testes desktop exigem Qt Test. O deploy deve disponibilizar plataforma Windows, imports QML, SQLite e codecs de imagem.

Os testes comuns usam renderização por software. Para verificar traços rápidos com a GPU no Windows (Direct3D 11), execute:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test-rendering-windows.ps1
```

Esse teste abre janelas temporárias, compara a tinta antes e depois de soltar o pincel e verifica malhas grandes, salvamento, a prévia incremental do marcador e passadas rápidas da borracha. Também verifica o zoom em páginas densas com apenas caneta, a fidelidade das curvas e da pressão na malha compacta, o descarte de geometria fora da tela, a reutilização dos blocos de imagem e o refinamento em segundo plano de dezenas de marcadores, inclusive ao desfazer. As medidas de sincronização da cena são de CPU; não representam o tempo total de cada quadro na GPU. O resultado fica em `build/gpu-ink-tests.txt`.

### Linux

Instale as dependências de desenvolvimento:

```sh
sudo apt install build-essential cmake ninja-build python3 libwebkit2gtk-4.1-dev xwayland qt6-base-dev qt6-declarative-dev qt6-pdf-dev qt6-svg-dev \
  libqt6sql6-sqlite qt6-image-formats-plugins qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-dialogs qml6-module-qt-labs-folderlistmodel qml6-module-qtquick-shapes \
  qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml qml6-module-qtqml-workerscript
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/scalar
```

Use Python >= 3.12. Para Qt instalado fora do sistema, acrescente `-DCMAKE_PREFIX_PATH=/caminho/Qt/6.x/gcc_64` ao comando de configuração.

Qt 6.4 é suportado no Linux Mint. Na abertura da interface, o app verifica uma única vez se o componente de desfoque pode ser carregado no Qt instalado. Com Qt >= 6.5 e Qt Quick Effects disponível, habilita o desfoque e sua opção nas configurações; caso contrário, usa fundo sólido e oculta a opção. Um build Linux com Qt 6.4 também pode aproveitar o efeito ao executar com Qt mais recente. `qml6-module-qtquick-effects` é opcional e não bloqueia a instalação do `.deb` em distribuições que não oferecem esse pacote.

### Núcleo sem Qt

Windows, com Visual Studio instalado:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test-core.ps1
```

Linux, com compilador C++20:

```sh
bash scripts/test-core.sh
```

Alternativa com CMake em ambos os sistemas:

```sh
cmake -S . -B build-core -DSCALAR_BUILD_DESKTOP=OFF -DBUILD_TESTING=ON
cmake --build build-core --config Release --parallel
ctest --test-dir build-core -C Release --output-on-failure
```

Escolha o script ou o CMake para gerar `build-core/`; não misture geradores no mesmo diretório.

## Matemática offline e dependências do pacote

O empacotamento inclui Node privado e MathJax. O usuário final não precisa instalar Python, Node, npm, TeX Live ou Qt separadamente no Windows; no Linux, o APT resolve as bibliotecas do sistema. A preparação inicial das dependências precisa de internet na máquina que gera o pacote.

Para preparar explicitamente a matemática no build de desenvolvimento:

```sh
cmake --build build --config Release --target math_runtime
```

`-DSCALAR_PREPARE_MATH=OFF` desativa a tentativa automática durante desenvolvimento. A instalação e o CPack continuam exigindo um runtime completo. `SCALAR_MATH_RUNTIME` permite apontar para outro runtime em testes.

WebP depende do plugin de formatos de imagem do Qt ou da alternativa libwebp detectada pelo CMake. SVG depende do módulo/plugin Qt SVG; inclua ambos os formatos no deploy.

## Ícones e menu do sistema

Depois de alterar `icon.png`, regenere o ícone nativo Windows e execute novamente o comando de empacotamento:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\update-windows-icon.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

`packaging/scalar.ico` é incorporado ao executável. Após reinstalar, se o Windows mantiver o ícone antigo na barra de tarefas, desafixe o atalho e fixe novamente pelo menu Iniciar.

No Linux, `packaging/scalar.desktop` informa as categorias Office, Education e Graphics. No Windows 11, a classificação automática em Produtividade ou Outros não é controlada pelo NSIS; veja as [orientações da Microsoft](https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/customize-the-windows-11-start-menu#all-section).

No Linux, compilar e executar `./build/scalar` atualiza o ícone da janela, mas o painel e o menu podem usar o ícone da instalação anterior. Para registrar a compilação local e seu ícone no menu do usuário:

```sh
python3 scripts/register-linux-build.py
./build/scalar
```

O script cria `~/.local/share/applications/scalar.desktop` (ou usa `XDG_DATA_HOME`), apontando para o executável em `build/`. Execute-o novamente depois de alterar `icon.png`; para outro diretório de compilação, passe o caminho do executável como argumento. O menu passa a abrir essa compilação. Para voltar ao aplicativo instalado pelo APT, remova esse arquivo `.desktop` local. Feche e reabra o app; atalhos já fixados no painel podem precisar ser desafixados e fixados novamente.

Os pacotes Linux usam um nome de ícone baseado no conteúdo de `icon.png`, evitando reutilizar o cache de uma imagem anterior após a reinstalação.

## Validação da interface

Após compilar, execute o teste rápido de abertura:

```powershell
.\build\Release\scalar.exe --smoke-test
```

No Linux:

```sh
./build/scalar --smoke-test
mkdir -p build/screenshots
./build/scalar --editor --smoke-test --screenshot build/screenshots/editor.png
```

Em Linux sem sessão gráfica, `QT_QPA_PLATFORM=offscreen` permite testar a inicialização. Capturas com backend software podem omitir os meshes do canvas; não comprovam renderização GPU, desempenho ou funcionamento de stylus.

Para investigar botões, coordenadas e pausas de uma stylus física, execute uma sessão curta com:

```sh
bash scripts/trace-tablet.sh
```

O script recompila o executável e abre o app com `SCALAR_TABLET_TRACE` apontando para `build/tablet-input.jsonl`. Abrir o app normalmente não ativa o diagnóstico.

Reproduza a troca para a borracha e feche o app. O arquivo registra os eventos de tablet, ferramenta e cursor antes/depois, duração do processamento, sincronizações de renderização acima de 8 ms e intervalos da interface acima de 50 ms. O diagnóstico fica desativado sem essa variável e não grava o conteúdo do quadro. O arquivo é substituído a cada execução e limitado a aproximadamente 16 MiB. O registro adiciona trabalho de diagnóstico; seus tempos ajudam a localizar a pausa, mas não substituem uma medição sem registro.

Valide manualmente criação de quadros, desenho, undo/redo, salvar/reabrir, PDF, LaTeX, temas, DPI e atalhos. Na instalação, confira o ícone e cada combinação das duas opções finais.

Os resultados antigos estão em [Histórico de validação](docs/BUILD_HISTORY.md). Os detalhes das entregas estão em [Milestone 1](docs/MILESTONE_1.md), [Milestone 2](docs/MILESTONE_2.md), [Milestone 3](docs/MILESTONE_3.md), [Milestone 4](docs/MILESTONE_4.md), [Milestone 5](docs/MILESTONE_5.md) e [Exportação PDF](docs/PDF_EXPORT.md).
