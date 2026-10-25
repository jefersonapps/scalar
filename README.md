# Scalar

Quadro branco desktop offline para Windows e Linux, em C++20, Qt 6 e Qt Quick.

**Estado: Scalar 0.5 — Milestone 5, ferramentas geométricas.** Régua com snap, compasso e construções geométricas estão integrados ao canvas vetorial. Projetos multipágina, miniaturas, PDF, imagens, texto e LaTeX offline permanecem disponíveis. O quadro inteiro pode ser exportado em PDF multipágina. Exportação PNG e opções avançadas de exportação permanecem para um ciclo futuro.

## Compilar a versão Release no Linux e no Windows

Release é a compilação otimizada do aplicativo. Você pode gerar um executável para testar no seu computador ou um instalador para distribuir. Execute os comandos na raiz do projeto, onde está o arquivo `CMakeLists.txt`. Compile no Linux para gerar o pacote Linux e no Windows para gerar o instalador Windows.

### Linux — Debian, Ubuntu e Linux Mint

**1. Prepare o ambiente.** Instale compilador C++20, CMake, Ninja, Python >= 3.12 e os módulos de desenvolvimento do Qt >= 6.4. A lista de pacotes e o comando de instalação estão em [BUILDING.md — Linux](BUILDING.md#linux).

**2. Para gerar o pacote Release `.deb`, execute:**

```sh
bash scripts/package-linux.sh
```

Esse comando configura o CMake em Release, compila, gera o `.deb` e valida o pacote. O resultado fica em `build-package/packages/`, junto do assistente `install-scalar.sh`. O nome do `.deb` varia conforme a versão e a arquitetura.

**3. Para instalar ou atualizar o aplicativo, execute:**

```sh
bash ./build-package/packages/install-scalar.sh
```

O assistente solicita autorização para instalar e oferece abrir o app e criar um atalho. Depois de instalado, você também pode abrir pelo menu **Scalar** ou pelo comando `scalar-whiteboard`.

**Para apenas compilar e executar localmente, sem gerar o instalador:**

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel
./build/scalar
```

O executável fica em `build/scalar`. Para atualizar essa compilação após alterar o código, repita `cmake --build build --parallel` e reabra o executável.

### Windows — PowerShell

**1. Prepare o ambiente.** Instale Visual Studio 2022 ou Build Tools com C++ x64 e Windows SDK, CMake >= 3.21, Ninja, Python >= 3.12, Qt >= 6.5 com kit MSVC x64 e NSIS. Os módulos Qt necessários estão em [BUILDING.md — Ferramentas necessárias](BUILDING.md#ferramentas-necessárias).

O script está configurado para Qt em `C:\Qt\6.8.2\msvc2022_64` e NSIS em `C:\NSIS\nsis-3.10`. Se suas instalações usam outros caminhos, ajuste as variáveis em [scripts/package-windows.ps1](scripts/package-windows.ps1) antes de executar. CMake e Ninja precisam estar acessíveis pelo `PATH`.

**2. Para compilar em Release e gerar o instalador `.exe`, execute no PowerShell:**

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

O script carrega o ambiente MSVC, configura o CMake em Release, compila e gera o instalador com as dependências necessárias. O resultado fica em `build-package\packages\Scalar-<versão>-win64.exe`.

**3. Execute o instalador gerado para instalar ou atualizar o Scalar.** Depois, abra o aplicativo pelo menu Iniciar ou pelo atalho criado pelo instalador.

Para distribuir o aplicativo, envie o instalador de `build-package\packages\`. O arquivo `build-package\scalar.exe` é a saída da compilação e depende dos arquivos do Qt e do runtime preparados pelo empacotamento.

**Para apenas compilar e executar localmente, sem gerar o instalador**, abra o **Developer PowerShell for VS 2022** e ajuste o caminho do Qt nos comandos:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" `
  -DBUILD_TESTING=OFF
cmake --build build --config Release --parallel
& "C:\Qt\6.8.2\msvc2022_64\bin\windeployqt.exe" --release --qmldir qml build/Release/scalar.exe
.\build\Release\scalar.exe
```

Nesse caso, o executável fica em `build\Release\scalar.exe`. O parâmetro `--config Release` seleciona a versão otimizada no gerador do Visual Studio. Use uma pasta de compilação diferente se `build/` já tiver sido configurada com outro gerador, como Ninja.

### Recompilar e testar

Os scripts de empacotamento reutilizam `build-package/`: execute novamente o script da sua plataforma para gerar um instalador atualizado. A pasta `build/` fica disponível para compilações locais e testes.

Para habilitar e executar os testes, consulte [BUILDING.md — Desenvolvimento e testes](BUILDING.md#desenvolvimento-e-testes). Sem Qt/CMake, você pode verificar apenas o núcleo com `bash scripts/test-core.sh` no Linux ou `powershell -ExecutionPolicy Bypass -File .\scripts\test-core.ps1` no Windows.

## Uso previsto

Cada página mantém seu zoom e posição durante a sessão. Páginas novas herdam o zoom anterior e começam no topo; ao reiniciar o aplicativo, a vista volta ao ajuste padrão. A escrita livre usa curvas adaptativas já durante o desenho, com transições suaves de pressão, incluindo no PDF exportado. O zoom reutiliza a geometria da caneta e as imagens dos marcadores; o refinamento dos marcadores é preparado em segundo plano.

A função Apagar/Erase do Wacom Center e a ponta de borracha da caneta ativam temporariamente a borracha quando o driver informa o modo Eraser. Ao voltar à ponta normal ou retirar a caneta da proximidade do tablet, a ferramenta anterior é restaurada. O atalho de teclado E continua selecionando a borracha.

No Linux, os botões laterais da stylus têm atalhos padrão no quadro: o inferior (evento de botão direito) ativa a borracha enquanto pressionado e restaura a ferramenta anterior ao soltar; o superior (evento de botão do meio) seleciona a caneta. Os botões funcionam em suspensão, mas só desenham/apagam com a ponta em contato. Sair da proximidade da mesa ou desativar a janela também encerra a borracha temporária. O mapeamento usa os eventos de tablet recebidos do sistema, sem alterar os botões do mouse nem exigir configuração no Wacom Center; um remapeamento prévio no driver pode mudar qual botão físico envia esses eventos.

Clique em Mover quadro para selecionar a ferramenta, ou segure H fora dos campos de texto para usá-la temporariamente; ao soltar H, a ferramenta anterior volta. O cursor mostra uma mão aberta e se fecha durante o arraste.

Crie um quadro na Home; escolha página, orientação e fundo, ou use “Criar com padrão”. Seleção, caneta, borracha, imagens, formas e mão ficam na toolbar flutuante. Clique novamente na caneta para cores, espessura e curva de pressão. A rolagem move a página verticalmente; Alt + rolagem move na horizontal e Ctrl + rolagem dá zoom. Espaço + arraste move a página. Touch move o quadro e dois dedos fazem pinch. Salvar como cria uma cópia em local escolhido; quadros novos já possuem arquivo gerenciado na pasta de dados do aplicativo.

Texto: selecione T e arraste para definir a caixa. Escreva diretamente no quadro; clique fora ou use Ctrl+Enter para concluir, e Esc para cancelar. Duplo clique permite editar textos existentes. O botão no canto superior esquerdo abre fonte, tamanho, cor, alinhamento, negrito e itálico; a fonte LaTeX é o padrão e a lista inclui Lobster Two e as fontes instaladas no sistema. Use $...$ ou $$...$$ para matemática LaTeX, convertida em geometria vetorial ao concluir a edição. O botão LaTeX acima da caixa desativa a interpretação de fórmulas, permitindo digitar valores como R$ normalmente; a escolha é salva por texto.

Atalhos: V seleção, P caneta, M marcador, E borracha, T texto, H mão; Ctrl+Z/Ctrl+Shift+Z, Ctrl+D duplicar, Delete excluir selecionados, Ctrl+V colar imagem ou texto, Ctrl+S, Ctrl+Shift+S, Ctrl+O e Ctrl+N. Escape cancela a operação ativa.

Ao criar, mover ou editar uma reta, o quadro mostra seu ângulo de 0° a 360°, medido no sentido anti-horário a partir da horizontal para a direita. Segure Ctrl ao criar, girar ou arrastar uma extremidade para encaixar em múltiplos de 15°. O menu de formas e as propriedades do objeto permitem ajustar o comprimento dos traços, o intervalo do tracejado e a distância entre os pontos, em milímetros. Ao selecionar um setor circular, marque “Mostrar ângulo” para exibir sua abertura no quadro; essa opção fica salva por objeto.

Desenhe e mantenha pressionado por 1 segundo no final para reconhecer linha, círculo, elipse, triângulo, retângulo, quadrado, paralelogramo, setores circulares e polígonos regulares de 5 a 12 lados. Arcos circulares reconhecidos fecham um setor com dois raios e preenchimento translúcido de 10%. O tempo é configurável. Segurar solicita sempre a melhor candidata, mesmo com tremor; Shift aplica tracejado. Linhas separadas com endpoints próximos (3 mm) fecham um polígono preenchido automaticamente; Ctrl+Z restaura as linhas. Uma linha reconhecida acompanha o ponteiro enquanto pressionado. Ao soltar, a conversão é confirmada; Ctrl+Z restaura o traço original. Em seleção, Shift alterna objetos; arrastar no vazio seleciona uma área. Handles editam endpoints, vértices de triângulo, centro/raio, escala proporcional e rotação.

A borracha preserva os segmentos fora da região apagada; cada gesto completo pode ser desfeito. Ela acompanha todo o caminho entre os eventos, incluindo movimentos rápidos, e preserva a pressão do pincel e a transparência do marcador. Para apagar formas, marque “Apagar formas também” ou segure Ctrl; Shift restaura os trechos apagados nesta sessão do quadro. Ao fechar o quadro ou o aplicativo, o histórico recuperável é descartado; o arquivo conserva apenas o resultado consolidado do apagamento. Imagens são incorporadas ao arquivo e não dependem de caminho externo. PNG/JPEG/BMP usam Qt; WebP usa plugin Qt ou libwebp, e SVG requer Qt SVG ou seu plugin de imagem.

O botão de páginas abre miniaturas, adiciona uma página ou duplica a atual. Cada miniatura tem um botão neutro de excluir no canto superior direito; excluir a última página cria uma página em branco. Ctrl+PgUp/PgDown navega entre páginas. Importe PDF pela Home, pelo painel de páginas ou arrastando o arquivo; escolha todas as páginas ou um intervalo como `1, 3-5`. Cada página mantém seu tamanho físico, o PDF original é incorporado e as anotações ficam acima dele. Undo/redo é separado por página.

Para navegar rapidamente, use as setas no topo do editor; o contador abre as miniaturas. O botão de exportação no topo (seta para cima) salva **todas as páginas em PDF**, em sua ordem e tamanho físico, incluindo fundos, grades e conteúdo. Traços, formas, texto e matemática são vetoriais; imagens e a base PDF importada permanecem raster. Veja [exportação PDF](docs/PDF_EXPORT.md).

Uma seleção pode ser movida arrastando qualquer ponto dentro da sua caixa. O painel de propriedades e o menu do botão direito permitem exportar apenas os objetos selecionados como PDF ou SVG, recortados ao conteúdo e sem o fundo do quadro. O SVG mantém transparência; partes apagadas usam imagens com alpha, enquanto os demais traços e formas permanecem vetoriais.

Régua: arraste o corpo para mover, o handle esquerdo para girar e o direito para alterar o comprimento. Segure Ctrl durante a rotação para ajustar o ângulo em passos de 15°; ao soltar Ctrl, a rotação volta a ser livre. Com a régua visível, desenhe perto de uma borda com a caneta: os pontos são projetados sobre a reta, preservando pressão. Compasso: arraste a dobradiça ou a haste metálica para mover; arraste a haste azul para ajustar abertura e orientação com a ponta seca fixa. Arraste a ponta do lápis para desenhar. Uma volta completa cria uma circunferência vetorial; uma volta parcial mantém o arco como traço. O compasso marca o centro com um ponto de tinta apagável por padrão; desative “Marcar centro” no painel para manter essa escolha durante a sessão do quadro, em todas as páginas. Clique novamente no botão da ferramenta para medidas em mm, snap, centralização e ocultação. Escape cancela o gesto; Ctrl+Z desfaz o desenho.

Selecione uma linha para criar uma paralela, perpendicular ou mediatriz pelo painel contextual. Selecione um triângulo para criar sua circunferência circunscrita. As construções são objetos editáveis independentes; régua e compasso são guias temporários e não aparecem na exportação.

## Documentação

- [Arquitetura](ARCHITECTURE.md)
- [Design System](DESIGN_SYSTEM.md)
- [Build e execução](BUILDING.md)
- [Formato do projeto](FILE_FORMAT.md)
- [Entrega inicial](docs/MILESTONE_1.md)
- [Milestone 2, borracha e imagens](docs/MILESTONE_2.md)
- [Milestone 3, lixeira e validação](docs/MILESTONE_3.md)
- [Milestone 4, PDF e páginas](docs/MILESTONE_4.md)
- [Milestone 5, régua e compasso](docs/MILESTONE_5.md)

Fundo e grade: botão de grade no topo. A cor é independente do preset de grade; trocar a grade preserva as cores, opacidade e espessura ajustadas. Novos quadros usam grade pautada por padrão e começam brancos no tema claro e pretos no escuro; mudar o tema não recolore páginas salvas. Ajustes são aplicados em tempo real. Texto: T, clique na página e escreva; `$...$` insere matemática no texto e `$$...$$` em destaque. Seleção → “Editar texto / LaTeX” reabre a fonte.

Nomes: o botão de ajustes no card abre a edição do quadro. No quadro aberto, clique no título para renomear; Enter ou um clique fora salva o nome. A busca da biblioteca encontra quadros em todas as pastas e ignora maiúsculas e acentos. “Recentes” mostra os 12 quadros alterados mais recentemente; “Todos os quadros” inclui toda a biblioteca.

Lixeira: o botão de excluir fica no modal de edição do quadro, pede confirmação e move o arquivo e thumbnail. Na lixeira é possível restaurar ou excluir permanentemente. A expiração após 30 dias é verificada na abertura e a cada hora com o app aberto; com o app fechado, a limpeza ocorre na próxima abertura.

Pastas: “Nova pasta” cria uma pasta no nível atual, com nome e cinco cores (azul, verde, ocre, roxo e rosa). A navegação lateral permite abrir e expandir subpastas. Arraste um card para uma pasta ou para Voltar para subir um nível. No ícone de ajustes, altere nome/cor ou exclua a pasta; o conteúdo volta ao nível anterior. Ao sair de um quadro, a navegação retorna à pasta de origem. Organização é local no SQLite, sem mover arquivos; salvamento e restauração da lixeira preservam a pasta. A interface usa azul primário nos temas claro e escuro.

Não há serviços em nuvem ou telemetria. O build prepara automaticamente o MathJax e Node locais. Para preparar explicitamente o executável de desenvolvimento: `cmake --build build --target math_runtime`. A instalação/CPack exige e incorpora o runtime completo; o usuário final não precisa instalar npm/Node e a conversão funciona offline.

Texto: Latin Modern Roman acompanha o aplicativo como fonte padrão, exibida como **LaTeX**; Lobster Two permanece como segunda opção, com seu nome original. As duas fontes funcionam offline e suportam os acentos do português. O seletor pesquisa as fontes instaladas e mostra seus nomes na própria fonte. Acima da caixa, **B** e *I* formatam o trecho selecionado (Ctrl+B/Ctrl+I); a formatação é preservada ao salvar e reabrir. As licenças estão em `assets/fonts/LatinModern-GUST-LICENSE.txt` e `assets/fonts/LobsterTwo-OFL.txt`.

Marcador (M): desenhe com cor, largura e opacidade próprias, começando em 10%. Clique novamente na ferramenta para ajustar a cor, a largura e a opacidade. Ctrl + clique tenta preencher uma região delimitada por traços ou contornos de formas, inclusive duas linhas e um arco. Pontas com aberturas de até 2 mm podem ser conectadas. Regiões que alcançam a borda da página são recusadas; grade, imagens e traços do marcador não formam barreiras. Preenchimentos preservam vazios internos, podem ser movidos ou excluídos pela seleção e suportam desfazer/refazer e salvar/reabrir.
