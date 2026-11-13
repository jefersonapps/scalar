# Scalar 1.2.0 — Traços ajustáveis e configurações globais

O Scalar 1.2.0 permite corrigir o comprimento de traços guiados enquanto você desenha e facilita o acesso às preferências do aplicativo. A versão também aprimora o espaçamento dos controles e a distribuição portátil para Linux.

## Encurtar traço ao voltar

- Nova opção **Encurtar traço ao voltar**: ao desenhar pela régua ou pelo arco do transferidor, volte sobre o traço sem levantar a ponta para remover o trecho excedente.
- Avançar novamente estende o traço, permitindo ajustar seu comprimento durante o mesmo gesto.
- Funciona com mouse ou stylus e mantém o resultado como um único traço no histórico de desfazer/refazer.
- Disponível nas configurações gerais e no painel da caneta, com os dois controles sincronizados.
- A preferência começa desativada, fica salva no dispositivo e vale para todos os quadros.

## Configurações na sidebar

- Novo item **Configurações** na sidebar da tela inicial, identificado por uma engrenagem arredondada.
- As preferências abrem no painel à direita, seguindo a navegação da biblioteca.
- Opções organizadas por aparência, escrita e instrumentos, reconhecimento de formas, texto e equações e padrões de novos quadros.
- Em janelas estreitas, onde a sidebar fica oculta, o acesso permanece disponível por um botão com o texto **Configurações**.

## Controles mais claros

- Separador antes de **Curva de pressão** no painel da caneta.
- Mais espaço entre as seções de configurações, especialmente entre o controle de tempo do reconhecimento e a seção seguinte.
- Margens mais equilibradas nos dropdowns, com maior recuo do texto e alinhamento da seta e das opções.

## AppImage para Linux

- Script de geração do AppImage com base Debian 12 e inclusão de Qt/QML, Node/MathJax e navegador integrado WebKitGTK.
- Empacotamento dos processos auxiliares do navegador e verificações de dependências, matemática offline, abertura da interface e carregamento de página com JavaScript.
- Ícone preparado no tamanho aceito pelo empacotador e testes de abertura usando X11 virtual no contêiner.
- Inicialização ajustada para preferir a biblioteca C++ do sistema quando compatível, evitando conflitos entre o runtime incluído e drivers Mesa/LLVM mais recentes.
- O alvo de compatibilidade é Linux x86_64 com **glibc 2.36 ou superior**, como Debian 12+, Ubuntu 24.04+ e Mint 22+. O sistema ainda fornece os drivers gráficos e X11/XWayland; o navegador exige suporte a namespaces de usuário.

## Downloads

- **Windows:** `Scalar-1.2.0-win64.exe` (instalador) ou `Scalar-1.2.0-win64.zip` (portátil).
- **Debian, Ubuntu e Linux Mint:** pacote `.deb` da versão 1.2.0.
- **Linux portátil:** `Scalar-1.2.0-x86_64.AppImage`. Dê permissão de execução antes de abrir, se necessário.

Os quadros existentes continuam utilizáveis. Consulte o [guia de compilação e empacotamento](../BUILDING.md) para gerar os arquivos da nova versão e o [guia de uso](USAGE.md) para conhecer as opções.

**Alterações completas:** https://github.com/jefersonapps/scalar/compare/v1.1.0...v1.2.0
