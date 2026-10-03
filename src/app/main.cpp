#include "AppController.h"
#include "canvas/CanvasItem.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSurfaceFormat>
#include <QTimer>
#include <QQuickWindow>
#include <QCommandLineParser>
#include <QDir>
#include <QImage>
#include <QLoggingCategory>
int main(int argc,char** argv){
    QSurfaceFormat format;format.setSamples(4);QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc,argv);app.setOrganizationName("Scalar");app.setApplicationName("Scalar");app.setApplicationVersion("0.3.0");
    QQuickStyle::setStyle("Basic");
    QCommandLineParser parser;parser.addHelpOption();parser.addVersionOption();
    parser.addOption({"smoke-test","Exit after a short UI startup check."});
    parser.addOption({"screenshot","Save a startup screenshot to a PNG path.","path"});
    parser.addOption({"editor","Start with a new default project."});parser.process(app);
    qmlRegisterType<scalar::CanvasItem>("Scalar",1,0,"BoardCanvas");
    qmlRegisterUncreatableType<scalar::AppController>("Scalar",1,0,"ApplicationController","Use the App singleton context.");
    scalar::AppController controller;QQmlApplicationEngine engine;
    bool qmlWarnings=false;
    QObject::connect(&engine,&QQmlEngine::warnings,&app,[&](const QList<QQmlError>&){qmlWarnings=true;});engine.rootContext()->setContextProperty("App",&controller);
    engine.load(QUrl("qrc:/qml/Main.qml"));if(engine.rootObjects().isEmpty())return 1;
    if(parser.isSet("editor"))controller.newDefault();
    if(parser.isSet("screenshot"))QTimer::singleShot(1200,&app,[&]{auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!window||!window->grabWindow().save(parser.value("screenshot")))app.exit(2);});
    if(parser.isSet("smoke-test"))QTimer::singleShot(1800,&app,[&]{if(controller.shutdown())app.exit(qmlWarnings?4:0);else app.exit(3);});
    return app.exec();
}
