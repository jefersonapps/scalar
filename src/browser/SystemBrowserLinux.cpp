#include "SystemBrowser.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>

namespace scalar {
struct SystemBrowser::Native {
    QProcess process;
    QByteArray incoming;
    std::unique_ptr<QWindow> child;
    QPointer<QQuickWindow> window;
    QTimer timeout;
    QSize size;
    void command(const QJsonObject& object){if(process.state()==QProcess::Running)process.write(QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n');}
    ~Native(){child.reset();process.closeWriteChannel();if(!process.waitForFinished(1000)){process.kill();process.waitForFinished(1000);}}
};
SystemBrowser::SystemBrowser(QQuickItem* parent):QQuickItem(parent),native_(std::make_unique<Native>()) {
    native_->timeout.setSingleShot(true);native_->timeout.setInterval(30000);
    connect(&native_->timeout,&QTimer::timeout,this,[this]{native_->command({{"op","stop"}});fail("O site demorou para responder. Tente recarregar a página.");});
    connect(&native_->process,&QProcess::readyReadStandardOutput,this,[this]{
        native_->incoming+=native_->process.readAllStandardOutput();
        while(native_->incoming.contains('\n')){
            const auto end=native_->incoming.indexOf('\n');
            const auto message=QJsonDocument::fromJson(native_->incoming.left(end)).object();native_->incoming.remove(0,end+1);
            const auto event=message["event"].toString();
            if(event=="ready"){
                native_->child.reset(QWindow::fromWinId(message["window"].toString().toULongLong()));
                if(!native_->child){fail("Não foi possível integrar a janela do navegador.");continue;}
                native_->child->setParent(window());ready_=true;syncGeometry();
                native_->command({{"op","active"},{"active",window()&&window()->isActive()}});
                // GtkPlug needs the real XEmbed parent before loading a page.
                navigate(url_.toString());
            }else if(event=="state"){
                const auto uri=message["url"].toString();if(!uri.isEmpty())url_=QUrl(uri);
                title_=message["title"].toString();back_=message["back"].toBool();forward_=message["forward"].toBool();
                loading_=message["loading"].toBool();if(ready_&&!loading_)native_->timeout.stop();
            }else if(event=="started"){error_.clear();loading_=true;native_->timeout.start();}
            else if(event=="error"){native_->timeout.stop();fail(message["message"].toString());}
            else if(event=="focus")focused_=message["focused"].toBool();
            else if(event=="address"){if(window())window()->requestActivate();emit focusAddressRequested();}
            emit changed();
        }
    });
    connect(&native_->process,&QProcess::readyReadStandardError,this,[this]{native_->process.readAllStandardError();});
    connect(&native_->process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){native_->timeout.stop();ready_=false;fail("Não foi possível iniciar o navegador do sistema. Verifique a instalação do Scalar e do WebKitGTK.");});
    connect(&native_->process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{native_->timeout.stop();native_->child.reset();ready_=false;focused_=false;fail("O navegador foi encerrado. Tente novamente.");});
    connect(this,&QQuickItem::windowChanged,this,[this](QQuickWindow* next){
        if(native_->window){native_->window->removeEventFilter(this);disconnect(native_->window,nullptr,this,nullptr);}
        native_->window=next;
        if(next){
            next->installEventFilter(this);connect(next,&QQuickWindow::afterAnimating,this,&SystemBrowser::syncGeometry);
            connect(next,&QWindow::activeChanged,this,[this]{
                native_->command({{"op","active"},{"active",native_->window&&(native_->window->isActive()||(native_->child&&native_->child->isActive()))}});
            });
            connect(next,&QQuickWindow::activeFocusItemChanged,this,[this]{
                if(focused_&&native_->window){
                    native_->command({{"op","focus"},{"focused",false}});
                    native_->window->requestActivate();focused_=false;emit changed();
                }
            });
        }
        if(native_->child)native_->child->setParent(next);
        QTimer::singleShot(0,this,&SystemBrowser::initialize);
    });
    connect(this,&QQuickItem::visibleChanged,this,&SystemBrowser::syncGeometry);
    connect(this,&QQuickItem::enabledChanged,this,&SystemBrowser::syncGeometry);
}
SystemBrowser::~SystemBrowser(){
    disconnect(this,nullptr,this,nullptr);disconnect(&native_->process,nullptr,this,nullptr);
    if(native_->window){native_->window->removeEventFilter(this);disconnect(native_->window,nullptr,this,nullptr);}
}
bool SystemBrowser::eventFilter(QObject* object,QEvent* event){
    if(object==native_->window&&focused_&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::TabletPress||event->type()==QEvent::TouchBegin)){
        native_->command({{"op","focus"},{"focused",false}});
        native_->window->requestActivate();focused_=false;emit changed();
    }
    return QQuickItem::eventFilter(object,event);
}
QUrl SystemBrowser::addressUrl(const QString& address){
    const auto text=address.trimmed();if(text.isEmpty())return {};
    if(!text.contains(' ')&&(text.contains('.')||text.startsWith("localhost")||text.contains("://"))){
        const auto url=QUrl::fromUserInput(text);if(url.isValid()&&(url.scheme()=="https"||url.scheme()=="http"))return url;
    }
    return QUrl("https://www.google.com/search?q="+QString::fromLatin1(QUrl::toPercentEncoding(text)));
}
void SystemBrowser::fail(const QString& message){error_=message;loading_=false;emit changed();}
void SystemBrowser::initialize(){
    if(!window()||!isVisible()||native_->process.state()!=QProcess::NotRunning)return;
    if(QGuiApplication::platformName()!="xcb"){fail("O navegador integrado requer X11/XWayland. Inicie o Scalar com QT_QPA_PLATFORM=xcb.");return;}
    auto host=QCoreApplication::applicationDirPath()+"/scalar-webkit-host";
    if(!QFileInfo::exists(host))host=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../libexec/scalar/scalar-webkit-host");
    const auto profile=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/web-profile";QDir().mkpath(profile);
    native_->incoming.clear();loading_=true;native_->timeout.start();native_->process.start(host,{profile});emit changed();
}
void SystemBrowser::syncGeometry(){
    if(!native_->child){if(isVisible()&&error_.isEmpty())initialize();return;}
    const auto origin=mapToScene(QPointF(0,0));
    const QRect bounds(qRound(origin.x()),qRound(origin.y()),qMax(1,qRound(width())),qMax(1,qRound(height())));
    if(native_->child->geometry()!=bounds)native_->child->setGeometry(bounds);
    if(native_->size!=bounds.size()){
        native_->size=bounds.size();native_->command({{"op","size"},{"width",bounds.width()},{"height",bounds.height()}});
    }
    const bool shown=isVisible()&&isEnabled()&&!suspended_&&window()&&window()->isVisible()&&width()>0&&height()>0;
    const bool wasVisible=native_->child->isVisible();native_->child->setVisible(shown);
    if(wasVisible&&!shown&&window()){
        native_->command({{"op","focus"},{"focused",false}});
        window()->requestActivate();focused_=false;emit changed();
    }
}
void SystemBrowser::geometryChange(const QRectF& next,const QRectF& previous){QQuickItem::geometryChange(next,previous);syncGeometry();}
void SystemBrowser::itemChange(ItemChange change,const ItemChangeData& data){QQuickItem::itemChange(change,data);if(change==ItemVisibleHasChanged||change==ItemEnabledHasChanged||change==ItemDevicePixelRatioHasChanged)syncGeometry();}
void SystemBrowser::setSuspended(bool value){if(value==suspended_)return;suspended_=value;syncGeometry();emit suspendedChanged();}
void SystemBrowser::updateState(){emit changed();}
void SystemBrowser::navigate(const QString& address){
    const auto target=addressUrl(address);if(target.isEmpty())return;url_=target;error_.clear();loading_=true;
    if(ready_){native_->timeout.start();native_->command({{"op","navigate"},{"url",url_.toString(QUrl::FullyEncoded)}});}else initialize();emit changed();
}
void SystemBrowser::goBack(){native_->command({{"op","back"}});}
void SystemBrowser::goForward(){native_->command({{"op","forward"}});}
void SystemBrowser::reload(){error_.clear();if(ready_){native_->timeout.start();native_->command({{"op","reload"}});}else initialize();emit changed();}
void SystemBrowser::stop(){native_->timeout.stop();native_->command({{"op","stop"}});loading_=false;emit changed();}
void SystemBrowser::openExternal(){QDesktopServices::openUrl(url_);}
}
