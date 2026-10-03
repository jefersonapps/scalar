#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QImage>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QDir>
#include <QClipboard>
#include <QMimeData>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QTabletEvent>
#include <QPointingDevice>
#include <QSGTransformNode>
#include <memory>
#include <array>
#include <cmath>
#include "app/AppController.h"
#include "canvas/CanvasItem.h"
#include "clipboard/ImageImporter.h"
using namespace scalar;
QQuickItem* findVisualItem(QQuickItem* parent,const QString& name){
    if(parent->objectName()==name)return parent;
    for(auto* child:parent->childItems())if(auto* found=findVisualItem(child,name))return found;
    return nullptr;
}
class InspectableCanvas : public CanvasItem {
public:
    using CanvasItem::updatePaintNode;
    using CanvasItem::mousePressEvent;
    using CanvasItem::mouseReleaseEvent;
};
class DesktopTests : public QObject {
    Q_OBJECT
private slots:
    void liveBackgroundAndAdaptivePenColors(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);
        auto* pen=window->findChild<QObject*>("penOptions");QVERIFY(pen);QVERIFY(QMetaObject::invokeMethod(pen,"open"));QTRY_VERIFY(pen->property("opened").toBool());
        auto* palette=findVisualItem(window->contentItem(),"penPalette");QVERIFY(palette);
        auto* blue=findVisualItem(palette,"paletteColor_2");QVERIFY(blue);const auto lightInk=blue->property("swatch").value<QColor>();
        QVERIFY(QMetaObject::invokeMethod(pen,"close"));QTRY_VERIFY(!pen->property("visible").toBool());
        auto* background=window->findChild<QObject*>("backgroundOptions");QVERIFY(background);QVERIFY(QMetaObject::invokeMethod(background,"open"));QTRY_VERIFY(background->property("opened").toBool());
        auto* black=findVisualItem(window->contentItem(),"backgroundColor_9");QVERIFY(black);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,black->mapToScene(QPointF(black->width()/2,black->height()/2)).toPoint());
        QCOMPARE(controller.pageColor(),QColor("#000000"));QVERIFY(background->property("visible").toBool());controller.undo();QCOMPARE(controller.pageColor(),QColor("#ffffff"));controller.redo();QCOMPARE(controller.pageColor(),QColor("#000000"));
        auto* custom=findVisualItem(window->contentItem(),"customBackgroundColorButton");QVERIFY(custom);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,custom->mapToScene(QPointF(custom->width()/2,custom->height()/2)).toPoint());
        auto* color=window->findChild<QObject*>("customColorDialog");QVERIFY(color);QTRY_VERIFY(color->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(color,"load",Q_ARG(QVariant,QString("#abcdef"))));QCOMPARE(controller.pageColor(),QColor("#abcdef"));
        QVERIFY(QMetaObject::invokeMethod(color,"close"));QTRY_VERIFY(!color->property("visible").toBool());QCOMPARE(controller.pageColor(),QColor("#000000"));
        QVERIFY(QMetaObject::invokeMethod(background,"close"));QTRY_VERIFY(!background->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(pen,"open"));QTRY_VERIFY(pen->property("opened").toBool());
        blue=findVisualItem(palette,"paletteColor_2");QVERIFY(blue);const auto darkInk=blue->property("swatch").value<QColor>();QVERIFY(darkInk.lightnessF()>lightInk.lightnessF());QVERIFY(std::abs(darkInk.hslHueF()-lightInk.hslHueF())<0.01);QVERIFY(std::abs(darkInk.hslSaturationF()-lightInk.hslSaturationF())<0.01);
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/pen-adaptive-dark-page.png"));
        auto* customPen=findVisualItem(window->contentItem(),"customPenColorButton");QVERIFY(customPen);const auto previous=canvas->penColor();QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,customPen->mapToScene(QPointF(customPen->width()/2,customPen->height()/2)).toPoint());
        auto* penColor=window->findChild<QObject*>("penColorDialog");QVERIFY(penColor);QTRY_VERIFY(penColor->property("opened").toBool());QVERIFY(!canvas->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(penColor,"load",Q_ARG(QVariant,QString("#ff8800"))));QCOMPARE(canvas->penColor().name(),QString("#ff8800"));
        auto* plane=findVisualItem(window->contentItem(),"colorSaturationPlane");QVERIFY(plane);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,plane->mapToScene(QPointF(plane->width()/2,plane->height()/2)).toPoint());QVERIFY(canvas->penColor()!=QColor("#ff8800"));
        QVERIFY(QMetaObject::invokeMethod(penColor,"load",Q_ARG(QVariant,QString("#ff8800"))));
        QVERIFY(QMetaObject::invokeMethod(penColor,"setRgb",Q_ARG(QVariant,2),Q_ARG(QVariant,255)));QCOMPARE(canvas->penColor().name(),QString("#ff88ff"));
        QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/color-picker-complete.png"));
        window->resize(520,640);QTest::qWait(200);QVERIFY(penColor->property("height").toDouble()<=window->height()-32);QVERIFY(window->grabWindow().save("screenshots/color-picker-small.png"));
        QVERIFY(QMetaObject::invokeMethod(penColor,"close"));QTRY_VERIFY(!penColor->property("visible").toBool());QCOMPARE(canvas->penColor(),previous);
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void resizePreservesViewportCenter(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->fitPage();
        const auto center=canvas->viewportCenter();const double zoom=canvas->zoom();
        QVERIFY(QLineF(center,QPointF(controller.pageWidth()/2,controller.pageHeight()/2)).length()<1e-8);
        window->resize(1920,1080);QTRY_COMPARE(int(canvas->width()),1920);
        QVERIFY(QLineF(center,canvas->viewportCenter()).length()<1e-8);QCOMPARE(canvas->zoom(),zoom);
        canvas->zoomBy(1.6);canvas->setTool("hand");
        const auto start=canvas->mapToScene(QPointF(400,300)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,start+QPoint(80,45));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(80,45));
        const auto panned=canvas->viewportCenter();const auto pannedZoom=canvas->zoom();QVERIFY(QLineF(center,panned).length()>1);
        window->resize(800,600);QTRY_COMPARE(int(canvas->width()),800);
        QVERIFY(QLineF(panned,canvas->viewportCenter()).length()<1e-8);QCOMPARE(canvas->zoom(),pannedZoom);
        window->resize(1920,1080);QTRY_COMPARE(int(canvas->width()),1920);
        QVERIFY(QLineF(panned,canvas->viewportCenter()).length()<1e-8);QCOMPARE(canvas->zoom(),pannedZoom);
        QVERIFY(controller.shutdown());
    }
    void shakyHoldAndConnectedLines(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);const auto origin=canvas->mapToScene(QPointF(420,240)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,origin);for(int i=1;i<=30;++i)QTest::mouseMove(window,origin+QPoint(i*4,0));
        for(const auto& point:std::array<QPoint,4>{QPoint(118,6),QPoint(123,-4),QPoint(119,5),QPoint(121,2)})QTest::mouseMove(window,origin+point);
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecido"),2000);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,origin+QPoint(121,2));QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Line);
        controller.undo();controller.undo();QVERIFY(controller.page()->shapes.empty());
        for(const auto& vertices:std::vector<std::vector<Point>>{{{30,30},{90,30}},{{90.5,30.8},{90,80}},{{90,80},{30,80}},{{30,79.5},{30.8,30.5}}}){ShapeObject line;line.id=newId();line.kind=ShapeKind::Line;line.vertices=vertices;controller.addShape(line);}
        QTRY_COMPARE(controller.page()->shapes.size(),std::size_t(1));QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Polygon);QVERIFY(controller.page()->shapes[0].fillOpacity>0);
        controller.undo();QCOMPARE(controller.page()->shapes.size(),std::size_t(4));for(const auto& shape:controller.page()->shapes)QCOMPARE(shape.kind,ShapeKind::Line);controller.redo();QCOMPARE(controller.page()->shapes.size(),std::size_t(1));QVERIFY(controller.shutdown());
    }
    void homeTrashConfirmationAndRestore(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.setTheme("Dark");controller.newProject("Quadro na lixeira","A4",210,297,false,"Branco");QTRY_VERIFY(!controller.dirty());controller.home();
        QTRY_COMPARE(controller.recentProjects().size(),1);const auto project=controller.recentProjects()[0].toMap();const auto id=project["id"].toString();const auto path=project["path"].toString();
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(findVisualItem(window->contentItem(),"trashProjectButton"));auto* remove=findVisualItem(window->contentItem(),"trashProjectButton");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,remove->mapToScene(QPointF(remove->width()/2,remove->height()/2)).toPoint());
        auto* dialog=window->findChild<QObject*>("deleteProjectDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());QVERIFY(QFile::exists(path));QVERIFY(controller.trashedProjects().isEmpty());
        auto* cancel=findVisualItem(window->contentItem(),"cancelProjectDeletion");QVERIFY(cancel);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,cancel->mapToScene(QPointF(cancel->width()/2,cancel->height()/2)).toPoint());QTRY_VERIFY(!dialog->property("visible").toBool());QCOMPARE(controller.recentProjects().size(),1);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,remove->mapToScene(QPointF(remove->width()/2,remove->height()/2)).toPoint());QTRY_VERIFY(dialog->property("opened").toBool());
        auto* confirm=findVisualItem(window->contentItem(),"confirmProjectDeletion");QVERIFY(confirm);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,confirm->mapToScene(QPointF(confirm->width()/2,confirm->height()/2)).toPoint());
        QTRY_VERIFY(!controller.busy());QCOMPARE(controller.trashedProjects().size(),1);QVERIFY(controller.recentProjects().isEmpty());QVERIFY(!QFile::exists(path));QTRY_VERIFY(!dialog->property("visible").toBool());
        auto* open=findVisualItem(window->contentItem(),"openTrashButton");QVERIFY(open);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,open->mapToScene(QPointF(open->width()/2,open->height()/2)).toPoint());auto* trash=window->findChild<QObject*>("trashDialog");QVERIFY(trash);QTRY_VERIFY(trash->property("opened").toBool());QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/trash-dark.png"));
        controller.restoreProject(id);QTRY_VERIFY(!controller.busy());QVERIFY(controller.trashedProjects().isEmpty());QCOMPARE(controller.recentProjects().size(),1);QVERIFY(QFile::exists(path));
        controller.trashProject(id);QTRY_VERIFY(!controller.busy());controller.deleteProjectPermanently(id);QTRY_VERIFY(!controller.busy());QVERIFY(controller.trashedProjects().isEmpty());QVERIFY(controller.recentProjects().isEmpty());QVERIFY(!QFile::exists(path));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void milestone3BackgroundAndText(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newProject("Milestone 3","A4",210,297,false,"Pontilhado");controller.setTheme("Dark");
        QCOMPARE(controller.page()->backgroundStyle.gridType,GridType::Dots);controller.applyBackgroundPreset("Isométrico");QCOMPARE(controller.page()->backgroundStyle.gridType,GridType::Isometric);
        controller.undo();QCOMPARE(controller.page()->backgroundStyle.gridType,GridType::Dots);controller.redo();
        auto custom=controller.background();custom["spacingX"]=7;custom["spacingY"]=9;controller.setBackground(custom);controller.saveBackgroundPreset("Grade de aula",custom);
        QVERIFY(controller.backgroundPresets().size()>9);controller.applyBackgroundPreset("Grade de aula");QCOMPARE(controller.page()->backgroundStyle.spacingY,9.);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setPenLineStyle("dotted");canvas->setDotSpacing(3.5);
        QCOMPARE(canvas->penLineStyle(),QString("dotted"));canvas->setTool("text");QSignalSpy request(canvas,&CanvasItem::textRequested);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(QPointF(260,220)).toPoint());QTRY_COMPARE(request.size(),1);
        auto* dialog=window->findChild<QObject*>("textDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());
        auto* editor=findVisualItem(window->contentItem(),"textSource");QVERIFY(editor);editor->setProperty("text","Aula de geometria");
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/milestone3-text-dialog.png"));
        controller.upsertText({},QPointF(30,40),{{"source","Aula de geometria\nTexto editável"},{"fontSizePt",22},{"fontFamily","DejaVu Sans"},{"bold",true},{"alignment",1},{"color","#397ce0"}});
        QTRY_VERIFY(!controller.textBusy());QVERIFY2(controller.textError().isEmpty(),qPrintable(controller.textError()));QCOMPARE(controller.page()->texts.size(),std::size_t(1));
        const auto id=QString::fromStdString(controller.page()->texts[0].id);QCOMPARE(controller.textValues(id)["fontSizePt"].toDouble(),22.);QVERIFY(!controller.image(id.toStdString()).isNull());
        controller.undo();QVERIFY(controller.page()->texts.empty());controller.redo();QCOMPARE(controller.page()->texts.size(),std::size_t(1));
        auto values=controller.textValues(id);values["source"]="Texto revisado";values["italic"]=true;controller.upsertText(id,{},values);QTRY_VERIFY(!controller.textBusy());QVERIFY(controller.page()->texts[0].italic);controller.undo();QVERIFY(!controller.page()->texts[0].italic);controller.redo();
        QTRY_VERIFY(!dialog->property("visible").toBool());QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/milestone3-dark-text.png"));
        auto* background=window->findChild<QObject*>("backgroundOptions");QVERIFY(background);QVERIFY(QMetaObject::invokeMethod(background,"open"));QTRY_VERIFY(background->property("opened").toBool());
        for(int i=0;i<9;++i){const auto* swatch=findVisualItem(window->contentItem(),"backgroundColor_"+QString::number(i));QVERIFY(swatch);const auto x=swatch->mapToScene(QPointF(swatch->width(),0)).x();QVERIFY(x<=window->width());}
        QVERIFY(window->grabWindow().save("screenshots/milestone3-background.png"));
        window->resize(520,640);QTest::qWait(200);
        const auto panelRight=background->property("x").toDouble()+background->property("width").toDouble();
        double firstRow=0;bool wrapped=false;
        for(int i=0;i<9;++i){const auto* swatch=findVisualItem(window->contentItem(),"backgroundColor_"+QString::number(i));QVERIFY(swatch);const auto corner=swatch->mapToScene(QPointF(swatch->width(),0));QVERIFY(corner.x()<panelRight);if(i==0)firstRow=corner.y();else wrapped|=corner.y()>firstRow;}
        QVERIFY(wrapped);QVERIFY(window->grabWindow().save("screenshots/milestone3-background-small.png"));
        window->resize(1200,800);QTest::qWait(200);
        auto* choose=findVisualItem(window->contentItem(),"customBackgroundColorButton");QVERIFY(choose);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,choose->mapToScene(QPointF(choose->width()/2,choose->height()/2)).toPoint());
        auto* colorDialog=window->findChild<QObject*>("customColorDialog");QVERIFY(colorDialog);QTRY_VERIFY(colorDialog->property("opened").toBool());QVERIFY(background->property("visible").toBool());QVERIFY(!canvas->isEnabled());
        QVERIFY(window->grabWindow().save("screenshots/milestone3-color-picker.png"));QVERIFY(QMetaObject::invokeMethod(colorDialog,"close"));QTRY_VERIFY(!colorDialog->property("visible").toBool());QVERIFY(QMetaObject::invokeMethod(background,"close"));
        controller.upsertText({},QPointF(30,70),{{"source","Área $\\pi r^2$\n$$\\int_a^b f(x)dx$$"},{"fontSizePt",20},{"color","#263345"}});QTRY_VERIFY(!controller.textBusy());QVERIFY2(controller.textError().isEmpty(),qPrintable(controller.textError()));QCOMPARE(controller.page()->texts.size(),std::size_t(2));QCOMPARE(controller.page()->texts.back().math.size(),std::size_t(2));QVERIFY(!controller.mathGeometry(controller.page()->texts.back().id).empty());
        QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/milestone3-latex.png"));
        const auto file=directory.filePath("m3.board");controller.saveAs(QUrl::fromLocalFile(file));QTRY_VERIFY(!controller.dirty());controller.openPath(file);QTRY_VERIFY(!controller.loading());QCOMPARE(controller.page()->texts[0].source,std::string("Texto revisado"));QCOMPARE(controller.page()->backgroundStyle.spacingX,7.);
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void uiAndMouseStroke(){
        QTemporaryDir dataDirectory;QVERIFY(dataDirectory.isValid());
        AppController controller(nullptr,dataDirectory.path());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("App",&controller);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* newButton=window->findChild<QQuickItem*>("newProjectButton");QVERIFY(newButton);
        auto* label=newButton->property("contentItem").value<QQuickItem*>();QVERIFY(label);
        QCOMPARE(label->property("color").value<QColor>(),QColor("#ffffff"));
        QDir().mkpath("screenshots");
        QTest::qWait(100);QVERIFY(window->grabWindow().save("screenshots/home-light.png"));
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,newButton->mapToScene(QPointF(newButton->width()/2,newButton->height()/2)).toPoint());
        auto* dialog=window->findChild<QObject*>("newProjectDialog");QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto* nameInput=window->findChild<QQuickItem*>("projectNameInput");QVERIFY(nameInput);nameInput->setProperty("text","Test desktop");
        QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/new-project.png"));
        auto* createButton=window->findChild<QQuickItem*>("createProjectButton");QVERIFY(createButton);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,createButton->mapToScene(QPointF(createButton->width()/2,createButton->height()/2)).toPoint());
        QTRY_VERIFY(controller.active());
        QCOMPARE(controller.projectName(),QString("Test desktop"));
        QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);
        QTRY_VERIFY(canvas->zoom()>0);
        QTRY_VERIFY(canvas->isEnabled());
        const auto page=controller.page();QVERIFY(page);QCOMPARE(page->size.widthMm,210.);
        const QPoint start=canvas->mapToScene(QPointF(canvas->width()/2,canvas->height()/2)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
        for(int i=1;i<=12;++i)QTest::mouseMove(window,start+QPoint(i*4,i*2),2);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(48,24));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        QVERIFY(controller.page()->strokes.front().samples.size()>2);
        const auto points=controller.page()->strokes.front().samples;
        canvas->zoomBy(1.5);QCOMPARE(controller.page()->strokes.front().samples.front().position.x,points.front().position.x);
        controller.undo();QVERIFY(controller.page()->strokes.empty());controller.redo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        QTest::qWait(100);QVERIFY(window->grabWindow().save("screenshots/editor-light.png"));
        auto* options=window->findChild<QObject*>("penOptions");QVERIFY(options);QVERIFY(QMetaObject::invokeMethod(options,"open"));
        QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/pen-options.png"));QVERIFY(QMetaObject::invokeMethod(options,"close"));
        controller.setTheme("Dark");QTest::qWait(200);QCOMPARE(controller.pageColor(),QColor("#ffffff"));
        QVERIFY(window->grabWindow().save("screenshots/editor-dark.png"));
        auto* settings=window->findChild<QObject*>("settingsDialog");QVERIFY(settings);QVERIFY(QMetaObject::invokeMethod(settings,"open"));
        QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/settings-dark.png"));QVERIFY(QMetaObject::invokeMethod(settings,"close"));
        controller.setTheme("Light");
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.filePath("desktop.board");
        controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.busy());
        QVERIFY(!controller.dirty());controller.home();QVERIFY(!controller.active());
        controller.openPath(path);QTRY_VERIFY(!controller.loading());QVERIFY(controller.active());
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        QCOMPARE(controller.page()->strokes.front().samples.size(),points.size());
        QCOMPARE(warnings.count(),0);
        QVERIFY(controller.shutdown());
    }
    void holdRecognizesSmoothCurves(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);
        engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(canvas->isEnabled());
        const auto center=canvas->mapToScene({canvas->width()/2,canvas->height()/2-100}).toPoint();
        for(const auto kind:{ShapeKind::Circle,ShapeKind::Ellipse}){
            std::vector<QPoint> points;
            for(int i=0;i<=180;++i){const double a=2*3.141592653589793*i/180;
                const double rx=kind==ShapeKind::Circle?50+2*std::sin(3*a):80;
                const double ry=kind==ShapeKind::Circle?rx:8;
                points.push_back(center+QPoint(qRound(rx*std::cos(a)),qRound(ry*std::sin(a))));
            }
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,points.front());
            for(std::size_t i=1;i<points.size();++i)QTest::mouseMove(window,points[i]);
            QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecido"),2000);
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,points.back());
            QCOMPARE(controller.page()->shapes.back().kind,kind);
            if(kind==ShapeKind::Ellipse)QVERIFY(controller.page()->shapes.back().radiusX/controller.page()->shapes.back().radiusY>8);
            controller.undo();QVERIFY(!controller.page()->strokes.empty());controller.redo();
        }
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void regularPolygonAndIndependentFill(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);
        engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(canvas->isEnabled());
        const auto origin=canvas->mapToScene({canvas->width()/2-70,canvas->height()/2-100}).toPoint();
        const std::array<QPointF,5> corners{{{60,0},{117,42},{95,109},{25,109},{3,42}}};
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,origin+corners[0].toPoint());
        for(std::size_t edge=0;edge<corners.size();++edge)for(int i=1;i<=30;++i){
            const double t=i/30.;auto point=corners[edge]+(corners[(edge+1)%corners.size()]-corners[edge])*t;
            if(edge==1)point.setX(point.x()+3*std::sin(3.141592653589793*t));
            QTest::mouseMove(window,origin+point.toPoint());
        }
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecido"),2000);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,origin+corners[0].toPoint());
        QCOMPARE(controller.page()->shapes.size(),std::size_t(1));
        QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Polygon);QCOMPARE(controller.page()->shapes[0].vertices.size(),std::size_t(5));
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));controller.redo();
        canvas->setTool("select");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,origin+QPoint(55,50));
        QCOMPARE(canvas->selectionName(),QString("Polígono"));
        int vertices=0;for(const auto& handle:canvas->selectionHandles())if(handle.toMap()["type"]=="vertex")++vertices;
        QCOMPARE(vertices,5);
        const auto border=controller.page()->shapes[0].style.rgba;
        auto* redFill=findVisualItem(window->contentItem(),"fillColor_3");QVERIFY(redFill);QVERIFY(redFill->isVisible());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,redFill->mapToScene({redFill->width()/2,redFill->height()/2}).toPoint());
        QCOMPARE(controller.page()->shapes[0].fillColor(),std::uint32_t(0xcc5364ff));QCOMPARE(controller.page()->shapes[0].style.rgba,border);
        controller.undo();QVERIFY(!controller.page()->shapes[0].fillRgba);controller.redo();
        canvas->setSelectedColor(QColor("#397ce0"));QCOMPARE(controller.page()->shapes[0].style.rgba,std::uint32_t(0x397ce0ff));
        QCOMPARE(controller.page()->shapes[0].fillColor(),std::uint32_t(0xcc5364ff));
        QCOMPARE(canvas->selectedFillColor(),QColor("#cc5364"));
        // Editing a generic vertex is independent of the group resize handle.
        const auto handle=canvas->selectionHandles()[0].toMap();
        const auto vertexPoint=canvas->mapToScene({handle["x"].toDouble(),handle["y"].toDouble()}).toPoint();
        const auto oldVertex=controller.page()->shapes[0].vertices[0];
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,vertexPoint);
        QTest::mouseMove(window,vertexPoint+QPoint(-12,-5));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,vertexPoint+QPoint(-12,-5));
        QVERIFY(length(controller.page()->shapes[0].vertices[0]-oldVertex)>1);controller.undo();
        QVERIFY(length(controller.page()->shapes[0].vertices[0]-oldVertex)<1e-8);
        controller.setTheme("Dark");QTest::qWait(200);QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/polygon-fill-panel.png"));
        const auto path=directory.filePath("polygon.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.busy());
        controller.home();controller.openPath(path);QTRY_VERIFY(!controller.loading());
        QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Polygon);QCOMPARE(controller.page()->shapes[0].fillColor(),std::uint32_t(0xcc5364ff));
        QCOMPARE(controller.page()->shapes[0].style.rgba,std::uint32_t(0x397ce0ff));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void shiftRecognitionAndManualPatterns(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);
        engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(canvas->isEnabled());
        const auto start=canvas->mapToScene({canvas->width()/2-80,canvas->height()/2-100}).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
        for(int i=1;i<=20;++i)QTest::mouseMove(window,start+QPoint(i*6,0));
        QTest::keyPress(window,Qt::Key_Shift);
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecido"),2000);
        QTest::keyRelease(window,Qt::Key_Shift);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(120,0));
        QCOMPARE(controller.page()->shapes[0].style.pattern,LinePattern::Dashed);
        controller.undo();QCOMPARE(controller.page()->strokes[0].style.pattern,LinePattern::Solid);
        controller.redo();QCOMPARE(controller.page()->shapes[0].style.pattern,LinePattern::Dashed);
        auto* popup=window->findChild<QObject*>("shapeOptions");QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(popup,"open"));QTest::qWait(200);
        auto* style=window->findChild<QObject*>("shapeLineStyleControl");QVERIFY(style);
        QVERIFY(QMetaObject::invokeMethod(style,"selected",Q_ARG(int,1)));
        QCOMPARE(canvas->shapeLineStyle(),QString("dashed"));
        controller.setTheme("Dark");QTest::qWait(200);
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/shape-options-styles.png"));
        auto* circleButton=findVisualItem(window->contentItem(),"shapeChoice_circle");QVERIFY(circleButton);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,circleButton->mapToScene({circleButton->width()/2,circleButton->height()/2}).toPoint());
        QTRY_VERIFY(canvas->isEnabled());QCOMPARE(canvas->tool(),QString("circle"));
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(0,90));
        QTest::mouseMove(window,start+QPoint(80,170));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(80,170));
        QCOMPARE(controller.page()->shapes.size(),std::size_t(2));QCOMPARE(controller.page()->shapes[1].style.pattern,LinePattern::Dashed);
        canvas->setShapeLineStyle("dotted");canvas->setTool("rectangle");
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(100,90));
        QTest::mouseMove(window,start+QPoint(180,170));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(180,170));
        QCOMPARE(controller.page()->shapes[2].style.pattern,LinePattern::Dotted);
        const auto path=directory.filePath("patterns.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.busy());
        controller.home();controller.openPath(path);QTRY_VERIFY(!controller.loading());
        QCOMPARE(controller.page()->shapes[0].style.pattern,LinePattern::Dashed);
        QCOMPARE(controller.page()->shapes[1].style.pattern,LinePattern::Dashed);
        auto* settings=window->findChild<QObject*>("settingsDialog");QVERIFY(settings);QVERIFY(QMetaObject::invokeMethod(settings,"open"));
        QTest::qWait(200);QVERIFY(settings->property("height").toDouble()<600);
        auto* hint=window->findChild<QObject*>("recognitionShiftHint");QVERIFY(hint);QVERIFY(hint->property("text").toString().contains("Shift"));
        QVERIFY(window->grabWindow().save("screenshots/settings-compact-dark.png"));
        controller.setTheme("Light");QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/settings-compact-light.png"));
        window->resize(520,640);QTest::qWait(200);QVERIFY(settings->property("height").toDouble()<window->height());
        QVERIFY(window->grabWindow().save("screenshots/settings-compact-small.png"));
        QVERIFY(QMetaObject::invokeMethod(settings,"close"));QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void eraserPreviewRetainsLayerOrder(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());
        controller.newDefault();QTRY_VERIFY(!controller.busy());
        StrokeObject stroke{newId(),{},{{{10,30},0.5},{{80,30},0.5}}};controller.addStroke(stroke);
        QImage bitmap(80,80,QImage::Format_ARGB32);bitmap.fill(Qt::red);
        const auto path=directory.filePath("layer.png");QVERIFY(bitmap.save(path));
        controller.importImage(QUrl::fromLocalFile(path),{50,30});QTRY_COMPARE(controller.page()->images.size(),std::size_t(1));
        InspectableCanvas canvas;canvas.setWidth(800);canvas.setHeight(600);canvas.setController(&controller);canvas.fitPage();canvas.setTool("eraser");
        const double scale=canvas.zoom()*96/25.4;
        const QPointF point((800-210*scale)/2+20*scale,(600-297*scale)/2+30*scale);
        QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        canvas.mousePressEvent(&press);
        // Inspect the actual Scene Graph order during preview without requiring a GPU.
        std::unique_ptr<QSGNode> scene(canvas.updatePaintNode(nullptr,nullptr));
        auto* clip=scene->firstChild()->nextSibling();QVERIFY(clip);
        QCOMPARE(clip->childCount(),3); // two stroke fragments, followed by the image
        QCOMPARE(clip->lastChild()->type(),QSGNode::TransformNodeType);
        QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        canvas.mouseReleaseEvent(&release);
        scene.reset(canvas.updatePaintNode(scene.release(),nullptr));
        QCOMPARE(scene->firstChild()->nextSibling()->lastChild()->type(),QSGNode::TransformNodeType);
        QVERIFY(controller.shutdown());
    }
    void webpImportWithoutQtPlugin(){
        QTemporaryDir directory;const auto path=directory.filePath("sample.webp");
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray::fromBase64("UklGRh4AAABXRUJQVlA4TBEAAAAvA4AAEAdQvKIUubSBiOh/AAA="));file.close();
        const auto imported=importImageFile(path,{50,50},{});
        QVERIFY2(imported.error.isEmpty(),qPrintable(imported.error));
        QCOMPARE(imported.image.size(),QSize(4,3));
        QCOMPARE(imported.image.pixelColor(0,0),QColor(40,120,200,180));
        QVERIFY(imported.object.png&&!imported.object.png->empty());
        QFile corrupt(directory.filePath("corrupt.webp"));QVERIFY(corrupt.open(QIODevice::WriteOnly));
        corrupt.write("RIFFxxxxWEBPbroken");corrupt.close();
        QVERIFY(!importImageFile(corrupt.fileName(),{50,50},{}).error.isEmpty());
    }
    void imagesProtectCoveredStrokes(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(canvas->isEnabled());
        const auto start=canvas->mapToScene({canvas->width()/2-100,canvas->height()/2-90}).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
        for(int i=1;i<=20;++i)QTest::mouseMove(window,start+QPoint(i*10,0));
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(200,0));
        const auto original=controller.page()->strokes.front();
        const Point center=(original.samples.front().position+original.samples.back().position)*0.5;
        QImage bitmap(80,80,QImage::Format_ARGB32);bitmap.fill(Qt::red);
        const auto path=directory.filePath("shield.png");QVERIFY(bitmap.save(path));
        controller.importImage(QUrl::fromLocalFile(path),{center.x,center.y});QTRY_COMPARE(controller.page()->images.size(),std::size_t(1));
        const auto image=controller.page()->images.front();QVERIFY(image.properties.zIndex>original.properties.zIndex);
        canvas->setTool("eraser");canvas->setEraserRadius(2);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(100,0));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));QCOMPARE(controller.page()->strokes.front().id,original.id);
        // The sweep crosses both exposed ends and the image: retain only the covered middle.
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(window,start+QPoint(200,0));
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(200,0));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        const auto remnant=controller.page()->strokes.front();
        QCOMPARE(remnant.properties.zIndex,original.properties.zIndex);
        QVERIFY(remnant.samples.front().position.x>original.samples.front().position.x);
        QVERIFY(remnant.samples.back().position.x<original.samples.back().position.x);
        // Moving the image exposes the preserved stroke to the next eraser gesture.
        const auto moved=transformed(image,center,{0,50});
        controller.changeObjects({{CanvasObject(image),moved}},CommandKind::TransformObject);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(100,0));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(2));
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        QVERIFY(controller.shutdown());
    }
    void holdRecognizesBowedSquare(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("App",&controller);
        engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(canvas->isEnabled());
        const auto origin=canvas->mapToScene({canvas->width()/2-60,canvas->height()/2-120}).toPoint();
        const std::array<QPointF,4> corners{{{0,0},{100,0},{100,100},{0,100}}};
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,origin);
        for(std::size_t edge=0;edge<4;++edge){
            const auto a=corners[edge],delta=corners[(edge+1)%4]-a;
            for(int i=1;i<=30;++i){const double t=i/30.;auto point=a+delta*t;
                if(edge==0)point.setY(point.y()+12*std::sin(3.141592653589793*t));
                QTest::mouseMove(window,origin+point.toPoint());
            }
        }
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecido"),2000);
        QTest::keyPress(window,Qt::Key_Shift);QVERIFY(canvas->interactionHint().contains("tracejado"));
        QTest::keyRelease(window,Qt::Key_Shift);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,origin);
        QCOMPARE(controller.page()->shapes.size(),std::size_t(1));
        QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Square);
        QCOMPARE(controller.page()->shapes[0].style.pattern,LinePattern::Dashed);
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        QVERIFY(controller.shutdown());
    }
    void tabletEraserDragAndConsecutiveGestures(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("App",&controller);
        engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(canvas->isEnabled());
        const auto start=canvas->mapToScene({canvas->width()/2-120,canvas->height()/2-80}).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
        for(int i=1;i<=30;++i)QTest::mouseMove(window,start+QPoint(i*8,0));
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(240,0));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        const auto original=controller.page()->strokes.front();
        canvas->setTool("eraser");canvas->setEraserRadius(2);
        QPointingDevice pen("Test stylus",123,QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Pen,QInputDevice::Capability::Position|QInputDevice::Capability::Pressure,1,1);
        auto send=[&](QEvent::Type type,QPoint position){
            QTabletEvent event(type,&pen,position,window->mapToGlobal(position),type==QEvent::TabletRelease?0.:0.7,
                0,0,0,0,0,Qt::NoModifier,type==QEvent::TabletMove?Qt::NoButton:Qt::LeftButton,
                type==QEvent::TabletRelease?Qt::NoButton:Qt::LeftButton);
            QCoreApplication::sendEvent(window,&event);
        };
        send(QEvent::TabletPress,start+QPoint(60,0));
        // A sparse move must erase the entire swept path, including the gap between events.
        send(QEvent::TabletMove,start+QPoint(115,0));
        send(QEvent::TabletRelease,start+QPoint(120,0));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(2));
        const auto firstCuts=controller.page()->strokes;
        auto totalLength=[&](){double result=0;for(const auto& stroke:controller.page()->strokes)
            for(std::size_t i=1;i<stroke.samples.size();++i)result+=length(stroke.samples[i].position-stroke.samples[i-1].position);return result;};
        const double firstLength=totalLength();
        double originalLength=0;for(std::size_t i=1;i<original.samples.size();++i)originalLength+=length(original.samples[i].position-original.samples[i-1].position);
        QVERIFY(firstLength<originalLength*0.8);
        send(QEvent::TabletPress,start+QPoint(175,0));
        send(QEvent::TabletMove,start+QPoint(185,0));
        send(QEvent::TabletRelease,start+QPoint(195,0));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(3));QVERIFY(totalLength()<firstLength);
        controller.undo();QCOMPARE(controller.page()->strokes.size(),firstCuts.size());
        QVERIFY(std::abs(totalLength()-firstLength)<1e-8);
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        QCOMPARE(controller.page()->strokes[0].id,original.id);
        controller.redo();controller.redo();QCOMPARE(controller.page()->strokes.size(),std::size_t(3));
        // Mouse dragging follows the same continuous eraser pipeline.
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(25,-20));
        QTest::mouseMove(window,start+QPoint(25,20));
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(25,20));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(4));
        QVERIFY(controller.shutdown());
    }
    void recognitionEraserSelectionAndImages(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(canvas->isEnabled());
        auto screen=[&](QPointF local){return canvas->mapToScene(local).toPoint();};
        const auto start=screen({canvas->width()/2-90,canvas->height()/2-90});
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
        for(int i=1;i<=20;++i)QTest::mouseMove(window,start+QPoint(i*6,1),2);
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecido"),2000);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(140,20));
        QCOMPARE(controller.page()->shapes.size(),std::size_t(1));QVERIFY(controller.page()->strokes.empty());QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Line);
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));QVERIFY(controller.page()->shapes.empty());
        canvas->setTool("eraser");canvas->setEraserRadius(2);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(60,1));QCOMPARE(controller.page()->strokes.size(),std::size_t(2));
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));controller.redo();QCOMPARE(controller.page()->strokes.size(),std::size_t(2));
        canvas->setTool("circle");const auto shapeStart=start+QPoint(0,130);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,shapeStart);QTest::mouseMove(window,shapeStart+QPoint(50,50));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,shapeStart+QPoint(50,50));QCOMPARE(controller.page()->shapes.size(),std::size_t(1));
        canvas->setTool("select");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,shapeStart+QPoint(25,25));QCOMPARE(canvas->selectedCount(),1);QVERIFY(canvas->selectionHandles().size()>=4);
        const auto center=controller.page()->shapes[0].center;
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,shapeStart+QPoint(25,25));QTest::mouseMove(window,shapeStart+QPoint(45,35));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,shapeStart+QPoint(45,35));QVERIFY(length(controller.page()->shapes[0].center-center)>1);
        controller.undo();QVERIFY(length(controller.page()->shapes[0].center-center)<1e-8);
        const auto resizeHandles=canvas->selectionHandles();QVariantMap resize;
        for(const auto& h:resizeHandles)if(h.toMap()["type"].toString()=="resize")resize=h.toMap();
        QVERIFY(!resize.isEmpty());const auto radius=controller.page()->shapes[0].radiusX;
        const auto resizePoint=screen({resize["x"].toDouble(),resize["y"].toDouble()});
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,resizePoint);QTest::mouseMove(window,resizePoint+QPoint(25,25));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,resizePoint+QPoint(25,25));QVERIFY(controller.page()->shapes[0].radiusX>radius);controller.undo();
        QVariantMap rotate;for(const auto& h:canvas->selectionHandles())if(h.toMap()["type"].toString()=="rotate")rotate=h.toMap();
        const auto rotatePosition=screen({rotate["x"].toDouble(),rotate["y"].toDouble()});
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,rotatePosition);QTest::mouseMove(window,rotatePosition+QPoint(30,10));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,rotatePosition+QPoint(30,10));QVERIFY(std::abs(controller.page()->shapes[0].rotation)>0.1);controller.undo();
        canvas->setSelectedFill(0.25);QCOMPARE(controller.page()->shapes[0].fillOpacity,0.25);canvas->duplicateSelection();QCOMPARE(controller.page()->shapes.size(),std::size_t(2));canvas->deleteSelection();QCOMPARE(controller.page()->shapes.size(),std::size_t(1));
        QImage bitmap(80,60,QImage::Format_ARGB32);bitmap.fill(QColor("#397ce0"));QGuiApplication::clipboard()->setImage(bitmap);
        canvas->forceActiveFocus();QTest::keyClick(window,Qt::Key_V,Qt::ControlModifier);QTRY_COMPARE(controller.page()->images.size(),std::size_t(1));
        const auto centerPoint=screen({canvas->width()/2,canvas->height()/2});QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,centerPoint);QCOMPARE(canvas->selectionName(),QString("Imagem"));
        const auto originalCorners=controller.page()->images[0].corners;canvas->duplicateSelection();QCOMPARE(controller.page()->images.size(),std::size_t(2));
        QVERIFY(!controller.image(controller.page()->images.back().id).isNull());canvas->deleteSelection();QCOMPARE(controller.page()->images.size(),std::size_t(1));
        QCOMPARE(controller.page()->images[0].corners,originalCorners);
        const auto path=directory.filePath("source.png");QVERIFY(bitmap.save(path));controller.importImage(QUrl::fromLocalFile(path),{100,100});QTRY_COMPARE(controller.page()->images.size(),std::size_t(2));
        auto* mime=new QMimeData;mime->setUrls({QUrl::fromLocalFile(path)});QGuiApplication::clipboard()->setMimeData(mime);canvas->pasteImage();QTRY_COMPARE(controller.page()->images.size(),std::size_t(3));
        QMimeData droppedMime;droppedMime.setUrls({QUrl::fromLocalFile(path)});
        QDragEnterEvent enter(centerPoint,Qt::CopyAction,&droppedMime,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&enter);
        QDropEvent drop(QPointF(centerPoint),Qt::CopyAction,&droppedMime,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&drop);
        QTRY_COMPARE(controller.page()->images.size(),std::size_t(4));
        const auto boardPath=directory.filePath("features.board");controller.saveAs(QUrl::fromLocalFile(boardPath));QTRY_VERIFY(!controller.busy());QVERIFY(!controller.dirty());controller.home();controller.openPath(boardPath);QTRY_VERIFY(!controller.loading());QVERIFY(controller.active());QCOMPARE(controller.page()->images.size(),std::size_t(4));QCOMPARE(controller.page()->shapes.size(),std::size_t(1));
        controller.setTheme("Dark");QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/milestone2-dark-images.png"));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());QGuiApplication::clipboard()->clear();
    }

};
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);app.setOrganizationName("ScalarTests");app.setApplicationName("Desktop");
    QQuickStyle::setStyle("Basic");
    qmlRegisterType<CanvasItem>("Scalar",1,0,"BoardCanvas");
    qmlRegisterUncreatableType<AppController>("Scalar",1,0,"ApplicationController","Use App");
    DesktopTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "desktop_tests.moc"
