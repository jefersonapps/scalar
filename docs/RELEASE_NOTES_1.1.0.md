# Scalar 1.1.0 — Construções magnéticas, transferidor e gravação passo a passo

O Scalar 1.1.0 amplia as ferramentas de construção geométrica, permite gravar e reproduzir construções no próprio quadro e reorganiza os controles para deixar o desenho e a edição de texto mais acessíveis. Esta versão também melhora o desempenho em quadros carregados e corrige problemas de interação, renderização e navegação.

## Transferidor com desenho guiado

- Novo transferidor na toolbar inferior, com ícone próprio e corpo transparente integrado ao visual da régua.
- Duas escalas de 0° a 180°, graduações e linhas radiais para apoiar construções e leitura de ângulos.
- Um único **90** centralizado entre as escalas, com espaço na linha ao redor do número.
- Números e graduações proporcionais ao tamanho exibido, com melhor leitura ao ampliar o zoom. A margem inferior acomoda os valores das extremidades por inteiro.
- Controles de escala, ângulo, exibição dos números, centralização e ocultação na barra contextual superior.
- Movimentação pelo corpo e rotação ao redor do centro. Clicar no transferidor com a seleção ativa passa automaticamente para sua ferramenta.
- Centro com encaixe magnético em interseções e extremidades quando o ímã está ligado.
- A caneta segue o arco e mantém a aderência até soltar, inclusive quando um traço livre alcança o contorno. Ao ultrapassar a base, não há salto para a outra extremidade nem preenchimento automático de todo o semicírculo.

## Encaixes magnéticos para construções

- Novo botão de ímã na toolbar inferior, com indicadores azuis nos pontos de encaixe.
- Detecção de extremidades e interseções de traços e formas, incluindo curvas e formas rotacionadas, respeitando trechos apagados e objetos ocultos.
- A ponta seca do compasso pode se prender aos pontos de construção próximos.
- A régua procura alinhar sua borda a dois pontos próximos e só encaixa ao finalizar o movimento, a rotação ou o redimensionamento, sem disputar a posição com o cursor durante o gesto.
- Apenas o lado com as graduações da régua participa do encaixe. Inverter as graduações também inverte o lado magnético.
- Interseções têm prioridade sobre extremidades na escolha dos pontos e da âncora de rotação. A régua pode girar ao redor do ponto encaixado, inclusive quando apenas um ponto está disponível.
- Correções para evitar pontos artificiais em amostras duplicadas, arcos retraçados e traços retos feitos com movimentos de ida e volta. Extremidades internas cobertas por traços retos sobrepostos deixam de atrair os instrumentos.

## Gravação e reprodução de construções

- Novo botão de câmera na toolbar inferior para iniciar e parar a gravação dos passos.
- Timeline compacta na parte inferior, com reprodução, pausa, passo anterior, próximo passo e navegação direta pela agulha.
- Os passos são exibidos no próprio canvas, preservando o quadro atual para retomar a edição ao escolher uma ferramenta.
- Menu contextual na agulha com a opção **Apagar passo**, para retirar um estado da gravação final.
- Exportação da sequência como **GIF animado** ou **vídeo MP4**.
- Agulha no estilo de editores de vídeo, progresso no azul do aplicativo, espaçamento equilibrado e mensagens exibidas apenas durante a exportação ou em caso de erro.

## Interface e acesso às ferramentas

- Opções da ferramenta ativa reunidas na barra superior existente, com layout mais compacto e mais espaço útil para o quadro.
- Acesso direto a cores, cores personalizadas, espessura, estilo de traço, pressão e opções específicas de cada ferramenta.
- Cinco espessuras predefinidas com representação visual substituem os sliders de espessura da caneta e do marcador. A caneta oferece **0,1 mm** para traços auxiliares e mantém **0,4 mm** como padrão; a borracha continua com slider.
- Rolagem horizontal das opções com a roda do mouse, sem barra de rolagem visível.
- Espaçamentos mais uniformes entre o contexto da ferramenta, Arquivo, Páginas e controles de visualização. O campo de nome do quadro ocupa menos espaço.
- Configurações gerais movidas para a tela inicial.
- Controles para navegar e criar páginas no canto inferior direito, substituindo a mensagem indicativa anterior.
- **Arquivo → Novo quadro** salva e fecha o quadro atual e cria outro no mesmo diretório e pasta da biblioteca.
- Menu Arquivo com ícones e textos alinhados à esquerda. Dropdowns seguem o mesmo padrão visual, com padding mais compacto, indicação de seleção e fechamento ao clicar novamente no botão de abertura.
- Seletor de fontes com largura adequada para leitura dos nomes e ajustes de rolagem e foco.
- Corrigida a abertura intermitente do diálogo de cores personalizadas pela barra superior.

