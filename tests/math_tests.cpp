#include <QtTest>
#include <QGuiApplication>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QRawFont>
#include <QTextDocument>
#include "math/MathRenderer.h"
#include "math/SvgValidation.h"
#include "rendering/TextRenderer.h"
#include "rendering/MathMesh.h"
#include "geometry/Geometry.h"
#include "persistence/ProjectStore.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QtConcurrent>
using namespace scalar;
class MathTests : public QObject {
    Q_OBJECT
private slots:
    void plainTextKeepsCurrencyAndPersistsLatexChoice(){
        TextObject text;text.id=newId();text.source="Investiu R$ 8.000,00 e recebeu R$ 10.464,98. $x$";text.latexEnabled=false;
        const auto prepared=prepareText(text);QVERIFY2(prepared.error.isEmpty(),qPrintable(prepared.error));QVERIFY(prepared.object.math.empty());QVERIFY(prepared.geometry.empty());QCOMPARE(prepared.object.source,text.source);
        Project project;project.id=newId();project.name="Financeiro";Page page;page.id=newId();page.texts.push_back(prepared.object);project.pages.push_back(page);
        const auto loaded=ProjectStore::deserialize(ProjectStore::serialize(project));QVERIFY2(loaded,qPrintable(loaded.error));QVERIFY(!loaded.project.pages[0].texts[0].latexEnabled);
        auto json=QJsonDocument::fromJson(ProjectStore::serialize(project)).object();auto pages=json["pages"].toArray();auto first=pages[0].toObject();auto objects=first["objects"].toArray();
        // Older files omit the option and retain their LaTeX interpretation.
        auto object=objects[0].toObject();object.remove("latexEnabled");objects[0]=object;first["objects"]=objects;pages[0]=first;json["pages"]=pages;
        const auto legacy=ProjectStore::deserialize(QJsonDocument(json).toJson());QVERIFY2(legacy,qPrintable(legacy.error));QVERIFY(legacy.project.pages[0].texts[0].latexEnabled);
    }
    void bundledLatexFontSupportsStylesAndAccents(){
        const auto family=latexTextFont();QVERIFY(!family.isEmpty());
        for(const bool bold:{false,true})for(const bool italic:{false,true}){
            const auto raw=QRawFont::fromFont(documentFont(family,24,bold,italic));QVERIFY(raw.isValid());
            QVERIFY(raw.familyName().contains("Roman"));
            QCOMPARE(raw.weight()>=QFont::Bold,bold);QCOMPARE(raw.style()==QFont::StyleItalic,italic);
            for(const auto character:QString::fromUtf8("áàâãéêíóôõúüçÁÀÂÃÉÊÍÓÔÕÚÜÇ"))QVERIFY(raw.supportsCharacter(character));
        }
    }
    void installedFontsAndPortugueseAccents(){
        registerTextFonts();
        const auto font=documentFont("Lobster Two",24);
        const auto raw=QRawFont::fromFont(font);QVERIFY(raw.isValid());
        for(const auto character:QString::fromUtf8("áàâãéêíóôõúüçÁÀÂÃÉÊÍÓÔÕÚÜÇ"))QVERIFY(raw.supportsCharacter(character));
        QTextDocument document;document.setPlainText(QString::fromUtf8("Expressão: á ç ∫ Σ α 😊"));
        for(const auto& family:QFontDatabase::families()){
            document.setDefaultFont(documentFont(family,24));QVERIFY(document.size().height()>0);
            QVERIFY(QFontMetricsF(document.defaultFont()).horizontalAdvance(family)>=0);
        }
    }
    void mixedEmphasisChangesRendering(){
        TextObject text;text.source="Aula de geometria";text.fontFamily="DejaVu Sans";
        const auto plain=textBitmap(text,96./25.4);
        text.formats={{0,4,true,false},{8,8,false,true}};
        const auto formatted=textBitmap(text,96./25.4);QVERIFY(formatted!=plain);
        const auto prepared=prepareText(text);QVERIFY(prepared.error.isEmpty());QCOMPARE(prepared.object.formats,text.formats);
    }
    void delimiters(){
        const auto parts=mathFragments("Área $x^2$ e $$\\frac{a}{b}$$; custo \\$5. Sem $fim");
        QCOMPARE(parts.size(),std::size_t(2));QCOMPARE(parts[0].latex,std::string("x^2"));QVERIFY(!parts[0].display);
        QCOMPARE(parts[1].latex,std::string("\\frac{a}{b}"));QVERIFY(parts[1].display);
        QVERIFY(mathFragments("Sem matemática e \\$3").empty());
    }
    void svgValidation(){
        QVERIFY(validMathSvg("<svg xmlns=\"http://www.w3.org/2000/svg\"><g><path d=\"M0 0L1 1\"/></g></svg>"));
        QVERIFY(!validMathSvg("<svg><script/></svg>"));QVERIFY(!validMathSvg("<svg><use href=\"https://example.org/x.svg\"/></svg>"));
        QVERIFY(!validMathSvg("<!DOCTYPE svg SYSTEM 'file:///etc/passwd'><svg/>"));QVERIFY(!validMathSvg("<svg><g>"));
    }
    void plainText(){
        TextObject t;t.id=newId();t.source="Aula de geometria\nÁrea do círculo";t.fontSizePt=20;t.bold=true;
        const auto prepared=prepareText(t);QVERIFY2(prepared.error.isEmpty(),qPrintable(prepared.error));QVERIFY(!prepared.image.isNull());QCOMPARE(prepared.object.corners.size(),std::size_t(4));
        bool ink=false;for(int y=0;y<prepared.image.height();++y)for(int x=0;x<prepared.image.width();++x)if(qAlpha(prepared.image.pixel(x,y)))ink=true;QVERIFY(ink);
        const auto small=textBitmap(prepared.object,2),large=textBitmap(prepared.object,8);QVERIFY(large.width()>small.width());QVERIFY(large.height()>small.height());
    }
    void storedSvgRendersOffline(){
        if(!svgRenderingAvailable())QSKIP("SVG renderer unavailable");
        TextObject t;t.source="$x$";t.style.rgba=0xcc5364ff;t.math={{"x","<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1000 1000\"><path fill=\"currentColor\" d=\"M0 0H1000V1000H0Z\"/></svg>",false,0,3,1,1}};
        const auto image=textBitmap(t,4);QVERIFY(!image.isNull());bool colored=false;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).red()>100&&image.pixelColor(x,y).alpha()>100)colored=true;QVERIFY(colored);
    }
    void drawnTextBoxWrapsAndKeepsDimensions(){
        TextObject t;t.source="Uma aula de geometria com exemplos e construções para os alunos.";
        t.fontFamily="DejaVu Sans";t.boxWidthMm=40;t.boxHeightMm=35;t.corners={{12,17}};
        const auto prepared=prepareText(t);QVERIFY2(prepared.error.isEmpty(),qPrintable(prepared.error));
        QCOMPARE(prepared.object.corners[0].x,12.);QCOMPARE(prepared.object.corners[0].y,17.);
        QVERIFY(std::abs(length(prepared.object.corners[1]-prepared.object.corners[0])-40)<1e-8);
        QVERIFY(length(prepared.object.corners[3]-prepared.object.corners[0])>=35);
        t.boxWidthMm=100;t.boxHeightMm=0;
        const auto wide=prepareText(t);QVERIFY(wide.error.isEmpty());
        QVERIFY(wide.naturalSize.height()<prepared.naturalSize.height());
    }
    void vectorMeshHolesAndTransforms(){
        const auto result=svgMathMesh("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1000 1000\"><g transform=\"translate(100 100) scale(2)\"><path d=\"M0 0H300V300H0Z M100 100V200H200V100H100Z\"/></g></svg>");
        QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(!result.vertices.empty());
        const auto covered=[&](Point p){const auto cross=[](Point a,Point b){return a.x*b.y-a.y*b.x;};for(std::size_t i=0;i+2<result.vertices.size();i+=3){const auto a=result.vertices[i],b=result.vertices[i+1],c=result.vertices[i+2];const double x=cross(b-a,p-a),y=cross(c-b,p-b),z=cross(a-c,p-c);if((x>=0&&y>=0&&z>=0)||(x<=0&&y<=0&&z<=0))return true;}return false;};
        QVERIFY(covered({.2,.2}));QVERIFY(!covered({.4,.4}));QVERIFY(!covered({.8,.8}));
    }
    void realMathJax(){
        if(!QFile::exists(mathRuntimeDirectory()+"/node_modules/mathjax-full/js/mathjax.js"))QSKIP("MathJax package unavailable: restricted network; real conversion remains unverified");
        auto parts=mathFragments("$x^2$ $$\\frac{a}{b}+\\sqrt{x}$$");const auto error=renderMath(parts);QVERIFY2(error.isEmpty(),qPrintable(error));
        for(const auto& p:parts){QVERIFY(validMathSvg(p.svg));QVERIFY(p.widthEm>0);QVERIFY(p.heightEm>0);const auto mesh=svgMathMesh(p.svg);QVERIFY2(mesh.error.isEmpty(),qPrintable(mesh.error));QVERIFY(!mesh.vertices.empty());}
        TextObject equation;equation.source="$$\\int_a^b f(x)dx$$";const auto vectorEquation=prepareText(equation);QVERIFY2(vectorEquation.error.isEmpty(),qPrintable(vectorEquation.error));QVERIFY(!vectorEquation.geometry.empty());
        for(int y=0;y<vectorEquation.image.height();++y)for(int x=0;x<vectorEquation.image.width();++x)QVERIFY(qAlpha(vectorEquation.image.pixel(x,y))==0);
        TextObject t;t.source="Área $\\pi r^2$";const auto prepared=prepareText(t);QVERIFY2(prepared.error.isEmpty(),qPrintable(prepared.error));QVERIFY(!prepared.image.isNull());
    }
    void mathRuntimeReusesProcessAndRecoversFromInvalidLatex(){
        if(!QFile::exists(mathRuntimeDirectory()+"/node_modules/mathjax-full/js/mathjax.js"))QSKIP("MathJax package unavailable");
        auto first=mathFragments("$x^{17}$");QElapsedTimer timer;timer.start();
        auto error=renderMath(first);QVERIFY2(error.isEmpty(),qPrintable(error));const auto cold=timer.elapsed();
        auto second=mathFragments("$$x^{18}$$");timer.restart();
        error=renderMath(second);QVERIFY2(error.isEmpty(),qPrintable(error));const auto warm=timer.elapsed();
        auto repeated=mathFragments("Texto diferente: $x^{17}$");timer.restart();
        error=renderMath(repeated);QVERIFY2(error.isEmpty(),qPrintable(error));const auto cached=timer.elapsed();
        QCOMPARE(repeated[0].svg,first[0].svg);QVERIFY(repeated[0].start!=first[0].start);
        QVERIFY(second[0].display);
        auto invalid=mathFragments("$\\frac{$");QVERIFY(!renderMath(invalid).isEmpty());
        auto macro=mathFragments("$\\newcommand{\\scalarprobe}{x}\\scalarprobe$");
        error=renderMath(macro);QVERIFY2(error.isEmpty(),qPrintable(error));
        auto isolated=mathFragments("$\\scalarprobe$");error=renderMath(isolated);QVERIFY2(error.isEmpty(),qPrintable(error));
        // MathJax's noundefined package displays unknown commands literally.
        QVERIFY(isolated[0].widthEm>macro[0].widthEm);
        auto future=QtConcurrent::run([]{auto parts=mathFragments("$y^{19}$");return renderMath(parts);});
        auto recovered=mathFragments("$z^{20}$");error=renderMath(recovered);QVERIFY2(error.isEmpty(),qPrintable(error));
        future.waitForFinished();QVERIFY2(future.result().isEmpty(),qPrintable(future.result()));
        TextObject text;text.source="$x^{17}$";timer.restart();const auto prepared=prepareText(text);
        QVERIFY2(prepared.error.isEmpty(),qPrintable(prepared.error));QVERIFY(!prepared.geometry.empty());
        qInfo("Math latency: first=%lld ms, new formula=%lld ms, cached=%lld ms, full cached text preparation=%lld ms",cold,warm,cached,timer.elapsed());
    }
};
QTEST_MAIN(MathTests)
#include "math_tests.moc"
