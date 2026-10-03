# Design System — Scalar

Fonte de verdade: `qml/theme/Theme.qml`. Tokens semânticos separam interface e página. Fontes são as fontes de sistema do Qt, sem assets proprietários.

| Token | Light | Dark |
|---|---|---|
| background | #f3f5f7 | #121b22 |
| workspace | #e9eef1 | #19232b |
| surface | #ffffff | #24313c |
| glass | branco, alpha 0.93 | #24313c, alpha 0.93 |
| accent | #167b69 | #69d4bf |
| accentSoft | #dcefe9 | #28483f |
| text | #243345 | #eef5f6 |
| secondary | #66798b | #aabdc8 |
| border | #dce5e9 | #425260 |

Espaçamento: 4/8/12/16/24/32; seção: 48. Raios: 8/12/18/24. Touch target: 48 pixels lógicos. Escala tipográfica: título 36, heading 22, body 15, caption 13. Breakpoints e dimensões da janela/toolbar/cards são centralizados no singleton. Números geométricos dos ícones e valores físicos de páginas não são tokens de layout.

Animações: 100 ms para hover/press; 160 ms para abrir superfícies. Selected icon scale 1.06; pressed 0.96. Reduzir efeitos zera essas durações e remove a camada de sombra. Superfícies usam borda discreta, transparência e uma camada deslocada de sombra simples. Este ciclo não tem blur; a camada de sombra não é um desfoque difuso real. Isso exige avaliação visual antes de aprovar o design.

## Componentes

GlassPanel: superfície flutuante. GlassPopover: fechamento externo/Escape e fade. ModernDialog: modal, centrado, largura limitada. IconButton/ToolButton: normal, hover, pressed, selected e disabled; seleção tem borda além de cor. ActionButton: botão textual normal/primário. Field e SelectField: entradas próprias, com foco visível. ModernSlider: track e handle consistentes. SegmentedControl: escolhas mutuamente exclusivas. ColorButton/ColorPicker: cores com indicação adicional de check. ProjectCard: thumbnail, título e data. FloatingToolbar: caneta, mão, undo e redo. PenOptions: opções contextuais sem painel permanente.

Ícones têm um único estilo linear, desenhado com Qt Quick Shapes, traço 1.7, pontas/junções arredondadas e base 24 × 24. Não há emojis. Buttons expõem Accessible.name; campos e navegação por teclado precisam de revisão com leitor de tela. A Home é responsiva por Flow/GridView; criação tem Flickable para caber em alturas menores.

## Temas

Light/Dark/System são preferências SQLite. Sistema usa QStyleHints colorScheme em Qt >= 6.5 e luminância de palette em Qt 6.4. O fundo do documento é independente. Uma página branca continua branca em tema dark. Mudanças do tema atualizam superfícies e ícones, sem modificar dados do documento.

## Revisão executada e pendências

Home light, criação, editor light/dark, popover da caneta e configurações dark foram executados e capturados em offscreen. A revisão corrigiu o alinhamento do cabeçalho da Home e o texto dos botões primários, renomeando o token para accentText. O teste verifica contraste do texto primário e que dark mantém a página branca.

Pendente: revisão em sessão gráfica acelerada, teclado/leitor de tela, hover/pressed em uso real e DPI 100/125/150/200%. O backend software da captura não exibiu os meshes dos traços; screenshots não validam o renderer GPU.
