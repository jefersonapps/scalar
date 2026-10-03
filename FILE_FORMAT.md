# Formato .board — versão 1

`.board` é ZIP padrão, não JSON renomeado. Perfil inicial: uma única entrada `project.json`, método STORE sem compressão, sem encryption, sem ZIP64. Cabeçalhos local/central, EOCD, nome, tamanhos e CRC-32 são validados. O leitor recusa perfis diferentes. Para manter interoperabilidade no futuro, uma versão nova deverá expandir o leitor antes de adicionar entries assets/pages/thumbnails ou compressão. Pastas vazias não são gravadas.

```json
{
  "format": "scalar.board",
  "version": 1,
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
      "samples": [[10, 20, 0.7, 0, 0, 0, 1234, 1, 1]]
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
