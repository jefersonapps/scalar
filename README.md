# Scalar

Quadro branco desktop offline para Windows e Linux, em C++20, Qt 6 e Qt Quick.

**Estado: Scalar 0.2, Milestone 2 com borracha por trecho e importação de imagens. Compilado com Qt 6.4.2/GCC 13.3; sete testes passaram.** A UI e interações são testadas em offscreen. Renderer acelerado e stylus física precisam de revisão manual da versão nova.

Além da base do M1, há seleção simples/múltipla/por área, handles, formas vetoriais, reconhecimento local ao segurar a caneta, propriedades contextuais, borracha que divide traços, imagens por arquivo/Ctrl+V/drop e tema dark neutro inspirado no shadcn/ui. PDF, textos, LaTeX, grades, régua e compasso continuam nos próximos milestones.

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

Crie um quadro na Home; escolha página, orientação e fundo, ou use “Criar com padrão”. Seleção, caneta, borracha, imagens, formas e mão ficam na toolbar flutuante. Clique novamente na caneta para cores, espessura e curva de pressão. A rolagem faz zoom; Espaço + arraste move a página. Touch move o quadro e dois dedos fazem pinch. Salvar como cria uma cópia em local escolhido; quadros novos já possuem arquivo gerenciado na pasta de dados do aplicativo.

Atalhos: V seleção, P caneta, E borracha, H mão; Ctrl+Z/Ctrl+Shift+Z, Ctrl+D duplicar, Delete excluir selecionados, Ctrl+V colar imagem, Ctrl+S, Ctrl+Shift+S, Ctrl+O e Ctrl+N. Escape cancela a operação ativa.

Desenhe e mantenha pressionado por 500 ms no final para reconhecer linha, círculo, elipse, triângulo, retângulo ou quadrado. O tempo é configurável. Uma linha reconhecida acompanha o ponteiro enquanto pressionado. Ao soltar, a conversão é confirmada; Ctrl+Z restaura o traço original. Em seleção, Shift alterna objetos; arrastar no vazio seleciona uma área. Handles editam endpoints, vértices de triângulo, centro/raio, escala proporcional e rotação.

A borracha preserva os segmentos fora da região apagada; a operação completa pode ser desfeita. Imagens são incorporadas ao arquivo e não dependem de caminho externo. PNG/JPEG/BMP e demais formatos dependem dos plugins de imagem Qt instalados (WebP/SVG podem exigir plugins adicionais).

## Documentação

- [Arquitetura](ARCHITECTURE.md)
- [Design System](DESIGN_SYSTEM.md)
- [Build e execução](BUILDING.md)
- [Formato do projeto](FILE_FORMAT.md)
- [Entrega inicial](docs/MILESTONE_1.md)
- [Milestone 2, borracha e imagens](docs/MILESTONE_2.md)

Não há serviços em nuvem, telemetria ou download em runtime. Qt PDF e MathJax serão integrados nos respectivos milestones, sem impor essas dependências ao ciclo inicial.
