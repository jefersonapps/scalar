# Navegação rápida e exportação PDF

Use as setas no topo do editor para a página anterior/próxima. O contador indica a posição e abre as miniaturas. Em janelas estreitas, a navegação ocupa uma segunda linha do cabeçalho, acima do canvas. Ctrl+PgUp/PgDown continua disponível. Os controles nas extremidades ficam desabilitados e não iniciam traços no canvas.

O botão no topo com seta para cima abre “Exportar todas as páginas como PDF”. Escolha o destino: a exportação inclui todas as páginas em ordem, com tamanho físico individual, margens zero, fundo, grade, imagens e objetos visíveis. A operação roda em worker sobre um snapshot; alterações posteriores ficam para a próxima exportação. O status informa sucesso ou erro.

Traços com pressão e estilos, contornos e preenchimentos são paths vetoriais; texto comum usa fonte incorporada e permanece selecionável. Equações usam a geometria do SVG armazenado. Imagens permanecem raster. A base PDF importada é renderizada pelo Qt PDF, até 4096 px por dimensão / 16 MP; o original continua preservado no `.board`, mas não é copiado como conteúdo vetorial para o PDF de saída.

O writer produz um temporário e valida contagem de páginas e medidas com Qt PDF antes de substituir o destino via QSaveFile. Falhas deixam o arquivo anterior intacto. Não é uma captura da viewport; zoom, pan e posição dos painéis não interferem na saída.

Testes executados verificam PDF com A4 retrato, A4 paisagem e Carta; medidas físicas, contagem, texto selecionável, geometria sem imagens raster para documentos vetoriais, base PDF com anotação acima, exportação assíncrona e preservação do destino em falha. A navegação foi exercitada na UI e capturada no tema escuro em janela de 520 × 640. Testes offscreen não validam GPU, stylus física ou Windows.

Este fluxo exporta todas as páginas com configurações fixas. Intervalos, qualidade configurável, margens adicionais, desligar fundo/grade e exportação PNG continuam como evolução do Milestone 5.