## Texto, fontes e LaTeX

- Fonte, tamanho, espaçamento entre linhas, alinhamento, negrito, itálico, cor e opção LaTeX concentrados na barra contextual superior. Removidos os controles flutuantes acima da caixa e as instruções que ocupavam esse espaço.
- Tamanhos de fonte predefinidos em dropdown, mantendo a digitação de valores personalizados com duplo clique no campo.
- Espaçamento entre linhas ajustável em **1; 1,15; 1,5; 2; 2,5 e 3**, preservado ao salvar e reabrir o quadro. Arquivos antigos usam o espaçamento padrão quando não possuem essa configuração.
- Ícones mais claros de negrito e itálico, com identificação ao passar o mouse e atalhos Ctrl+B/Ctrl+I.
- Alças de redimensionamento nos quatro cantos da caixa de texto, inclusive para caixas rotacionadas.
- Duplo clique em um texto, a partir da seleção, ativa a ferramenta de texto e inicia a edição.
- Ao clicar fora de uma caixa vazia, ela é descartada e o aplicativo retorna à seleção.
- Maior consistência de fonte, espaçamento, tamanho e quebras de linha entre a edição e o texto finalizado.
- Correção do alinhamento vertical de fórmulas LaTeX em linha, incluindo símbolos como o conjunto dos números naturais.

## Navegação, régua, compasso e formas

- **Roda do mouse:** zoom. **Ctrl + roda:** deslocamento vertical. **Alt + roda:** deslocamento horizontal.
- Seleção por área da direita para a esquerda inclui objetos com contato parcial; da esquerda para a direita continua exigindo que estejam inteiramente dentro da área.
- Formas predefinidas mantêm o ponto inicial durante a criação. Círculos e elipses usam esse ponto como centro, evitando que a origem se desloque enquanto o tamanho é ajustado.
- A régua mantém o traço preso à borda até soltar, limita o desenho às extremidades e pode capturar um traço que já estava em andamento ao alcançar a borda, inclusive em movimentos rápidos.
- Botão central para inverter verticalmente as graduações da régua; controles de rotação posicionados perto de ambas as extremidades e controle separado de comprimento.
- Ângulos da régua normalizados e apresentados no sentido correto. Correção de deslocamentos inesperados ao clicar no canvas com a ferramenta ativa.
- Melhorias na troca entre instrumentos ao clicar neles e retorno à seleção com Escape.
- Compasso com proporções preservadas durante o zoom, medidas mais legíveis e controle de desenho afastado da ponta para facilitar o uso.
- **Ctrl** durante o ajuste da abertura do compasso encaixa o raio em milímetros inteiros.
- Gestos do compasso mais estáveis: menos saltos ao iniciar, ajustar ou interromper a interação, preservação da ponta e do raio e tratamento de movimentos ambíguos perto do centro.
- Corrigidas linhas temporárias distantes e artefatos ao desenhar arcos após traços longos, separando o estado do compasso do processamento de escrita livre.

## Desempenho e navegador Linux

- Desfazer e refazer objetos adicionados ficou mais eficiente, reduzindo o trabalho ao usar Ctrl+Z repetidamente.
- Reutilização da geometria de traços pontilhados e tracejados durante o desenho, preservando a continuidade dos padrões.
- Cache de textos sem alterações para evitar renderizações desnecessárias e melhorar a resposta em quadros com muitos elementos.
- Navegador lateral habilitado no Linux com **WebKitGTK 4.1**, executado em um processo auxiliar separado da interface Qt.
- Correções de foco e ativação para restaurar a exibição do cursor nos campos de texto do navegador.
- Tratamento de fontes locais problemáticas e limite de espera para reduzir travamentos durante o carregamento de páginas.
- Dependências e empacotamento Linux atualizados para WebKitGTK e XWayland, sem incluir um motor de navegador privado.
- Cobertura ampliada de testes para geometria, encaixes magnéticos, mouse e stylus, renderização, texto, persistência, interface e gravação/exportação.

## Observações de uso

- A gravação de construções fica na sessão e **não é salva no arquivo `.scalar`**. Exporte a animação antes de encerrar o aplicativo ou descartar a gravação.
- **Apagar passo** remove o estado selecionado da reprodução; não apaga objetos do quadro nem reescreve os estados posteriores.
- A exportação GIF/MP4 requer **FFmpeg disponível no PATH**.
- No Linux, o navegador lateral depende de WebKitGTK e usa X11/XWayland. No Windows, continua utilizando o WebView2 do sistema.

**Alterações completas:** https://github.com/jefersonapps/scalar/compare/v1.0.0...v1.1.0
