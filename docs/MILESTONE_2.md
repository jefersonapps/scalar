# Scalar 0.2 — formas, seleção, borracha e imagens

Escopo deste ciclo: M2, mais borracha por trecho e importação/clipboard/drop de imagens solicitados pelo usuário. Não foram antecipados PDF, matemática, régua ou compasso.

## Concluído em código e testes

- Reconhecimento local: linha, círculo, elipse, triângulo, retângulo, quadrado e polígonos genéricos (incluindo trapézios e formas côncavas), inclusive formas rotacionadas, quadrados/triângulos com lados curvos e início no meio de um lado nos testes. Simplificação em múltiplas escalas com limites de erro; curvas suaves são diferenciadas de polígonos com quinas.
- Hold configurável (padrão 500 ms), preview e ajuste do endpoint de linha antes de soltar. Worker QtConcurrent e invalidação de resposta antiga após cancel/release.
- Primeiro undo após reconhecimento restaura o stroke original; redo aplica a forma.
- Formas explícitas via toolbar: linha, círculo, elipse, triângulo, retângulo. Contornos contínuos, tracejados e pontilhados permanecem vetoriais; Shift durante o hold aplica tracejado à forma reconhecida. Padrões persistidos no .board e nas thumbnails.
- Seleção simples, múltipla com Shift e marquee. Move, escala proporcional, rotação; endpoints de linha, vértices de triângulo, centro/raios de curvas e vértices de polígonos genéricos.
- Painel contextual: cores independentes de contorno/preenchimento, largura, opacidade do preenchimento, conversão de stroke selecionado, duplicar e excluir.
- Borracha circular com divisão analítica dos strokes: segmentos fora da área ficam preservados. Preview na camada original, proteção dos trechos cobertos por imagens superiores (inclusive rotacionadas), tamanho configurável e undo/redo do gesto inteiro. Ao mover a imagem, o trecho exposto volta a ser apagável.
- Imagens por arquivo, bitmap/URL no clipboard (Ctrl+V) e drag/drop. Texturas, seleção/transformação e dados PNG incorporados ao .board.
- Configurações compactas com controles em linha, seletor de formas em grade e ícones redesenhados com traço arredondado consistente.
- Dark neutral/zinc inspirado no shadcn/ui; fundo da página independente. Caneta começa clara em página escura, sem recolorir strokes existentes.
- Formato v2 com leitura de v1 e imagens imutáveis compartilhadas entre snapshots/undo/duplicação.

## Validação

Build Release com GCC 13.3/Qt 6.4.2. CTest: 7/7 passaram — core, recognition, eraser, persistence, desktop, smoke_home, smoke_editor. Qt Test realiza input de mouse e eventos sintéticos de stylus e abre a UI real em offscreen; não é uma simulação isolada de funções. Foram verificados importação e drag/drop reais por eventos Qt, save/load de imagem e conversão de forma. Também foram verificados hold de quadrado com lado curvo, arraste contínuo de borracha com eventos espaçados, dois gestos consecutivos sem restaurar cortes anteriores e undo/redo. Também há testes da ordem real dos nós do Scene Graph durante o preview, proteção por imagens/movimento e importação WEBP com transparência. Shift foi testado antes do hold de linha e após o preview de quadrado, com undo/redo, save/load e seleção dos estilos pela UI. Configurações foram capturadas em dark/light e janela 520 × 640. Foram acrescentados testes de trapézio com lado curvo ao segurar, edição de vértice, cores independentes pela UI, undo/redo e save/load. Polígonos regulares de 5–12 lados, uma estrela de 24 vértices, figuras côncavas e área/recortes da triangulação são verificados no núcleo; formas com cruzamentos são rejeitadas. Runtime não emitiu warnings QML nos testes.

Correção da borracha: inicializar o gesto cancelava o estado `tabletActive_`, fazendo movimentos e soltura da stylus serem ignorados. O estado de contato agora é estabelecido após a inicialização da ferramenta, e a soltura confirma os fragmentos. Zoom/touch não interrompem um gesto de apagar.

Capturas reais em build/screenshots: editor-dark.png, settings-dark.png, milestone2-dark-images.png, além de Home/criação e opções da caneta. O renderer de meshes não aparece no backend software de offscreen; screenshots verificam UI/texturas, não qualidade GPU de traços/filled shapes. Usuário confirmou funcionamento da versão anterior em sua sessão Linux; a versão nova ainda precisa de revisão física.

Recognition e eraser também passaram com AddressSanitizer + UndefinedBehaviorSanitizer. LeakSanitizer foi desabilitado por incompatibilidade com ptrace do ambiente; vazamentos não foram validados por ele.

## Limites

Reconhecimento é heurístico: o número de vértices é variável, com reamostragem em 512 pontos. Polígonos simples convexos/côncavos são suportados; cruzamentos, rabiscos incompletos, múltiplas voltas e ruído podem impedir uma conversão; baixa confiança mantém stroke. Tolerâncias geométricas ainda não têm controles na UI, apenas ativação/tempo de hold. Não há snap angular nem snapping geométrico. Escala é proporcional para manter círculos/retângulos/imagens sem distorção; elipses têm handles de raios independentes. Não há painel numérico de raio/ângulo nesta versão.

Borracha por trecho atua nos strokes livres. Shapes/images usam seleção + Delete e não têm edição de pixels. Ponteira invertida de stylus não troca ferramenta automaticamente. Imagens são normalizadas em PNG raster; SVG depende do plugin Qt e perde editabilidade interna de paths ao ser importado como imagem. Clipboard de texto/objetos vetoriais ainda não implementado. Só uma imagem é importada por vez. WEBP usa o plugin Qt quando disponível, com alternativa opcional libwebp detectada pelo CMake.

Ainda pendentes: culling/índice espacial, benchmarks de 1000/5000/10000 strokes, GPU/HiDPI/multimonitor/Windows, stylus física, touch/trackpad e latência. Curvas leves no lado são mescladas: um vértice precisa de mudança de direção de aproximadamente 29°. Circunferências irregulares e elipses achatadas têm testes específicos; foram verificadas proporções até 40:1 e o gesto de hold na interface para círculo e elipse 10:1. A distinção entre muitos lados suaves e uma curva é heurística. O mesh ativo ainda é retesselado; borracha percorre strokes do documento. App offline, sem reconhecimento em nuvem.

## Uso

V selecionar; P caneta; E borracha; H mão. Clique novamente em E para raio da borracha, em P para propriedades da caneta. Formas e imagens possuem botões próprios. Ctrl+D duplica seleção; Delete remove; Ctrl+V importa imagem no centro da viewport. Segure após desenhar para reconhecer. Ajuste ativação/tempo nas configurações.

## Próximo ciclo

Revisar versão nova com caneta em sessão acelerada. Depois, M3: fundos/grades/presets, estilos de stroke, texto e LaTeX offline. PDF import/multipáginas/exportação seguem os milestones próprios; exportação precisa preservar dimensões físicas reais.
