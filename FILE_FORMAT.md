# Formato .board — versão 2 (leitura da versão 1)

`.board` é ZIP padrão, não JSON renomeado. Perfil inicial: uma única entrada `project.json`, método STORE sem compressão, sem encryption, sem ZIP64. Cabeçalhos local/central, EOCD, nome, tamanhos e CRC-32 são validados. O leitor recusa perfis diferentes. Para manter interoperabilidade no futuro, uma versão nova deverá expandir o leitor antes de adicionar entries assets/pages/thumbnails ou compressão. Pastas vazias não são gravadas.

```json
{
  "format": "scalar.board",
  "version": 2,
  "units": "mm",
  "id": "project-id",
  "name": "Aula de geometria",
  "createdAt": "2026-10-03T12:00:00.000Z",
  "updatedAt": "2026-10-03T12:01:00.000Z",
  "pages": [{
    "id": "page-id",
    "widthMm": 210,
    "heightMm": 297,
    "background": 4294967295,
    "objects": [{
      "type": "stroke",
      "id": "stroke-id",
      "style": {
        "rgba": 640894463,
        "minWidthMm": 0.15,
        "maxWidthMm": 0.85,
        "gamma": 1.2,
        "sensitivity": 1
      },
      "samples": [[10, 20, 0.7, 0, 0, 0, 1234, 1, 1]],
      "zIndex": 0,
      "locked": false,
      "visible": true
    }]
  }]
}
```

Amostra compacta: `[xMm,yMm,pressure,tiltX,tiltY,rotation,timestamp,buttons,device]`. Device: 0 mouse, 1 stylus, 2 touch. Timestamp é o relógio de eventos do dispositivo/Qt em ms, não data civil. RGBA é inteiro sem sinal `0xRRGGBBAA`, serializado em número JSON. UTF-8 preserva nomes em português. IDs são únicos no documento; pontos não ocupam linhas SQLite individuais.

Limites: arquivo 128 MiB, 1000 páginas, 2 milhões de amostras, tamanho físico 10–5000 mm por dimensão. Campos numéricos precisam ser finitos; pressão 0–1; larguras positivas e consistentes; versão desconhecida é recusada. Esses limites protegem parsing/importação, sem prometer performance em projetos próximos deles.

## Salvamento

Snapshot recebe datas; JSON é gerado e validado em worker. Bytes ZIP são gerados com CRC. QSaveFile escreve em arquivo temporário e `commit()` faz substituição atômica. Direct write fallback está desabilitado: se não houver substituição segura, retorna erro e mantém o documento dirty. A durabilidade em power loss depende de filesystem/OS; não é uma garantia de proteção contra falha física de disco.

## Arquivos auxiliares

- AppDataLocation/projects/<id>.board: projetos gerenciados.
- <projeto>.board.png: thumbnail secundário, regenerável, fora do ZIP no ciclo inicial.
- AppDataLocation/library.sqlite: recentes, preferências e marcador de sessão.
- AppDataLocation/session.board: snapshot de recuperação.

Se o aplicativo terminar sem shutdown normal, cleanShutdown=false e session.board disponível oferecem recuperação na próxima abertura. Dados posteriores ao último snapshot concluído podem ser perdidos; stroke ainda ativo só entra no documento no commit. SQLite não é o arquivo portátil do projeto. Copiar o `.board` basta para transferir seus traços e páginas; histórico de undo e preferências locais não viajam com ele.

## Objetos da versão 2

O writer produz v2; o reader aceita v1/v2. Arquivos v1 com strokes recebem zIndex na ordem original e defaults locked=false/visible=true. Abrir não regrava; após alteração/save, o arquivo migra para v2. Aplicativos Scalar 0.1 não leem v2.

Shape: type=shape, kind=0 linha/1 círculo/2 elipse/3 triângulo/4 retângulo/5 quadrado/6 polígono genérico, style igual ao stroke, vertices (2/3/4 pontos, 3–2048 para polígono genérico, ou vazio para curvas), center, radiusX/radiusY em mm, rotation em radianos e fillOpacity 0–1. Circle exige raios iguais. Propriedades comuns: zIndex, locked e visible. Revision é transient e não viaja no JSON.

Image: type=image, corners com quatro posições em mm, pixelWidth/pixelHeight, png em base64 e propriedades comuns. O backing é PNG incorporado e normalizado na importação, incluindo transparência. O limite por imagem é 32 MiB codificados, 8192 px por lado e 32 MP; o limite do arquivo inteiro de 128 MiB continua valendo, incluindo overhead base64. Load confere as dimensões reais antes de decodificar. Não depende do arquivo de origem nem da área de transferência depois de salvar. Este ciclo usa uma entrada ZIP única; assets binários separados e compressão ficam para revisão futura do formato.

O estilo aceita `pattern`: 0 contínuo, 1 tracejado, 2 pontilhado. `dashLengthMm`, `gapLengthMm` e `dotSpacingMm` guardam espaçamentos físicos positivos (padrões 3, 2 e 2,5 mm). Campos são opcionais para leitura de projetos antigos, cujo contorno permanece contínuo. O padrão percorre o comprimento do caminho vetorial e acompanha zoom, transformação, thumbnails e undo/redo.

Formas aceitam `fillRgba` opcional (inteiro `0xRRGGBBAA` ou null). Ausente/null preserva a aparência de projetos antigos, seguindo a cor do contorno. Uma cor explícita de preenchimento permanece independente de `style.rgba`; `fillOpacity` controla sua transparência. Polígonos genéricos preservam os vértices em ordem e podem ser côncavos; preenchimento por triangulação e vértices editáveis.
