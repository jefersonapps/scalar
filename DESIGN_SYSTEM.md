# Design System — Scalar

Fonte de verdade: `qml/theme/Theme.qml`. Tokens semânticos separam interface e página. Fontes são as fontes de sistema do Qt, sem assets proprietários.

| Token | Light | Dark |
|---|---|---|
| background | #f3f5f7 | #09090b |
| workspace | #e9eef1 | #111113 |
| surface | #ffffff | #18181b |
| glass | branco, alpha 0.93 | #18181b, alpha 0.93 |
| accent | #167b69 | #fafafa |
| accentSoft | #dcefe9 | #27272a |
| text | #243345 | #fafafa |
| secondary | #66798b | #a1a1aa |
| border | #dce5e9 | #303033 |

Espaçamento: 4/8/12/16/24/32; seção: 48. Raios: 8/12/18/24. Alvos da toolbar: 44 pixels lógicos; campos e controles compactos: 40. Escala tipográfica: título 36, heading 20, body 14, caption 13. Segmentos compartilham uma única superfície, sem bordas repetidas; os ícones usam traço 1,6 com terminais e junções arredondados. Breakpoints e dimensões da janela/toolbar/cards são centralizados no singleton. Números geométricos dos ícones e valores físicos de páginas não são tokens de layout.

Animações: 100 ms para hover/press; 160 ms para abrir superfícies. Selected icon scale 1.06; pressed 0.96. Reduzir efeitos zera essas durações e remove a camada de sombra. Superfícies usam borda discreta, transparência e uma camada deslocada de sombra simples. Este ciclo não tem blur; a camada de sombra não é um desfoque difuso real. Isso exige avaliação visual antes de aprovar o design.

## Componentes

GlassPanel: superfície flutuante. GlassPopover: fechamento externo/Escape e fade. ModernDialog: modal, centrado, largura limitada. IconButton/ToolButton: normal, hover, pressed, selected e disabled; seleção tem borda além de cor. ActionButton: botão textual normal/primário. Field e SelectField: entradas próprias, com foco visível. ModernSlider: track e handle consistentes. SegmentedControl: escolhas mutuamente exclusivas. ColorButton/ColorPicker: cores com indicação adicional de check. ProjectCard: thumbnail, título e data. FloatingToolbar: seleção, caneta, borracha, importação, formas, mão, undo e redo. PropertiesPanel: ações e estilo para seleção. ShapeOptions: formas explícitas. PenOptions: opções contextuais sem painel permanente.

Ícones têm um único estilo linear, desenhado com Qt Quick Shapes, traço 1.7, pontas/junções arredondadas e base 24 × 24. Não há emojis. Buttons expõem Accessible.name; campos e navegação por teclado precisam de revisão com leitor de tela. A Home é responsiva por Flow/GridView; criação tem Flickable para caber em alturas menores.

## Temas

Light/Dark/System são preferências SQLite. Sistema usa QStyleHints colorScheme em Qt >= 6.5 e luminância de palette em Qt 6.4. O fundo do documento é independente. Uma página branca continua branca em tema dark. Mudanças do tema atualizam superfícies e ícones, sem modificar dados do documento.

## Revisão executada e pendências

Home light, criação, editor light/dark, popover da caneta e configurações dark foram executados e capturados em offscreen. A revisão corrigiu o alinhamento do cabeçalho da Home e o texto dos botões primários, renomeando o token para accentText. O teste verifica contraste do texto primário e que dark mantém a página branca.

Pendente: revisão em sessão gráfica acelerada, teclado/leitor de tela, hover/pressed em uso real e DPI 100/125/150/200%. O backend software da captura não exibiu os meshes dos traços; screenshots não validam o renderer GPU.

## Revisão 0.2

