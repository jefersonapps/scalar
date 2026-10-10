# Compilar, testar e gerar instaladores

Execute os comandos abaixo na raiz do projeto (`scalar/`).

## Gerar o instalador Windows

No PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

Esse é o comando principal para recompilar o aplicativo e gerar o instalador. O script inicializa o ambiente MSVC, configura o CMake, compila e empacota com NSIS.

**Instalador para distribuir:** `build-package\packages\Scalar-0.5.0-win64.exe` (o nome acompanha a versão do projeto). O executável `build-package\scalar.exe` é uma saída de compilação; distribua o instalador em `packages/`, que inclui as dependências.

### Ferramentas necessárias

- Visual Studio ou Build Tools com ferramentas C++ x64 e Windows SDK.
- CMake >= 3.21 e Ninja disponíveis no `PATH`.
- Qt >= 6.5, kit MSVC x64, com Qt Quick, Quick Controls, Quick Effects, SQL, Concurrent, PDF, SVG e `windeployqt`.
- Python >= 3.12 para preparar Node e MathJax durante o empacotamento.
- NSIS instalado.

O script está configurado para Qt em `C:\Qt\6.8.2\msvc2022_64`, CMake em `C:\Program Files\CMake\bin` e NSIS em `C:\NSIS\nsis-3.10`. Ajuste os caminhos em [package-windows.ps1](scripts/package-windows.ps1) se sua instalação for diferente. CMake e Ninja também podem ser encontrados pelo `PATH`.

O script recria `build-package/` a cada execução. Ao terminar, execute o instalador para atualizar o app. A tela final oferece **Executar o Scalar** e **Criar atalho na área de trabalho**, ambas marcadas por padrão. Instalações silenciosas não executam essas ações.

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
sudo apt install build-essential cmake ninja-build python3 qt6-base-dev qt6-declarative-dev qt6-pdf-dev qt6-svg-dev \
  libqt6sql6-sqlite qt6-image-formats-plugins qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-dialogs qml6-module-qtquick-shapes \
  qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml qml6-module-qtqml-workerscript
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/scalar
```

Use Python >= 3.12. Para Qt instalado fora do sistema, acrescente `-DCMAKE_PREFIX_PATH=/caminho/Qt/6.x/gcc_64` ao comando de configuração.

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

Valide manualmente criação de quadros, desenho, undo/redo, salvar/reabrir, PDF, LaTeX, temas, DPI e atalhos. Na instalação, confira o ícone e cada combinação das duas opções finais.

Os resultados antigos estão em [Histórico de validação](docs/BUILD_HISTORY.md). Os detalhes das entregas estão em [Milestone 1](docs/MILESTONE_1.md), [Milestone 2](docs/MILESTONE_2.md), [Milestone 3](docs/MILESTONE_3.md), [Milestone 4](docs/MILESTONE_4.md), [Milestone 5](docs/MILESTONE_5.md) e [Exportação PDF](docs/PDF_EXPORT.md).
