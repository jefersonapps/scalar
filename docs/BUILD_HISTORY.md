# Histórico de validação

Relatos de versões anteriores e dos ambientes usados naquela época. Os resultados abaixo não representam uma execução atual dos testes. Para comandos de compilação e empacotamento, consulte [BUILDING.md](../BUILDING.md).

## Resultado neste ambiente

Linux Mint 22.3, GCC 13.3, Qt 6.4.2. Após a instalação das dependências pelo usuário, CMake configurou e o aplicativo compilou. O caminho dos recursos QML foi corrigido com BASE explícito; ApplicationPaletteChange substituiu o sinal deprecated paletteChanged.

`ctest --test-dir build --output-on-failure`: **7/7 passaram** — core, recognition, eraser, persistence, desktop, smoke_home e smoke_editor. O teste desktop abre Home, cria quadro pela UI, desenha com eventos de mouse, verifica undo/redo, zoom sem mutação, tema dark com página branca, save/load e ausência de warnings QML. Popovers e configurações também foram abertos. Cada teste desktop usa diretório temporário próprio, sem tocar nos projetos do usuário.

Screenshots reais de offscreen ficam em `build/screenshots/`: home-light.png, new-project.png, editor-light.png, editor-dark.png, pen-options.png e settings-dark.png. **Essas capturas usam o backend software, que neste ambiente não exibiu os meshes de traços; não comprovam renderização acelerada do canvas.** O teste valida os objetos/amostras desenhados, não a aparência do stroke na GPU.

A sessão gráfica `:0` não pode ser acessada pelo sandbox (`could not connect to display`); `/dev/dri` não está disponível. Inicie `./build/scalar` no seu terminal gráfico para validar traços visíveis, MSAA, tablet e desempenho. O build ainda pode emitir aviso não bloqueante do qmlimportscanner se qml6-module-qtqml estiver ausente; ele está incluído na lista de dependências acima.

## Validação 0.2

Desktop test também cobre hold-to-line, ajuste antes do release, undo para stroke, borracha parcial e undo/redo, inserção de círculo, seleção/handles, mover, escala/raio e rotação, estilo/fill, duplicar/excluir, Ctrl+V de bitmap, URLs no clipboard, importação por arquivo e drag/drop. Imagens e formas são salvas/reabertas. O teste de persistência verifica v1/v2, PNG incorporado, círculo e decode real; testes de núcleo verificam fitting de formas e cortes analíticos da borracha.

Captura do tema novo com imagens: `build/screenshots/milestone2-dark-images.png`. Veja `docs/MILESTONE_2.md` para uso e limites. SVG/WebP dependem de plugins de imagem Qt; o diálogo inclui extensões, mas importer retorna erro claro se o codec não estiver disponível. Nenhuma biblioteca de imagem externa foi adicionada.

## Milestone 4

Qt PDF é obrigatório no build desktop. Após instalar novos módulos, execute novamente a configuração CMake antes do build. A suíte adicional `pdf` usa documentos reais gerados temporariamente para verificar A4, A4 paisagem, Carta, intervalos, cache, incorporação e reabertura sem o arquivo original. Há dez entradas CTest; a UI também cobre miniaturas, navegação, importação e layout em janela pequena. Veja [o relatório](MILESTONE_4.md) para resultados e limites.

A exportação multipágina usa as mesmas dependências, sem biblioteca adicional. A suíte `pdf` também valida a saída A4/paisagem/Carta, texto selecionável, geometria vetorial, imagens, anotações sobre PDF e substituição segura do destino. A suíte desktop verifica o navegador de páginas persistente no editor. Veja [PDF_EXPORT.md](PDF_EXPORT.md).

## Milestone 5

Régua, snap, compasso e construções usam Qt/STL, sem novas dependências. Configure e compile com os comandos do README. Há onze entradas CTest, incluindo `geometry_tools`. A suíte `desktop` simula mouse, stylus com pressão e touch para os guias, cancela gestos, verifica undo/redo e abre os painéis em janela pequena. Capturas ficam em `build/screenshots/milestone5-*.png`; veja [o relatório](MILESTONE_5.md). Stylus física, renderização GPU e execução no Windows ainda precisam de validação nesses ambientes.
