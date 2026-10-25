'use strict';
// Offline TeX -> self-contained SVG. No DOM, browser, network or external fonts.
const {mathjax} = require('mathjax-full/js/mathjax.js');
const {TeX} = require('mathjax-full/js/input/tex.js');
const {SVG} = require('mathjax-full/js/output/svg.js');
const {liteAdaptor} = require('mathjax-full/js/adaptors/liteAdaptor.js');
const {RegisterHTMLHandler} = require('mathjax-full/js/handlers/html.js');
const {AllPackages} = require('mathjax-full/js/input/tex/AllPackages.js');
const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);
function convert(input) {
    // Isolate user macros and equation numbering between requests.
    const document = mathjax.document('', {
        InputJax: new TeX({packages: AllPackages.filter(p => !['require', 'autoload', 'html'].includes(p)), maxBuffer: 32768}),
        OutputJax: new SVG({fontCache: 'none'})
    });
    return JSON.parse(input).map(({latex, display}) => {
        const node = document.convert(latex, {display, em: 16, ex: 8, containerWidth: 1280});
        const svg = adaptor.firstChild(node);
        if(adaptor.outerHTML(svg).includes('data-mjx-error')) throw Error('Invalid LaTeX');
        const viewBox = adaptor.getAttribute(svg, 'viewBox').split(/\s+/).map(Number);
        return {svg: adaptor.outerHTML(svg), widthEm: viewBox[2]/1000, heightEm: viewBox[3]/1000};
    });
}
const worker = process.argv.includes('--worker');
let input = '';
process.stdin.setEncoding('utf8');
function respond(line) {
    try {
        const result = convert(line);
        process.stdout.write(JSON.stringify(worker ? {result} : result) + (worker ? '\n' : ''));
    } catch(error) {
        if(worker) process.stdout.write(JSON.stringify({error: String(error.message)}) + '\n');
        else { process.stderr.write(String(error.message)); process.exitCode = 1; }
    }
}
process.stdin.on('data', chunk => {
    input += chunk;
    if(worker) {
        let end;
        while((end = input.indexOf('\n')) >= 0) {
            const line = input.slice(0, end); input = input.slice(end + 1);
            if(line.length > 131072) process.exit(2);
            respond(line);
        }
    }
    if(input.length > 131072) process.exit(2);
});
process.stdin.on('end', () => { if(!worker) respond(input); });
