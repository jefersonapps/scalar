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
using namespace scalar;
class MathTests : public QObject {
    Q_OBJECT
private slots:
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
};
QTEST_MAIN(MathTests)
#include "math_tests.moc"
