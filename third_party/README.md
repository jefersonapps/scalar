# Dependências de matemática

MathJax 3.2.2: Apache-2.0, https://github.com/mathjax/MathJax-src/tree/3.2.2 . Node 22.23.3: distribuição oficial e licenças em NODE-LICENSE gerado pelo bootstrap; https://nodejs.org/en/blog/release/v22.23.3 . JS e runtime são baixados automaticamente na máquina de build/empacotamento e incorporados à instalação. O aplicativo usa apenas assets locais.

package.json/package-lock.json e tex-svg.cjs são versionados. node_modules, runtime binário e marcador de preparação não são versionados. O pacote instalado inclui os arquivos de licença dos módulos e do Node.
