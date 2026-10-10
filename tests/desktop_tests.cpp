#include <QtTest>
#include "app/Branding.h"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QFontDatabase>
#include <QSyntaxHighlighter>
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
#include <QWheelEvent>
#include <QHoverEvent>
#include <QPointingDevice>
#include <QPdfWriter>
#include <QPainter>
#include <QPainterPath>
#include <QPageSize>
#include <QSGTransformNode>
#include <QSGGeometryNode>
#include <QSGSimpleTextureNode>
#include <memory>
#include <array>
#include <atomic>
#include <cmath>
#include <numbers>
#include "app/AppController.h"
#include "canvas/CanvasItem.h"
#include "clipboard/ImageImporter.h"
#include "tools/RegionFill.h"
#include "rendering/TextRenderer.h"
#include "rendering/PageRenderer.h"
#include "rendering/ShapeRasterCache.h"
#include "rendering/StrokeMesh.h"
#include "rendering/TextFormatting.h"
using namespace scalar;
QQuickItem* findVisualItem(QQuickItem* parent,const QString& name){
    if(parent->objectName()==name)return parent;
    for(auto* child:parent->childItems())if(auto* found=findVisualItem(child,name))return found;
    return nullptr;
}
class InspectableCanvas : public CanvasItem {
public:
    using CanvasItem::CanvasItem;
    using CanvasItem::updatePaintNode;
    using CanvasItem::mousePressEvent;
    using CanvasItem::mouseMoveEvent;
    using CanvasItem::mouseReleaseEvent;
    using CanvasItem::mouseDoubleClickEvent;
    using CanvasItem::mouseUngrabEvent;
    using CanvasItem::hoverMoveEvent;
};
class ZoomInspectableCanvas : public CanvasItem {
public:
    using CanvasItem::CanvasItem;
    double tileWidth=0;
    int renderedFrames=0;
    std::vector<std::pair<QSGTexture*,QRectF>> textures;
    qint64 maxSyncNs=0;
protected:
    QSGNode* updatePaintNode(QSGNode* old,UpdatePaintNodeData* data) override {
        QElapsedTimer timer;timer.start();auto* root=CanvasItem::updatePaintNode(old,data);maxSyncNs=std::max(maxSyncNs,timer.nsecsElapsed());++renderedFrames;
        auto* clip=root->firstChild()->nextSibling();
        textures.clear();for(auto* item=clip->firstChild();item;item=item->nextSibling())for(auto* child=item->firstChild();child;child=child->nextSibling())if(auto* tile=dynamic_cast<QSGSimpleTextureNode*>(child))textures.emplace_back(tile->texture(),tile->rect());
        auto* object=clip->lastChild();
        if(object&&object->firstChild())if(auto* texture=dynamic_cast<QSGSimpleTextureNode*>(object->firstChild()))tileWidth=texture->rect().width();
        return root;
    }
};
class MarkerInspectableCanvas : public CanvasItem {
public:
    using CanvasItem::CanvasItem;
    std::atomic<int> reusedTiles=0,tileCount=0;
    std::atomic<int> renderedFrames=0;
    std::atomic<qint64> maxSyncNs=0;
protected:
    QSGNode* updatePaintNode(QSGNode* old,UpdatePaintNodeData* data) override {
        QElapsedTimer timer;timer.start();
        auto* root=CanvasItem::updatePaintNode(old,data);
        std::map<std::pair<double,double>,QSGTexture*> textures;
        auto* live=root->firstChild()->nextSibling()->lastChild();
        if((drawing()||tool()=="eraser")&&live)for(auto* child=live->firstChild();child;child=child->nextSibling())if(auto* tile=dynamic_cast<QSGSimpleTextureNode*>(child))textures[{tile->rect().x(),tile->rect().y()}]=tile->texture();
        int reused=0;for(const auto& [key,texture]:textures)if(previous_.contains(key)&&previous_[key]==texture)++reused;
        reusedTiles=reused;tileCount=int(textures.size());previous_=std::move(textures);maxSyncNs=std::max(maxSyncNs.load(),timer.nsecsElapsed());++renderedFrames;return root;
    }
private:
    std::map<std::pair<double,double>,QSGTexture*> previous_;
};
class DesktopTests : public QObject {
    Q_OBJECT
private slots:
    void erasureRecoveryEndsWhenBoardCloses(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        StrokeObject marker;marker.id=newId();marker.marker=true;marker.style.minWidthMm=marker.style.maxWidthMm=6;marker.samples={{{40,60},1},{{100,60},1}};controller.addStroke(marker);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("eraser");canvas.setEraserRadius(2);
        const auto erase=[&](Qt::KeyboardModifiers modifiers){const auto p=canvas.screenPoint({70,60});QMouseEvent press(QEvent::MouseButtonPress,p,p,Qt::LeftButton,Qt::LeftButton,modifiers),release(QEvent::MouseButtonRelease,p,p,Qt::LeftButton,Qt::NoButton,modifiers);canvas.mousePressEvent(&press);canvas.mouseReleaseEvent(&release);};
        erase(Qt::NoModifier);QVERIFY(!hitTest(controller.page()->strokes.front(),{70,60},0));erase(Qt::ShiftModifier);QVERIFY(hitTest(controller.page()->strokes.front(),{70,60},0));erase(Qt::NoModifier);
        const auto path=directory.filePath("session-erase.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.busy());QVERIFY(!controller.page()->strokes.front().erasedRegions.empty());controller.undo();QVERIFY(hitTest(controller.page()->strokes.front(),{70,60},0));controller.redo();QVERIFY(!hitTest(controller.page()->strokes.front(),{70,60},0));
        controller.home();controller.openPath(path);QTRY_VERIFY(!controller.loading());QVERIFY(controller.page()->strokes.front().erasedRegions.empty());QVERIFY(controller.page()->erasedInk.empty());QVERIFY(controller.page()->strokes.front().eraseMask);QVERIFY(!controller.canUndo());erase(Qt::ShiftModifier);QVERIFY(!hitTest(controller.page()->strokes.front(),{70,60},0));QVERIFY(controller.page()->strokes.front().erasedRegions.empty());QVERIFY(controller.shutdown());
    }
    void deletingPagesKeepsCurrentPageAndLastBlankPage(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();controller.addPage();controller.addPage();const auto currentId=controller.page()->id;
        auto* popup=window->findChild<QObject*>("pagesPanel");QVERIFY(popup);QVERIFY(QMetaObject::invokeMethod(popup,"open"));QTest::qWait(100);auto* button=findVisualItem(window->contentItem(),"deletePageButton_1");QVERIFY(button);QVERIFY(button->isVisible());QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene({button->width()/2,button->height()/2}).toPoint());QCOMPARE(controller.pageCount(),2);QCOMPARE(controller.currentPage(),1);QCOMPARE(controller.page()->id,currentId);
        controller.deletePage(1);QCOMPARE(controller.pageCount(),1);QCOMPARE(controller.currentPage(),0);StrokeObject pen;pen.id=newId();pen.samples={{{10,10},1},{{20,20},1}};controller.addStroke(pen);controller.deletePage(0);QCOMPARE(controller.pageCount(),1);QVERIFY(controller.page()->strokes.empty());controller.deletePage(-1);controller.deletePage(10);QCOMPARE(controller.pageCount(),1);
        const auto path=directory.filePath("deleted-pages.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.busy());const auto loaded=ProjectStore::load(path);QVERIFY(loaded);QCOMPARE(loaded.project.pages.size(),std::size_t(1));QVERIFY(loaded.project.pages.front().strokes.empty());QVERIFY(warnings.isEmpty());QVERIFY(controller.shutdown());
    }
    void erasedGeometryCannotBeSelectedAndAnglesHide(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        ShapeObject sector;sector.id=newId();sector.kind=ShapeKind::CircularSector;sector.center={80,80};sector.radiusX=sector.radiusY=12;sector.showAngle=true;sector.vertices={sector.center};for(int i=0;i<=32;++i){const double a=i*std::numbers::pi/64;sector.vertices.push_back(sector.center+Point{12*std::cos(a),12*std::sin(a)});}controller.addShape(sector);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("select");QCOMPARE(canvas.sectorAngles().size(),1);
        const auto send=[&](QEvent::Type type,QPointF world){const auto p=canvas.screenPoint(world);QMouseEvent event(type,p,p,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);if(type==QEvent::MouseButtonPress)canvas.mousePressEvent(&event);else if(type==QEvent::MouseMove)canvas.mouseMoveEvent(&event);else canvas.mouseReleaseEvent(&event);};
        send(QEvent::MouseButtonPress,{84,84});send(QEvent::MouseButtonRelease,{84,84});QCOMPARE(canvas.selectedCount(),1);
        auto hidden=sector;hidden.erasedRegions.push_back({{80,80},{80,80},30});++hidden.properties.revision;controller.changeObjects({{CanvasObject(sector),CanvasObject(hidden)}},CommandKind::ChangeStyle);QCOMPARE(canvas.selectedCount(),0);QVERIFY(canvas.sectorAngles().isEmpty());
        // A click outside the eraser disk but within selection tolerance of the
        // original contour must not select invisible geometry.
        send(QEvent::MouseButtonPress,{92,80});send(QEvent::MouseButtonRelease,{92,80});QCOMPARE(canvas.selectedCount(),0);
        send(QEvent::MouseButtonPress,{40,40});send(QEvent::MouseMove,{120,120});send(QEvent::MouseButtonRelease,{120,120});QCOMPARE(canvas.selectedCount(),0);
        controller.undo();QCOMPARE(canvas.sectorAngles().size(),1);send(QEvent::MouseButtonPress,{84,84});send(QEvent::MouseButtonRelease,{84,84});QCOMPARE(canvas.selectedCount(),1);controller.redo();QCOMPARE(canvas.selectedCount(),0);QVERIFY(canvas.sectorAngles().isEmpty());
        ShapeObject line;line.id=newId();line.kind=ShapeKind::Line;line.vertices={{40,140},{100,140}};line.erasedRegions={{{40,140},{70,140},2}};controller.addShape(line);
        send(QEvent::MouseButtonPress,{50,142.2});send(QEvent::MouseButtonRelease,{50,142.2});QCOMPARE(canvas.selectedCount(),0);
        send(QEvent::MouseButtonPress,{90,140});send(QEvent::MouseButtonRelease,{90,140});QCOMPARE(canvas.selectedCount(),1);QVERIFY(controller.shutdown());
    }
    void sectorAngleTogglePersistsAndFollowsView(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);
        ShapeObject sector;sector.id=newId();sector.kind=ShapeKind::CircularSector;sector.center={100,100};sector.radiusX=sector.radiusY=20;sector.vertices={sector.center};for(int i=0;i<=32;++i){const double a=i*std::numbers::pi/64;sector.vertices.push_back(sector.center+Point{20*std::cos(a),20*std::sin(a)});}controller.addShape(sector);canvas->setTool("select");
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(canvas->screenPoint({107,107})).toPoint());QCOMPARE(canvas->selectionName(),QString::fromUtf8("Setor circular"));auto* toggle=findVisualItem(window->contentItem(),"showSectorAngleToggle");QVERIFY(toggle);QVERIFY(toggle->isVisible());QVERIFY(!canvas->selectedShowAngle());QVERIFY(QMetaObject::invokeMethod(toggle,"toggle"));QVERIFY(QMetaObject::invokeMethod(toggle,"toggled"));QVERIFY(canvas->selectedShowAngle());QCOMPARE(canvas->sectorAngles().size(),1);QVERIFY(std::abs(canvas->sectorAngles().front().toMap()["angle"].toDouble()-90)<1e-8);
        QTRY_VERIFY(findVisualItem(window->contentItem(),"sectorAngleLabel"));const auto position=canvas->sectorAngles().front().toMap();canvas->zoomBy(1.5);QVERIFY(canvas->sectorAngles().front().toMap()!=position);controller.undo();QVERIFY(canvas->sectorAngles().isEmpty());controller.redo();QCOMPARE(canvas->sectorAngles().size(),1);
        const auto path=directory.filePath("angle.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.busy());controller.home();controller.openPath(path);QTRY_VERIFY(!controller.loading());QVERIFY(controller.page()->shapes.front().showAngle);QCOMPARE(canvas->sectorAngles().size(),1);QVERIFY(warnings.isEmpty());QVERIFY(controller.shutdown());
    }
    void eraserReleaseHidesBeforePositionChanges(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("eraser");
        const auto send=[&](QEvent::Type type,QPointF world){const auto p=canvas.screenPoint(world);QMouseEvent event(type,p,p,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);if(type==QEvent::MouseButtonPress)canvas.mousePressEvent(&event);else if(type==QEvent::MouseMove)canvas.mouseMoveEvent(&event);else canvas.mouseReleaseEvent(&event);};
        send(QEvent::MouseButtonPress,{50,50});for(int i=0;i<100;++i)send(QEvent::MouseMove,{50+i*.1,50});QVERIFY(canvas.eraserVisible());
        bool visibleDuringRelease=false;connect(&canvas,&CanvasItem::selectionChanged,&canvas,[&]{visibleDuringRelease|=canvas.eraserVisible();});
        send(QEvent::MouseButtonRelease,{0,0});QVERIFY(!visibleDuringRelease);QVERIFY(!canvas.eraserVisible());QVERIFY(controller.shutdown());
    }
    void segmentAnglesSnapAndPatternSettingsUndo(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        ShapeObject line;line.id=newId();line.kind=ShapeKind::Line;line.vertices={{50,100},{100,100}};controller.addShape(line);
        InspectableCanvas canvas;canvas.setWidth(1200);canvas.setHeight(1000);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("select");
        const auto send=[&](QEvent::Type type,QPointF world,Qt::KeyboardModifiers modifiers=Qt::NoModifier){const auto p=canvas.screenPoint(world);QMouseEvent event(type,p,p,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,modifiers);if(type==QEvent::MouseButtonPress)canvas.mousePressEvent(&event);else if(type==QEvent::MouseMove)canvas.mouseMoveEvent(&event);else canvas.mouseReleaseEvent(&event);};
        send(QEvent::MouseButtonPress,{75,100});QCOMPARE(canvas.segmentGuide()["angle"].toDouble(),0.);send(QEvent::MouseButtonRelease,{75,100});QVERIFY(canvas.segmentGuide().isEmpty());
        const double radians=22*std::numbers::pi/180;const QPointF target(50+50*std::cos(radians),100-50*std::sin(radians));
        send(QEvent::MouseButtonPress,{100,100});send(QEvent::MouseMove,target,Qt::ControlModifier);QVERIFY(std::abs(canvas.segmentGuide()["angle"].toDouble()-15)<1e-8);send(QEvent::MouseButtonRelease,target,Qt::ControlModifier);QVERIFY(canvas.segmentGuide().isEmpty());
        const auto angle=[&]{const auto delta=controller.page()->shapes.front().vertices[1]-controller.page()->shapes.front().vertices[0];return std::atan2(-delta.y,delta.x)*180/std::numbers::pi;};
        QVERIFY(std::abs(angle()-15)<1e-8);controller.undo();QVERIFY(std::abs(angle())<1e-8);controller.redo();QVERIFY(std::abs(angle()-15)<1e-8);
        auto rotateHandle=QVariantMap{};for(const auto& handle:canvas.selectionHandles())if(handle.toMap()["type"].toString()=="rotate")rotateHandle=handle.toMap();QVERIFY(!rotateHandle.isEmpty());
        const auto shape=controller.page()->shapes.front();const auto center=bounds(CanvasObject(shape)).center();const QPointF pivot(center.x,center.y);const auto screenOrigin=canvas.screenPoint({0,0});const double scale=canvas.screenPoint({1,0}).x()-screenOrigin.x();const auto pointer=(QPointF(rotateHandle["x"].toDouble(),rotateHandle["y"].toDouble())-screenOrigin)/scale;
        const double turn=-17*std::numbers::pi/180;const auto offset=pointer-pivot;const QPointF rotated=pivot+QPointF(offset.x()*std::cos(turn)-offset.y()*std::sin(turn),offset.x()*std::sin(turn)+offset.y()*std::cos(turn));
        send(QEvent::MouseButtonPress,pointer);send(QEvent::MouseMove,rotated,Qt::ControlModifier);QVERIFY(std::abs(canvas.segmentGuide()["angle"].toDouble()-30)<1e-8);send(QEvent::MouseButtonRelease,rotated,Qt::ControlModifier);QVERIFY(std::abs(angle()-30)<1e-8);
        canvas.setSelectedPattern(1);canvas.setSelectedDashLength(2.25);canvas.setSelectedGapLength(.45);QCOMPARE(controller.page()->shapes.front().style.dashLengthMm,2.25);QCOMPARE(controller.page()->shapes.front().style.gapLengthMm,.45);controller.undo();QCOMPARE(controller.page()->shapes.front().style.gapLengthMm,PenStyle{}.gapLengthMm);controller.redo();QCOMPARE(canvas.selectedGapLength(),.45);
        canvas.setSelectedPattern(2);canvas.setSelectedDotSpacing(.6);QCOMPARE(controller.page()->shapes.front().style.dotSpacingMm,.6);QVERIFY(PenStyle{}.dashLengthMm<3);QVERIFY(PenStyle{}.dotSpacingMm<2.5);QVERIFY(controller.shutdown());
    }
    void penShortcutPreservesActiveInk(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);
        const auto screen=[&](QPointF p){return canvas->mapToScene(canvas->screenPoint(p)).toPoint();};
        const std::vector<std::pair<int,QString>> shortcuts{{Qt::Key_P,"pen"},{Qt::Key_M,"marker"},{Qt::Key_E,"eraser"},{Qt::Key_V,"select"},{Qt::Key_T,"text"},{Qt::Key_H,"hand"}};
        for(const auto& tool:{QString("pen"),QString("marker")})for(const auto& [key,target]:shortcuts){
            canvas->setTool(tool);canvas->forceActiveFocus();const auto count=controller.page()->strokes.size();
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen({30,40}));QTest::mouseMove(window,screen({50,50}),0);QVERIFY(canvas->drawing());QTest::keyPress(window,Qt::Key(key));
            QCOMPARE(canvas->tool(),target);if(tool==target)QVERIFY(canvas->drawing());else{QVERIFY(!canvas->drawing());QCOMPARE(controller.page()->strokes.size(),count+1);QCOMPARE(controller.page()->strokes.back().marker,tool=="marker");}
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({70,40}),0);QCOMPARE(controller.page()->strokes.size(),count+1);QCOMPARE(controller.page()->strokes.back().marker,tool=="marker");QVERIFY(controller.page()->strokes.back().samples.size()>=2);
            QTest::keyRelease(window,Qt::Key(key));
            controller.undo();QCOMPARE(controller.page()->strokes.size(),count);controller.redo();QCOMPARE(controller.page()->strokes.size(),count+1);
        }
        QVERIFY(controller.shutdown());
    }
    void manyMarkersRefineAsynchronouslyAndIgnoreStaleResults(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();auto background=controller.background();background["gridType"]=int(GridType::None);controller.setBackground(background);
        std::vector<std::string> ids;
        for(int n=0;n<32;++n){StrokeObject marker;marker.id=newId();marker.marker=true;marker.style.rgba=0xff000059;marker.style.minWidthMm=marker.style.maxWidthMm=4;for(int i=0;i<4000;++i)marker.samples.push_back({{30+140.*i/3999,40+n*6.+std::sin(i*.008)},1});ids.push_back(marker.id);controller.addStroke(marker);}
        QQuickWindow window;window.resize(1000,700);auto* canvas=new ZoomInspectableCanvas(window.contentItem());canvas->setWidth(1000);canvas->setHeight(700);canvas->setController(&controller);canvas->fitPage();window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));QVERIFY(!window.grabWindow().isNull());const auto before=canvas->textures;QVERIFY(before.size()>32);canvas->maxSyncNs=0;
        for(int i=0;i<4;++i){canvas->zoomBy(1.4);QVERIFY(!window.grabWindow().isNull());}
        QTest::qWait(180);QVERIFY(!window.grabWindow().isNull());
        // Undo while refinement may still be running. A worker owns its data;
        // its eventual result must never resurrect the removed marker.
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(31));
        QTRY_VERIFY_WITH_TIMEOUT((window.grabWindow(),canvas->textures!=before),5000);
        QTest::qWait(200);QVERIFY(!window.grabWindow().isNull());QVERIFY(!findObject(*controller.page(),ids.back()));
        qInfo()<<"32 markers / 128,000 samples; maximum refinement CPU scene synchronization:"<<canvas->maxSyncNs/1000000.<<"ms";
        controller.redo();QCOMPARE(controller.page()->strokes.size(),std::size_t(32));canvas->fitPage();QVERIFY(!window.grabWindow().isNull());QVERIFY(findObject(*controller.page(),ids.back()));window.hide();delete canvas;QVERIFY(controller.shutdown());
    }
    void selectionFramesRotateAndTextResizeReflows(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        TextObject text;text.id=newId();text.source="A sentence with enough words to wrap onto several lines when the text box becomes narrow.";text.fontFamily="Lobster Two";text.boxWidthMm=80;text.corners={{50,80},{130,80},{130,100},{50,100}};
        const auto prepared=prepareText(text);QVERIFY(prepared.error.isEmpty());text=prepared.object;controller.cacheTextVisual(text.id,{prepared.image,prepared.geometry,prepared.error,prepared.naturalSize});controller.changeObjects({{{},CanvasObject(text)}},CommandKind::AddObject);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(800);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("select");
        const auto send=[&](QEvent::Type type,QPointF screen,Qt::KeyboardModifiers modifiers=Qt::NoModifier){QMouseEvent event(type,screen,screen,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,modifiers);if(type==QEvent::MouseButtonPress)canvas.mousePressEvent(&event);else if(type==QEvent::MouseMove)canvas.mouseMoveEvent(&event);else canvas.mouseReleaseEvent(&event);};
        const auto click=[&](Point p,Qt::KeyboardModifiers modifiers=Qt::NoModifier){const auto s=canvas.screenPoint({p.x,p.y});send(QEvent::MouseButtonPress,s,modifiers);send(QEvent::MouseButtonRelease,s,modifiers);};
        const auto handle=[&](const QString& type){for(const auto& value:canvas.selectionHandles()){const auto h=value.toMap();if(h["type"].toString()==type)return QPointF(h["x"].toDouble(),h["y"].toDouble());}return QPointF{};};
        click(text.corners[0]+Point{5,5});QCOMPARE(canvas.selectedCount(),1);
        const auto frame=canvas.selectionFrame();const auto center=frame.center(),rotate=handle("rotate");const auto delta=rotate-center;const QPointF rotated=center+QPointF(-delta.y(),delta.x());
        send(QEvent::MouseButtonPress,rotate);send(QEvent::MouseMove,rotated);QVERIFY(std::abs(canvas.selectionRotation()-90)<1e-6);send(QEvent::MouseButtonRelease,rotated);
        const auto rotatedText=controller.page()->texts[0];QVERIFY(std::abs(canvas.selectionRotation()-90)<1e-6);
        const auto frameOrigin=canvas.selectionFrame().topLeft();const auto resize=handle("resize");
        const auto bottomRight=frameOrigin+QPointF(-canvas.selectionFrame().height(),canvas.selectionFrame().width());QVERIFY(QLineF(resize,bottomRight).length()<1e-6);
        QHoverEvent hover(QEvent::HoverMove,resize,resize,resize,Qt::NoModifier);canvas.hoverMoveEvent(&hover);QCOMPARE(canvas.cursor().shape(),Qt::SizeBDiagCursor);
        const auto target=frameOrigin+(resize-frameOrigin)/2;
        send(QEvent::MouseButtonPress,resize);QCOMPARE(canvas.cursor().shape(),Qt::SizeBDiagCursor);send(QEvent::MouseMove,target);QCOMPARE(canvas.cursor().shape(),Qt::SizeBDiagCursor);send(QEvent::MouseButtonRelease,target);QCOMPARE(canvas.cursor().shape(),Qt::ArrowCursor);
        const auto resized=controller.page()->texts[0];QCOMPARE(resized.fontSizePt,text.fontSizePt);QVERIFY(resized.boxWidthMm<text.boxWidthMm*.6);
        QVERIFY(length(resized.corners[3]-resized.corners[0])>length(rotatedText.corners[3]-rotatedText.corners[0]));
        const auto natural=textNaturalSize(resized);QVERIFY(std::abs(length(resized.corners[1]-resized.corners[0])/natural.width()-1)<1e-6);
        QVERIFY(length(resized.corners[0]-rotatedText.corners[0])<1e-6);
        QVERIFY(resized.boxHeightMm>=5);
        const auto heightResize=handle("resize");const auto taller=heightResize+QPointF(-canvas.selectionFrame().height(),0);
        send(QEvent::MouseButtonPress,heightResize);send(QEvent::MouseMove,taller);send(QEvent::MouseButtonRelease,taller);
        const auto expanded=controller.page()->texts[0];QCOMPARE(expanded.fontSizePt,text.fontSizePt);QVERIFY(std::abs(expanded.boxWidthMm-resized.boxWidthMm)<1e-6);QVERIFY(expanded.boxHeightMm>resized.boxHeightMm);
        controller.undo();
        controller.undo();QCOMPARE(controller.page()->texts[0].boxWidthMm,text.boxWidthMm);QVERIFY(std::abs(canvas.selectionRotation()-90)<1e-6);
        controller.undo();QVERIFY(std::abs(canvas.selectionRotation())<1e-6);controller.redo();QVERIFY(std::abs(canvas.selectionRotation()-90)<1e-6);
        canvas.setTool("pen");canvas.setTool("select");
        StrokeObject first;first.id=newId();first.samples={{{40,180},1},{{70,180},1}};auto second=first;second.id=newId();second.samples={{{40,210},1},{{70,210},1}};controller.addStroke(first);controller.addStroke(second);
        click({50,180});click({50,210},Qt::ShiftModifier);QCOMPARE(canvas.selectedCount(),2);
        const auto group=canvas.selectionFrame();const auto groupRotate=handle("rotate"),groupDelta=groupRotate-group.center();const auto groupTarget=group.center()+QPointF(-groupDelta.y(),groupDelta.x());
        send(QEvent::MouseButtonPress,groupRotate);send(QEvent::MouseMove,groupTarget);send(QEvent::MouseButtonRelease,groupTarget);QVERIFY(std::abs(canvas.selectionRotation()-90)<1e-6);
        const auto groupResize=handle("resize");const auto doubleSize=canvas.selectionFrame().topLeft()+(groupResize-canvas.selectionFrame().topLeft())*2;
        send(QEvent::MouseButtonPress,groupResize);send(QEvent::MouseMove,doubleSize);send(QEvent::MouseButtonRelease,doubleSize);
        const auto& stroke=controller.page()->strokes[0];QVERIFY(std::abs(length(stroke.samples.back().position-stroke.samples.front().position)-60)<1e-6);
        controller.undo();controller.undo();QVERIFY(std::abs(canvas.selectionRotation())<1e-6);controller.redo();QVERIFY(std::abs(canvas.selectionRotation()-90)<1e-6);QVERIFY(controller.shutdown());
    }
    void rotatedTextControlsAndInlineCornerResize(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        controller.upsertText({},QPointF(90,100),{{"source","Short text"},{"fontSizePt",16},{"boxWidthMm",60},{"boxHeightMm",30}});QTRY_VERIFY(!controller.textBusy());QCOMPARE(controller.page()->texts.size(),std::size_t(1));
        const auto original=controller.page()->texts[0];const auto rotated=transformed(CanvasObject(original),original.corners[0],{},1,1,std::numbers::pi/4);controller.changeObjects({{CanvasObject(original),rotated}},CommandKind::TransformObject);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("select");canvas->selectText(QString::fromStdString(original.id));
        auto* controls=findVisualItem(window->contentItem(),"selectedTextControls");QVERIFY(controls);QVERIFY(controls->isVisible());QVERIFY(std::abs(controls->rotation()-45)<1e-6);
        canvas->editSelectedText();auto* inlineEditor=window->findChild<QObject*>("inlineTextEditor");QVERIFY(inlineEditor);QTRY_VERIFY(inlineEditor->property("active").toBool());QCoreApplication::processEvents();
        auto* selectionFrame=findVisualItem(window->contentItem(),"selectionFrame");auto* handles=window->findChild<QObject*>("selectionHandleRepeater");QVERIFY(selectionFrame);QVERIFY(handles);QVERIFY(!selectionFrame->isVisible());QCOMPARE(handles->property("count").toInt(),0);
        auto* box=findVisualItem(window->contentItem(),"inlineTextBox");auto* inlineControls=findVisualItem(window->contentItem(),"inlineTextControls");auto* resize=findVisualItem(window->contentItem(),"inlineTextResizeHandle");QVERIFY(box);QVERIFY(inlineControls);QVERIFY(resize);QCOMPARE(inlineControls->parentItem(),box);QVERIFY(std::abs(box->rotation()-45)<1e-6);
        const double width=inlineEditor->property("boxWidth").toDouble(),height=inlineEditor->property("boxHeight").toDouble();
        const auto start=resize->mapToScene({resize->width()/2,resize->height()/2});const auto end=box->mapToScene({box->width()+40,box->height()+30});
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start.toPoint());QTest::mouseMove(window,end.toPoint());QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,end.toPoint());
        QVERIFY(inlineEditor->property("boxWidth").toDouble()>width);QVERIFY(inlineEditor->property("boxHeight").toDouble()>height);QCOMPARE(inlineEditor->property("draft").toMap()["fontSizePt"].toDouble(),16.);
        QVERIFY(!selectionFrame->isVisible());QCOMPARE(handles->property("count").toInt(),0);
        QTest::keyClick(window,Qt::Key_Return,Qt::ControlModifier);QTRY_VERIFY(!inlineEditor->property("active").toBool());const auto updated=controller.page()->texts[0];QCOMPARE(updated.fontSizePt,16.);QVERIFY(updated.boxWidthMm>width);QVERIFY(updated.boxHeightMm>height);const auto edge=updated.corners[1]-updated.corners[0];QVERIFY(std::abs(std::atan2(edge.y,edge.x)-std::numbers::pi/4)<1e-6);
        QTRY_VERIFY(selectionFrame->isVisible());QTRY_VERIFY(handles->property("count").toInt()>0);
        canvas->editSelectedText();QTRY_VERIFY(inlineEditor->property("active").toBool());QCoreApplication::processEvents();
        auto* source=findVisualItem(window->contentItem(),"textSource");QVERIFY(source);QVERIFY(QMetaObject::invokeMethod(source,"select",Q_ARG(int,0),Q_ARG(int,5)));
        auto* swatch=findVisualItem(window->contentItem(),"inlineTextColor_3");QVERIFY(swatch);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,swatch->mapToScene({swatch->width()/2,swatch->height()/2}).toPoint());
        QCOMPARE(source->property("selectionStart").toInt(),0);QCOMPARE(source->property("selectionEnd").toInt(),5);
        auto* wrapper=source->property("textDocument").value<QObject*>();const auto coloredFormats=controller.textFormats(wrapper,false,false);QVERIFY(!coloredFormats.isEmpty());QCOMPARE(coloredFormats[0].toMap()["start"].toInt(),0);QCOMPARE(coloredFormats[0].toMap()["length"].toInt(),5);QVERIFY(coloredFormats[0].toMap().contains("color"));
        auto* formatButton=findVisualItem(window->contentItem(),"textFormatButton");QVERIFY(formatButton);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,formatButton->mapToScene({formatButton->width()/2,formatButton->height()/2}).toPoint());
        auto* dialog=window->findChild<QObject*>("textDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());auto draft=dialog->property("draft").toMap();draft["color"]="#cc5364";dialog->setProperty("draft",draft);
        auto* apply=findVisualItem(window->contentItem(),"applyTextFormatButton");QVERIFY(apply);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,apply->mapToScene({apply->width()/2,apply->height()/2}).toPoint());QTRY_VERIFY(!dialog->property("visible").toBool());
        const auto chosen=controller.textFormats(wrapper,false,false);QCOMPARE(QColor(chosen[0].toMap()["color"].toString()),QColor("#cc5364"));QCOMPARE(chosen[0].toMap()["length"].toInt(),5);
        QVERIFY(window->grabWindow().save("text-editing-toolbar.png"));
        QTest::keyClick(window,Qt::Key_Return,Qt::ControlModifier);QTRY_VERIFY(!inlineEditor->property("active").toBool());const auto coloredText=controller.page()->texts[0];QVERIFY(coloredText.formats[0].rgba);QCOMPARE(*coloredText.formats[0].rgba,std::uint32_t(0xcc5364ff));QCOMPARE(coloredText.style.rgba,updated.style.rgba);
        const auto loaded=ProjectStore::deserialize(ProjectStore::serialize(Project{newId(),"Colors","now","now",{*controller.page()}}));QVERIFY(loaded);QCOMPARE(loaded.project.pages[0].texts[0].formats,coloredText.formats);
        canvas->editSelectedText();QTRY_VERIFY(inlineEditor->property("active").toBool());QCoreApplication::processEvents();QVERIFY(QMetaObject::invokeMethod(source,"select",Q_ARG(int,0),Q_ARG(int,0)));
        swatch=findVisualItem(window->contentItem(),"inlineTextColor_1");QVERIFY(swatch);const auto wholeColor=swatch->property("swatch").value<QColor>();QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,swatch->mapToScene({swatch->width()/2,swatch->height()/2}).toPoint());
        const auto wholeFormats=controller.textFormats(wrapper,false,false);QCOMPARE(wholeFormats.size(),1);QCOMPARE(wholeFormats[0].toMap()["length"].toInt(),source->property("text").toString().size());QCOMPARE(QColor(wholeFormats[0].toMap()["color"].toString()).rgba(),wholeColor.rgba());
        formatButton=findVisualItem(window->contentItem(),"textFormatButton");QVERIFY(formatButton);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,formatButton->mapToScene({formatButton->width()/2,formatButton->height()/2}).toPoint());QTRY_VERIFY(dialog->property("visible").toBool());draft=dialog->property("draft").toMap();draft["color"]="#397ce0";dialog->setProperty("draft",draft);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,apply->mapToScene({apply->width()/2,apply->height()/2}).toPoint());QTRY_VERIFY(!dialog->property("visible").toBool());const auto all=controller.textFormats(wrapper,false,false);QCOMPARE(all.size(),1);QCOMPARE(all[0].toMap()["length"].toInt(),source->property("text").toString().size());QCOMPARE(QColor(all[0].toMap()["color"].toString()),QColor("#397ce0"));
        QTest::keyClick(window,Qt::Key_Return,Qt::ControlModifier);QTRY_VERIFY(!inlineEditor->property("active").toBool());
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void mathSourceUsesMonospaceOnlyWhileEditing(){
        QQmlEngine engine;QQmlComponent component(&engine);
        component.setData("import QtQuick\nTextEdit { font.family: \"Lobster Two\"; font.pixelSize: 24; textFormat: TextEdit.PlainText }",QUrl());
        std::unique_ptr<QObject> editor(component.create());QVERIFY2(editor,qPrintable(component.errorString()));
        const auto source=QStringLiteral("Área \\$20 $x^2$ fim\n$$a+b\n=c$$ final");editor->setProperty("text",source);
        auto* wrapper=qobject_cast<QQuickTextDocument*>(editor->property("textDocument").value<QObject*>());QVERIFY(wrapper);
        restoreTextFormats(wrapper,{});auto* document=wrapper->textDocument();QCOMPARE(document->toPlainText(),source);
        const auto familyAt=[&](int position){const auto block=document->findBlock(position);for(const auto& format:block.layout()->formats())
            if(position-block.position()>=format.start&&position-block.position()<format.start+format.length)return format.format.font().families().value(0);return QString{};};
        const auto mono=QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
        QCOMPARE(familyAt(source.indexOf("x^2")),mono);QVERIFY(familyAt(0).isEmpty());QVERIFY(familyAt(source.indexOf("20")).isEmpty());
        QCOMPARE(familyAt(source.indexOf("a+b")),mono);QCOMPARE(familyAt(source.indexOf("=c")),mono);QVERIFY(familyAt(source.indexOf("final")).isEmpty());
        QVERIFY(textFormats(wrapper,false,false).isEmpty());
        const auto updated=source+" $\\int_a^b";editor->setProperty("text",updated);QCoreApplication::processEvents();
        QCOMPARE(familyAt(updated.indexOf("\\int")),mono);QCOMPARE(document->toPlainText(),updated);
        restoreTextFormats(wrapper,{});QCOMPARE(document->findChildren<QSyntaxHighlighter*>().size(),1);
        const auto colored=QStringLiteral("normal $\\frac{12.5}{x_2} \\% % ignored $\n+y$ end");
        editor->setProperty("text",colored);QCoreApplication::processEvents();
        const auto colorAt=[&](int position){const auto block=document->findBlock(position);for(const auto& span:block.layout()->formats())
            if(position-block.position()>=span.start&&position-block.position()<span.start+span.length)return span.format.foreground().color();return QColor{};};
        QCOMPARE(colorAt(colored.indexOf("\\frac")),QColor("#93c5fd"));
        QCOMPARE(colorAt(colored.indexOf("12.5")),QColor("#fcd34d"));
        QCOMPARE(colorAt(colored.indexOf('{')),QColor("#c4b5fd"));
        QCOMPARE(colorAt(colored.indexOf("ignored")),QColor("#94a3b8"));
        QCOMPARE(colorAt(colored.indexOf("\\%")),QColor("#93c5fd"));
        QCOMPARE(familyAt(colored.indexOf("+y")),mono);QVERIFY(familyAt(colored.indexOf("end")).isEmpty());
        editor->setProperty("syntaxBackground",QColor("white"));restoreTextFormats(wrapper,{});
        QCOMPARE(colorAt(colored.indexOf("\\frac")),QColor("#1d4ed8"));
        QVERIFY(textFormats(wrapper,false,false).isEmpty());QCOMPARE(document->toPlainText(),colored);
        editor->setProperty("text","Alpha Beta");restoreTextFormats(wrapper,{{QVariantMap{{"start",0},{"length",5},{"bold",true},{"italic",false}}}});
        colorTextSelection(wrapper,6,10,QColor("#cc5364"));const auto spans=textFormats(wrapper,false,false);QCOMPARE(spans.size(),2);QVERIFY(!spans[0].toMap().contains("color"));QCOMPARE(spans[1].toMap()["start"].toInt(),6);QCOMPARE(spans[1].toMap()["length"].toInt(),4);
        restoreTextFormats(wrapper,spans);QCOMPARE(textFormats(wrapper,false,false),spans);
        TextObject t;t.source="Alpha Beta";t.fontFamily="Lobster Two";t.style.rgba=0x397ce0ff;t.formats={{6,4,false,false,0xcc5364ff}};
        const auto bitmap=textBitmap(t,8);QVERIFY(!bitmap.isNull());bool red=false,blue=false;for(int y=0;y<bitmap.height();++y)for(int x=0;x<bitmap.width();++x){const auto color=bitmap.pixelColor(x,y);if(color.alpha()>220){red|=color.red()>color.blue()*1.5;blue|=color.blue()>color.red()*1.5;}}QVERIFY(red);QVERIFY(blue);
        QImage vector(bitmap.size(),QImage::Format_ARGB32_Premultiplied);vector.fill(Qt::transparent);QPainter painter(&vector);painter.scale(8,8);QVERIFY(paintVectorText(painter,t).isEmpty());painter.end();red=false;blue=false;for(int y=0;y<vector.height();++y)for(int x=0;x<vector.width();++x){const auto color=vector.pixelColor(x,y);if(color.alpha()>220){red|=color.red()>color.blue()*1.5;blue|=color.blue()>color.red()*1.5;}}QVERIFY(red);QVERIFY(blue);
        t.source="$x$";t.formats={{1,1,false,false,0xcc5364ff}};t.math={{"x","<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1000 1000\"><path d=\"M0 0L1000 0L0 1000Z\"/></svg>",false,0,3,1,1}};
        const auto mathVisual=textVisual(t,8);QVERIFY(mathVisual.error.isEmpty());QVERIFY(!mathVisual.math.empty());QCOMPARE(mathVisual.mathColors.size(),std::size_t(1));QCOMPARE(mathVisual.mathColors[0].rgba,std::uint32_t(0xcc5364ff));QCOMPARE(mathVisual.mathColors[0].count,mathVisual.math.size());
    }
    void selectionMovesFromEmptyInteriorAndOffersExports(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        StrokeObject first;first.id=newId();first.samples={{{85,135},1},{{105,135},1}};auto second=first;second.id=newId();second.samples={{{85,165},1},{{105,165},1}};
        controller.addStroke(first);controller.addStroke(second);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTest::qWait(50);canvas->setTool("select");canvas->setZoom(1);QTest::qWait(50);
        const auto point=[&](Point p){return canvas->mapToScene(canvas->screenPoint({p.x,p.y})).toPoint();};
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point({95,135}));QCOMPARE(canvas->selectedCount(),1);QTest::mouseClick(window,Qt::LeftButton,Qt::ShiftModifier,point({95,165}));QCOMPARE(canvas->selectedCount(),2);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point({95,150}));QTest::mouseMove(window,point({100,155}));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,point({100,155}));
        QCOMPARE(canvas->selectedCount(),2);QVERIFY(std::abs(controller.page()->strokes[0].samples[0].position.x-90)<.3);
        controller.undo();QCOMPARE(controller.page()->strokes[0].samples[0].position.x,85.);
        QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,point({95,150}));
        auto* menu=window->findChild<QObject*>("selectionContextMenu");QVERIFY(menu);QTRY_VERIFY(menu->property("opened").toBool());QCOMPARE(canvas->selectedCount(),2);
        QVERIFY(findVisualItem(window->contentItem(),"contextExportSelectionPdfButton"));QVERIFY(findVisualItem(window->contentItem(),"contextExportSelectionSvgButton"));
        QVERIFY(QMetaObject::invokeMethod(menu,"close"));
        QVERIFY(findVisualItem(window->contentItem(),"exportSelectionPdfButton"));QVERIFY(findVisualItem(window->contentItem(),"exportSelectionSvgButton"));
        canvas->exportSelection(QUrl::fromLocalFile(directory.filePath("selected.svg")),true);QTRY_VERIFY(!controller.exporting());QVERIFY(QFileInfo::exists(directory.filePath("selected.svg")));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void freehandSmoothingRemovesJitterAndKeepsLivePathStable(){
        std::vector<PointerSample> raw;for(int i=0;i<100;++i)raw.push_back({{i*.2,std::sin(i*.07)+(i%2?.08:-.08)},.7});
        const auto smooth=smoothStrokeSamples(raw);QVERIFY(smooth.front().position==raw.front().position);QVERIFY(smooth.back().position==raw.back().position);
        double before=0,after=0;for(int i=3;i<97;++i){before+=std::abs(raw[i].position.y-std::sin(i*.07));
            double nearest=1e9;Point point{};for(const auto& sample:smooth){const double distance=std::abs(sample.position.x-raw[i].position.x);if(distance<nearest){nearest=distance;point=sample.position;}}
            after+=std::abs(point.y-std::sin(point.x*.35));}QVERIFY(after<before*.5);
        std::vector<PointerSample> input,live{raw.front()};std::size_t stable=1;
        for(const auto& sample:raw){input.push_back(sample);live.resize(stable);
            if(input.size()>=5){appendSmoothStrokeSegment(input,input.size()-5,live);stable=live.size();}
            for(std::size_t segment=input.size()>4?input.size()-4:0;segment+1<input.size();++segment)appendSmoothStrokeSegment(input,segment,live);}
        QCOMPARE(live.size(),smooth.size());for(std::size_t i=0;i<live.size();++i)QVERIFY(length(live[i].position-smooth[i].position)<1e-9);
        const std::vector<PointerSample> corner{{{0,0},1},{{2,0},1},{{2,2},1}};const auto result=smoothStrokeSamples(corner);
        QVERIFY(std::any_of(result.begin(),result.end(),[](const auto& sample){return length(sample.position-Point{2,0})<1e-9;}));
    }
    void compactPenMeshPreservesPressureCurvesAndRoundEnds(){
        std::vector<StrokeObject> cases;
        StrokeObject curve;curve.style.minWidthMm=.15;curve.style.maxWidthMm=1.2;
        for(int i=0;i<1200;++i)curve.samples.push_back({{2+36.*i/1199,10+3*std::sin(i*.01)},.5+.4*std::sin(i*.02)});cases.push_back(curve);
        auto straight=curve;for(auto& sample:straight.samples)sample.position.y=6;cases.push_back(straight);
        auto corners=curve;corners.samples={{{5,5},1},{{15,15},1},{{15,5},1},{{25,5},1},{{5,5},1}};cases.push_back(corners);
        auto reversed=corners;reversed.samples={{{5,5},1},{{30,5},1},{{5,5},1},{{5,5},.3},{{30,8},1}};cases.push_back(reversed);
        auto tap=corners;tap.samples={{{20,10},1},{{20,10},.3},{{20,10},1}};cases.push_back(tap);
        const auto image=[](const std::vector<Point>& mesh){QImage bitmap(1344,704,QImage::Format_RGB32);bitmap.fill(Qt::white);QPainter painter(&bitmap);painter.scale(32,32);QPainterPath path;path.setFillRule(Qt::WindingFill);
            for(std::size_t i=0;i+2<mesh.size();i+=3){auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];if((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)<0)std::swap(b,c);path.moveTo(a.x,a.y);path.lineTo(b.x,b.y);path.lineTo(c.x,c.y);path.closeSubpath();}painter.fillPath(path,Qt::black);return bitmap;};
        for(const auto& stroke:cases){const auto original=stroke.samples;const auto compact=strokeDisplayMesh(stroke);QVERIFY(!compact.empty());QCOMPARE(compact.size()%3,std::size_t(0));for(auto p:compact)QVERIFY(std::isfinite(p.x)&&std::isfinite(p.y));
            const auto reference=image(strokeMesh(stroke)),actual=image(compact);int missing=0,extra=0,ink=0;
            const auto nearby=[](const QImage& bitmap,int x,int y){for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(qRed(bitmap.pixel(x+dx,y+dy))<128)return true;return false;};
            for(int y=1;y+1<actual.height();++y)for(int x=1;x+1<actual.width();++x){if(qRed(reference.pixel(x,y))<128){++ink;if(!nearby(actual,x,y))++missing;}if(qRed(actual.pixel(x,y))<128&&!nearby(reference,x,y))++extra;}
            qInfo()<<"Compact pen coverage: missing/extra/reference pixels"<<missing<<extra<<ink;
            QVERIFY(missing<ink*.015+10);QVERIFY(extra<ink*.015+10);QCOMPARE(stroke.samples.size(),original.size());
            for(std::size_t i=0;i<original.size();++i){QVERIFY(stroke.samples[i].position==original[i].position);QCOMPARE(stroke.samples[i].pressure,original[i].pressure);}
        }
        QVERIFY(strokeDisplayMesh(curve).size()<strokeMesh(curve).size()/3);
    }
    void densePenZoomRetainsMeshesAndCullsInvisibleInk(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();auto background=controller.background();background["gridType"]=int(GridType::None);controller.setBackground(background);
        std::size_t legacy=0,compact=0;
        for(int n=0;n<120;++n){StrokeObject pen;pen.id=newId();for(int i=0;i<1200;++i)pen.samples.push_back({{20+160.*i/1199,20+n*2.+std::sin(i*.015)},.5+.3*std::sin(i*.008)});legacy+=strokeMesh(pen).size();compact+=strokeDisplayMesh(pen).size();controller.addStroke(pen);}
        StrokeObject distant;distant.id=newId();distant.samples={{{1000,1000},1},{{1100,1000},1}};controller.addStroke(distant);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.fitPage();std::unique_ptr<QSGNode> scene(canvas.updatePaintNode(nullptr,nullptr));auto* clip=scene->firstChild()->nextSibling();
        QVERIFY(clip->lastChild()->firstChild()->isSubtreeBlocked());std::vector<QSGNode*> nodes;for(auto* child=clip->firstChild();child;child=child->nextSibling())nodes.push_back(child);
        QElapsedTimer timer;timer.start();for(int step=0;step<160;++step){canvas.zoomBy(step<80?1.03:1/1.03);scene.reset(canvas.updatePaintNode(scene.release(),nullptr));std::size_t index=0;for(auto* child=clip->firstChild();child;child=child->nextSibling())QCOMPARE(child,nodes[index++]);QCOMPARE(index,nodes.size());}
        qInfo()<<"Dense pen: original/display vertices"<<legacy<<compact<<"; 160 CPU scene updates"<<timer.elapsed()<<"ms";QVERIFY(compact<legacy/3);QCOMPARE(controller.page()->strokes.front().samples.size(),std::size_t(1200));QVERIFY(controller.shutdown());
    }
    void densePenOnlyZoomRendersOnGpu(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();auto background=controller.background();background["gridType"]=int(GridType::None);background["color"]="#ffffff";controller.setBackground(background);
        for(int n=0;n<120;++n){StrokeObject pen;pen.id=newId();pen.style.rgba=0x202020ff;for(int i=0;i<1200;++i)pen.samples.push_back({{20+160.*i/1199,20+n*2.+std::sin(i*.015)},.5+.3*std::sin(i*.008)});controller.addStroke(pen);}
        QQuickWindow window;window.resize(1000,700);auto* canvas=new ZoomInspectableCanvas(window.contentItem());canvas->setWidth(1000);canvas->setHeight(700);canvas->setController(&controller);canvas->fitPage();window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        if(window.rendererInterface()->graphicsApi()==QSGRendererInterface::Software)QSKIP("Custom pen meshes require the RHI backend.");
        const auto countInk=[](const QImage& image){int count=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).lightness()<100)++count;return count;};
        const auto before=window.grabWindow();QVERIFY(!before.isNull());const int initialInk=countInk(before);QVERIFY(initialInk>1000);canvas->maxSyncNs=0;
        for(int direction:{1,-1})for(int frame=0;frame<4;++frame){for(int step=0;step<5;++step)canvas->zoomBy(direction>0?1.05:1/1.05);const auto rendered=window.grabWindow();QVERIFY(!rendered.isNull());QVERIFY(countInk(rendered)>1000);}
        const auto after=window.grabWindow();QVERIFY(std::abs(countInk(after)-initialInk)<initialInk*.02+20);
        qInfo()<<"Pen-only GPU zoom, 144,000 samples; maximum CPU scene synchronization:"<<canvas->maxSyncNs/1000000.<<"ms";
        QCOMPARE(controller.page()->strokes.size(),std::size_t(120));QCOMPARE(controller.page()->strokes[0].samples.size(),std::size_t(1200));window.hide();delete canvas;QVERIFY(controller.shutdown());
    }
    void denseInkZoomReusesTexturesInBothDirections(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        auto background=controller.background();background["gridType"]=int(GridType::None);controller.setBackground(background);
        for(int n=0;n<24;++n){StrokeObject marker;marker.id=newId();marker.marker=true;marker.style.rgba=0xff000059;marker.style.minWidthMm=marker.style.maxWidthMm=3;
            for(int i=0;i<1000;++i)marker.samples.push_back({{40+100.*i/999,50+n*5.+std::sin(i*.03)},1});controller.addStroke(marker);}
        StrokeObject pen;pen.id=newId();for(int i=0;i<3000;++i)pen.samples.push_back({{40+100.*i/2999,190+std::sin(i*.03)},.5});controller.addStroke(pen);
        QQuickWindow window;window.resize(1000,700);auto* canvas=new ZoomInspectableCanvas(window.contentItem());canvas->setWidth(1000);canvas->setHeight(700);canvas->setController(&controller);canvas->fitPage();
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));QVERIFY(!window.grabWindow().isNull());
        const auto initial=canvas->textures;QVERIFY(initial.size()>24);canvas->maxSyncNs=0;
        // Cross several resolution bands; zooming out reveals more paper but
        // none of these fully cached objects requires new pixels.
        for(int i=0;i<8;++i){for(int step=0;step<4;++step)canvas->zoomBy(1/1.05);QVERIFY(!window.grabWindow().isNull());QCOMPARE(canvas->textures,initial);}
        for(int i=0;i<8;++i){for(int step=0;step<6;++step)canvas->zoomBy(1.05);QVERIFY(!window.grabWindow().isNull());QCOMPARE(canvas->textures,initial);}
        qInfo()<<"80 zoom steps with 24,000 marker samples and 3,000 pen samples; maximum CPU scene synchronization:"<<canvas->maxSyncNs/1000000.<<"ms";
        QTest::qWait(200);QTRY_VERIFY_WITH_TIMEOUT((window.grabWindow(),canvas->textures!=initial),5000);
        window.hide();delete canvas;
        QVERIFY(controller.shutdown());
    }
    void externalEditsInvalidateEraserSnapshot(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        StrokeObject stroke;stroke.id=newId();stroke.samples={{{20,50},1},{{80,50},1}};controller.addStroke(stroke);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("eraser");
        const auto press=[&](QPointF point){const auto p=canvas.screenPoint(point);QMouseEvent event(QEvent::MouseButtonPress,p,p,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);canvas.mousePressEvent(&event);};
        const auto release=[&](QPointF point){const auto p=canvas.screenPoint(point);QMouseEvent event(QEvent::MouseButtonRelease,p,p,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);canvas.mouseReleaseEvent(&event);};
        press({50,50});const auto moved=transformed(stroke,{},{0,100});controller.changeObjects({{CanvasObject(stroke),moved}},CommandKind::TransformObject);release({50,50});
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));QVERIFY(controller.page()->erasedInk.empty());QVERIFY(hitTest(controller.page()->strokes[0],{50,150},0));
        press({50,150});release({50,150});QCOMPARE(controller.page()->strokes.size(),std::size_t(2));QVERIFY(!controller.page()->erasedInk.empty());
        QVERIFY(controller.shutdown());
    }
    void fastMarkerErasingReusesRenderedTiles(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        StrokeObject marker;marker.id=newId();marker.marker=true;marker.style.rgba=0xff000059;marker.style.minWidthMm=marker.style.maxWidthMm=8;
        for(int i=0;i<10000;++i)marker.samples.push_back({{20+160.*i/9999,60+std::sin(i*.005)},1});controller.addStroke(marker);
        QQuickWindow window;window.resize(1000,700);auto* canvas=new MarkerInspectableCanvas(window.contentItem());canvas->setWidth(1000);canvas->setHeight(700);canvas->setController(&controller);canvas->restorePageView();canvas->setTool("eraser");canvas->setEraserRadius(1.5);
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));const auto before=window.grabWindow();QVERIFY(!before.isNull());const auto allocated=canvas->tileCount.load();QVERIFY(allocated>1);canvas->maxSyncNs=0;
        const auto screen=[&](QPointF p){return canvas->screenPoint(p).toPoint();};
        const auto sample=[&](const QImage& image,QPointF p){const auto logical=canvas->screenPoint(p);return image.pixelColor(int(logical.x()*image.width()/window.width()),int(logical.y()*image.height()/window.height()));};
        const auto untouched=sample(before,{50,60}),paper=sample(before,{125,30});
        QTest::mousePress(&window,Qt::LeftButton,Qt::NoModifier,screen({125,60}));
        for(int i=0;i<300;++i){QTest::mouseMove(&window,screen({125+std::sin(i*.05)*.3,60+std::cos(i*.05)*.3}),0);if(i%20==0){
            const auto frames=canvas->renderedFrames.load();canvas->update();QTRY_VERIFY_WITH_TIMEOUT(canvas->renderedFrames.load()>frames,2500);
            QVERIFY(!window.grabWindow().isNull());QCOMPARE(canvas->tileCount.load(),allocated);QVERIFY(canvas->reusedTiles.load()>0);
        }}
        QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,screen({125,60}),0);const auto erased=window.grabWindow();QCOMPARE(sample(erased,{125,60}),paper);QCOMPARE(sample(erased,{50,60}),untouched);
        qInfo()<<"300 fast marker eraser moves; maximum CPU scene synchronization:"<<canvas->maxSyncNs.load()/1000000.<<"ms";
        controller.undo();const auto restored=window.grabWindow();QCOMPARE(sample(restored,{125,60}),sample(before,{125,60}));controller.redo();QCOMPARE(sample(window.grabWindow(),{125,60}),paper);
        QVERIFY(controller.shutdown());
    }
    void mixedEraserFastSweepsPreserveRecoveryAndUnrelatedObjects(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        StrokeObject pen;pen.id=newId();for(int i=0;i<5000;++i)pen.samples.push_back({{20+60.*i/4999,40},.2+.8*(i%10)/9});controller.addStroke(pen);
        auto marker=pen;marker.id=newId();marker.marker=true;marker.style.minWidthMm=marker.style.maxWidthMm=8;marker.style.rgba=0xff000059;for(auto& sample:marker.samples)sample.position.y=60;controller.addStroke(marker);
        ShapeObject shape;shape.id=newId();shape.kind=ShapeKind::Rectangle;shape.vertices={{35,45},{65,45},{65,70},{35,70}};controller.addShape(shape);
        std::vector<std::string> untouched;
        for(int object=0;object<300;++object){StrokeObject distant;distant.id=newId();for(int i=0;i<100;++i)distant.samples.push_back({{1000+object*10.+i*.05,1000+std::sin(i*.05)},1});untouched.push_back(distant.id);controller.addStroke(distant);}
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("eraser");canvas.setEraserRadius(3);canvas.setEraserShapes(true);
        const auto send=[&](QEvent::Type type,QPointF point,Qt::KeyboardModifiers modifiers=Qt::NoModifier){const auto p=canvas.screenPoint(point);QMouseEvent event(type,p,p,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,modifiers);if(type==QEvent::MouseButtonPress)canvas.mousePressEvent(&event);else if(type==QEvent::MouseButtonRelease)canvas.mouseReleaseEvent(&event);else canvas.mouseMoveEvent(&event);};
        QElapsedTimer timer;timer.start();send(QEvent::MouseButtonPress,{50,25});send(QEvent::MouseMove,{50,75});
        for(int i=0;i<500;++i)send(QEvent::MouseMove,{50+std::sin(i*.1),60+std::cos(i*.1)});
        send(QEvent::MouseButtonRelease,{50,75});qInfo()<<"Fast mixed erase: 500 moves, 300 unrelated objects:"<<timer.elapsed()<<"ms";
        QVERIFY(!hitTest(controller.page()->shapes.front(),{50,60},0));
        QVERIFY(std::none_of(controller.page()->strokes.begin(),controller.page()->strokes.end(),[&](const auto& stroke){return !stroke.marker&&hitTest(stroke,{50,40},0);}));
        for(const auto& id:untouched){const auto object=findObject(*controller.page(),id);QVERIFY(object);QCOMPARE(properties(*object).revision,std::uint64_t(0));QCOMPARE(std::get<StrokeObject>(*object).samples.size(),std::size_t(100));}
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(302));QVERIFY(controller.page()->shapes.front().erasedRegions.empty());controller.redo();QVERIFY(!controller.page()->erasedInk.empty());
        send(QEvent::MouseButtonPress,{50,25},Qt::ShiftModifier);send(QEvent::MouseMove,{50,75},Qt::ShiftModifier);send(QEvent::MouseButtonRelease,{50,75},Qt::ShiftModifier);
        QVERIFY(hitTest(controller.page()->shapes.front(),{50,60},0));QVERIFY(std::any_of(controller.page()->strokes.begin(),controller.page()->strokes.end(),[](const auto& stroke){return !stroke.marker&&hitTest(stroke,{50,40},0);}));
        const auto filename=directory.filePath("mixed-eraser.board");controller.saveAs(QUrl::fromLocalFile(filename));QTRY_VERIFY(!controller.busy());const auto saved=ProjectStore::load(filename);QVERIFY(saved);QCOMPARE(saved.project.pages[0].strokes.size(),controller.page()->strokes.size());QCOMPARE(saved.project.pages[0].shapes.front().erasedRegions.size(),controller.page()->shapes.front().erasedRegions.size());
        QVERIFY(controller.shutdown());
    }
    void longMarkerErasureStaysIncrementalAndReversible(){
        StrokeObject marker;marker.id=newId();marker.marker=true;marker.style.rgba=0xff000059;marker.style.minWidthMm=marker.style.maxWidthMm=10;
        for(int i=0;i<5000;++i)marker.samples.push_back({{20+60.*i/4999,40+4*std::sin(i*.01)},1});
        // Retracing must retain a single marker opacity.
        for(int i=4999;i>=0;--i)marker.samples.push_back(marker.samples[std::size_t(i)]);
        ShapeRasterCache raster;const QRectF visible(0,20,100,40);QElapsedTimer timer;timer.start();raster.update(marker,8,visible);const auto initialMs=timer.elapsed();
        const auto alphaAt=[&](Point point){for(const auto& [key,tile]:raster.tiles())if(tile.worldRect.contains(QPointF(point.x,point.y)))return tile.image.pixelColor(int((point.x-tile.worldRect.left())*8)+1,int((point.y-tile.worldRect.top())*8)+1).alpha();return -1;};
        QCOMPARE(alphaAt({50,40}),89);const auto allocated=raster.tiles().size();std::map<ShapeRasterCache::Key,qint64> originals;for(const auto& [key,tile]:raster.tiles())originals[key]=tile.original.cacheKey();timer.restart();
        Page exported;exported.id=newId();exported.size=PageSize{100,80};exported.background=0x00000000;exported.backgroundStyle.gridType=GridType::None;exported.strokes={marker};
        QImage bitmap(800,640,QImage::Format_ARGB32_Premultiplied);bitmap.fill(Qt::transparent);QPainter painter(&bitmap);painter.scale(8,8);QVERIFY(paintPage(painter,exported,8).isEmpty());painter.end();QCOMPARE(bitmap.pixelColor(400,320).alpha(),89);
        timer.restart();
        for(int i=0;i<200;++i){const Point a{45+(i%20)*.5,37},b{a.x+.5,43};marker.erasedRegions.push_back({a,b,1.5,i>=100});raster.update(marker,8,visible);QVERIFY(raster.updatedTiles()<=4);QCOMPARE(raster.tiles().size(),allocated);}
        qInfo()<<"10,000-point marker: initial raster"<<initialMs<<"ms; 200 erase/restore updates"<<timer.elapsed()<<"ms";
        QCOMPARE(alphaAt({50,40}),89);
        ShapeRasterCache reloaded;reloaded.update(marker,8,visible);for(const auto& [key,tile]:raster.tiles())QCOMPARE(tile.image,reloaded.tiles().at(key).image);
        marker.erasedRegions.push_back({{20,40},{80,40},20});raster.update(marker,8,visible);QCOMPARE(alphaAt({50,40}),0);
        marker.erasedRegions.pop_back();raster.update(marker,8,visible);QCOMPARE(alphaAt({50,40}),89);
        for(const auto& [key,tile]:raster.tiles())QCOMPARE(tile.original.cacheKey(),originals.at(key));
    }
    void largePenMeshesKeepEveryTriangleAndPersist(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        auto background=controller.background();background["gridType"]=int(GridType::None);controller.setBackground(background);
        StrokeObject stroke;stroke.id=newId();
        for(int i=0;i<25000;++i)stroke.samples.push_back({{20+170.*i/24999,60+10*std::sin(i*.03)},.2+.8*(i%10)/9});
        controller.addStroke(stroke);
        const auto expected=strokeDisplayMesh(stroke);QVERIFY(expected.size()>65535);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();
        std::unique_ptr<QSGNode> scene(canvas.updatePaintNode(nullptr,nullptr));auto* object=scene->firstChild()->nextSibling()->firstChild();QVERIFY(object);QVERIFY(object->childCount()>1);
        std::size_t offset=0;
        for(auto* child=object->firstChild();child;child=child->nextSibling()){
            auto* node=dynamic_cast<QSGGeometryNode*>(child);QVERIFY(node);const auto* geometry=node->geometry();QVERIFY(geometry->vertexCount()<=60000);QCOMPARE(geometry->vertexCount()%3,0);
            const auto* points=geometry->vertexDataAsPoint2D();
            for(int i=0;i<geometry->vertexCount();++i){QCOMPARE(points[i].x,float(expected[offset].x));QCOMPARE(points[i].y,float(expected[offset].y));++offset;}
        }
        QCOMPARE(offset,expected.size());
        controller.undo();QVERIFY(controller.page()->strokes.empty());controller.redo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        const auto filename=directory.filePath("large-fast-pen.board");controller.saveAs(QUrl::fromLocalFile(filename));QTRY_VERIFY(!controller.busy());const auto saved=ProjectStore::load(filename);QVERIFY(saved);
        QCOMPARE(saved.project.pages[0].strokes[0].samples.size(),stroke.samples.size());QVERIFY(saved.project.pages[0].strokes[0].samples.back().position==stroke.samples.back().position);
        QVERIFY(controller.shutdown());
    }
    void fastPenRemainsVisibleAfterRelease(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        auto background=controller.background();background["gridType"]=int(GridType::None);controller.setBackground(background);
        QQuickWindow window;window.resize(1000,700);
        auto* canvas=new CanvasItem(window.contentItem());canvas->setWidth(1000);canvas->setHeight(700);canvas->setController(&controller);canvas->restorePageView();canvas->setPenColor(Qt::red);canvas->setPenWidth(2);
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        if(window.rendererInterface()->graphicsApi()==QSGRendererInterface::Software)QSKIP("Run with the RHI backend to verify GPU geometry; software rendering does not support this custom mesh.");
        const auto screen=[&](QPointF point){return canvas->screenPoint(point).toPoint();};
        QTest::mousePress(&window,Qt::LeftButton,Qt::NoModifier,screen({20,60}));
        for(int i=1;i<=40;++i)QTest::mouseMove(&window,screen({i%2?190.:20.,60+i*.7}),0);
        const auto countInk=[&](const QImage& image){int count=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto c=image.pixelColor(x,y);if(c.red()>200&&c.green()<150&&c.blue()<150)++count;}return count;};
        const auto preview=window.grabWindow();QVERIFY(!preview.isNull());const int before=countInk(preview);QVERIFY(before>1000);
        QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,screen({20,88}),0);
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));QVERIFY(!canvas->drawing());
        const auto vertices=strokeDisplayMesh(controller.page()->strokes.back()).size();QVERIFY(vertices>0);qInfo()<<"Fast pen completed compact mesh vertices:"<<vertices;
        const auto finished=window.grabWindow();QVERIFY(!finished.isNull());const int after=countInk(finished);qInfo()<<"Ink pixels before/after release:"<<before<<after;QVERIFY(after>before*.9);
        QVERIFY(controller.shutdown());
    }
    void markerPreviewReusesUntouchedTiles(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        QQuickWindow window;window.resize(1000,700);
        auto* canvas=new MarkerInspectableCanvas(window.contentItem());canvas->setWidth(1000);canvas->setHeight(700);canvas->setController(&controller);canvas->restorePageView();canvas->setTool("marker");canvas->setPenColor(Qt::red);canvas->setMarkerOpacity(.35);
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto screen=[&](QPointF point){return canvas->screenPoint(point).toPoint();};
        const auto colorAt=[&](const QImage& image,QPointF point){const auto logical=canvas->screenPoint(point);return image.pixelColor(int(logical.x()*image.width()/window.width()),int(logical.y()*image.height()/window.height()));};
        const auto background=colorAt(window.grabWindow(),{50,40});
        QTest::mousePress(&window,Qt::LeftButton,Qt::NoModifier,screen({20,40}));
        QTest::mouseMove(&window,screen({160,40}));QVERIFY(!window.grabWindow().isNull());QVERIFY(canvas->tileCount.load()>1);
        QTest::mouseMove(&window,screen({180,40}));const auto preview=window.grabWindow();QVERIFY(!preview.isNull());QVERIFY(canvas->reusedTiles.load()>0);
        const auto ink=colorAt(preview,{50,40});qInfo()<<"Marker background/preview:"<<background<<ink;
        const double opacity=canvas->markerOpacity();
        QVERIFY(std::abs(ink.red()-(255*opacity+background.red()*(1-opacity)))<5);
        QVERIFY(std::abs(ink.green()-background.green()*(1-opacity))<5);QVERIFY(std::abs(ink.blue()-background.blue()*(1-opacity))<5);
        QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,screen({180,40}));QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        const auto finished=colorAt(window.grabWindow(),{50,40});QVERIFY(std::abs(ink.green()-finished.green())<5);
        QVERIFY(controller.shutdown());
    }
    void rapidMouseStrokesAndLostGrabKeepInk(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();
        const auto start=canvas.screenPoint({30,40}),end=canvas.screenPoint({130,40});
        for(int i=0;i<20;++i){
            QMouseEvent press(i%2?QEvent::MouseButtonDblClick:QEvent::MouseButtonPress,start,start,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            if(i%2)canvas.mouseDoubleClickEvent(&press);else canvas.mousePressEvent(&press);
            QVERIFY(canvas.drawing());
            QMouseEvent release(QEvent::MouseButtonRelease,end,end,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);canvas.mouseReleaseEvent(&release);
            QCOMPARE(controller.page()->strokes.size(),std::size_t(i+1));QVERIFY(!canvas.drawing());
            QVERIFY(length(controller.page()->strokes.back().samples.back().position-Point{130,40})<1e-6);
        }
        QMouseEvent press(QEvent::MouseButtonPress,start,start,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);canvas.mousePressEvent(&press);
        QMouseEvent move(QEvent::MouseMove,end,end,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);canvas.mouseMoveEvent(&move);
        canvas.mouseUngrabEvent();QCOMPARE(controller.page()->strokes.size(),std::size_t(21));QVERIFY(!canvas.drawing());
        QVERIFY(controller.shutdown());
    }
    void mouseWheelPansVerticallyUnlessControlIsHeld(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QVERIFY(canvas->setZoom(1.5));
        const auto position=canvas->mapToScene({canvas->width()/2+80,canvas->height()/2+35});
        const auto wheel=[&](int angle,Qt::KeyboardModifiers modifiers=Qt::NoModifier,QPoint pixels={}){
            QWheelEvent event(position,window->mapToGlobal(position.toPoint()),pixels,QPoint(0,angle),Qt::NoButton,modifiers,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(window,&event);
        };
        const auto initial=canvas->viewportCenter();const double scale=canvas->zoom()*96/25.4;
        wheel(-120);QCOMPARE(canvas->zoom(),1.5);QCOMPARE(canvas->viewportCenter().x(),initial.x());QVERIFY(std::abs(canvas->viewportCenter().y()-initial.y()-60/scale)<1e-8);
        wheel(120);QVERIFY(QLineF(canvas->viewportCenter(),initial).length()<1e-8);
        wheel(-60);QVERIFY(std::abs(canvas->viewportCenter().y()-initial.y()-30/scale)<1e-8);
        const auto beforePixels=canvas->viewportCenter();wheel(0,Qt::NoModifier,{10,-20});QCOMPARE(canvas->zoom(),1.5);QCOMPARE(canvas->viewportCenter().x(),beforePixels.x());QVERIFY(std::abs(canvas->viewportCenter().y()-beforePixels.y()-20/scale)<1e-8);
        const auto anchor=[&]{return canvas->viewportCenter()+QPointF(80,35)/(canvas->zoom()*96/25.4);};const auto beforeZoom=anchor();
        wheel(120,Qt::ControlModifier);QVERIFY(std::abs(canvas->zoom()-1.575)<1e-8);QVERIFY(QLineF(anchor(),beforeZoom).length()<1e-8);
        canvas->setProperty("wheelZoomEnabled",false);const auto disabledCenter=canvas->viewportCenter();wheel(120,Qt::ControlModifier);QVERIFY(std::abs(canvas->zoom()-1.575)<1e-8);QCOMPARE(canvas->viewportCenter(),disabledCenter);
        QVERIFY(controller.shutdown());
    }
    void handCursorAndHoldShortcutRestorePreviousTool(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("marker");
        const auto key=[&](QEvent::Type type,bool repeat=false){QKeyEvent event(type,Qt::Key_H,Qt::NoModifier,"h",repeat);QCoreApplication::sendEvent(&canvas,&event);};
        key(QEvent::KeyPress);QCOMPARE(canvas.tool(),QString("hand"));QCOMPARE(canvas.cursor().shape(),Qt::OpenHandCursor);
        key(QEvent::KeyRelease,true);key(QEvent::KeyPress,true);QCOMPARE(canvas.tool(),QString("hand"));
        const auto before=canvas.viewportCenter();
        QMouseEvent press(QEvent::MouseButtonPress,{400,300},{400,300},Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);canvas.mousePressEvent(&press);QCOMPARE(canvas.cursor().shape(),Qt::ClosedHandCursor);
        QMouseEvent move(QEvent::MouseMove,{450,350},{450,350},Qt::NoButton,Qt::LeftButton,Qt::NoModifier);canvas.mouseMoveEvent(&move);QVERIFY(canvas.viewportCenter()!=before);
        QMouseEvent release(QEvent::MouseButtonRelease,{450,350},{450,350},Qt::LeftButton,Qt::NoButton,Qt::NoModifier);canvas.mouseReleaseEvent(&release);QCOMPARE(canvas.cursor().shape(),Qt::OpenHandCursor);
        key(QEvent::KeyRelease);QCOMPARE(canvas.tool(),QString("marker"));QCOMPARE(canvas.cursor().shape(),Qt::CrossCursor);QVERIFY(controller.page()->strokes.empty());
        canvas.setTool("hand");key(QEvent::KeyPress);key(QEvent::KeyRelease);QCOMPARE(canvas.tool(),QString("hand"));QCOMPARE(canvas.cursor().shape(),Qt::OpenHandCursor);
        canvas.mousePressEvent(&press);QCOMPARE(canvas.cursor().shape(),Qt::ClosedHandCursor);canvas.cancelStroke();QCOMPARE(canvas.cursor().shape(),Qt::OpenHandCursor);
        canvas.setTool("pen");key(QEvent::KeyPress);canvas.setEnabled(false);QCOMPARE(canvas.tool(),QString("pen"));QCOMPARE(canvas.cursor().shape(),Qt::CrossCursor);
        QVERIFY(controller.shutdown());
    }
    void holdHandDoesNotInterceptTextFields(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("pen");canvas->forceActiveFocus();
        QTest::keyPress(window,Qt::Key_H);QCOMPARE(canvas->tool(),QString("hand"));
        auto* title=findVisualItem(window->contentItem(),"projectTitleArea");QVERIFY(title);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,title->mapToScene({10,10}).toPoint());
        auto* input=findVisualItem(window->contentItem(),"projectTitleInput");QVERIFY(input);QTRY_VERIFY(input->hasActiveFocus());QCOMPARE(canvas->tool(),QString("pen"));
        QTest::keyRelease(window,Qt::Key_H);input->setProperty("text","");QTest::keyClick(window,Qt::Key_H);QCOMPARE(input->property("text").toString().toLower(),QString("h"));QCOMPARE(canvas->tool(),QString("pen"));
        QTest::keyClick(window,Qt::Key_Escape);canvas->forceActiveFocus();QTest::keyPress(window,Qt::Key_H);QCOMPARE(canvas->tool(),QString("hand"));
        QEvent deactivate(QEvent::WindowDeactivate);QCoreApplication::sendEvent(window,&deactivate);QCOMPARE(canvas->tool(),QString("pen"));QTest::keyRelease(window,Qt::Key_H);
        QVERIFY(controller.shutdown());
    }
    void nativeTabletEraserRestoresToolAndDoesNotEraseInHover(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();canvas.setTool("marker");
        QPointingDevice pen("Wacom pen",1,QInputDevice::DeviceType::Stylus,QPointingDevice::PointerType::Pen,QInputDevice::Capability::Position|QInputDevice::Capability::Pressure,1,3);
        QPointingDevice eraser("Wacom eraser",2,QInputDevice::DeviceType::Stylus,QPointingDevice::PointerType::Eraser,QInputDevice::Capability::Position|QInputDevice::Capability::Pressure,1,3);
        StrokeObject ink;ink.id=newId();ink.samples={{{50,50},1}};controller.addStroke(ink);
        const auto position=canvas.screenPoint({50,50});
        const auto send=[&](QEvent::Type type,const QPointingDevice& device,double pressure,Qt::MouseButton button,Qt::MouseButtons buttons){
            QTabletEvent event(type,&device,position,position,pressure,0,0,0,0,0,Qt::NoModifier,button,buttons);QCoreApplication::sendEvent(&canvas,&event);
        };
        send(QEvent::TabletPress,eraser,0,Qt::RightButton,Qt::RightButton);QCOMPARE(canvas.tool(),QString("eraser"));QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        send(QEvent::TabletMove,eraser,0,Qt::NoButton,Qt::RightButton);QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        send(QEvent::TabletPress,eraser,.5,Qt::LeftButton,Qt::LeftButton|Qt::RightButton);
        send(QEvent::TabletRelease,eraser,0,Qt::LeftButton,Qt::NoButton);QVERIFY(controller.page()->strokes.empty());
        send(QEvent::TabletMove,pen,0,Qt::NoButton,Qt::NoButton);QCOMPARE(canvas.tool(),QString("marker"));
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        send(QEvent::TabletMove,eraser,0,Qt::NoButton,Qt::NoButton);QCOMPARE(canvas.tool(),QString("eraser"));
        QTabletEvent leave(QEvent::TabletLeaveProximity,&eraser,position,position,0,0,0,0,0,0,Qt::NoModifier,Qt::NoButton,Qt::NoButton);QCoreApplication::sendEvent(QGuiApplication::instance(),&leave);QCOMPARE(canvas.tool(),QString("marker"));
        canvas.setTool("eraser");send(QEvent::TabletMove,pen,0,Qt::NoButton,Qt::NoButton);QCOMPARE(canvas.tool(),QString("eraser"));
        QVERIFY(controller.shutdown());
    }
    void fastTabletContactRecoversAfterZeroPressurePress(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();
        QPointingDevice pen("Pen",1,QInputDevice::DeviceType::Stylus,QPointingDevice::PointerType::Pen,QInputDevice::Capability::Position|QInputDevice::Capability::Pressure,1,3);
        const auto send=[&](QEvent::Type type,Point point,double pressure,Qt::MouseButtons buttons){
            const auto position=canvas.screenPoint({point.x,point.y});
            QTabletEvent event(type,&pen,position,position,pressure,0,0,0,0,0,Qt::NoModifier,type==QEvent::TabletMove?Qt::NoButton:Qt::LeftButton,buttons);QCoreApplication::sendEvent(&canvas,&event);
        };
        for(const QString tool:{QString("pen"),QString("marker")}){
            canvas.setTool(tool);
            send(QEvent::TabletPress,{20,30},0,Qt::LeftButton);
            send(QEvent::TabletMove,{25,30},.7,Qt::LeftButton);
            QVERIFY(canvas.drawing());
            for(int i=0;i<1000;++i)send(QEvent::TabletMove,{25+i*.1,30+std::sin(i*.05)},.7,Qt::LeftButton);
            send(QEvent::TabletRelease,{130,30},0,Qt::NoButton);
            QVERIFY(!canvas.drawing());QVERIFY(!controller.page()->strokes.empty());
            const auto& stroke=controller.page()->strokes.back();QVERIFY(stroke.samples.size()>1000);QVERIFY(length(stroke.samples.back().position-Point{130,30})<1e-6);
        }
        QCOMPARE(controller.page()->strokes.size(),std::size_t(2));
        // A stream whose initial press was omitted must recover as well.
        send(QEvent::TabletMove,{40,40},.7,Qt::LeftButton);send(QEvent::TabletRelease,{100,40},0,Qt::NoButton);
        QCOMPARE(controller.page()->strokes.size(),std::size_t(3));
        QVERIFY(controller.shutdown());
    }
    void tabletEraserCanSwitchDuringContact(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.setRecognitionEnabled(false);
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();
        QPointingDevice pen("Pen",1,QInputDevice::DeviceType::Stylus,QPointingDevice::PointerType::Pen,QInputDevice::Capability::Position|QInputDevice::Capability::Pressure,1,3);
        QPointingDevice eraser("Eraser",2,QInputDevice::DeviceType::Stylus,QPointingDevice::PointerType::Eraser,QInputDevice::Capability::Position|QInputDevice::Capability::Pressure,1,3);
        const auto position=canvas.screenPoint({50,50});
        const auto send=[&](QEvent::Type type,const QPointingDevice& device,double pressure){
            QTabletEvent event(type,&device,position,position,pressure,0,0,0,0,0,Qt::NoModifier,type==QEvent::TabletMove?Qt::NoButton:Qt::LeftButton,type==QEvent::TabletRelease?Qt::NoButton:Qt::LeftButton);QCoreApplication::sendEvent(&canvas,&event);
        };
        send(QEvent::TabletPress,pen,.5);send(QEvent::TabletMove,eraser,.5);QCOMPARE(canvas.tool(),QString("eraser"));
        send(QEvent::TabletMove,pen,.5);QCOMPARE(canvas.tool(),QString("pen"));QVERIFY(controller.page()->strokes.empty());
        send(QEvent::TabletRelease,pen,0);QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        controller.undo();QVERIFY(controller.page()->strokes.empty());controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        QVERIFY(controller.shutdown());
    }
    void pageViewsAreIndependentAndSessionOnly(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        InspectableCanvas canvas;canvas.setWidth(1000);canvas.setHeight(700);canvas.setController(&controller);canvas.restorePageView();
        const double initialZoom=canvas.zoom();QVERIFY(canvas.setZoom(1.7));canvas.setTool("hand");
        QMouseEvent press(QEvent::MouseButtonPress,{400,300},{400,300},Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);canvas.mousePressEvent(&press);
        QMouseEvent move(QEvent::MouseMove,{470,110},{470,110},Qt::NoButton,Qt::LeftButton,Qt::NoModifier);canvas.mouseMoveEvent(&move);
        QMouseEvent release(QEvent::MouseButtonRelease,{470,110},{470,110},Qt::LeftButton,Qt::NoButton,Qt::NoModifier);canvas.mouseReleaseEvent(&release);
        const auto firstCenter=canvas.viewportCenter();controller.addPage();QCOMPARE(controller.currentPage(),1);QCOMPARE(canvas.zoom(),1.7);
        QVERIFY(std::abs(canvas.screenPoint({0,0}).y()-40)<1e-8);
        QVERIFY(canvas.setZoom(2.4));const auto secondCenter=canvas.viewportCenter();
        controller.selectPage(0);QCOMPARE(canvas.zoom(),1.7);QVERIFY(QLineF(firstCenter,canvas.viewportCenter()).length()<1e-8);
        controller.selectPage(1);QCOMPARE(canvas.zoom(),2.4);QVERIFY(QLineF(secondCenter,canvas.viewportCenter()).length()<1e-8);
        const auto filename=directory.filePath("views.board");controller.saveAs(QUrl::fromLocalFile(filename));QTRY_VERIFY(!controller.busy());
        QTemporaryDir freshDirectory;AppController fresh(nullptr,freshDirectory.path());fresh.openPath(filename);QTRY_VERIFY(fresh.active());QTRY_VERIFY(!fresh.busy());
        InspectableCanvas reopened;reopened.setWidth(1000);reopened.setHeight(700);reopened.setController(&fresh);reopened.restorePageView();QCOMPARE(reopened.zoom(),initialZoom);
        QVERIFY(fresh.shutdown());QVERIFY(controller.shutdown());
    }
    void handwritingFilterReducesJitterWithoutFilteringGuides(){
        InputManager filter;double error=0;
        for(int i=0;i<100;++i){PointerSample sample{{i*.04,(i%2?.15:-.15)},.7};sample.timestamp=100+i*8;
            const auto filtered=filter.filter(sample,true);if(i>20)error+=std::abs(filtered.position.y);}
        QVERIFY(error/79<.08);
        filter.reset();PointerSample start{{0,0},.2};start.timestamp=100;filter.filter(start,true);
        PointerSample fast{{10,0},.8};fast.timestamp=110;const auto moved=filter.filter(fast,true);QVERIFY(moved.position.x>7.5);QVERIFY(moved.position.x<=10);
        filter.reset();QCOMPARE(filter.filter(fast,true).position.x,10.);
        PointerSample guide{{3,4},.5};QCOMPARE(filter.filter(guide).position.x,3.);QCOMPARE(filter.filter(guide).position.y,4.);
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QCOMPARE(controller.holdDelay(),1000);QVERIFY(controller.shutdown());
    }
    void highZoomRetainsVectorGeometryAndLayerOrder(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto background=controller.background();background["gridType"]=int(GridType::None);controller.setBackground(background);
        ShapeObject circle;circle.id=newId();circle.kind=ShapeKind::Circle;circle.center={105,148.5};circle.radiusX=circle.radiusY=20;circle.properties.zIndex=1;
        controller.addShape(circle);auto second=circle;second.id=newId();second.radiusX=second.radiusY=40;second.properties.zIndex=2;controller.addShape(second);
        InspectableCanvas canvas;canvas.setWidth(800);canvas.setHeight(600);canvas.setController(&controller);canvas.fitPage();
        std::unique_ptr<QSGNode> scene(canvas.updatePaintNode(nullptr,nullptr));auto* clip=scene->firstChild()->nextSibling();
        auto* first=clip->firstChild();auto* last=clip->lastChild();QVERIFY(first!=last);QCOMPARE(clip->childCount(),2);
        auto* border=static_cast<QSGGeometryNode*>(first->lastChild());auto* geometry=border->geometry();QCOMPARE(geometry->vertexCount(),128*6);
        QElapsedTimer timer;timer.start();
        for(int i=0;i<300;++i){QVERIFY(canvas.setZoom(2+(i%61)*.1));scene.reset(canvas.updatePaintNode(scene.release(),nullptr));
            QCOMPARE(clip->firstChild(),first);QCOMPARE(clip->lastChild(),last);QCOMPARE(border->geometry(),geometry);}
        qInfo()<<"300 vector scene updates at 200–800% zoom:"<<timer.nsecsElapsed()/1000000.<<"ms (CPU scene synchronization)";
        second=controller.page()->shapes.back();auto moved=second;moved.properties.zIndex=-1;++moved.properties.revision;
        controller.changeObjects({{CanvasObject(second),CanvasObject(moved)}},CommandKind::ChangeStyle);
        scene.reset(canvas.updatePaintNode(scene.release(),nullptr));QCOMPARE(clip->lastChild(),first);QCOMPARE(clip->childCount(),2);
        controller.undo();scene.reset(canvas.updatePaintNode(scene.release(),nullptr));QCOMPARE(clip->firstChild(),first);QCOMPARE(clip->childCount(),2);
        QVERIFY(controller.shutdown());
    }
    void zoomReusesErasedShapeTexturesUntilSettled(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();QTRY_VERIFY(!controller.busy());
        QQuickWindow window;window.resize(800,700);auto* canvas=new ZoomInspectableCanvas(window.contentItem());canvas->setWidth(800);canvas->setHeight(700);canvas->setController(&controller);
        ShapeObject circle;circle.id=newId();circle.kind=ShapeKind::Circle;circle.center={105,148.5};circle.radiusX=circle.radiusY=20;
        for(int i=0;i<400;++i){Point p{105+(i%20)*.5,148.5+(i/20)*.5};circle.erasedRegions.push_back({p,p,.5,i%3==0});}
        controller.addShape(circle);canvas->fitPage();window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));QVERIFY(canvas->setZoom(2));QTest::qWait(220);QVERIFY(canvas->tileWidth>0);
        const auto initialWidth=canvas->tileWidth;const int frames=canvas->renderedFrames;QElapsedTimer timer;timer.start();
        for(int i=0;i<10;++i){canvas->zoomBy(1.05);QTest::qWait(12);QCOMPARE(canvas->tileWidth,initialWidth);}
        QVERIFY(canvas->renderedFrames>frames);qInfo()<<"10 zoom steps with cached mask textures:"<<timer.elapsed()<<"ms (includes 120 ms of event waits)";
        QTRY_VERIFY(canvas->tileWidth<initialWidth);QCOMPARE(controller.page()->shapes[0].erasedRegions.size(),circle.erasedRegions.size());
        // Zooming back out also updates the viewport and remains bounded.
        for(int i=0;i<10;++i){canvas->zoomBy(1/1.05);QTest::qWait(12);}QTRY_COMPARE(canvas->tileWidth,initialWidth);
        window.hide();delete canvas;QVERIFY(controller.shutdown());
    }
    void selectedContourPatternsAndIncircle(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        controller.newDefault();QTRY_VERIFY(!controller.busy());auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);
        ShapeObject triangle;triangle.id=newId();triangle.kind=ShapeKind::Triangle;triangle.vertices={{70,100},{140,100},{105,160}};triangle.style.pattern=LinePattern::Dashed;controller.addShape(triangle);
        canvas->setTool("select");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(canvas->screenPoint({105,120})).toPoint());QCOMPARE(canvas->selectedCount(),1);QCOMPARE(canvas->selectedPattern(),1);
        for(int pattern:{0,2,1}){
            auto* button=findVisualItem(window->contentItem(),"selectedPattern_"+QString::number(pattern));QVERIFY(button);QVERIFY(button->isVisible());
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene({button->width()/2,button->height()/2}).toPoint());
            QCOMPARE(canvas->selectedPattern(),pattern);QCOMPARE(int(controller.page()->shapes[0].style.pattern),pattern);
        }
        controller.undo();QCOMPARE(canvas->selectedPattern(),2);controller.redo();QCOMPARE(canvas->selectedPattern(),1);canvas->setSelectedPattern(99);QCOMPARE(canvas->selectedPattern(),1);
        auto* dashField=findVisualItem(window->contentItem(),"selecteddashLengthField");QVERIFY(dashField);QVERIFY(dashField->isVisible());dashField->setProperty("text","2.75");QVERIFY(QMetaObject::invokeMethod(dashField,"editingFinished"));QCOMPARE(canvas->selectedDashLength(),2.75);
        canvas->setSelectedDashLength(1.75);QCOMPARE(dashField->property("text").toString(),QString("1.75"));
        for(const auto& input:std::vector<std::pair<QString,double>>{{"2.2",2.2},{"2,2",2.2},{"0.5",.5},{"0,5",.5}}){
            dashField->forceActiveFocus();QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);for(const auto character:input.first)QTest::keyClick(window,character.toLatin1());QTest::keyClick(window,Qt::Key_Return);
            QCOMPARE(canvas->selectedDashLength(),input.second);QCOMPARE(dashField->property("text").toString(),QString::number(input.second,'f',2));
        }
        canvas->setSelectedPattern(2);auto* dotField=findVisualItem(window->contentItem(),"selecteddotSpacingField");QVERIFY(dotField);QVERIFY(dotField->isVisible());QVERIFY(!dashField->isVisible());dotField->setProperty("text","0.65");QVERIFY(QMetaObject::invokeMethod(dotField,"editingFinished"));QCOMPARE(canvas->selectedDotSpacing(),.65);
        canvas->setSelectedPattern(1);
        auto* inscribed=findVisualItem(window->contentItem(),"constructIncircleButton");QVERIFY(inscribed);QVERIFY(QMetaObject::invokeMethod(inscribed,"clicked"));
        QCOMPARE(controller.page()->shapes.size(),std::size_t(2));const auto circle=controller.page()->shapes.back();QCOMPARE(circle.kind,ShapeKind::Circle);
        for(int i=0;i<3;++i)QVERIFY(std::abs(distanceToSegment(circle.center,triangle.vertices[i],triangle.vertices[(i+1)%3])-circle.radiusX)<1e-8);
        controller.undo();QCOMPARE(controller.page()->shapes.size(),std::size_t(1));controller.redo();QCOMPARE(controller.page()->shapes.size(),std::size_t(2));
        controller.setTheme("Dark");window->resize(760,600);QTest::qWait(100);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(canvas->screenPoint({105,120})).toPoint());QTest::qWait(50);auto* panel=findVisualItem(window->contentItem(),"objectPropertiesPanel");QVERIFY(panel);QVERIFY(panel->isVisible());QVERIFY(panel->height()+panel->y()<=canvas->parentItem()->height()-89);
        auto* duplicate=findVisualItem(window->contentItem(),"duplicateSelectionButton");QVERIFY(duplicate);QVERIFY(duplicate->mapToScene({0,duplicate->height()}).y()<=window->height());
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/object-properties-organized.png"));QVERIFY(warnings.isEmpty());QVERIFY(controller.shutdown());
    }
    void bucketFillErasesLocallyAndRestores(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto background=controller.background();background["color"]="#000000";background["gridType"]=int(GridType::None);controller.setBackground(background);
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QVERIFY(canvas->setZoom(3));
        StrokeObject boundary;boundary.id=newId();boundary.samples={{{90,135}},{{120,135}},{{120,165}},{{90,165}},{{90,135}}};controller.addStroke(boundary);
        controller.fillRegion({105,150},Qt::white,.25);QTRY_VERIFY(!controller.documentBusy());QCOMPARE(controller.page()->images.size(),std::size_t(1));const auto original=controller.page()->images.front();QVERIFY(original.inkFill);
        const auto screen=[&](QPointF p){return canvas->mapToScene(canvas->screenPoint(p)).toPoint();};
        const auto sample=[&](QPointF p){QTest::qWait(50);const auto image=window->grabWindow();const auto point=screen(p);return image.pixelColor(int(point.x()*image.width()/double(window->width())),int(point.y()*image.height()/double(window->height()))).red();};
        canvas->setTool("hand");QCOMPARE(sample({105,150}),64);
        canvas->setTool("eraser");canvas->setEraserRadius(1);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen({102,150}));
        for(int i=0;i<=10;++i)QTest::mouseMove(window,screen({102+i*.6,150}));
        QCOMPARE(sample({105,150}),0);QCOMPARE(sample({105,152}),64); // live preview before release
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({108,150}));canvas->setTool("hand");
        QCOMPARE(controller.page()->images.size(),std::size_t(1));QCOMPARE(controller.page()->images.front().id,original.id);QVERIFY(controller.page()->images.front().png==original.png);QVERIFY(!controller.page()->images.front().erasedRegions.empty());
        controller.undo();QCOMPARE(sample({105,150}),64);controller.redo();QCOMPARE(sample({105,150}),0);
        canvas->setTool("eraser");QTest::mouseClick(window,Qt::LeftButton,Qt::ShiftModifier,screen({105,150}));canvas->setTool("hand");QCOMPARE(sample({105,150}),64);QCOMPARE(sample({102,150}),0);
        for(int i=0;i<3;++i){canvas->setTool("eraser");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,screen({105,150}));canvas->setTool("hand");QCOMPARE(sample({105,150}),0);QCOMPARE(sample({105,152}),64);}
        const auto filename=directory.filePath("erased-fill.board");controller.saveAs(QUrl::fromLocalFile(filename));QTRY_VERIFY(!controller.busy());const auto loaded=ProjectStore::load(filename);QVERIFY2(bool(loaded),qPrintable(loaded.error));const auto& imageObject=loaded.project.pages[0].images.front();QVERIFY(imageObject.erasedRegions.empty());QVERIFY(imageObject.eraseMask);
        QImage exported(840,1188,QImage::Format_ARGB32_Premultiplied);exported.fill(Qt::transparent);QPainter painter(&exported);painter.scale(4,4);QVERIFY(paintPage(painter,loaded.project.pages[0],4).isEmpty());painter.end();QCOMPARE(exported.pixelColor(420,600).red(),0);QCOMPARE(exported.pixelColor(420,608).red(),64);
        const auto moved=std::get<ImageObject>(transformed(CanvasObject(imageObject),{0,0},{5,0},1,1,.3));QVERIFY(length(moved.eraseMask->corners.front()-rotatePoint(imageObject.eraseMask->corners.front(),{0,0},.3)-Point{5,0})<1e-8);
        // Only tiles touched by the brush are recomposed, including rotated fills.
        ShapeRasterCache cache;auto fill=imageObject;const auto bitmap=controller.image(fill.id);cache.update(fill,bitmap,16,QRectF(80,120,60,60));const auto allocated=cache.tiles().size();
        for(int i=0;i<100;++i){fill.erasedRegions.push_back({{105,150},{105.2,150},.5,bool(i%2)});cache.update(fill,bitmap,16,QRectF(80,120,60,60));QVERIFY(cache.updatedTiles()<=4);QCOMPARE(cache.tiles().size(),allocated);}
        QVERIFY(warnings.isEmpty());QVERIFY(controller.shutdown());
    }
    void markerErasingPreservesOpacityAndRestoresLocally(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto background=controller.background();background["color"]="#000000";background["gridType"]=int(GridType::None);controller.setBackground(background);
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QVERIFY(canvas->setZoom(3));
        StrokeObject marker;marker.id=newId();marker.marker=true;marker.style.rgba=0xffffff1a;marker.style.minWidthMm=marker.style.maxWidthMm=10;
        // Retraced segments share one opacity, including after a partial cut.
        marker.samples={{{85,148.5}},{{125,148.5}},{{85,148.5}}};controller.addStroke(marker);
        const auto screen=[&](QPointF p){return canvas->mapToScene(canvas->screenPoint(p)).toPoint();};
        const auto sample=[&](QPointF p){QTest::qWait(50);const auto image=window->grabWindow();if(image.isNull())return -1;const auto point=screen(p);return image.pixelColor(int(point.x()*image.width()/double(window->width())),int(point.y()*image.height()/double(window->height()))).red();};
        canvas->setTool("hand");QCOMPARE(sample({105,148.5}),26);
        const auto gesture=[&](Qt::KeyboardModifiers modifiers){canvas->setTool("eraser");canvas->setEraserRadius(1);QTest::mouseClick(window,Qt::LeftButton,modifiers,screen({105,148.5}));canvas->setTool("hand");};
        for(int pass=0;pass<3;++pass){gesture(Qt::NoModifier);QCOMPARE(sample({105,148.5}),0);QCOMPARE(sample({105,151}),26);QCOMPARE(sample({112,148.5}),26);}
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));QCOMPARE(controller.page()->strokes[0].id,marker.id);QCOMPARE(controller.page()->strokes[0].samples.size(),marker.samples.size());QVERIFY(controller.page()->erasedInk.empty());
        gesture(Qt::ShiftModifier);QCOMPARE(sample({105,148.5}),26);QCOMPARE(sample({105,151}),26);
        controller.undo();QCOMPARE(sample({105,148.5}),0);controller.redo();QCOMPARE(sample({105,148.5}),26);
        gesture(Qt::ControlModifier);QCOMPARE(sample({105,148.5}),0);
        const auto filename=directory.filePath("masked-marker.board");controller.saveAs(QUrl::fromLocalFile(filename));QTRY_VERIFY(!controller.busy());
        const auto loaded=ProjectStore::load(filename);QVERIFY2(bool(loaded),qPrintable(loaded.error));
        const auto& saved=loaded.project.pages[0].strokes[0];QVERIFY(saved.marker);QVERIFY(saved.erasedRegions.empty());QVERIFY(saved.eraseMask);QVERIFY(!controller.page()->strokes[0].erasedRegions.empty());
        const auto& page=loaded.project.pages[0];QImage image(840,1188,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);painter.scale(4,4);QVERIFY(paintPage(painter,page,4).isEmpty());painter.end();
        QCOMPARE(image.pixelColor(420,594).red(),0);QCOMPARE(image.pixelColor(420,604).red(),26);
        auto moved=std::get<StrokeObject>(transformed(CanvasObject(saved),{0,0},{5,0},1,1,0));QCOMPARE(moved.eraseMask->corners.front().x,saved.eraseMask->corners.front().x+5);
        QVERIFY(controller.shutdown());
    }
    void nativeShapeMaskSceneGraph(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto background=controller.background();background["color"]="#000000";background["gridType"]=int(GridType::None);controller.setBackground(background);
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QVERIFY(canvas->setZoom(3));
        ShapeObject shape;shape.id=newId();shape.kind=ShapeKind::Circle;shape.center={105,148.5};shape.radiusX=shape.radiusY=20;shape.fillOpacity=.25;shape.style.rgba=0xffffffff;
        // A distant cut selects the texture path without touching this circle.
        // Qt's software backend does not render the app's custom triangle nodes.
        shape.erasedRegions.push_back({{1,1},{1,1},.5});controller.addShape(shape);
        auto screen=[&](QPointF p){return canvas->mapToScene(canvas->screenPoint(p)).toPoint();};
        const auto sample=[&](QPointF p){QTest::qWait(50);const auto image=window->grabWindow();if(image.isNull())return -1;const auto point=screen(p);return image.pixelColor(int(point.x()*image.width()/double(window->width())),int(point.y()*image.height()/double(window->height()))).red();};
        canvas->setTool("hand");const auto original=sample({105,148.5});QVERIFY(original>=60&&original<=66);
        const auto gesture=[&](Qt::KeyboardModifiers modifier){canvas->setTool("eraser");canvas->setEraserRadius(3);QTest::mouseClick(window,Qt::LeftButton,modifier,screen({105,148.5}));canvas->setTool("hand");};
        gesture(Qt::ControlModifier);QCOMPARE(sample({105,148.5}),0);QVERIFY(std::abs(sample({115,148.5})-original)<=2);
        gesture(Qt::ShiftModifier);QVERIFY(std::abs(sample({105,148.5})-original)<=2);
        gesture(Qt::ControlModifier);QCOMPARE(sample({105,148.5}),0);
        controller.undo();QVERIFY(std::abs(sample({105,148.5})-original)<=2);
        controller.redo();QCOMPARE(sample({105,148.5}),0);
        QVERIFY(canvas->setZoom(4));QCOMPARE(sample({105,148.5}),0);gesture(Qt::ShiftModifier);QVERIFY(std::abs(sample({105,148.5})-original)<=2);
        QVERIFY(warnings.isEmpty());QVERIFY(controller.shutdown());
    }
    void tiledShapeErasureIsBoundedAndReversible(){
        ShapeObject shape;shape.id=newId();shape.kind=ShapeKind::Circle;shape.center={60,60};shape.radiusX=shape.radiusY=20;
        ShapeRasterCache raster;const QRectF visible(0,0,120,120);raster.update(shape,16,visible);
        const auto alphaAt=[&](Point point){for(const auto& [key,tile]:raster.tiles())if(tile.worldRect.contains(QPointF(point.x,point.y)))return tile.image.pixelColor(int((point.x-tile.worldRect.left())*16)+1,int((point.y-tile.worldRect.top())*16)+1).alpha();return -1;};
        const int opacity=alphaAt({60,60});QVERIFY(opacity>0);
        shape.erasedRegions.push_back({{60,60},{60,60},2});raster.update(shape,16,visible);QCOMPARE(alphaAt({60,60}),0);QCOMPARE(alphaAt({64,60}),opacity);
        shape.erasedRegions.push_back({{60,60},{60,60},1,true});raster.update(shape,16,visible);QCOMPARE(alphaAt({60,60}),opacity);QCOMPARE(alphaAt({61.5,60}),0);
        shape.erasedRegions.push_back({{60,60},{60,60},.5});raster.update(shape,16,visible);QCOMPARE(alphaAt({60,60}),0);
        shape.erasedRegions.pop_back();raster.update(shape,16,visible);QCOMPARE(alphaAt({60,60}),opacity);
        raster.update(shape,16,visible);QCOMPARE(raster.updatedTiles(),std::size_t(0));
        const auto tileCount=raster.tiles().size();std::size_t maxUpdated=0;QElapsedTimer timer;timer.start();
        for(int i=0;i<600;++i){const double angle=(i%100)*.062;const Point p{60+20*std::sin(angle),60-20*std::cos(angle)};
            shape.erasedRegions.push_back({p,p,3,(i/100)%2!=0});raster.update(shape,16,visible);maxUpdated=std::max(maxUpdated,raster.updatedTiles());
            QCOMPARE(raster.tiles().size(),tileCount);QVERIFY(raster.updatedTiles()<=4);
        }
        qInfo()<<"600 native mask updates:"<<timer.elapsed()<<"ms; maximum touched tiles:"<<maxUpdated<<"; allocated tiles:"<<tileCount;
        // Reloading a long operation history has the same final image. No mesh
        // generated during the brush gesture is needed to save or display it.
        ShapeRasterCache reloaded;reloaded.update(shape,16,visible);
        for(const auto& [key,tile]:raster.tiles()){
            const auto& saved=reloaded.tiles().at(key);
            QCOMPARE(tile.image,saved.image);
        }
    }
    void tiledEraserLongGesturesStayIncremental(){
        ShapeObject shape;shape.id=newId();shape.kind=ShapeKind::Rectangle;
        shape.vertices={{0,0},{120,0},{120,30},{0,30}};
        ShapeRasterCache raster;const QRectF visible(0,0,120,30);
        raster.update(shape,16,visible);const auto allocated=raster.tiles().size();
        for(int gesture=0;gesture<3;++gesture){
            shape.erasedRegions.push_back({{5,15},{5,15},2,gesture==1});
            raster.update(shape,16,visible);
            for(int step=1;step<=200;++step){
                shape.erasedRegions.back().to={5+step*.5,15};
                raster.update(shape,16,visible);
                QVERIFY(raster.updatedTiles()<=4);QCOMPARE(raster.tiles().size(),allocated);
            }
            ShapeRasterCache reloaded;reloaded.update(shape,16,visible);
            for(const auto& [key,tile]:raster.tiles())QCOMPARE(tile.image,reloaded.tiles().at(key).image);
            // Repeated identical passes must not darken edges or upload textures.
            shape.erasedRegions.push_back(shape.erasedRegions.back());
            raster.update(shape,16,visible);QCOMPARE(raster.updatedTiles(),std::size_t(0));
        }
    }
    void shapeEraseRestorePreviewCycles(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();QTRY_VERIFY(!controller.busy());
        ShapeObject circle;circle.id=newId();circle.kind=ShapeKind::Circle;circle.center={60,60};circle.radiusX=circle.radiusY=12;controller.addShape(circle);
        InspectableCanvas canvas;canvas.setWidth(800);canvas.setHeight(600);canvas.setController(&controller);canvas.fitPage();canvas.setTool("eraser");canvas.setEraserRadius(3);
        std::unique_ptr<QSGNode> scene(canvas.updatePaintNode(nullptr,nullptr));
        const auto visibleAt=[&](Point p){
            auto* shape=scene->firstChild()->nextSibling()->lastChild();
            auto* fill=static_cast<QSGGeometryNode*>(shape->firstChild());const auto* vertices=fill->geometry()->vertexDataAsPoint2D();
            const auto cross=[](Point a,Point b){return a.x*b.y-a.y*b.x;};
            for(int i=0;i+2<fill->geometry()->vertexCount();i+=3){Point a{vertices[i].x,vertices[i].y},b{vertices[i+1].x,vertices[i+1].y},c{vertices[i+2].x,vertices[i+2].y};
                auto x=cross(b-a,p-a),y=cross(c-b,p-b),z=cross(a-c,p-c);if((x>=-1e-5&&y>=-1e-5&&z>=-1e-5)||(x<=1e-5&&y<=1e-5&&z<=1e-5))return true;}
            return false;
        };
        const auto gesture=[&](Qt::KeyboardModifiers modifiers){
            const auto point=canvas.screenPoint({60,48});QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,modifiers);canvas.mousePressEvent(&press);
            scene.reset(canvas.updatePaintNode(scene.release(),nullptr));
            QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,modifiers);canvas.mouseReleaseEvent(&release);
            scene.reset(canvas.updatePaintNode(scene.release(),nullptr));
        };
        for(int i=0;i<5;++i){
            gesture(Qt::ControlModifier);QVERIFY(!visibleAt({60,49}));QVERIFY(visibleAt({60,60}));
            gesture(Qt::ShiftModifier);QVERIFY(visibleAt({60,49}));
            gesture(Qt::ShiftModifier);QVERIFY(visibleAt({60,49}));
            gesture(Qt::ControlModifier);QVERIFY(!visibleAt({60,49}));
        }
        controller.undo();scene.reset(canvas.updatePaintNode(scene.release(),nullptr));QVERIFY(visibleAt({60,49}));
        controller.redo();scene.reset(canvas.updatePaintNode(scene.release(),nullptr));QVERIFY(!visibleAt({60,49}));
        gesture(Qt::ShiftModifier);QVERIFY(visibleAt({60,49}));QVERIFY(controller.shutdown());
    }
    void shiftRestoresErasedInkAndShapes(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("eraser");canvas->setEraserRadius(3);
        auto screen=[&](QPointF point){return canvas->mapToScene(canvas->screenPoint(point)).toPoint();};
        ShapeObject shape;shape.id=newId();shape.kind=ShapeKind::Rectangle;shape.vertices={{30,30},{70,30},{70,70},{30,70}};controller.addShape(shape);
        QTest::mousePress(window,Qt::LeftButton,Qt::ControlModifier,screen({50,40}));QTest::mouseRelease(window,Qt::LeftButton,Qt::ControlModifier,screen({50,60}));
        QVERIFY(!hitTest(controller.page()->shapes.front(),{50,50},0));
        canvas->setEraserRadius(1);QTest::mouseClick(window,Qt::LeftButton,Qt::ShiftModifier,screen({50,50}));
        QVERIFY(hitTest(controller.page()->shapes.front(),{50,50},0));QVERIFY(!hitTest(controller.page()->shapes.front(),{50,55},0));
        controller.undo();QVERIFY(!hitTest(controller.page()->shapes.front(),{50,50},0));controller.redo();QVERIFY(hitTest(controller.page()->shapes.front(),{50,50},0));
        StrokeObject stroke;stroke.id=newId();stroke.samples={{{20,80}},{{80,80}}};controller.addStroke(stroke);
        canvas->setEraserRadius(3);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen({50,80}));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({60,80}));
        QVERIFY(!controller.page()->erasedInk.empty());
        auto inkAt=[&](Point p){for(const auto& ink:controller.page()->strokes)if(hitTest(ink,p,.01))return true;return false;};
        QVERIFY(!inkAt({50,80}));QVERIFY(!inkAt({60,80}));
        canvas->setEraserRadius(1);QTest::mouseClick(window,Qt::LeftButton,Qt::ShiftModifier,screen({50,80}));
        QVERIFY(inkAt({50,80}));QVERIFY(!inkAt({60,80}));QVERIFY(inkAt({30,80}));
        controller.undo();QVERIFY(!inkAt({50,80}));controller.redo();QVERIFY(inkAt({50,80}));
        const auto count=controller.page()->strokes.size();QTest::mouseClick(window,Qt::LeftButton,Qt::ShiftModifier,screen({50,80}));QCOMPARE(controller.page()->strokes.size(),count);
        const auto project=directory.filePath("restore.board");controller.saveAs(QUrl::fromLocalFile(project));QTRY_VERIFY(!controller.busy());controller.openPath(project);QTRY_VERIFY(!controller.loading());
        QVERIFY(hitTest(controller.page()->shapes.front(),{50,50},0));QVERIFY(!hitTest(controller.page()->shapes.front(),{50,55},0));
        QTest::mouseClick(window,Qt::LeftButton,Qt::ShiftModifier,screen({60,80}));QVERIFY(inkAt({60,80}));
        QVERIFY(warnings.isEmpty());controller.shutdown();
    }
    void eraserSizeKeyboardShortcuts(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("eraser");canvas->forceActiveFocus();canvas->setEraserRadius(3);
        QTest::keyClick(window,Qt::Key_Plus);QCOMPARE(canvas->eraserRadius(),3*1.05);
        QTest::keyClick(window,Qt::Key_Minus);QVERIFY(std::abs(canvas->eraserRadius()-3)<1e-10);
        QTest::keyClick(window,Qt::Key_BracketRight);QCOMPARE(canvas->eraserRadius(),3*1.05);
        QTest::keyClick(window,Qt::Key_BracketLeft);QVERIFY(std::abs(canvas->eraserRadius()-3)<1e-10);
        QTest::keyClick(window,Qt::Key_Plus,Qt::KeypadModifier);QVERIFY(canvas->eraserRadius()>3);
        canvas->setEraserRadius(3);
        for(int i=0;i<20;++i){QKeyEvent repeat(QEvent::KeyPress,Qt::Key_BracketRight,Qt::NoModifier,QStringLiteral("]"),true);QCoreApplication::sendEvent(window,&repeat);}
        QVERIFY(std::abs(canvas->eraserRadius()-3*std::pow(1.05,20))<1e-10);
        for(int i=0;i<100;++i)QTest::keyClick(window,Qt::Key_Plus);QCOMPARE(canvas->eraserRadius(),12.);
        for(int i=0;i<100;++i)QTest::keyClick(window,Qt::Key_Minus);QCOMPARE(canvas->eraserRadius(),.5);
        canvas->setEraserRadius(std::numeric_limits<double>::quiet_NaN());QCOMPARE(canvas->eraserRadius(),.5);
        canvas->setTool("pen");QTest::keyClick(window,Qt::Key_BracketRight);QCOMPARE(canvas->eraserRadius(),.5);
        QVERIFY(warnings.isEmpty());controller.shutdown();
    }
    void boundedInkRegionFill(){
        Page page;page.size={100,100};
        auto ink=[&](std::vector<Point> points){StrokeObject stroke;stroke.id=newId();for(auto p:points)stroke.samples.push_back({p});page.strokes.push_back(stroke);};
        ink({{20,21},{20,60},{60,60},{60,20},{21,20}});
        auto result=fillInkRegion(page,{40,40},Qt::blue);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
        QCOMPARE(result.image.pixelColor(result.image.width()/2,result.image.height()/2).alpha(),26);
        const auto stronger=fillInkRegion(page,{40,40},Qt::blue,2.,.35);QVERIFY(stronger.error.isEmpty());
        QCOMPARE(stronger.image.pixelColor(stronger.image.width()/2,stronger.image.height()/2).alpha(),89);
        QVERIFY(result.object.corners.front().x>=19&&result.object.corners[2].x<=61);
        QVERIFY(!fillInkRegion(page,{80,80},Qt::blue).error.isEmpty());
        QVERIFY(!fillInkRegion(Page{}, {40,40},Qt::blue).error.isEmpty());
        page.strokes.clear();ink({{20,20},{20,60},{60,60},{60,20}});
        QVERIFY(!fillInkRegion(page,{40,40},Qt::blue).error.isEmpty()); // large gap
        page.strokes.clear();ink({{20,20},{80,20}});ink({{20,20},{20,80}});
        StrokeObject arc;arc.id=newId();for(int i=0;i<=90;++i){const double a=std::numbers::pi*i/180.;arc.samples.push_back({{20+30*std::cos(a),20+30*std::sin(a)}});}page.strokes.push_back(arc);
        result=fillInkRegion(page,{30,30},Qt::red);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
        QVERIFY(result.object.corners[2].x<=51&&result.object.corners[2].y<=51);
        QVERIFY(!fillInkRegion(page,{65,65},Qt::red).error.isEmpty());
        // Inner contours remain holes; the grid never contributes a barrier.
        page.strokes.clear();ink({{10,10},{90,10},{90,90},{10,90},{10,10}});ink({{40,40},{60,40},{60,60},{40,60},{40,40}});
        result=fillInkRegion(page,{20,20},Qt::green);QVERIFY(result.error.isEmpty());
        const auto origin=result.object.corners[0];const double scale=result.image.width()/(result.object.corners[1].x-origin.x);
        QCOMPARE(result.image.pixelColor(int((50-origin.x)*scale),int((50-origin.y)*scale)).alpha(),0);
        page.strokes.clear();page.backgroundStyle.gridType=GridType::Square;
        QVERIFY(!fillInkRegion(page,{20,20},Qt::green).error.isEmpty());
    }
    void markerDrawingAndCtrlFill(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("marker");QCOMPARE(canvas->tool(),QString("marker"));canvas->setPenColor(Qt::red);
        auto screen=[&](QPointF p){return canvas->mapToScene(canvas->screenPoint(p)).toPoint();};
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen({30,30}));QTest::mouseMove(window,screen({60,30}));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({60,30}));
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));QCOMPARE(controller.page()->strokes[0].style.rgba&255,std::uint32_t(26));QCOMPARE(controller.page()->strokes[0].style.maxWidthMm,4.);
        QCOMPARE(canvas->markerOpacity(),.10);QVERIFY(controller.page()->strokes[0].marker);
        canvas->setMarkerOpacity(.35);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen({30,35}));QTest::mouseMove(window,screen({60,35}));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({60,35}));
        QCOMPARE(controller.page()->strokes.back().style.rgba&255,std::uint32_t(89));
        StrokeObject boundary;boundary.id=newId();for(auto p:std::vector<Point>{{40,51},{40,80},{70,80},{70,50},{41,50}})boundary.samples.push_back({p});controller.addStroke(boundary);
        QTest::mouseClick(window,Qt::LeftButton,Qt::ControlModifier,screen({55,65}));QTRY_VERIFY(!controller.documentBusy());QCOMPARE(controller.page()->images.size(),std::size_t(1));
        const auto fillBitmap=controller.image(controller.page()->images[0].id);QCOMPARE(fillBitmap.pixelColor(fillBitmap.width()/2,fillBitmap.height()/2).alpha(),89);
        controller.undo();QVERIFY(controller.page()->images.empty());controller.redo();QCOMPARE(controller.page()->images.size(),std::size_t(1));
        QTest::mouseClick(window,Qt::LeftButton,Qt::ControlModifier,screen({90,65}));QTRY_VERIFY(!controller.documentBusy());QCOMPARE(controller.page()->images.size(),std::size_t(1));
        canvas->setTool("pen");const Point sectorCenter{120,100};
        const auto first=screen({140,100});QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,first);
        for(int i=1;i<=90;++i){const double a=1.4*i/90.;QTest::mouseMove(window,screen({sectorCenter.x+20*std::cos(a),sectorCenter.y+20*std::sin(a)}));}
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({sectorCenter.x+20*std::cos(1.4),sectorCenter.y+20*std::sin(1.4)}));
        QCOMPARE(controller.page()->shapes.size(),std::size_t(1));
        const auto straightened=controller.page()->shapes.back();QCOMPARE(straightened.kind,ShapeKind::Line);QCOMPARE(straightened.vertices.size(),std::size_t(2));
        QVERIFY(length(straightened.vertices.front()-Point{140,100})<.5);
        QVERIFY(length(straightened.vertices.back()-Point{sectorCenter.x+20*std::cos(1.4),sectorCenter.y+20*std::sin(1.4)})<.5);
        controller.undo();QVERIFY(controller.page()->shapes.empty());QVERIFY(controller.page()->strokes.back().samples.size()>20);
        controller.redo();QCOMPARE(controller.page()->shapes.back().kind,ShapeKind::Line);
        const Point corner{120,100},endA{150,100},endB{120+30*std::cos(1.4),100+30*std::sin(1.4)};
        for(const auto end:{endA,endB}){ShapeObject edge;edge.id=newId();edge.kind=ShapeKind::Line;edge.vertices={corner,end};controller.addShape(edge);}
        const Point shifted{121,99.3};
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen({shifted.x+20,shifted.y}));
        for(int i=1;i<=90;++i){const double a=1.4*i/90.;QTest::mouseMove(window,screen({shifted.x+20*std::cos(a),shifted.y+20*std::sin(a)}));}
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({shifted.x+20*std::cos(1.4),shifted.y+20*std::sin(1.4)}));
        const auto& snapped=controller.page()->shapes.back();QCOMPARE(snapped.kind,ShapeKind::CircularSector);
        QVERIFY(length(snapped.center-corner)<1e-8);QVERIFY(distanceToSegment(snapped.vertices[1],corner,endA)<1e-8);QVERIFY(distanceToSegment(snapped.vertices.back(),corner,endB)<1e-8);QCOMPARE(snapped.fillOpacity,.10);
        const Point squareCorner{160,140};
        for(const auto end:{Point{180,140},Point{160,160}}){ShapeObject edge;edge.id=newId();edge.kind=ShapeKind::Line;edge.vertices={squareCorner,end};controller.addShape(edge);}
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen({160,144}));
        for(int i=1;i<=20;++i)QTest::mouseMove(window,screen({160+i*.2,144}));
        for(int i=1;i<=20;++i)QTest::mouseMove(window,screen({164,144-i*.2}));
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({164,140}));
        QCOMPARE(controller.page()->shapes.back().kind,ShapeKind::RightAngle);QVERIFY(length(controller.page()->shapes.back().vertices[0]-squareCorner)<1e-8);
        const auto filename=directory.filePath("marker.board");controller.saveAs(QUrl::fromLocalFile(filename));QTRY_VERIFY(!controller.busy());
        const auto saved=ProjectStore::load(filename);QVERIFY(saved);QCOMPARE(saved.project.pages[0].images.size(),std::size_t(1));QVERIFY(saved.project.pages[0].images[0].inkFill);QCOMPARE(saved.project.pages[0].strokes[0].style.rgba&255,std::uint32_t(26));
        QCOMPARE(saved.project.pages[0].shapes.back().kind,ShapeKind::RightAngle);QVERIFY(length(saved.project.pages[0].shapes.back().vertices[0]-squareCorner)<1e-8);
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void partialShapeEraserAndTextResolution(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.newDefault();QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("eraser");canvas->setEraserRadius(2);
        ShapeObject shape;shape.id=newId();shape.kind=ShapeKind::Rectangle;shape.vertices={{30,30},{70,30},{70,70},{30,70}};controller.addShape(shape);
        auto screen=[&](QPointF p){return canvas->mapToScene(canvas->screenPoint(p)).toPoint();};
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,screen({50,30}));QVERIFY(controller.page()->shapes[0].erasedRegions.empty());
        QTest::mouseClick(window,Qt::LeftButton,Qt::ControlModifier,screen({50,30}));
        QCOMPARE(controller.page()->shapes.size(),std::size_t(1));QVERIFY(!controller.page()->shapes[0].erasedRegions.empty());QVERIFY(!canvas->eraserShapes());
        QVERIFY(!hitTest(controller.page()->shapes[0],{50,30},0));QVERIFY(hitTest(controller.page()->shapes[0],{35,30},0));
        controller.undo();QVERIFY(controller.page()->shapes[0].erasedRegions.empty());controller.redo();QVERIFY(!controller.page()->shapes[0].erasedRegions.empty());
        canvas->setEraserShapes(true);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen({50,40}));QTest::mouseMove(window,screen({50,60}));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen({50,60}));
        QVERIFY(!hitTest(controller.page()->shapes[0],{50,50},0));QVERIFY(hitTest(controller.page()->shapes[0],{35,50},0));
        // Small moves remain inside the first erased disk while its rim reaches
        // new ink. They must erase continuously, not wait for the center to exit.
        canvas->setEraserShapes(false);QTest::mousePress(window,Qt::LeftButton,Qt::ControlModifier,screen({35,50}));
        for(int i=1;i<=12;++i){const auto pos=screen({35+i*.125,50});QMouseEvent move(QEvent::MouseMove,pos,pos,window->mapToGlobal(pos),Qt::NoButton,Qt::LeftButton,Qt::ControlModifier);QCoreApplication::sendEvent(window,&move);}
        QTest::mouseRelease(window,Qt::LeftButton,Qt::ControlModifier,screen({36.5,50}));QVERIFY(!canvas->eraserShapes());
        QVERIFY(!hitTest(controller.page()->shapes[0],{38,50},0));QVERIFY(hitTest(controller.page()->shapes[0],{39,50},0));
        Page rendered;rendered.id=newId();rendered.size=PageSize{100,100};rendered.background=0x000000ff;
        shape.style.rgba=0xffffffff;rendered.shapes={shape};
        const auto render=[&]{QImage image(1000,1000,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);const auto error=paintPage(painter,rendered,10);painter.end();return image;};
        const auto before=render();rendered.shapes[0].erasedRegions={{{50,50},{50,50},2}};const auto after=render();
        QCOMPARE(after.pixelColor(350,500),before.pixelColor(350,500));QCOMPARE(after.pixelColor(500,500).alpha(),0);
        controller.upsertText({},QPointF(90,100),{{"source","Teste"},{"fontSizePt",18},{"fontFamily","Lobster Two"},{"color","#ffffff"}});QTRY_VERIFY(!controller.textBusy());
        QCOMPARE(controller.page()->texts.size(),std::size_t(1));const auto text=controller.page()->texts[0];const auto natural=textNaturalSize(text);
        controller.refreshTextTextures(3);QTRY_VERIFY(controller.image(text.id).width()<=std::ceil(natural.width()*8));
        controller.refreshTextTextures(16);controller.refreshTextTextures(30);
        QTRY_VERIFY_WITH_TIMEOUT(controller.image(text.id).width()>=std::floor(natural.width()*64),4000);
        const auto filename=directory.filePath("erased.board");controller.saveAs(QUrl::fromLocalFile(filename));QTRY_VERIFY(!controller.busy());
        const auto saved=ProjectStore::load(filename);QVERIFY(saved);QVERIFY(saved.project.pages[0].shapes[0].erasedRegions.empty());QVERIFY(saved.project.pages[0].shapes[0].eraseMask);QVERIFY(!hitTest(saved.project.pages[0].shapes[0],{50,50},0));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void projectNamesAndLibrarySearch(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());
        controller.newProject("Área de matemática","A4",210,297,false,"Sem grade");
        QTRY_VERIFY(!controller.busy());controller.home();
        const auto entry=controller.recentProjects().first().toMap();const auto id=entry["id"].toString(),path=entry["path"].toString();
        const auto folder=controller.saveFolder({},"Aulas","#268fb5");QVERIFY(controller.moveProjectToFolder(id,folder));
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto click=[&](QQuickItem* item){QVERIFY(item);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,item->mapToScene({item->width()/2,item->height()/2}).toPoint());};
        auto* search=findVisualItem(window->contentItem(),"boardSearchField");QVERIFY(search);search->setProperty("text","AREA");
        QTRY_VERIFY(findVisualItem(window->contentItem(),"projectCard_"+id));QCOMPARE(controller.searchProjects("area",{},"all").size(),1);
        search->setProperty("text","inexistente");QTRY_VERIFY(!findVisualItem(window->contentItem(),"projectCard_"+id));search->setProperty("text","area");
        QTRY_VERIFY(findVisualItem(window->contentItem(),"editProjectButton_"+id));click(findVisualItem(window->contentItem(),"editProjectButton_"+id));
        auto* dialog=window->findChild<QObject*>("editProjectDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());
        auto* field=findVisualItem(window->contentItem(),"editProjectNameField");QVERIFY(field);field->setProperty("text","Geometria avançada");click(findVisualItem(window->contentItem(),"saveProjectNameButton"));QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.recentProjects().first().toMap()["name"].toString(),QString("Geometria avançada"));QCOMPARE(controller.recentProjects().first().toMap()["folderId"].toString(),folder);
        QCOMPARE(QString::fromStdString(ProjectStore::load(path).project.name),QString("Geometria avançada"));
        QTRY_VERIFY(!dialog->property("visible").toBool());search->setProperty("text","geometria");QTRY_VERIFY(findVisualItem(window->contentItem(),"projectCard_"+id));QTest::qWait(100);click(findVisualItem(window->contentItem(),"projectCard_"+id));QTRY_VERIFY(controller.active());
        click(findVisualItem(window->contentItem(),"projectTitleArea"));auto* title=findVisualItem(window->contentItem(),"projectTitleInput");QVERIFY(title);QTRY_VERIFY(title->hasActiveFocus());title->setProperty("text","Geometria da turma");QTest::keyClick(window,Qt::Key_Return);QCOMPARE(controller.projectName(),QString("Geometria da turma"));
        click(findVisualItem(window->contentItem(),"projectTitleArea"));title->setProperty("text","Geometria final");
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("hand");click(canvas);QTRY_COMPARE(controller.projectName(),QString("Geometria final"));
        click(findVisualItem(window->contentItem(),"editorBackButton"));QTRY_VERIFY(!controller.active());QCOMPARE(QString::fromStdString(ProjectStore::load(path).project.name),QString("Geometria final"));
        QVERIFY(!controller.renameProject(id," "));QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void homeFoldersAndDrag(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.setTheme("Dark");controller.newDefault();controller.home();QTRY_VERIFY(!controller.busy());QTRY_COMPARE(controller.recentProjects().size(),1);const auto projectId=controller.recentProjects()[0].toMap()["id"].toString();
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* newFolder=findVisualItem(window->contentItem(),"newFolderButton");QVERIFY(newFolder);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,newFolder->mapToScene({newFolder->width()/2,newFolder->height()/2}).toPoint());auto* dialog=window->findChild<QObject*>("folderDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());auto* name=window->findChild<QObject*>("folderNameField");QVERIFY(name);name->setProperty("text","Geometria");auto* save=findVisualItem(window->contentItem(),"saveFolderButton");QVERIFY(save);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,save->mapToScene({save->width()/2,save->height()/2}).toPoint());QTRY_COMPARE(controller.folders().size(),1);QTRY_VERIFY(!dialog->property("visible").toBool());const auto folderId=controller.folders()[0].toMap()["id"].toString();
        auto* folder=findVisualItem(window->contentItem(),"folderCard_"+folderId);QVERIFY(folder);auto* project=findVisualItem(window->contentItem(),"projectCard_"+projectId);QVERIFY(project);const auto from=project->mapToScene({project->width()/2,project->height()/3}).toPoint(),to=folder->mapToScene({folder->width()/2,folder->height()/2}).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);for(int i=1;i<=12;++i)QTest::mouseMove(window,from+(to-from)*i/12,20);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,to);QTRY_COMPARE(controller.recentProjects()[0].toMap()["folderId"].toString(),folderId);QVERIFY(!controller.active());
        auto* edit=findVisualItem(window->contentItem(),"editFolder_"+folderId);QVERIFY(edit);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,edit->mapToScene({edit->width()/2,edit->height()/2}).toPoint());QTRY_VERIFY(dialog->property("opened").toBool());name->setProperty("text","Aulas");auto* content=dialog->property("contentItem").value<QQuickItem*>();QVERIFY(content);auto* purple=findVisualItem(content,"paletteColor_3");QVERIFY(purple);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,purple->mapToScene({purple->width()/2,purple->height()/2}).toPoint());save=findVisualItem(window->contentItem(),"saveFolderButton");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,save->mapToScene({save->width()/2,save->height()/2}).toPoint());QTRY_VERIFY(!dialog->property("visible").toBool());QCOMPARE(controller.folders()[0].toMap()["name"].toString(),QString("Aulas"));QCOMPARE(controller.folders()[0].toMap()["color"].toString(),QString("#9765c5"));
        folder=findVisualItem(window->contentItem(),"folderCard_"+folderId);QVERIFY(folder);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,folder->mapToScene({folder->width()/3,folder->height()/2}).toPoint());project=findVisualItem(window->contentItem(),"projectCard_"+projectId);QTRY_VERIFY(project);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,project->mapToScene({project->width()/2,project->height()/2}).toPoint());QTRY_VERIFY(controller.active());auto* editorBack=findVisualItem(window->contentItem(),"editorBackButton");QVERIFY(editorBack);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,editorBack->mapToScene({editorBack->width()/2,editorBack->height()/2}).toPoint());QTRY_VERIFY(!controller.active());QTRY_VERIFY((project=findVisualItem(window->contentItem(),"projectCard_"+projectId)));QVERIFY(findVisualItem(window->contentItem(),"newProjectInFolderButton")->isVisible());QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/home-folders-dark.png"));controller.setTheme("Light");QTest::qWait(100);QVERIFY(window->grabWindow().save("screenshots/home-folders-light.png"));window->resize(520,640);QTest::qWait(100);QVERIFY(window->grabWindow().save("screenshots/home-folders-small.png"));
        project=findVisualItem(window->contentItem(),"projectCard_"+projectId);auto* unfiled=findVisualItem(window->contentItem(),"backToProjectsButton");QVERIFY(project&&unfiled);const auto moveFrom=project->mapToScene({project->width()/2,project->height()/3}).toPoint(),moveTo=unfiled->mapToScene({unfiled->width()/2,unfiled->height()/2}).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,moveFrom);for(int i=1;i<=12;++i)QTest::mouseMove(window,moveFrom+(moveTo-moveFrom)*i/12,20);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,moveTo);QTRY_VERIFY(controller.recentProjects()[0].toMap()["folderId"].toString().isEmpty());QCOMPARE(controller.folders()[0].toMap()["count"].toInt(),0);QCOMPARE(warnings.size(),0);QVERIFY(controller.shutdown());
    }
    void selectedCompassDrawsAndRulerReleasesSnap(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.newDefault();controller.setRecognitionEnabled(false);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(!controller.busy());
        canvas->setTool("compass");canvas->setCompassRadius(25);canvas->setTool("select");const auto m=canvas->compassGeometry();
        const auto circlePoint=[&](double angle){return canvas->mapToScene({m["cx"].toDouble()+m["radius"].toDouble()*std::cos(angle),m["cy"].toDouble()+m["radius"].toDouble()*std::sin(angle)}).toPoint();};
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,circlePoint(0));QVERIFY(canvas->drawing());for(int i=1;i<=180;++i)QTest::mouseMove(window,circlePoint(i*std::numbers::pi/90));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,circlePoint(2*std::numbers::pi));QCOMPARE(controller.page()->shapes.size(),std::size_t(1));QCOMPARE(controller.page()->shapes[0].radiusX,25.);QCOMPARE(canvas->tool(),QString("select"));controller.undo();QVERIFY(controller.page()->shapes.empty());controller.redo();
        canvas->setCompassVisible(false);canvas->setTool("ruler");canvas->setRulerAngle(0);const auto r=canvas->rulerGeometry();const double scale=r["scale"].toDouble();
        const auto edgePoint=[&](double x,double y){return canvas->mapToScene({r["x"].toDouble()+x*scale,r["y"].toDouble()+y*scale}).toPoint();};
        canvas->setTool("pen");QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,edgePoint(20,-1));QTest::mouseMove(window,edgePoint(40,-1));QTest::mouseMove(window,edgePoint(60,-15));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,edgePoint(70,-15));QCOMPARE(controller.page()->strokes.size(),std::size_t(1));const auto& samples=controller.page()->strokes[0].samples;QVERIFY(samples.size()>=3);const double initialY=samples.front().position.y;QVERIFY(std::abs(samples[1].position.y-initialY)<1e-8);QVERIFY(samples.back().position.y<initialY-10);QCOMPARE(warnings.size(),0);QVERIFY(controller.shutdown());
    }
    void rulerBlocksInkInsideBody(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.newDefault();controller.setRecognitionEnabled(false);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setRulerVisible(true);canvas->setTool("pen");
        for(double angle:{0.,25.})for(bool snap:{false,true}){
            canvas->setRulerAngle(angle);canvas->setRulerSnap(snap);const auto map=canvas->rulerGeometry();const double scale=map["scale"].toDouble(),a=angle*std::numbers::pi/180;
            const auto point=[&](double x,double y){return canvas->mapToScene({map["x"].toDouble()+scale*(x*std::cos(a)-y*std::sin(a)),map["y"].toDouble()+scale*(x*std::sin(a)+y*std::cos(a))}).toPoint();};
            const auto start=point(20,-1),along=point(40,-1),inside=point(40,6);
            const auto count=controller.page()->strokes.size();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,along);QTest::mouseMove(window,inside);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,inside);
            QCOMPARE(controller.page()->strokes.size(),count+1);QVERIFY(!canvas->drawing());
            const auto& stroke=controller.page()->strokes.back();const auto last=stroke.samples.back().position;
            const auto screenPoint=point(40,0);const auto center=canvas->viewportCenter();const double worldY=center.y()+(screenPoint.y()-canvas->y()-canvas->height()/2)/scale;
            QVERIFY(std::abs(last.y-worldY)<1.);
            controller.undo();QCOMPARE(controller.page()->strokes.size(),count);
        }
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void rulerSnapAndCompassGestures(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.setTheme("Dark");controller.newDefault();controller.setRecognitionEnabled(false);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(!controller.busy());
        canvas->setTool("ruler");QVERIFY(canvas->rulerVisible());auto ruler=canvas->rulerGeometry();
        const auto rulerPoint=[&](double x,double y){const auto m=canvas->rulerGeometry();const double a=m["angle"].toDouble()*std::numbers::pi/180,s=m["scale"].toDouble();return canvas->mapToScene({m["x"].toDouble()+s*(x*std::cos(a)-y*std::sin(a)),m["y"].toDouble()+s*(x*std::sin(a)+y*std::cos(a))}).toPoint();};
        auto center=rulerPoint(canvas->rulerLength()/2,6);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(window,center+QPoint(25,-15));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,center+QPoint(25,-15));QVERIFY(std::abs(canvas->rulerGeometry()["x"].toDouble()-ruler["x"].toDouble()-25)<1);
        const auto beforeCancel=canvas->rulerGeometry();center=rulerPoint(canvas->rulerLength()/2,6);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(window,center+QPoint(20,0));QTest::keyClick(window,Qt::Key_Escape);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,center+QPoint(20,0));QCOMPARE(canvas->rulerGeometry()["x"],beforeCancel["x"]);
        const auto resizeStart=rulerPoint(canvas->rulerLength(),6),resizeEnd=rulerPoint(150,6);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,resizeStart);QTest::mouseMove(window,resizeEnd);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,resizeEnd);QVERIFY(std::abs(canvas->rulerLength()-150)<1);
        const auto rotateStart=rulerPoint(0,6),rotateEnd=rulerPoint(canvas->rulerLength()/2,6-canvas->rulerLength()/2);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,rotateStart);QTest::mouseMove(window,rotateEnd);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,rotateEnd);QVERIFY(std::abs(std::cos(canvas->rulerAngle()*std::numbers::pi/180))<.03);
        canvas->setRulerAngle(25);canvas->setTool("pen");
        QPointingDevice pen("Geometry test pen",456,QInputDevice::DeviceType::Stylus,QPointingDevice::PointerType::Pen,QInputDevice::Capability::Position|QInputDevice::Capability::Pressure,1,1);
        const auto send=[&](QEvent::Type type,QPoint pos,double pressure){QTabletEvent event(type,&pen,pos,window->mapToGlobal(pos),pressure,0,0,0,0,0,Qt::NoModifier,type==QEvent::TabletMove?Qt::NoButton:Qt::LeftButton,type==QEvent::TabletRelease?Qt::NoButton:Qt::LeftButton);QCoreApplication::sendEvent(window,&event);};
        send(QEvent::TabletPress,rulerPoint(20,-1),.2);for(int i=1;i<=20;++i)send(QEvent::TabletMove,rulerPoint(20+i*3,-1+(i%3)*.2),.2+i*.035);send(QEvent::TabletRelease,rulerPoint(80,-1),0);
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));const auto stroke=controller.page()->strokes.front();const auto a=stroke.samples.front().position,b=stroke.samples.back().position;
        for(const auto& sample:stroke.samples)QVERIFY(length(sample.position-projectPointOntoLine(sample.position,a,b))<1e-8);QVERIFY(stroke.samples.back().pressure>stroke.samples.front().pressure+.4);
        controller.undo();QVERIFY(controller.page()->strokes.empty());controller.redo();QCOMPARE(controller.page()->strokes[0].samples.size(),stroke.samples.size());
        canvas->zoomBy(1.5);canvas->setRulerSnap(false);auto start=rulerPoint(25,-2),end=rulerPoint(65,-3);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,end);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,end);QCOMPARE(controller.page()->strokes.size(),std::size_t(2));
        canvas->setRulerVisible(false);canvas->setTool("compass");canvas->setCompassRadius(25);auto compass=canvas->compassGeometry();
        const auto compassCenter=canvas->mapToScene({compass["cx"].toDouble(),compass["cy"].toDouble()}).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,compassCenter);QTest::mouseMove(window,compassCenter+QPoint(15,8));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,compassCenter+QPoint(15,8));QVERIFY(std::abs(canvas->compassGeometry()["cx"].toDouble()-compass["cx"].toDouble()-15)<1);
        compass=canvas->compassGeometry();const auto opening=canvas->mapToScene({compass["ox"].toDouble(),compass["oy"].toDouble()}).toPoint();const auto openingEnd=opening+QPoint(qRound(compass["radius"].toDouble()*.4),0);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,opening);QTest::mouseMove(window,opening);QVERIFY(std::abs(canvas->compassRadius()-25)<.1);QTest::mouseMove(window,openingEnd);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,openingEnd);QVERIFY(std::abs(canvas->compassRadius()-35)<1);QCOMPARE(canvas->compassGeometry()["cx"],compass["cx"]);canvas->setCompassRadius(25);
        compass=canvas->compassGeometry();const auto grip=canvas->mapToScene({compass["ox"].toDouble(),compass["oy"].toDouble()}).toPoint()+QPoint(3,-2);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,grip);QTest::mouseMove(window,grip+QPoint(14,21));const auto opened=canvas->compassGeometry();QVERIFY(std::abs(opened["px"].toDouble()-compass["px"].toDouble()-14)<1);QVERIFY(std::abs(opened["py"].toDouble()-compass["py"].toDouble()-21)<1);QCOMPARE(opened["cx"],compass["cx"]);QTest::keyClick(window,Qt::Key_Escape);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,grip+QPoint(14,21));QCOMPARE(canvas->compassGeometry()["px"],compass["px"]);
        const auto hinge=canvas->mapToScene({compass["hx"].toDouble(),compass["hy"].toDouble()}).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,hinge);QTest::mouseMove(window,hinge+QPoint(-12,8));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,hinge+QPoint(-12,8));QVERIFY(std::abs(canvas->compassGeometry()["cx"].toDouble()-compass["cx"].toDouble()+12)<1);QCOMPARE(canvas->compassRadius(),25.);
        compass=canvas->compassGeometry();auto* touch=QTest::createTouchDevice(QInputDevice::DeviceType::TouchScreen);const auto touchStart=canvas->mapToScene({compass["cx"].toDouble(),compass["cy"].toDouble()}).toPoint();QTest::touchEvent(window,touch).press(0,touchStart,window);QTest::touchEvent(window,touch).move(0,touchStart+QPoint(12,6),window);QTest::touchEvent(window,touch).release(0,touchStart+QPoint(12,6),window);QVERIFY(std::abs(canvas->compassGeometry()["cx"].toDouble()-compass["cx"].toDouble()-12)<1);QCOMPARE(controller.page()->strokes.size(),std::size_t(2));
        const auto compassPoint=[&](double angle){const auto m=canvas->compassGeometry();return canvas->mapToScene({m["cx"].toDouble()+m["radius"].toDouble()*std::cos(angle),m["cy"].toDouble()+m["radius"].toDouble()*std::sin(angle)}).toPoint();};
        send(QEvent::TabletPress,compassPoint(0),.6);for(int i=1;i<=180;++i)send(QEvent::TabletMove,compassPoint(i*2*std::numbers::pi/180),.6);send(QEvent::TabletRelease,compassPoint(2*std::numbers::pi),0);
        QCOMPARE(controller.page()->shapes.size(),std::size_t(1));QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Circle);QCOMPARE(controller.page()->shapes[0].radiusX,25.);QCOMPARE(controller.page()->shapes[0].radiusY,25.);QVERIFY(!canvas->drawing());
        controller.undo();QVERIFY(controller.page()->shapes.empty());controller.redo();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,compassPoint(0));for(int i=1;i<=45;++i)QTest::mouseMove(window,compassPoint(i*std::numbers::pi/90));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,compassPoint(std::numbers::pi/2));QCOMPARE(controller.page()->strokes.size(),std::size_t(3));
        const auto arc=controller.page()->strokes.back();QVERIFY(arc.samples.size()>30);for(const auto& sample:arc.samples)QVERIFY(std::abs(length(sample.position-controller.page()->shapes[0].center)-25)<1e-8);
        const auto m=canvas->compassGeometry();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene({m["px"].toDouble(),m["py"].toDouble()}).toPoint());QTest::mouseMove(window,compassPoint(2));QTest::keyClick(window,Qt::Key_Escape);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,compassPoint(2));QCOMPARE(controller.page()->strokes.size(),std::size_t(3));QVERIFY(!canvas->drawing());
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/milestone5-compass-dark.png"));canvas->setTool("ruler");canvas->centerGeometryTools();QVERIFY(window->grabWindow().save("screenshots/milestone5-guides-dark.png"));
        canvas->zoomBy(2);canvas->setTool("compass");const auto blueGeometry=canvas->compassGeometry();
        const auto bluePoint=canvas->mapToScene({blueGeometry["hx"].toDouble()*.13+blueGeometry["px"].toDouble()*.87,blueGeometry["hy"].toDouble()*.13+blueGeometry["py"].toDouble()*.87}).toPoint();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,bluePoint);QCOMPARE(canvas->compassGeometry()["cx"],blueGeometry["cx"]);QCOMPARE(canvas->compassGeometry()["cy"],blueGeometry["cy"]);QCOMPARE(canvas->compassRadius(),25.);
        canvas->setTool("select");QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,bluePoint);QTest::mouseMove(window,bluePoint+QPoint(15,6));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,bluePoint+QPoint(15,6));QCOMPARE(canvas->selectedGuide(),QString("compass"));QCOMPARE(canvas->tool(),QString("select"));QCOMPARE(canvas->compassGeometry()["cx"],blueGeometry["cx"]);QVERIFY(canvas->compassRadius()!=25.);
        QTest::keyClick(window,Qt::Key_Delete);QVERIFY(!canvas->compassVisible());QVERIFY(canvas->rulerVisible());QCOMPARE(controller.page()->shapes.size(),std::size_t(1));
        const auto rulerCenter=rulerPoint(canvas->rulerLength()/2,6);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,rulerCenter);QCOMPARE(canvas->selectedGuide(),QString("ruler"));QTest::keyClick(window,Qt::Key_Delete);QVERIFY(!canvas->rulerVisible());QVERIFY(canvas->selectedGuide().isEmpty());QCOMPARE(controller.page()->strokes.size(),std::size_t(3));
        const auto path=dir.filePath("geometry.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.busy());controller.openPath(path);QTRY_VERIFY(!controller.loading());QCOMPARE(controller.page()->strokes.size(),std::size_t(3));QCOMPARE(controller.page()->shapes.size(),std::size_t(1));QVERIFY(!canvas->rulerVisible());QVERIFY(!canvas->compassVisible());
        window->resize(520,640);QTest::qWait(100);QVERIFY(window->grabWindow().save("screenshots/milestone5-small.png"));QCOMPARE(warnings.size(),0);QVERIFY(controller.shutdown());
    }
    void constructionsAndGeometryOptions(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.newDefault();QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(!controller.busy());
        const auto worldPoint=[&](Point p){const auto c=canvas->viewportCenter();const double scale=canvas->zoom()*96/25.4;return canvas->mapToScene({canvas->width()/2+(p.x-c.x())*scale,canvas->height()/2+(p.y-c.y())*scale}).toPoint();};
        ShapeObject line;line.id=newId();line.kind=ShapeKind::Line;line.vertices={{70,100},{140,100}};controller.addShape(line);canvas->setTool("select");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,worldPoint({100,100}));QCOMPARE(canvas->selectedCount(),1);QVERIFY(canvas->constructFromSelection("bisector"));QCOMPARE(controller.page()->shapes.size(),std::size_t(2));controller.undo();QCOMPARE(controller.page()->shapes.size(),std::size_t(1));controller.redo();
        ShapeObject triangle;triangle.id=newId();triangle.kind=ShapeKind::Triangle;triangle.vertices={{70,150},{140,150},{105,210}};controller.addShape(triangle);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,worldPoint({105,165}));QVERIFY(canvas->constructFromSelection("circumcircle"));QCOMPARE(controller.page()->shapes.size(),std::size_t(4));const auto circle=controller.page()->shapes.back();for(auto vertex:triangle.vertices)QVERIFY(std::abs(length(vertex-circle.center)-circle.radiusX)<1e-8);
        canvas->setTool("ruler");auto* options=window->findChild<QObject*>("geometryOptions");QVERIFY(options);QVERIFY(QMetaObject::invokeMethod(options,"open"));QTRY_VERIFY(options->property("opened").toBool());QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/milestone5-ruler-options-light.png"));QVERIFY(QMetaObject::invokeMethod(options,"close"));QTRY_VERIFY(!options->property("visible").toBool());
        canvas->setRulerVisible(false);canvas->setTool("compass");QVERIFY(QMetaObject::invokeMethod(options,"open"));QTRY_VERIFY(options->property("opened").toBool());QVERIFY(window->grabWindow().save("screenshots/milestone5-compass-options-light.png"));QVERIFY(QMetaObject::invokeMethod(options,"close"));QTRY_VERIFY(!options->property("visible").toBool());
        canvas->drawCompassCircle();QCOMPARE(controller.page()->shapes.size(),std::size_t(5));
        window->resize(520,640);canvas->setTool("ruler");QVERIFY(QMetaObject::invokeMethod(options,"open"));QTRY_VERIFY(options->property("opened").toBool());
        QVERIFY(options->property("height").toDouble()<window->height());QVERIFY(options->property("y").toDouble()>=0);QVERIFY(options->property("y").toDouble()+options->property("height").toDouble()<=window->height());
        QVERIFY(window->grabWindow().save("screenshots/milestone5-ruler-options-small.png"));QVERIFY(QMetaObject::invokeMethod(options,"close"));QTRY_VERIFY(!options->property("visible").toBool());QCOMPARE(warnings.size(),0);QVERIFY(controller.shutdown());
    }
    void quickPageNavigation(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.setTheme("Dark");controller.newDefault();controller.addPage();controller.selectPage(0);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));QTRY_VERIFY(!controller.busy());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);
        auto* next=findVisualItem(window->contentItem(),"nextPageButton");QVERIFY(next);QVERIFY(next->mapToScene(QPointF(0,next->height())).y()<=canvas->y());
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/quick-pages-dark-top.png"));
        auto click=[&](const QString& name){auto* button=findVisualItem(window->contentItem(),name);if(!button)return false;QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());return true;};
        QVERIFY(click("nextPageButton"));QCOMPARE(controller.currentPage(),1);QVERIFY(controller.page()->strokes.empty());QVERIFY(click("previousPageButton"));QCOMPARE(controller.currentPage(),0);
        QVERIFY(!findVisualItem(window->contentItem(),"previousPageButton")->isEnabled());QVERIFY(click("pageCounterButton"));auto* panel=window->findChild<QObject*>("pagesPanel");QVERIFY(panel);QTRY_VERIFY(panel->property("opened").toBool());QVERIFY(QMetaObject::invokeMethod(panel,"close"));QTRY_VERIFY(!panel->property("visible").toBool());
        auto* newPage=findVisualItem(window->contentItem(),"newPageButton");QVERIFY(newPage);
        QSignalSpy enabledChanges(newPage,&QQuickItem::enabledChanged);
        controller.save();QVERIFY(!controller.documentBusy());QVERIFY(newPage->isEnabled());
        QVERIFY(click("newPageButton"));QCOMPARE(controller.pageCount(),3);QCOMPARE(controller.currentPage(),2);
        QTRY_VERIFY(!controller.busy());QTest::qWait(1500);QTRY_VERIFY(!controller.busy());
        QVERIFY(newPage->isEnabled());QCOMPARE(enabledChanges.count(),0);
        window->resize(520,640);QTest::qWait(200);QVERIFY(click("fileMenuButton"));auto* fileOptions=window->findChild<QObject*>("fileOptions");QVERIFY(fileOptions);QTRY_VERIFY(fileOptions->property("opened").toBool());
        auto* exportButton=findVisualItem(window->contentItem(),"exportPdfButton");QVERIFY(exportButton);QVERIFY(exportButton->isVisible());QVERIFY(exportButton->mapToScene(QPointF(exportButton->width(),0)).x()<=window->width());
        QVERIFY(window->grabWindow().save("screenshots/editor-file-actions-small.png"));
        QVERIFY(QMetaObject::invokeMethod(fileOptions,"close"));QTRY_VERIFY(!fileOptions->property("visible").toBool());
        QVERIFY(next->mapToScene(QPointF(0,next->height())).y()<=canvas->y());
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/quick-pages-dark-small.png"));QCOMPARE(warnings.size(),0);QVERIFY(controller.shutdown());
    }
    void infiniteCanvasDrawsOutsidePhysicalPage(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.newProject("Infinite","Infinito",0,0,false,"Sem grade","#ffffff");QTRY_VERIFY(!controller.busy());QVERIFY(controller.pageInfinite());
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QVERIFY(canvas->setZoom(.5));
        const auto screen=[&](Point p){return canvas->mapToScene(canvas->screenPoint({p.x,p.y})).toPoint();};
        const auto draw=[&](Point a,Point b){QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,screen(a));QTest::mouseMove(window,screen(b));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,screen(b));};
        draw({-80,80},{-60,90});QCOMPARE(controller.page()->strokes.size(),std::size_t(1));QVERIFY(controller.page()->strokes[0].samples[0].position.x<0);
        draw({400,200},{420,210});QCOMPARE(controller.page()->strokes.size(),std::size_t(2));
        canvas->setTool("hand");QTest::qWait(80);const auto shot=window->grabWindow();const auto outside=screen({-100,100});QCOMPARE(shot.pixelColor(outside),QColor(Qt::white));
        auto* panel=window->findChild<QObject*>("backgroundOptions");QVERIFY(panel);QVERIFY(QMetaObject::invokeMethod(panel,"open"));QTRY_VERIFY(panel->property("opened").toBool());
        QCOMPARE(findVisualItem(window->contentItem(),"currentPageSizeSelector")->property("currentIndex").toInt(),3);
        QVERIFY(QMetaObject::invokeMethod(panel,"close"));QVERIFY(controller.setPageSize("A4",0,0,false));QVERIFY(!controller.pageInfinite());controller.undo();QVERIFY(controller.pageInfinite());controller.redo();QVERIFY(!controller.pageInfinite());controller.undo();
        controller.duplicatePage();QVERIFY(controller.pageInfinite());controller.addPage();QVERIFY(controller.pageInfinite());
        const auto path=dir.filePath("infinite.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.dirty());const auto loaded=ProjectStore::load(path);QVERIFY2(loaded,qPrintable(loaded.error));for(const auto& page:loaded.project.pages)QVERIFY(page.size.infinite);
        Page fillPage;fillPage.size.infinite=true;ShapeObject box;box.id=newId();box.kind=ShapeKind::Polygon;box.vertices={{-100,-100},{-80,-100},{-80,-80},{-100,-80}};box.style.rgba=0x000000ff;fillPage.shapes={box};const auto fill=fillInkRegion(fillPage,{-90,-90},Qt::red);QVERIFY2(fill.error.isEmpty(),qPrintable(fill.error));QVERIFY(fill.object.corners.front().x<0);QVERIFY(fill.object.corners.front().y<0);
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void duplicatePageWithRecoverableInkSavesAndCloses(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.newDefault();
        StrokeObject stroke;stroke.id=newId();stroke.samples={{{10,10},1},{{20,20},1}};controller.addStroke(stroke);
        controller.eraseObjects({{CanvasObject(stroke),std::nullopt}},{stroke});QCOMPARE(controller.page()->erasedInk.size(),std::size_t(1));
        controller.duplicatePage();QCOMPARE(controller.pageCount(),2);QVERIFY(controller.page()->erasedInk[0].id!=stroke.id);
        StrokeObject active;active.id=newId();active.samples={{{30,30},1},{{40,40},1}};controller.addStroke(active);controller.duplicatePage();
        const auto path=dir.filePath("duplicate.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.busy());QVERIFY2(!controller.dirty(),qPrintable(controller.status()));
        const auto loaded=ProjectStore::load(path);QVERIFY2(loaded,qPrintable(loaded.error));QCOMPARE(loaded.project.pages.size(),std::size_t(3));
        for(const auto& page:loaded.project.pages)QVERIFY(page.erasedInk.empty());
        controller.home();QVERIFY(!controller.active());QVERIFY(controller.shutdown());
    }
    void currentPageSizeAndFullScreen(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();controller.addPage();
        StrokeObject stroke;stroke.id=newId();stroke.samples={{{20,30},1},{{40,50},1}};controller.addStroke(stroke);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* fullScreen=findVisualItem(window->contentItem(),"fullScreenButton");QVERIFY(fullScreen);
        auto* fileButton=findVisualItem(window->contentItem(),"fileMenuButton");QVERIFY(fileButton);
        const auto centerY=[](QQuickItem* item){return item->mapToScene({item->width()/2,item->height()/2}).y();};
        QCOMPARE(centerY(fullScreen),36.);QCOMPARE(centerY(fileButton),centerY(fullScreen));
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,fullScreen->mapToScene({fullScreen->width()/2,fullScreen->height()/2}).toPoint());QTRY_COMPARE(window->visibility(),QWindow::FullScreen);
        QTRY_COMPARE(centerY(fullScreen),30.);QCOMPARE(centerY(fileButton),centerY(fullScreen));
        QTest::keyClick(window,Qt::Key_F11);QTRY_VERIFY(window->visibility()!=QWindow::FullScreen);
        QTRY_COMPARE(centerY(fullScreen),36.);
        auto* panel=window->findChild<QObject*>("backgroundOptions");QVERIFY(panel);QVERIFY(QMetaObject::invokeMethod(panel,"open"));QTRY_VERIFY(panel->property("opened").toBool());
        QCOMPARE(findVisualItem(window->contentItem(),"currentPageWidthField")->property("text").toString(),QString("210.00"));
        auto* orientation=findVisualItem(window->contentItem(),"currentPageOrientation");QVERIFY(orientation);QVERIFY(QMetaObject::invokeMethod(orientation,"selected",Q_ARG(int,1)));QCOMPARE(controller.pageWidth(),297.);QCOMPARE(controller.pageHeight(),210.);
        auto* preset=findVisualItem(window->contentItem(),"currentPageSizeSelector");QVERIFY(preset);preset->setProperty("currentIndex",1);
        QVERIFY(QMetaObject::invokeMethod(panel,"applyPageSize",Q_ARG(QVariant,true)));QCOMPARE(controller.pageWidth(),279.4);QCOMPARE(controller.pageHeight(),215.9);
        preset->setProperty("currentIndex",2);auto* width=findVisualItem(window->contentItem(),"currentPageWidthField");auto* height=findVisualItem(window->contentItem(),"currentPageHeightField");QVERIFY(width);QVERIFY(height);width->setProperty("text","300");height->setProperty("text","400");
        QVERIFY(QMetaObject::invokeMethod(panel,"applyPageSize",Q_ARG(QVariant,false)));QCOMPARE(controller.pageWidth(),300.);QCOMPARE(controller.pageHeight(),400.);
        QVERIFY(!controller.setPageSize("Personalizado",0,400,false));QCOMPARE(controller.pageWidth(),300.);
        QSignalSpy pageChanges(&controller,&AppController::pagesChanged);controller.undo();QCOMPARE(controller.pageWidth(),279.4);QCOMPARE(pageChanges.count(),1);controller.redo();QCOMPARE(controller.pageWidth(),300.);
        QCOMPARE(controller.page()->strokes.size(),std::size_t(1));QCOMPARE(controller.page()->strokes[0].samples[0].position.x,20.);QCOMPARE(controller.page()->strokes[0].samples[0].position.y,30.);
        QVERIFY(QMetaObject::invokeMethod(panel,"close"));QTRY_VERIFY(!panel->property("visible").toBool());controller.selectPage(0);QCOMPARE(controller.pageWidth(),210.);QCOMPARE(controller.pageHeight(),297.);controller.selectPage(1);
        const auto path=directory.filePath("resized.board");controller.saveAs(QUrl::fromLocalFile(path));QTRY_VERIFY(!controller.dirty());const auto loaded=ProjectStore::load(path);QVERIFY(loaded);QCOMPARE(loaded.project.pages[1].size.widthMm,300.);QCOMPARE(loaded.project.pages[1].size.heightMm,400.);
        window->resize(520,640);QTest::qWait(100);QVERIFY(fullScreen->mapToScene({fullScreen->width(),0}).x()<=window->width());QVERIFY(QMetaObject::invokeMethod(panel,"open"));QTRY_VERIFY(panel->property("opened").toBool());QVERIFY(panel->property("y").toDouble()+panel->property("height").toDouble()<=window->height());
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/current-page-settings-small.png"));QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void panelButtonsToggleWithoutReopening(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.newDefault();QTRY_VERIFY(!controller.busy());
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);
        struct PanelCase{const char* button;const char* panel;const char* tool;};
        for(const auto& value:std::vector<PanelCase>{{"backgroundOptionsButton","backgroundOptions","pen"},{"fileMenuButton","fileOptions","pen"},{"openPagesButton","pagesPanel","pen"},{"pageCounterButton","pagesPanel","pen"},{"penToolButton","penOptions","pen"},{"markerToolButton","penOptions","marker"},{"eraserToolButton","eraserOptions","eraser"},{"shapesToolButton","shapeOptions","pen"},{"rulerToolButton","geometryOptions","ruler"},{"compassToolButton","geometryOptions","compass"}}){
            canvas->setTool(value.tool);auto* button=findVisualItem(window->contentItem(),value.button);auto* panel=window->findChild<QObject*>(value.panel);QVERIFY(button);QVERIFY(panel);
            const auto click=[&](){QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene({button->width()/2,button->height()/2}).toPoint());};
            click();QTRY_VERIFY2(panel->property("opened").toBool(),value.button);
            click();QTRY_VERIFY2(!panel->property("visible").toBool(),value.button);
            click();QTRY_VERIFY2(panel->property("opened").toBool(),value.button);
            QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY2(!panel->property("visible").toBool(),value.button);
        }
        auto* button=findVisualItem(window->contentItem(),"backgroundOptionsButton");auto* panel=window->findChild<QObject*>("backgroundOptions");
        const auto click=[&](){QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene({button->width()/2,button->height()/2}).toPoint());};
        click();QTRY_VERIFY(panel->property("opened").toBool());QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene({180,160}).toPoint());QTRY_VERIFY(!panel->property("visible").toBool());
        click();QTRY_VERIFY(panel->property("opened").toBool());click();QTRY_VERIFY(!panel->property("visible").toBool());
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void ctrlWheelZoomWithOptionsOpen(){
        QTemporaryDir dir;AppController controller(nullptr,dir.path());controller.newDefault();QTRY_VERIFY(!controller.busy());
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QVERIFY(canvas->setZoom(1));
        auto* panel=window->findChild<QObject*>("backgroundOptions");QVERIFY(panel);QVERIFY(QMetaObject::invokeMethod(panel,"open"));QTRY_VERIFY(panel->property("opened").toBool());QVERIFY(!canvas->isEnabled());
        auto* field=findVisualItem(window->contentItem(),"currentPageSizeSelector");QVERIFY(field);const auto overPopup=field->mapToScene({field->width()/2,field->height()/2});
        const auto wheel=[&](QPointF position,Qt::KeyboardModifiers modifiers,int angle=120,QPoint pixels={}){QWheelEvent event(position,window->mapToGlobal(position.toPoint()),pixels,QPoint(0,angle),Qt::NoButton,modifiers,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(window,&event);};
        wheel(overPopup,Qt::ControlModifier);QCOMPARE(canvas->zoom(),1.05);QVERIFY(panel->property("opened").toBool());
        wheel(canvas->mapToScene({200,160}),Qt::ControlModifier,-120);QCOMPARE(canvas->zoom(),1.);QVERIFY(panel->property("opened").toBool());
        wheel(overPopup,Qt::NoModifier,-120);QCOMPARE(canvas->zoom(),1.);
        wheel(overPopup,Qt::ControlModifier,0,{0,40});QCOMPARE(canvas->zoom(),1.05);
        canvas->setProperty("wheelZoomEnabled",false);wheel(overPopup,Qt::ControlModifier);QCOMPARE(canvas->zoom(),1.05);
        QVERIFY(QMetaObject::invokeMethod(panel,"close"));QTRY_VERIFY(!panel->property("visible").toBool());
        canvas->setProperty("wheelZoomEnabled",true);wheel(canvas->mapToScene({200,160}),Qt::ControlModifier,-120);QCOMPARE(canvas->zoom(),1.);
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void controlledWheelAndEditableZoom(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("hand");QVERIFY(canvas->setZoom(1));
        const QPointF pointer(canvas->width()/2+80,canvas->height()/2+35);const auto wheelPosition=canvas->mapToScene(pointer);
        const auto anchoredWorld=[&](){return canvas->viewportCenter()+QPointF(80,35)/(canvas->zoom()*96/25.4);};const auto before=anchoredWorld();
        QWheelEvent up(wheelPosition,window->mapToGlobal(wheelPosition.toPoint()),{},QPoint(0,120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(window,&up);QCOMPARE(canvas->zoom(),1.05);QVERIFY(QLineF(before,anchoredWorld()).length()<1e-8);
        QWheelEvent down(wheelPosition,window->mapToGlobal(wheelPosition.toPoint()),{},QPoint(0,-120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(window,&down);QCOMPARE(canvas->zoom(),1.);
        auto* value=findVisualItem(window->contentItem(),"zoomValueArea");QVERIFY(value);auto* input=findVisualItem(window->contentItem(),"zoomValueInput");QVERIFY(input);
        const auto edit=[&](const QString& text){QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,value->mapToScene({value->width()/2,value->height()/2}).toPoint());input->setProperty("text",text);};
        edit("125.5");QTest::keyClick(window,Qt::Key_Return);QCOMPARE(canvas->zoom(),1.255);QVERIFY(!input->isVisible());
        const auto center=canvas->viewportCenter();edit("250");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene({200,160}).toPoint());QCOMPARE(canvas->zoom(),2.5);QVERIFY(QLineF(center,canvas->viewportCenter()).length()<1e-8);
        edit("500");QTest::keyClick(window,Qt::Key_Escape);QCOMPARE(canvas->zoom(),2.5);
        edit("900");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene({200,160}).toPoint());QCOMPARE(canvas->zoom(),2.5);QVERIFY(!canvas->setZoom(0));QVERIFY(!canvas->setZoom(NAN));QVERIFY(!canvas->setZoom(9));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void fullScreenAcrossNavigation(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));window->showMaximized();QTRY_COMPARE(window->visibility(),QWindow::Maximized);
        auto click=[&](const QString& name){QTest::qWait(50);auto* button=findVisualItem(window->contentItem(),name);if(!button)return false;QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene({button->width()/2,button->height()/2}).toPoint());return true;};
        QVERIFY(click("fullScreenButton"));QTRY_COMPARE(window->visibility(),QWindow::FullScreen);
        controller.home();QTRY_VERIFY(!controller.active());auto* homeButton=findVisualItem(window->contentItem(),"homeFullScreenButton");QVERIFY(homeButton);QVERIFY(homeButton->property("selected").toBool());
        QVERIFY(click("homeFullScreenButton"));QTRY_COMPARE(window->visibility(),QWindow::Maximized);
        QVERIFY(click("homeFullScreenButton"));QTRY_COMPARE(window->visibility(),QWindow::FullScreen);QTest::keyClick(window,Qt::Key_F11);QTRY_COMPARE(window->visibility(),QWindow::Maximized);
        window->showNormal();window->resize(520,640);QTest::qWait(100);QVERIFY(homeButton->mapToScene({homeButton->width(),0}).x()<=window->width());QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void multipleImagesOnSelectedPage(){
        QTemporaryDir directory;AppController controller(nullptr,directory.filePath("data"));controller.newDefault();controller.addPage();
        QImage image(64,48,QImage::Format_RGB32);image.fill(QColor("#397ce0"));QVariantList urls;
        for(const auto& suffix:QStringList{"png","jpg","bmp"}){const auto path=directory.filePath("image."+suffix);QVERIFY(image.save(path));urls.append(QUrl::fromLocalFile(path));}
        controller.importImages(urls,{50,60});QTRY_COMPARE(controller.page()->images.size(),std::size_t(3));
        controller.undo();QVERIFY(controller.page()->images.empty());controller.redo();QCOMPARE(controller.page()->images.size(),std::size_t(3));
        controller.selectPage(0);QVERIFY(controller.page()->images.empty());controller.selectPage(1);
        auto* mime=new QMimeData;mime->setUrls({urls[0].toUrl(),urls[1].toUrl()});QGuiApplication::clipboard()->setMimeData(mime);
        controller.pasteImage({80,80});QTRY_COMPARE(controller.page()->images.size(),std::size_t(5));controller.undo();QCOMPARE(controller.page()->images.size(),std::size_t(3));
        QGuiApplication::clipboard()->setText("Texto na segunda página");controller.pasteImage({80,100});QTRY_COMPARE(controller.page()->texts.size(),std::size_t(1));controller.selectPage(0);QVERIFY(controller.page()->texts.empty());
        QGuiApplication::clipboard()->clear();QVERIFY(controller.shutdown());
    }
    void themeDefaultAndIndependentGrid(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.setTheme("Dark");controller.newDefault();QCOMPARE(controller.pageColor(),QColor("#000000"));
        auto values=controller.background();values["color"]="#214f43";values["gridColor"]="#abcdef";values["opacity"]=0.7;values["thicknessMm"]=0.4;controller.setBackground(values);
        controller.applyBackgroundPreset("Milimetrado");QCOMPARE(controller.pageColor(),QColor("#214f43"));QCOMPARE(controller.background()["gridColor"].toString(),QString("#abcdef"));QCOMPARE(controller.background()["opacity"].toDouble(),0.7);QCOMPARE(controller.background()["thicknessMm"].toDouble(),0.4);QCOMPARE(controller.page()->backgroundStyle.gridType,GridType::Millimetric);
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));auto* background=window->findChild<QObject*>("backgroundOptions");QVERIFY(background);QVERIFY(QMetaObject::invokeMethod(background,"open"));QTRY_VERIFY(background->property("opened").toBool());
        int index=-1;const auto presets=controller.gridPresets();for(int i=0;i<presets.size();++i)if(presets[i].toMap()["name"]=="Isométrico")index=i;QVERIFY(index>=0);
        QVERIFY(QMetaObject::invokeMethod(background,"chooseGrid",Q_ARG(QVariant,index)));QCOMPARE(controller.pageColor(),QColor("#214f43"));QCOMPARE(controller.background()["gridColor"].toString(),QString("#abcdef"));QCOMPARE(controller.page()->backgroundStyle.gridType,GridType::Isometric);
        controller.setTheme("Light");QCOMPARE(controller.pageColor(),QColor("#214f43"));controller.newDefault();QCOMPARE(controller.pageColor(),QColor("#ffffff"));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void pagesAndPdfImportDialog(){
        QTemporaryDir directory;const auto path=directory.filePath("lesson.pdf");
        {QPdfWriter writer(path);writer.setResolution(72);writer.setPageSize(QPageSize(QPageSize::A4));QPainter painter(&writer);painter.fillRect(QRect(50,50,120,100),Qt::blue);writer.setPageSize(QPageSize(QPageSize::Letter));QVERIFY(writer.newPage());painter.fillRect(QRect(50,50,120,100),Qt::red);}
        AppController controller(nullptr,directory.filePath("data"));QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));controller.inspectPdfFile(QUrl::fromLocalFile(path));
        auto* dialog=window->findChild<QObject*>("importPdfDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());QTRY_VERIFY(!controller.pdfBusy());QCOMPARE(controller.pdfPageCount(),2);
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/milestone4-pdf-dialog.png"));
        auto* confirm=findVisualItem(window->contentItem(),"confirmPdfImport");QVERIFY(confirm);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,confirm->mapToScene(QPointF(confirm->width()/2,confirm->height()/2)).toPoint());
        QTRY_COMPARE(controller.pageCount(),2);QTRY_VERIFY(!dialog->property("visible").toBool());
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);QTRY_VERIFY(!controller.pdfImage().isNull());
        QTRY_VERIFY(!controller.pages()[0].toMap()["thumbnail"].toString().isEmpty());QTRY_VERIFY(!controller.busy());
        controller.duplicatePage();QCOMPARE(controller.pageCount(),3);QCOMPARE(controller.currentPage(),2);controller.addPage();QCOMPARE(controller.pageCount(),4);QVERIFY(!controller.page()->pdf);
        auto* open=findVisualItem(window->contentItem(),"openPagesButton");QVERIFY(open);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,open->mapToScene(QPointF(open->width()/2,open->height()/2)).toPoint());auto* panel=window->findChild<QObject*>("pagesPanel");QVERIFY(panel);QTRY_VERIFY(panel->property("opened").toBool());QVERIFY(!canvas->isEnabled());
        QTRY_VERIFY(!controller.pages()[3].toMap()["thumbnail"].toString().isEmpty());QTRY_VERIFY(!controller.busy());QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/milestone4-pages.png"));
        auto* thumbnail=findVisualItem(window->contentItem(),"pageThumbnail_1");QVERIFY(thumbnail);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,thumbnail->mapToScene(QPointF(thumbnail->width()/2,thumbnail->height()/2)).toPoint());QCOMPARE(controller.currentPage(),1);QVERIFY(std::abs(controller.pageWidth()-215.9)<0.4);
        QVERIFY(QMetaObject::invokeMethod(panel,"close"));QTRY_VERIFY(!panel->property("visible").toBool());QTest::keyClick(window,Qt::Key_PageUp,Qt::ControlModifier);QCOMPARE(controller.currentPage(),0);QTRY_VERIFY(!controller.pdfImage().isNull());
        const auto sourceBytes=*controller.page()->pdf->data;QSignalSpy request(&controller,&AppController::pdfImportRequested);
        QMimeData mime;mime.setUrls({QUrl::fromLocalFile(path)});const auto point=canvas->mapToScene(QPointF(260,200)).toPoint();QDragEnterEvent enter(point,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&enter);QDropEvent drop(QPointF(point),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&drop);
        QTRY_COMPARE(request.size(),1);QTRY_VERIFY(!controller.pdfBusy());QTRY_VERIFY(dialog->property("opened").toBool());QVERIFY(QMetaObject::invokeMethod(dialog,"close"));QTRY_VERIFY(!dialog->property("visible").toBool());QCOMPARE(*controller.page()->pdf->data,sourceBytes);
        window->resize(520,640);QVERIFY(QMetaObject::invokeMethod(panel,"open"));QTRY_VERIFY(panel->property("opened").toBool());QTest::qWait(200);QVERIFY(window->grabWindow().save("screenshots/milestone4-pages-small.png"));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
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
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
        QTest::mouseMove(window,origin+QPoint(150,40));
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,origin+QPoint(150,40));QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Line);
        const auto fixedOrigin=controller.page()->shapes[0].vertices[0];
        controller.undo();QCOMPARE(controller.page()->strokes.size(),std::size_t(1));
        QVERIFY(length(fixedOrigin-controller.page()->strokes[0].samples.front().position)<1e-8);
        controller.undo();QVERIFY(controller.page()->shapes.empty());
        for(const auto& vertices:std::vector<std::vector<Point>>{{{30,30},{90,30}},{{90.5,30.8},{90,80}},{{90,80},{30,80}},{{30,79.5},{30.8,30.5}}}){ShapeObject line;line.id=newId();line.kind=ShapeKind::Line;line.vertices=vertices;controller.addShape(line);}
        QTRY_COMPARE(controller.page()->shapes.size(),std::size_t(1));QCOMPARE(controller.page()->shapes[0].kind,ShapeKind::Polygon);QVERIFY(controller.page()->shapes[0].fillOpacity>0);
        controller.undo();QCOMPARE(controller.page()->shapes.size(),std::size_t(4));for(const auto& shape:controller.page()->shapes)QCOMPARE(shape.kind,ShapeKind::Line);controller.redo();QCOMPARE(controller.page()->shapes.size(),std::size_t(1));QVERIFY(controller.shutdown());
    }
    void folderTrashMovesAndRestoresWholeTree(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());
        const auto folder=controller.saveFolder({},"Pasta","#268fb5");const auto child=controller.saveFolder({},"Subpasta","#3ca889",folder);
        QStringList paths;
        for(const auto& target:QStringList{folder,child}){controller.newDefault();QTRY_VERIFY(!controller.dirty());controller.home();QTRY_VERIFY(!controller.active());const auto project=controller.recentProjects().first().toMap();paths.append(project["path"].toString());QVERIFY(controller.moveProjectToFolder(project["id"].toString(),target));}
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* dialog=window->findChild<QObject*>("deleteFolderDialog");QVERIFY(dialog);QVariant entry;for(const auto& f:controller.folders())if(f.toMap()["id"]==folder)entry=f;
        QVERIFY(QMetaObject::invokeMethod(dialog,"ask",Q_ARG(QVariant,entry)));QTRY_VERIFY(dialog->property("opened").toBool());
        auto* button=findVisualItem(window->contentItem(),"confirmFolderDeletion");QVERIFY(button);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene({button->width()/2,button->height()/2}).toPoint());QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.folders().isEmpty());QVERIFY(controller.recentProjects().isEmpty());QCOMPARE(controller.trashedProjects().size(),1);QVERIFY(controller.trashedProjects().first().toMap()["isFolder"].toBool());
        for(const auto& path:paths)QVERIFY(!QFile::exists(path));
        auto* open=findVisualItem(window->contentItem(),"openTrashButton");QVERIFY(open);QVERIFY(QMetaObject::invokeMethod(open,"clicked"));QTest::qWait(100);QVERIFY(warnings.isEmpty());
        controller.restoreProject(folder);QTRY_VERIFY(!controller.busy());QCOMPARE(controller.folders().size(),2);QCOMPARE(controller.recentProjects().size(),2);QVERIFY(controller.trashedProjects().isEmpty());for(const auto& path:paths)QVERIFY(QFile::exists(path));
        QVERIFY(controller.deleteFolder(folder));QTRY_VERIFY(!controller.busy());controller.deleteProjectPermanently(folder);QTRY_VERIFY(!controller.busy());QVERIFY(controller.trashedProjects().isEmpty());QVERIFY(controller.folders().isEmpty());QVERIFY(controller.recentProjects().isEmpty());QVERIFY(warnings.isEmpty());QVERIFY(controller.shutdown());
    }
    void homeTrashConfirmationAndRestore(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.setTheme("Dark");controller.newProject("Quadro na lixeira","A4",210,297,false,"Branco");QTRY_VERIFY(!controller.dirty());controller.home();
        QTRY_COMPARE(controller.recentProjects().size(),1);const auto project=controller.recentProjects()[0].toMap();const auto id=project["id"].toString();const auto path=project["path"].toString();
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        QDir().mkpath("screenshots");QTest::qWait(100);QVERIFY(window->grabWindow().save("screenshots/home-modern-dark.png"));
        window->resize(1920,1080);QTest::qWait(100);QVERIFY(window->grabWindow().save("screenshots/home-modern-wide.png"));
        window->resize(520,640);QTest::qWait(100);QVERIFY(window->grabWindow().save("screenshots/home-modern-small.png"));window->resize(1200,800);QTest::qWait(100);
        auto* edit=findVisualItem(window->contentItem(),"editProjectButton_"+id);QVERIFY(edit);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,edit->mapToScene({edit->width()/2,edit->height()/2}).toPoint());auto* editDialog=window->findChild<QObject*>("editProjectDialog");QVERIFY(editDialog);QTRY_VERIFY(editDialog->property("opened").toBool());auto* remove=findVisualItem(window->contentItem(),"trashProjectButton");QVERIFY(remove);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,remove->mapToScene(QPointF(remove->width()/2,remove->height()/2)).toPoint());
        auto* dialog=window->findChild<QObject*>("deleteProjectDialog");QVERIFY(dialog);QTRY_VERIFY(dialog->property("opened").toBool());QVERIFY(QFile::exists(path));QVERIFY(controller.trashedProjects().isEmpty());
        auto* content=dialog->property("contentItem").value<QQuickItem*>();QVERIFY(content);
        for(auto* item:content->findChildren<QQuickItem*>())QVERIFY(!item->property("text").toString().contains(path));
        auto* cancel=findVisualItem(window->contentItem(),"cancelProjectDeletion");QVERIFY(cancel);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,cancel->mapToScene(QPointF(cancel->width()/2,cancel->height()/2)).toPoint());QTRY_VERIFY(!dialog->property("visible").toBool());QCOMPARE(controller.recentProjects().size(),1);
        edit=findVisualItem(window->contentItem(),"editProjectButton_"+id);QVERIFY(edit);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,edit->mapToScene({edit->width()/2,edit->height()/2}).toPoint());QTRY_VERIFY(editDialog->property("opened").toBool());remove=findVisualItem(window->contentItem(),"trashProjectButton");QVERIFY(remove);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,remove->mapToScene(QPointF(remove->width()/2,remove->height()/2)).toPoint());QTRY_VERIFY(dialog->property("opened").toBool());
        auto* confirm=findVisualItem(window->contentItem(),"confirmProjectDeletion");QVERIFY(confirm);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,confirm->mapToScene(QPointF(confirm->width()/2,confirm->height()/2)).toPoint());
        QTRY_VERIFY(!controller.busy());QCOMPARE(controller.trashedProjects().size(),1);QVERIFY(controller.recentProjects().isEmpty());QVERIFY(!QFile::exists(path));QTRY_VERIFY(!dialog->property("visible").toBool());
        auto* open=findVisualItem(window->contentItem(),"openTrashButton");QVERIFY(open);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,open->mapToScene(QPointF(open->width()/2,open->height()/2)).toPoint());auto* trash=window->findChild<QObject*>("trashDialog");QVERIFY(trash);QTRY_VERIFY(trash->property("opened").toBool());QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/trash-dark.png"));
        controller.restoreProject(id);QTRY_VERIFY(!controller.busy());QVERIFY(controller.trashedProjects().isEmpty());QCOMPARE(controller.recentProjects().size(),1);QVERIFY(QFile::exists(path));
        controller.trashProject(id);QTRY_VERIFY(!controller.busy());controller.deleteProjectPermanently(id);QTRY_VERIFY(!controller.busy());QVERIFY(controller.trashedProjects().isEmpty());QVERIFY(controller.recentProjects().isEmpty());QVERIFY(!QFile::exists(path));
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void drawnInlineTextAndFormatting(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setTool("text");
        QSignalSpy boxes(canvas,&CanvasItem::textBoxRequested);
        const auto start=canvas->mapToScene(QPointF(400,220)).toPoint(),end=start+QPoint(260,120);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(window,end);
        QVERIFY(canvas->selectionRect().width()>200);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,end);QCOMPARE(boxes.size(),1);
        const auto box=boxes[0][0].toRectF();
        auto* inlineEditor=window->findChild<QObject*>("inlineTextEditor");QVERIFY(inlineEditor);QTRY_VERIFY(inlineEditor->property("active").toBool());
        auto* dialog=window->findChild<QObject*>("textDialog");QVERIFY(dialog);QVERIFY(!dialog->property("visible").toBool());
        auto* source=findVisualItem(window->contentItem(),"textSource");QVERIFY(source);QTRY_VERIFY(source->hasActiveFocus());
        for(const char c:std::string("Aula de geometria"))QTest::keyClick(window,c);
        QVERIFY(QMetaObject::invokeMethod(source,"select",Q_ARG(int,0),Q_ARG(int,4)));
        auto* bold=findVisualItem(window->contentItem(),"inlineBoldButton");QVERIFY(bold);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,bold->mapToScene({bold->width()/2,bold->height()/2}).toPoint());
        auto* textDocument=source->property("textDocument").value<QObject*>();QVERIFY(textDocument);
        QVERIFY(controller.textEmphasis(textDocument,0,4)["bold"].toBool());QVERIFY(!controller.textEmphasis(textDocument,5,7)["bold"].toBool());
        QVERIFY(QMetaObject::invokeMethod(source,"select",Q_ARG(int,8),Q_ARG(int,16)));
        QTest::keyClick(window,Qt::Key_I,Qt::ControlModifier);
        QVERIFY(controller.textEmphasis(textDocument,8,16)["italic"].toBool());QVERIFY(!controller.textEmphasis(textDocument,0,4)["italic"].toBool());
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/text-inline-cursive.png"));
        auto* format=findVisualItem(window->contentItem(),"textFormatButton");QVERIFY(format);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,format->mapToScene({format->width()/2,format->height()/2}).toPoint());QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(window->grabWindow().save("screenshots/text-formatting.png"));
        QVERIFY(!controller.fontFamilies().empty());QVERIFY(controller.fontFamilies().contains(controller.defaultTextFont()));
        auto* fontSelector=findVisualItem(window->contentItem(),"textFontSelector");QVERIFY(fontSelector);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,fontSelector->mapToScene({fontSelector->width()/2,fontSelector->height()/2}).toPoint());
        auto* fontPopup=window->findChild<QObject*>("fontPickerPopup");QVERIFY(fontPopup);QTRY_VERIFY(fontPopup->property("opened").toBool());
        auto* fontList=findVisualItem(window->contentItem(),"fontList");QVERIFY(fontList);QVERIFY(fontList->property("count").toInt()>6);
        QTest::qWait(100);
        auto* firstFont=findVisualItem(window->contentItem(),"fontOption_0");QVERIFY(firstFont);
        auto* firstFontBackground=findVisualItem(window->contentItem(),"fontOptionBackground_0");QVERIFY(firstFontBackground);
        const auto idleFontColor=firstFontBackground->property("color").value<QColor>();
        QTest::mouseMove(window,firstFont->mapToScene({firstFont->width()/2,firstFont->height()/2}).toPoint());
        QTRY_VERIFY(firstFont->property("hovered").toBool());
        QVERIFY(firstFontBackground->property("color").value<QColor>()!=idleFontColor);
        QVERIFY(firstFont->clip());
        auto* firstPreview=findVisualItem(window->contentItem(),"fontPreview_0");QVERIFY(firstPreview);QVERIFY(firstPreview->clip());
        QVERIFY(firstPreview->height()<firstFont->height());
        QVERIFY(window->grabWindow().save("screenshots/text-font-picker.png"));
        const auto wheelPosition=fontList->mapToScene({fontList->width()/2,fontList->height()/2});
        QTest::mouseMove(window,wheelPosition.toPoint());
        QTest::qWait(50);
        QWheelEvent wheel(wheelPosition,window->mapToGlobal(wheelPosition.toPoint()),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(window,&wheel);
        QTRY_VERIFY2(fontList->property("contentY").toDouble()>0,qPrintable(QString("wheel %1,%2; view %3x%4; content %5; accepted %6").arg(wheelPosition.x()).arg(wheelPosition.y()).arg(fontList->width()).arg(fontList->height()).arg(fontList->property("contentHeight").toDouble()).arg(wheel.isAccepted())));
        const double wheelEnd=fontList->property("contentY").toDouble();
        QVERIFY(!fontList->property("flicking").toBool());
        QTest::qWait(350);
        QCOMPARE(fontList->property("contentY").toDouble(),wheelEnd);
        QWheelEvent preciseWheel(wheelPosition,window->mapToGlobal(wheelPosition.toPoint()),QPoint(0,-17),{},Qt::NoButton,Qt::NoModifier,Qt::ScrollUpdate,false);
        QCoreApplication::sendEvent(window,&preciseWheel);
        QCOMPARE(fontList->property("contentY").toDouble(),wheelEnd+17.);
        auto* fontScrollBar=findVisualItem(window->contentItem(),"fontScrollBar");QVERIFY(fontScrollBar);
        fontList->setProperty("contentY",0.);
        QTest::qWait(100);
        const auto thumbStart=fontScrollBar->mapToScene({fontScrollBar->width()/2,5}).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,thumbStart);
        for(int step=1;step<=20;++step){
            QTest::mouseMove(window,thumbStart+QPoint(0,int((fontScrollBar->height()-15)*step/20)),10);
            QCoreApplication::processEvents();
        }
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,thumbStart+QPoint(0,int(fontScrollBar->height()-15)));
        QVERIFY(fontList->property("contentY").toDouble()>fontList->property("contentHeight").toDouble()/2);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,QPoint(600,170));
        QTRY_VERIFY(!fontPopup->property("visible").toBool());QVERIFY(dialog->property("visible").toBool());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,fontSelector->mapToScene({fontSelector->width()/2,fontSelector->height()/2}).toPoint());
        QTRY_VERIFY(fontPopup->property("opened").toBool());
        QVERIFY(window->grabWindow().save("screenshots/text-font-picker.png"));
        auto* fontSearch=findVisualItem(window->contentItem(),"fontSearchField");QVERIFY(fontSearch);fontSearch->setProperty("text","DejaVu Sans");
        QTRY_VERIFY(fontList->property("count").toInt()>0);QTRY_VERIFY(fontSearch->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!fontPopup->property("visible").toBool());
        QVERIFY(dialog->property("draft").toMap()["fontFamily"].toString().contains("DejaVu Sans"));
        auto draft=dialog->property("draft").toMap();draft["fontFamily"]="DejaVu Sans";draft["fontSizePt"]=20;draft["color"]="#397ce0";dialog->setProperty("draft",draft);
        auto* apply=findVisualItem(window->contentItem(),"applyTextFormatButton");QVERIFY(apply);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,apply->mapToScene({apply->width()/2,apply->height()/2}).toPoint());QTRY_VERIFY(!dialog->property("visible").toBool());QTRY_VERIFY(source->hasActiveFocus());
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/text-inline-editing.png"));
        QTest::keyClick(window,Qt::Key_Return,Qt::ControlModifier);QTRY_VERIFY(!inlineEditor->property("active").toBool());QCOMPARE(controller.page()->texts.size(),std::size_t(1));
        const auto text=controller.page()->texts[0];QCOMPARE(text.source,std::string("Aula de geometria"));QCOMPARE(text.fontFamily,std::string("DejaVu Sans"));QCOMPARE(text.fontSizePt,20.);QCOMPARE(text.boxWidthMm,box.width());
        QCOMPARE(text.formats.size(),std::size_t(2));QCOMPARE(text.formats[0],(TextFormat{0,4,true,false}));QCOMPARE(text.formats[1],(TextFormat{8,8,false,true}));
        const auto formatted=ProjectStore::deserialize(ProjectStore::serialize(Project{newId(),"Formatted text","now","now",{*controller.page()}}));QVERIFY(formatted);QCOMPARE(formatted.project.pages[0].texts[0].formats,text.formats);
        QVERIFY(length(text.corners[0]-Point{box.x(),box.y()})<1e-8);QCOMPARE(canvas->selectedCount(),1);
        canvas->editSelectedText();QTRY_VERIFY(inlineEditor->property("active").toBool());source->setProperty("text","Texto revisado");
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(QPointF(200,150)).toPoint());QTRY_VERIFY(!inlineEditor->property("active").toBool());QCOMPARE(controller.page()->texts[0].source,std::string("Texto revisado"));
        controller.undo();QCOMPARE(controller.page()->texts[0].source,text.source);controller.redo();
        controller.save();QTRY_VERIFY(!controller.dirty());const auto loaded=ProjectStore::load(controller.recentProjects()[0].toMap()["path"].toString());QVERIFY(loaded);QCOMPARE(loaded.project.pages[0].texts[0].boxWidthMm,box.width());
        canvas->editSelectedText();QTRY_VERIFY(inlineEditor->property("active").toBool());source->setProperty("text","Texto salvo ao fechar");
        window->close();QTRY_VERIFY(!window->isVisible());
        const auto closed=ProjectStore::load(controller.recentProjects()[0].toMap()["path"].toString());QVERIFY(closed);QCOMPARE(closed.project.pages[0].texts[0].source,std::string("Texto salvo ao fechar"));
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
        QCOMPARE(canvas->penLineStyle(),QString("dotted"));canvas->setTool("text");QSignalSpy request(canvas,&CanvasItem::textBoxRequested);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(QPointF(400,220)).toPoint());QTRY_COMPARE(request.size(),1);
        auto* dialog=window->findChild<QObject*>("inlineTextEditor");QVERIFY(dialog);QTRY_VERIFY(dialog->property("active").toBool());
        auto* editor=findVisualItem(window->contentItem(),"textSource");QVERIFY(editor);editor->setProperty("text","Aula de geometria");
        QDir().mkpath("screenshots");QVERIFY(window->grabWindow().save("screenshots/milestone3-inline-text.png"));QVERIFY(QMetaObject::invokeMethod(dialog,"cancel"));
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
    void toolbarBlurUpdatesWithoutSettlingDelay(){
        QTemporaryDir directory;AppController controller(nullptr,directory.path());controller.newDefault();
        QQmlApplicationEngine engine;QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("App",&controller);engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        if(window->rendererInterface()->graphicsApi()==QSGRendererInterface::Software)QSKIP("Backdrop blur needs the GPU renderer.");
        auto* toolbar=findVisualItem(window->contentItem(),"floatingToolbar");QVERIFY(toolbar);
        auto* canvas=window->findChild<CanvasItem*>("boardCanvas");QVERIFY(canvas);canvas->setZoom(2);
        auto background=controller.background();background["gridType"]=int(GridType::None);background["color"]="#000000";controller.setBackground(background);
        const QPoint sample=toolbar->mapToScene({toolbar->width()/2,8}).toPoint();
        const auto before=window->grabWindow();QVERIFY(!before.isNull());
        // Grab the next rendered frame immediately: a debounce would retain black.
        background["color"]="#ffffff";controller.setBackground(background);
        const auto after=window->grabWindow();QVERIFY(!after.isNull());
        QVERIFY(after.pixelColor(sample).lightness()>before.pixelColor(sample).lightness()+30);
        auto* settings=window->findChild<QObject*>("settingsDialog");QVERIFY(settings);
        QVERIFY(QMetaObject::invokeMethod(settings,"open"));
        auto* toggle=findVisualItem(window->contentItem(),"disableToolbarBlurSwitch");QVERIFY(toggle);
        toggle->setProperty("checked",true);QVERIFY(QMetaObject::invokeMethod(toggle,"toggled"));
        QVERIFY(controller.disableToolbarBlur());QVERIFY(toolbar->property("plainAppearance").toBool());
        QCOMPARE(toolbar->property("color").value<QColor>().alpha(),255);
        toggle->setProperty("checked",false);QVERIFY(QMetaObject::invokeMethod(toggle,"toggled"));
        QVERIFY(!controller.disableToolbarBlur());QVERIFY(!toolbar->property("plainAppearance").toBool());
        QCOMPARE(warnings.count(),0);QVERIFY(controller.shutdown());
    }
    void toolbarBlurPreferencePersists(){
        QTemporaryDir directory;
        { AppController controller(nullptr,directory.path());QVERIFY(!controller.disableToolbarBlur());
          controller.setDisableToolbarBlur(true);QVERIFY(!controller.reducedEffects());QVERIFY(controller.shutdown()); }
        { AppController controller(nullptr,directory.path());QVERIFY(controller.disableToolbarBlur());
          controller.setDisableToolbarBlur(false);QVERIFY(controller.shutdown()); }
        { AppController controller(nullptr,directory.path());QVERIFY(!controller.disableToolbarBlur());QVERIFY(controller.shutdown()); }
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
            QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
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
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
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
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
        QTest::keyRelease(window,Qt::Key_Shift);
        QVERIFY(!canvas->segmentGuide().isEmpty());
        const auto endpoint=start+QPoint(120,-47);QTest::mouseMove(window,endpoint);const auto freeAngle=canvas->segmentGuide()["angle"].toDouble();QVERIFY(freeAngle>15&&freeAngle<30);
        QTest::keyPress(window,Qt::Key_Control);QCOMPARE(canvas->segmentGuide()["angle"].toDouble(),15.);
        QTest::keyRelease(window,Qt::Key_Control);QVERIFY(std::abs(canvas->segmentGuide()["angle"].toDouble()-freeAngle)<1e-8);
        QTest::keyPress(window,Qt::Key_Control);QTest::mouseRelease(window,Qt::LeftButton,Qt::ControlModifier,endpoint);QTest::keyRelease(window,Qt::Key_Control);QVERIFY(canvas->segmentGuide().isEmpty());
        const auto delta=controller.page()->shapes[0].vertices[1]-controller.page()->shapes[0].vertices[0];QVERIFY(std::abs(std::atan2(-delta.y,delta.x)*180/std::numbers::pi-15)<1e-8);
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
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
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
        QTRY_VERIFY_WITH_TIMEOUT(canvas->interactionHint().contains("reconhecid"),2000);
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
        canvas->setSelectedFill(0.25);QCOMPARE(controller.page()->shapes[0].fillOpacity,0.25);
        const auto layerBefore=objects(*controller.page());const auto selectedId=controller.page()->shapes[0].id;
        QCOMPARE(objectId(layerBefore.back()),selectedId);
        canvas->forceActiveFocus();QTest::keyClick(window,Qt::Key_Down,Qt::ControlModifier);
        auto layerAfter=objects(*controller.page());QCOMPARE(objectId(layerAfter[layerAfter.size()-2]),selectedId);QCOMPARE(canvas->selectedCount(),1);
        controller.undo();QCOMPARE(objectId(objects(*controller.page()).back()),selectedId);
        controller.redo();QCOMPARE(objectId(objects(*controller.page())[layerAfter.size()-2]),selectedId);
        QTest::keyClick(window,Qt::Key_Up,Qt::ControlModifier);QCOMPARE(objectId(objects(*controller.page()).back()),selectedId);
        auto* duplicateButton=window->findChild<QQuickItem*>("duplicateSelectionButton");QVERIFY(duplicateButton);QCOMPARE(duplicateButton->property("text").toString(),QString("Duplicar"));
        auto* backwardButton=window->findChild<QQuickItem*>("moveLayerBackwardButton");QVERIFY(backwardButton);
        QCOMPARE(backwardButton->property("tooltip").toString(),QString("Mover para trás (Ctrl + seta para baixo)"));
        auto* forwardButton=window->findChild<QQuickItem*>("moveLayerForwardButton");QVERIFY(forwardButton);
        QCOMPARE(forwardButton->property("tooltip").toString(),QString("Mover para frente (Ctrl + seta para cima)"));
        canvas->setTool("pen");QCOMPARE(canvas->cursor().shape(),Qt::CrossCursor);
        canvas->setTool("select");QCOMPARE(canvas->cursor().shape(),Qt::ArrowCursor);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,backwardButton->mapToScene({backwardButton->width()/2,backwardButton->height()/2}).toPoint());
        QCOMPARE(objectId(objects(*controller.page())[layerAfter.size()-2]),selectedId);
        canvas->duplicateSelection();QCOMPARE(controller.page()->shapes.size(),std::size_t(2));canvas->deleteSelection();QCOMPARE(controller.page()->shapes.size(),std::size_t(1));
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
    qmlRegisterType<scalar::ApplicationLogo>("Scalar",1,0,"ApplicationLogo");
    qmlRegisterUncreatableType<AppController>("Scalar",1,0,"ApplicationController","Use App");
    DesktopTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "desktop_tests.moc"