Dark usa uma paleta neutral/zinc inspirada no [theming do shadcn/ui](https://ui.shadcn.com/docs/theming), adaptada a QML: fundo #09090b, workspace #111113, superfície #18181b, hover #27272a, borda #303033, texto #fafafa e texto secundário #a1a1aa. O destaque dark é claro com texto escuro; Light conserva o acento verde do Scalar. Não foi adicionada dependência web/React. O scrim dark é preto; transparências continuam discretas e sem blur.

Settings e criação têm scroll em telas menores. Toolbar expandida ainda cabe na largura mínima de 520 px. Handles têm 12 px visuais e tolerância de 12 px lógicos para hit test. Novas capturas: build/screenshots/milestone2-dark-images.png e settings-dark.png. Elas validam UI/texture de imagem em software; a aparência de meshes de shapes/strokes precisa de sessão acelerada.

Configurações organizam rótulo e controle na mesma linha, com divisores discretos. Tamanho e orientação padrão ficam lado a lado. O seletor de formas usa uma grade e estilos Contínuo, Tracejado e Pontilhado. Shift durante o hold aplica tracejado à forma reconhecida.

Propriedades da forma distinguem “Cor do contorno”, “Cor do preenchimento” e “Opacidade do preenchimento”. Cada paleta destaca a cor atual. O painel usa espaçamento compacto e também atende polígonos genéricos.

## Componentes do ciclo 3

BackgroundOptions reúne preset, paleta de nove fundos, hexadecimal, CustomColorDialog com HSV e preview, tipo de grade, cor/espessura/espaçamento/opacidade. O formulário tem largura vinculada ao viewport, Flow quebra a paleta em linhas e ScrollView tem apenas rolagem vertical. Paletas claras usam check escuro para contraste.

TextDialog reutiliza ModernDialog/Field/ActionButton/ColorPicker/SelectField para fonte, tamanho, negrito, itálico, alinhamento e código LaTeX. Entrada de canvas e atalhos ficam suspensos durante edição/modal. Tema não muda a página.

ProjectCard inclui ação acessível de lixeira. DeleteProjectDialog informa nome, caminho e retenção/irreversibilidade antes de confirmar; TrashDialog apresenta expiração, restaurar e excluir. Superfícies, margens, duração, typography e touch targets usam Theme, incluindo estados disabled/pressed/checked.

Paletas e seletor de cores: `Theme.backgroundColors` inclui preto puro; matizes e saturações da caneta são compartilhadas entre variantes claras e escuras, com luminosidade adaptada ao fundo da página. `CustomColorDialog` oferece plano saturação/luminosidade, barra de matiz, cores básicas, prévia anterior/nova, RGB e hexadecimal, além de ajuste por setas. Fundo e caneta usam o mesmo componente. Cancelar restaura a cor anterior; confirmar preserva a seleção. Ajustes de fundo são aplicados imediatamente, sem botão Aplicar.

## Componentes do ciclo 4

PageThumbnail usa miniatura, número, tamanho físico e indicação PDF; a página selecionada tem borda e check. PagesPanel apresenta duas colunas, rolagem vertical, criação, duplicação e importação. Largura e altura dos cards usam os tokens pagesPanelWidth/pageTileHeight; a largura se adapta à janela. ImportPdfDialog reutiliza superfícies, campos e controles existentes, mostra leitura assíncrona e erros sem fechar a entrada do intervalo.

Cor do fundo é um controle separado do preset de grade, inclusive na criação. O padrão inicial é branco no tema claro e preto no escuro; o documento aberto conserva sua cor. A seleção de grade preserva cor de fundo, cor da grade, opacidade e espessura. Capturas offscreen da M4 validam alinhamento, dimensões e textura PDF; não comprovam qualidade dos meshes no backend GPU.

PageNavigator mantém setas e contador sempre acessíveis no cabeçalho, com limites desabilitados e targets de 44 px. Clicar no contador abre PagesPanel. O canvas começa abaixo dos controles; sua exclusão de input cobre somente a toolbar inferior. Em larguras abaixo do token navigationInlineBreakpoint, o navegador ocupa uma segunda linha superior. Na janela estreita, o botão de miniaturas do topo fica oculto, pois o contador já oferece acesso; exportação PDF permanece visível. `build/screenshots/quick-pages-dark-small.png` documenta o layout em 520 × 640; `quick-pages-dark-top.png` mostra o cabeçalho largo.
