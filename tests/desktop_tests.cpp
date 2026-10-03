#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QImage>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QDir>
#include "app/AppController.h"
#include "canvas/CanvasItem.h"
using namespace scalar;
class DesktopTests : public QObject {
    Q_OBJECT
private slots:
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
};
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);app.setOrganizationName("ScalarTests");app.setApplicationName("Desktop");
    QQuickStyle::setStyle("Basic");
    qmlRegisterType<CanvasItem>("Scalar",1,0,"BoardCanvas");
    qmlRegisterUncreatableType<AppController>("Scalar",1,0,"ApplicationController","Use App");
    DesktopTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "desktop_tests.moc"
