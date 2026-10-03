# Scalar

Quadro branco desktop offline para Windows e Linux, em C++20, Qt 6 e Qt Quick.

**Estado: implementação inicial do Milestone 1 compilada com Qt 6.4.2 e GCC 13.3. Cinco testes passaram, incluindo persistência e interação desktop em offscreen.** Home, criação, editor, opções da caneta e configurações foram executados. Renderização acelerada, stylus física e performance ainda exigem validação em sessão gráfica real.

O código inclui Home, criação de páginas A4/Carta/customizadas, orientação, temas Claro/Escuro/Sistema, canvas vetorial com Scene Graph, pressão, fallback de mouse, touch pan/pinch, zoom, undo/redo, biblioteca SQLite, `.board`, autosave e recuperação. Recursos dos milestones 2–6 não foram implementados.

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

Crie um quadro na Home; escolha página, orientação e fundo, ou use “Criar com padrão”. A caneta e a mão ficam na toolbar flutuante. Clique novamente na caneta para cores, espessura e curva de pressão. A rolagem faz zoom; Espaço + arraste move a página. Touch move o quadro e dois dedos fazem pinch. Salvar como cria uma cópia em local escolhido; quadros novos já possuem arquivo gerenciado na pasta de dados do aplicativo.

Atalhos: P, H, Ctrl+Z, Ctrl+Shift+Z, Ctrl+S, Ctrl+Shift+S, Ctrl+O e Ctrl+N. Escape cancela um traço em andamento. Atalhos de seleção, borracha, texto e clipboard serão introduzidos com suas ferramentas.

## Documentação

- [Arquitetura](ARCHITECTURE.md)
- [Design System](DESIGN_SYSTEM.md)
- [Build e execução](BUILDING.md)
- [Formato do projeto](FILE_FORMAT.md)
- [Entrega e validação do ciclo](docs/MILESTONE_1.md)

Não há serviços em nuvem, telemetria ou download em runtime. Qt PDF e MathJax serão integrados nos respectivos milestones, sem impor essas dependências ao ciclo inicial.
