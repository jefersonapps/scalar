#include <QtTest>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPdfDocument>
#include <QPdfSelection>
#include "export/PdfExporter.h"
#include "rendering/TextRenderer.h"
#include "rendering/PageRenderer.h"
#include "pdf/PdfService.h"
#include "pdf/PdfRenderCache.h"
#include "app/AppController.h"
#include "persistence/ProjectStore.h"
#include "persistence/Thumbnail.h"
#include "geometry/Geometry.h"
using namespace scalar;
static bool makePdf(const QString& path){
    QPdfWriter writer(path);writer.setResolution(72);writer.setPageMargins(QMarginsF(0,0,0,0));writer.setPageSize(QPageSize(QSizeF(210,297),QPageSize::Millimeter));
    QPainter painter(&writer);if(!painter.isActive())return false;
    painter.fillRect(QRectF(20,20,100,80),Qt::red);
    writer.setPageSize(QPageSize(QSizeF(297,210),QPageSize::Millimeter));if(!writer.newPage())return false;painter.fillRect(QRectF(20,20,100,80),Qt::blue);
    writer.setPageSize(QPageSize(QPageSize::Letter));if(!writer.newPage())return false;painter.fillRect(QRectF(20,20,100,80),Qt::green);return painter.end();
}
class PdfTests : public QObject {
    Q_OBJECT
private slots:
    void exportPhysicalPagesAndVectorContent(){
        QTemporaryDir dir;Project project;project.id=newId();project.name="Physical pages";
        for(const auto size:std::vector<PageSize>{{210,297},{297,210},{215.9,279.4}}){Page page;page.id=newId();page.size=size;project.pages.push_back(page);}
        auto& page=project.pages[0];StrokeObject stroke;stroke.id=newId();stroke.style.rgba=0xff0000ff;stroke.samples={{{20,30},1},{{60,30},1}};page.strokes.push_back(stroke);
        ShapeObject shape;shape.id=newId();shape.kind=ShapeKind::Circle;shape.center={100,100};shape.radiusX=shape.radiusY=10;shape.style.rgba=0x0000ffff;shape.fillOpacity=1;page.shapes.push_back(shape);
        TextObject text;text.id=newId();text.source="Scalar vector text";text.corners={{20,50},{21,50},{21,51},{20,51}};const auto prepared=prepareText(text);QVERIFY2(prepared.error.isEmpty(),qPrintable(prepared.error));page.texts.push_back(prepared.object);
        TextObject math;math.id=newId();math.source="$x$";math.corners={{30,70},{40,70},{40,80},{30,80}};math.math={{"x","<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1000 1000\"><path d=\"M0 0H1000V1000H0Z\"/></svg>",false,0,3,1,1}};page.texts.push_back(math);
        const auto path=dir.filePath("all-pages.pdf");const auto error=exportProjectPdf(project,path);QVERIFY2(error.isEmpty(),qPrintable(error));
        QPdfDocument pdf(nullptr);QCOMPARE(pdf.load(path),QPdfDocument::Error::None);QCOMPARE(pdf.pageCount(),3);
        for(int i=0;i<3;++i){const auto size=pdf.pagePointSize(i);QVERIFY(std::abs(size.width()*25.4/72-project.pages[i].size.widthMm)<0.4);QVERIFY(std::abs(size.height()*25.4/72-project.pages[i].size.heightMm)<0.4);}
        QVERIFY(pdf.getAllText(0).text().contains("Scalar vector text"));const auto image=pdf.render(0,{420,594});QVERIFY(image.pixelColor(200,200).blue()>200);QVERIFY(image.pixelColor(200,200).red()<50);QVERIFY(image.pixelColor(80,60).red()>200);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(!file.readAll().contains("/Subtype /Image"));
        QVERIFY(!exportProjectPdf(project,dir.filePath("missing-directory/fail.pdf")).isEmpty());
    }
    void exportInfinitePageFitsAllContent(){
        QTemporaryDir dir;Project project;project.id=newId();project.name="Infinite";Page page;page.id=newId();page.size.infinite=true;
        StrokeObject a;a.id=newId();a.style.rgba=0xff0000ff;a.style.minWidthMm=a.style.maxWidthMm=2;a.samples={{{-100,-50},1},{{-80,-50},1}};
        auto b=a;b.id=newId();b.style.rgba=0x00ff00ff;b.samples={{{400,300},1},{{420,300},1}};
        auto hidden=a;hidden.id=newId();hidden.properties.visible=false;hidden.samples={{{100000,100000},1},{{100001,100001},1}};
        page.strokes={a,b,hidden};auto erased=hidden;erased.id=newId();erased.properties.visible=true;page.erasedInk={erased};project.pages={page};
        const auto area=pageRenderBounds(page);QCOMPARE(area.left,-111.);QCOMPARE(area.top,-61.);QCOMPARE(area.width(),542.);QCOMPARE(area.height(),372.);
        const auto path=dir.filePath("infinite.pdf");const auto error=exportProjectPdf(project,path);QVERIFY2(error.isEmpty(),qPrintable(error));
        QPdfDocument pdf(nullptr);QCOMPARE(pdf.load(path),QPdfDocument::Error::None);QCOMPARE(pdf.pageCount(),1);
        const auto physical=pdf.pagePointSize(0)*25.4/72;QVERIFY(std::abs(physical.width()-542)<.5);QVERIFY(std::abs(physical.height()-372)<.5);
        const auto image=pdf.render(0,{1084,744});QVERIFY(!image.isNull());
        QVERIFY(image.pixelColor(42,22).red()>200&&image.pixelColor(42,22).green()<50);
        QVERIFY(image.pixelColor(1042,722).green()>200&&image.pixelColor(1042,722).red()<50);
        QCOMPARE(image.pixelColor(2,2),QColor(Qt::white));QCOMPARE(project.pages[0].strokes[0].samples[0].position.x,-100.);
        const auto stored=ProjectStore::deserialize(ProjectStore::serialize(project));QVERIFY2(stored,qPrintable(stored.error));QVERIFY(stored.project.pages[0].size.infinite);
        auto malformed=QJsonDocument::fromJson(ProjectStore::serialize(project)).object();auto pages=malformed["pages"].toArray();auto first=pages[0].toObject();first["infinite"]="yes";pages[0]=first;malformed["pages"]=pages;QVERIFY(!ProjectStore::deserialize(QJsonDocument(malformed).toJson()));
        project.pages={Page{}};project.pages[0].id=newId();project.pages[0].size.infinite=true;
        QVERIFY(exportProjectPdf(project,dir.filePath("empty.pdf")).isEmpty());
        pdf.close();project.pages={page};project.pages[0].strokes[1].samples={{{20000,300},1},{{20020,300},1}};
        const auto largeError=exportProjectPdf(project,dir.filePath("large.pdf"));QVERIFY2(largeError.isEmpty(),qPrintable(largeError));
        QCOMPARE(pdf.load(dir.filePath("large.pdf")),QPdfDocument::Error::None);QVERIFY(std::abs(pdf.pagePointSize(0).width()*25.4/72-5000)<.5);
        const auto thumbnail=pageThumbnail(page,{640,400});QVERIFY(!thumbnail.isNull());
    }
    void exportImportedPdfAndAtomicFailure(){
        QTemporaryDir dir;const auto source=dir.filePath("source.pdf");QVERIFY(makePdf(source));const auto imported=inspectPdf(source);QVERIFY(imported);
        Project project;project.id=newId();Page page;page.id=newId();page.size=imported.sizes[0];page.pdf=PdfPageObject{imported.assetId,imported.data,0,3};project.pages.push_back(page);
        // Keep the sampled pixel inside the stroke, independently of the pen's UI default.
        StrokeObject stroke;stroke.id=newId();stroke.style.rgba=0x00ff00ff;stroke.style.minWidthMm=stroke.style.maxWidthMm=1;stroke.samples={{{20,30},1},{{60,30},1}};project.pages[0].strokes.push_back(stroke);
        QImage bitmap(20,20,QImage::Format_ARGB32);bitmap.fill(QColor("#ffcc00"));const auto importedImage=encodeImage(bitmap,{100,120},page.size);QVERIFY(importedImage.error.isEmpty());project.pages[0].images.push_back(importedImage.object);
        const auto destination=dir.filePath("annotated.pdf");QVERIFY(exportProjectPdf(project,destination).isEmpty());
        QPdfDocument pdf(nullptr);QCOMPARE(pdf.load(destination),QPdfDocument::Error::None);const auto image=pdf.render(0,{420,594});QVERIFY(image.pixelColor(80,60).green()>200);QVERIFY(image.pixelColor(40,40).red()>200);QVERIFY(image.pixelColor(200,240).red()>200);QVERIFY(image.pixelColor(200,240).blue()<50);pdf.close();
        QFile file(destination);QVERIFY(file.open(QIODevice::ReadOnly));const auto before=file.readAll();file.close();
        project.pages[0].pdf->pageIndex=99;QVERIFY(!exportProjectPdf(project,destination).isEmpty());QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),before);
        AppController controller(nullptr,dir.filePath("data"));controller.newDefault();controller.addPage();controller.exportPdf(QUrl::fromLocalFile(dir.filePath("async.pdf")));QTRY_VERIFY(!controller.exporting());QCOMPARE(pdf.load(dir.filePath("async.pdf")),QPdfDocument::Error::None);QCOMPARE(pdf.pageCount(),2);QVERIFY(controller.shutdown());
    }
    void ranges(){
        QString error;QCOMPARE(pdfPageRange("",3,error),(std::vector<int>{0,1,2}));QVERIFY(error.isEmpty());
        QCOMPARE(pdfPageRange("3, 1-2, 2",3,error),(std::vector<int>{2,0,1}));QVERIFY(error.isEmpty());
        for(const auto& range:QStringList{"0","4","2-1","1,","hello","1-9999999999"}){QVERIFY(pdfPageRange(range,3,error).empty());QVERIFY(!error.isEmpty());}
    }
    void inspectAndRender(){
        QTemporaryDir dir;const auto path=dir.filePath("lesson.pdf");QVERIFY(makePdf(path));const auto pdf=inspectPdf(path);QVERIFY2(bool(pdf),qPrintable(pdf.error));QCOMPARE(pdf.sizes.size(),std::size_t(3));
        const auto near=[](double a,double b){return std::abs(a-b)<0.4;};QVERIFY(near(pdf.sizes[0].widthMm,210));QVERIFY(near(pdf.sizes[0].heightMm,297));QVERIFY(near(pdf.sizes[1].widthMm,297));QVERIFY(near(pdf.sizes[1].heightMm,210));QVERIFY(near(pdf.sizes[2].widthMm,215.9));QVERIFY(near(pdf.sizes[2].heightMm,279.4));
        for(int i=0;i<3;++i){QString error;const auto bitmap=renderPdf({pdf.assetId,pdf.data,i,3},{600,600},&error);QVERIFY2(!bitmap.isNull(),qPrintable(error));const auto c=bitmap.pixelColor(40,50);if(i==0)QVERIFY(c.red()>200&&c.blue()<50);if(i==1)QVERIFY(c.blue()>200&&c.red()<50);if(i==2)QVERIFY(c.green()>100&&c.red()<50);}
        QString error;QVERIFY(renderPdf({pdf.assetId,pdf.data,99,3},{100,100},&error).isNull());QVERIFY(!error.isEmpty());QVERIFY(!inspectPdf(dir.filePath("missing.pdf")));
    }
    void importAnnotateSaveAndReopen(){
        QTemporaryDir dir;const auto source=dir.filePath("original.pdf");QVERIFY(makePdf(source));AppController controller(nullptr,dir.filePath("data"));
        controller.inspectPdfFile(QUrl::fromLocalFile(source));QTRY_VERIFY(!controller.pdfBusy());QCOMPARE(controller.pdfPageCount(),3);
        controller.importPdfPages("9");QVERIFY(!controller.pdfError().isEmpty());controller.importPdfPages("1-3");QCOMPARE(controller.pageCount(),3);QCOMPARE(controller.currentPage(),0);
        StrokeObject s;s.id=newId();s.samples={{{12,15},0.3},{{45,35},1}};controller.addStroke(s);
        controller.selectPage(1);QVERIFY(controller.page()->strokes.empty());QVERIFY(!controller.canUndo());controller.addStroke(s={newId(),{},{{{20,20},1},{{50,60},1}}});controller.undo();QVERIFY(controller.page()->strokes.empty());controller.redo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        controller.selectPage(0);QCOMPARE(controller.page()->strokes.size(),std::size_t(1));controller.undo();QVERIFY(controller.page()->strokes.empty());controller.redo();
        controller.refreshPdf(4);QTRY_VERIFY(!controller.pdfImage().isNull());const auto originalSize=controller.pdfImage().size();controller.refreshPdf(8);QTRY_VERIFY(controller.pdfImage().width()>originalSize.width());
        const auto board=dir.filePath("saved.board");controller.saveAs(QUrl::fromLocalFile(board));QTRY_VERIFY(!controller.dirty());const auto stored=ProjectStore::load(board);QVERIFY2(bool(stored),qPrintable(stored.error));QCOMPARE(stored.project.pages.size(),std::size_t(3));
        QVERIFY(stored.project.pages[0].pdf->data==stored.project.pages[1].pdf->data);QCOMPARE(*stored.project.pages[0].pdf->data,*controller.page()->pdf->data);
        const auto json=QJsonDocument::fromJson(ProjectStore::serialize(stored.project)).object();QCOMPARE(json["pdfAssets"].toObject().size(),1);
        QVERIFY(QFile::remove(source));controller.openPath(board);QTRY_VERIFY(!controller.loading());controller.refreshPdf(4);QTRY_VERIFY(!controller.pdfImage().isNull());QCOMPARE(controller.pageCount(),3);QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        const auto thumbnail=pageThumbnail(*controller.page(),{192,160});QVERIFY(!thumbnail.isNull());QTRY_VERIFY(!controller.pages()[0].toMap()["thumbnail"].toString().isEmpty());QVERIFY(controller.shutdown());
    }
    void boardColorDoesNotChangePdfPaper(){
        QTemporaryDir dir;const auto source=dir.filePath("transparent-paper.pdf");QVERIFY(makePdf(source));const auto info=inspectPdf(source);QVERIFY(info);
        Page page;page.id=newId();page.size={420,500};page.pdf=PdfPageObject{info.assetId,info.data,0,3,info.sizes[0]};
        const auto paint=[&](std::uint32_t background){page.background=background;QImage image(840,1000,QImage::Format_ARGB32_Premultiplied);QPainter painter(&image);painter.scale(2,2);const auto error=paintPage(painter,page,2);painter.end();if(!error.isEmpty())return QImage{};return image;};
        const auto white=paint(0xffffffff),black=paint(0x000000ff),blue=paint(0x397ce0ff);QVERIFY(!white.isNull());QVERIFY(!black.isNull());QVERIFY(!blue.isNull());
        const QRect pdfArea(0,0,int(info.sizes[0].widthMm*2)-1,int(info.sizes[0].heightMm*2)-1);
        QCOMPARE(white.copy(pdfArea),black.copy(pdfArea));QCOMPARE(white.copy(pdfArea),blue.copy(pdfArea));
        QCOMPARE(white.pixelColor(700,100),QColor(Qt::white));QCOMPARE(black.pixelColor(700,100),QColor(Qt::black));QCOMPARE(blue.pixelColor(700,100),QColor("#397ce0"));
        const auto bitmap=renderPdf(*page.pdf,{420,594});QVERIFY(!bitmap.isNull());QCOMPARE(bitmap.pixelColor(300,300),QColor(Qt::white));QVERIFY(!bitmap.hasAlphaChannel());
    }
    void resizingKeepsPdfPhysicalSize(){
        QTemporaryDir dir;const auto source=dir.filePath("original.pdf");QVERIFY(makePdf(source));
        AppController controller(nullptr,dir.filePath("data"));controller.inspectPdfFile(QUrl::fromLocalFile(source));QTRY_VERIFY(!controller.pdfBusy());controller.importPdfPages("1");
        const auto original=controller.page()->pdf->size;QVERIFY(original.valid());
        controller.refreshPdf(4);QTRY_VERIFY(!controller.pdfImage().isNull());const auto bitmapSize=controller.pdfImage().size();
        QVERIFY(controller.setPageSize("A4",0,0,false));QVERIFY(controller.setPageSize("Personalizado",420,500,false));
        controller.refreshPdf(4);QCOMPARE(controller.pdfImage().size(),bitmapSize);
        QCOMPARE(controller.page()->pdf->size.widthMm,original.widthMm);QCOMPARE(controller.page()->pdf->size.heightMm,original.heightMm);
        QImage image(1680,2000,QImage::Format_ARGB32_Premultiplied);QPainter painter(&image);painter.scale(4,4);QVERIFY(paintPage(painter,*controller.page(),4).isEmpty());painter.end();
        QVERIFY(image.pixelColor(100,100).red()>200&&image.pixelColor(100,100).green()<30);
        QVERIFY(image.pixelColor(240,100).green()>200);QVERIFY(image.pixelColor(1000,100).green()>200);
        const auto board=dir.filePath("resized.board");controller.saveAs(QUrl::fromLocalFile(board));QTRY_VERIFY(!controller.dirty());
        const auto loaded=ProjectStore::load(board);QVERIFY2(loaded,qPrintable(loaded.error));QCOMPARE(loaded.project.pages[0].size.widthMm,420.);QCOMPARE(loaded.project.pages[0].pdf->size.widthMm,original.widthMm);
        controller.undo();QCOMPARE(controller.pageWidth(),210.);QCOMPARE(controller.page()->pdf->size.widthMm,original.widthMm);
        controller.redo();QCOMPARE(controller.pageWidth(),420.);QVERIFY(controller.shutdown());
    }
    void cacheSwitchAndBounds(){
        QTemporaryDir dir;const auto source=dir.filePath("cache.pdf");QVERIFY(makePdf(source));const auto pdf=inspectPdf(source);QVERIFY(pdf);
        PdfRenderCache cache;QSignalSpy changed(&cache,&PdfRenderCache::imageChanged);cache.request(PdfPageObject{pdf.assetId,pdf.data,0,3},pdf.sizes[0],1);cache.request(PdfPageObject{pdf.assetId,pdf.data,1,3},pdf.sizes[1],2);QTRY_VERIFY(!cache.image().isNull());
        const auto size=cache.image().size();QCOMPARE(size,pdfRenderSize(pdf.sizes[1],2));QVERIFY(cache.image().pixelColor(40,40).blue()>200);
        const auto rev=cache.revision();cache.request(PdfPageObject{pdf.assetId,pdf.data,1,3},pdf.sizes[1],2);QCOMPARE(cache.revision(),rev);
        const auto large=pdfRenderSize({5000,5000},10000);QVERIFY(large.width()<=4096);QVERIFY(qint64(large.width())*large.height()<=16000000);
        const auto wide=pdfRenderSize({5000,4770},32);QVERIFY(wide.width()<=4096);QVERIFY(qint64(wide.width())*wide.height()<=16000000);
        cache.clear();QVERIFY(cache.image().isNull());
    }
};
QTEST_MAIN(PdfTests)
#include "pdf_tests.moc"
