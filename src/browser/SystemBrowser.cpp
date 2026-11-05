#include "SystemBrowser.h"
#include <QDesktopServices>
#include <QDir>
#include <QPointer>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QGuiApplication>
#include <cmath>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <objbase.h>
#include <wrl.h>
#include <WebView2.h>
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
#endif

namespace scalar {
struct SystemBrowser::Native {
#ifdef Q_OS_WIN
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> view;
    bool ole=false,starting=false;
    QRect bounds;
    bool visible=false;
    ~Native(){if(controller)controller->Close();view.Reset();controller.Reset();if(ole)OleUninitialize();}
#endif
    QPointer<QQuickWindow> window;
};
SystemBrowser::SystemBrowser(QQuickItem* parent):QQuickItem(parent),native_(std::make_unique<Native>()){
    const auto attachWindow=[this](QQuickWindow* next){
        if(native_->window==next)return;
        if(native_->window){native_->window->removeEventFilter(this);disconnect(native_->window,nullptr,this,nullptr);}
#ifdef Q_OS_WIN
        if(native_->controller)native_->controller->Close();
        native_->view.Reset();native_->controller.Reset();native_->starting=false;native_->bounds={};native_->visible=false;
#endif
        native_->window=next;ready_=false;
        if(next){
            next->installEventFilter(this);
            connect(next,&QWindow::widthChanged,this,&SystemBrowser::syncGeometry);
            connect(next,&QWindow::heightChanged,this,&SystemBrowser::syncGeometry);
            connect(next,&QWindow::visibleChanged,this,&SystemBrowser::syncGeometry);
            connect(next,&QWindow::screenChanged,this,&SystemBrowser::syncGeometry);
            connect(next,&QQuickWindow::afterAnimating,this,&SystemBrowser::syncGeometry);
#ifdef Q_OS_WIN
            const auto moved=[this]{if(native_->controller)native_->controller->NotifyParentWindowPositionChanged();};
            connect(next,&QWindow::xChanged,this,moved);
            connect(next,&QWindow::yChanged,this,moved);
#endif
            // Native child views must release focus when the board/address bar
            // becomes active again. Their keyboard input never enters Qt shortcuts.
            connect(next,&QQuickWindow::activeFocusItemChanged,this,[this]{
#ifdef Q_OS_WIN
                if(focused_&&native_->window){SetFocus(reinterpret_cast<HWND>(native_->window->winId()));focused_=false;emit changed();}
#endif
            });
        }
        QTimer::singleShot(0,this,&SystemBrowser::initialize);
    };
    connect(this,&QQuickItem::windowChanged,this,attachWindow);
    if(window())attachWindow(window());
    connect(this,&QQuickItem::visibleChanged,this,&SystemBrowser::syncGeometry);
    connect(this,&QQuickItem::enabledChanged,this,&SystemBrowser::syncGeometry);
}
SystemBrowser::~SystemBrowser(){
    // QQuickItem can emit windowChanged while its base destructor detaches it.
    // Disconnect before Native is destroyed, so that slot cannot access freed state.
    disconnect(this,nullptr,this,nullptr);
    if(native_->window){native_->window->removeEventFilter(this);disconnect(native_->window,nullptr,this,nullptr);}
}
bool SystemBrowser::eventFilter(QObject* object,QEvent* event){
#ifdef Q_OS_WIN
    if(object==native_->window&&focused_&&(event->type()==QEvent::MouseButtonPress||event->type()==QEvent::TabletPress||event->type()==QEvent::TouchBegin)){
        SetFocus(reinterpret_cast<HWND>(native_->window->winId()));focused_=false;emit changed();
    }
#endif
    return QQuickItem::eventFilter(object,event);
}
void SystemBrowser::fail(const QString& message){error_=message;loading_=false;emit changed();}
QUrl SystemBrowser::addressUrl(const QString& address){
    const auto text=address.trimmed();if(text.isEmpty())return {};
    // Text without a host is a search. Do not let typed javascript/file URLs
    // execute in a page or expose local files to it.
    if(!text.contains(' ')&&(text.contains('.')||text.startsWith("localhost")||text.contains("://"))){
        const auto url=QUrl::fromUserInput(text);
        if(url.isValid()&&(url.scheme()=="https"||url.scheme()=="http"))return url;
    }
    return QUrl("https://www.google.com/search?q="+QString::fromLatin1(QUrl::toPercentEncoding(text)));
}
void SystemBrowser::navigate(const QString& address){
    const auto target=addressUrl(address);if(target.isEmpty())return;
    url_=target;error_.clear();
#ifdef Q_OS_WIN
    if(native_->view){loading_=true;const auto value=url_.toString(QUrl::FullyEncoded).toStdWString();if(FAILED(native_->view->Navigate(value.c_str())))fail("Não foi possível abrir esse endereço.");}
    else initialize();
#else
    initialize();
#endif
    emit changed();
}
void SystemBrowser::initialize(){
    if(!window()||!isVisible())return;
#ifdef Q_OS_WIN
    if(native_->controller||native_->starting)return;
    if(QGuiApplication::platformName()!=QStringLiteral("windows")){fail("A visualização web requer uma sessão gráfica do Windows.");return;}
    if(!native_->ole){const auto hr=OleInitialize(nullptr);if(FAILED(hr)){fail("Não foi possível iniciar o navegador do sistema.");return;}native_->ole=true;}
    native_->starting=true;loading_=true;emit changed();
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/web-profile";
    QDir().mkpath(directory);const auto profile=QDir::toNativeSeparators(directory).toStdWString();
    QPointer<SystemBrowser> guard(this);const auto host=reinterpret_cast<HWND>(window()->winId());
    const auto result=CreateCoreWebView2EnvironmentWithOptions(nullptr,profile.c_str(),nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([guard,host](HRESULT result,ICoreWebView2Environment* environment)->HRESULT{
            if(!guard||!guard->window()||reinterpret_cast<HWND>(guard->window()->winId())!=host)return S_OK;
            if(FAILED(result)||!environment){guard->native_->starting=false;guard->fail("O Microsoft Edge WebView2 Runtime não está disponível. Use o navegador externo ou instale o runtime da Microsoft.");return S_OK;}
            const auto status=environment->CreateCoreWebView2Controller(host,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([guard,host](HRESULT status,ICoreWebView2Controller* controller)->HRESULT{
                    if(!guard||!guard->window()||reinterpret_cast<HWND>(guard->window()->winId())!=host){if(controller)controller->Close();return S_OK;}
                    auto& browser=*guard;browser.native_->starting=false;
                    if(FAILED(status)||!controller){browser.fail("Não foi possível criar o painel do navegador.");return S_OK;}
                    browser.native_->controller=controller;controller->get_CoreWebView2(&browser.native_->view);
                    controller->put_IsVisible(FALSE);browser.native_->visible=false;
                    auto* view=browser.native_->view.Get();if(!view){browser.fail("O navegador do sistema não respondeu.");return S_OK;}
                    EventRegistrationToken token{};
                    view->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>([guard](ICoreWebView2*,ICoreWebView2SourceChangedEventArgs*)->HRESULT{if(guard)guard->updateState();return S_OK;}).Get(),&token);
                    view->add_HistoryChanged(Callback<ICoreWebView2HistoryChangedEventHandler>([guard](ICoreWebView2*,IUnknown*)->HRESULT{if(guard)guard->updateState();return S_OK;}).Get(),&token);
                    view->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>([guard](ICoreWebView2*,IUnknown*)->HRESULT{if(guard)guard->updateState();return S_OK;}).Get(),&token);
                    view->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>([guard](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs*)->HRESULT{if(guard){guard->loading_=true;guard->error_.clear();emit guard->changed();}return S_OK;}).Get(),&token);
                    view->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>([guard](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args)->HRESULT{
                        if(guard){guard->loading_=false;BOOL success=FALSE;args->get_IsSuccess(&success);if(!success){COREWEBVIEW2_WEB_ERROR_STATUS error;args->get_WebErrorStatus(&error);if(error!=COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED)guard->error_="Não foi possível carregar o site. Verifique o endereço e a conexão.";}guard->updateState();}return S_OK;
                    }).Get(),&token);
                    // Keep target=_blank links in the research pane.
                    view->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>([guard](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* args)->HRESULT{
                        args->put_Handled(TRUE);if(guard){LPWSTR uri=nullptr;args->get_Uri(&uri);const auto url=QString::fromWCharArray(uri);CoTaskMemFree(uri);guard->navigate(url);}return S_OK;
                    }).Get(),&token);
                    view->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>([guard](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs* args)->HRESULT{
                        if(!guard)return S_OK;COREWEBVIEW2_PROCESS_FAILED_KIND kind;args->get_ProcessFailedKind(&kind);
                        QTimer::singleShot(0,guard.data(),[guard,kind]{if(!guard)return;
                            if(kind==COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED){
                                if(guard->native_->controller)guard->native_->controller->Close();guard->native_->view.Reset();guard->native_->controller.Reset();guard->native_->bounds={};guard->native_->visible=false;guard->ready_=false;guard->focused_=false;
                            }
                            guard->fail("O processo do navegador foi interrompido. Tente recarregar a página.");
                        });return S_OK;
                    }).Get(),&token);
                    controller->add_AcceleratorKeyPressed(Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>([guard](ICoreWebView2Controller*,ICoreWebView2AcceleratorKeyPressedEventArgs* args)->HRESULT{
                        UINT key=0;COREWEBVIEW2_KEY_EVENT_KIND kind;args->get_VirtualKey(&key);args->get_KeyEventKind(&kind);
                        if(guard&&key=='L'&&(GetKeyState(VK_CONTROL)&0x8000)&&(kind==COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN||kind==COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN)){
                            args->put_Handled(TRUE);QTimer::singleShot(0,guard.data(),[guard]{if(guard)emit guard->focusAddressRequested();});
                        }return S_OK;
                    }).Get(),&token);
                    controller->add_GotFocus(Callback<ICoreWebView2FocusChangedEventHandler>([guard](ICoreWebView2Controller*,IUnknown*)->HRESULT{if(guard){guard->focused_=true;emit guard->changed();}return S_OK;}).Get(),&token);
                    controller->add_LostFocus(Callback<ICoreWebView2FocusChangedEventHandler>([guard](ICoreWebView2Controller*,IUnknown*)->HRESULT{if(guard){guard->focused_=false;emit guard->changed();}return S_OK;}).Get(),&token);
                    browser.ready_=true;browser.syncGeometry();browser.navigate(browser.url_.toString());return S_OK;
                }).Get());
            if(FAILED(status)){guard->native_->starting=false;guard->fail("Não foi possível criar o navegador do sistema.");}
            return S_OK;
        }).Get());
    if(FAILED(result)){native_->starting=false;fail("Microsoft Edge WebView2 indisponível. Abra o navegador externo ou instale o WebView2 Runtime.");}
#else
    fail("Nesta plataforma, use o navegador padrão pelo botão Abrir externamente e arraste as imagens para o quadro.");
#endif
}
void SystemBrowser::syncGeometry(){
#ifdef Q_OS_WIN
    if(native_->controller&&window()){
        const auto topLeft=mapToScene(QPointF(0,0));const auto dpr=window()->devicePixelRatio();
        const auto rect=QRectF(topLeft,QSizeF(width(),height()));
        const auto clipped=rect.intersected(QRectF(0,0,window()->width(),window()->height()));
        RECT bounds{LONG(std::lround(clipped.left()*dpr)),LONG(std::lround(clipped.top()*dpr)),LONG(std::lround(clipped.right()*dpr)),LONG(std::lround(clipped.bottom()*dpr))};
        const QRect nextBounds(bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top);
        if(nextBounds!=native_->bounds){native_->controller->put_Bounds(bounds);native_->bounds=nextBounds;}
        const bool shown=isVisible()&&isEnabled()&&!suspended_&&window()->isVisible()&&!clipped.isEmpty();
        if(shown!=native_->visible){native_->controller->put_IsVisible(shown);native_->visible=shown;}
    }else if(isVisible()&&error_.isEmpty())QTimer::singleShot(0,this,&SystemBrowser::initialize);
#endif
}
void SystemBrowser::geometryChange(const QRectF& next,const QRectF& previous){QQuickItem::geometryChange(next,previous);syncGeometry();}
void SystemBrowser::itemChange(ItemChange change,const ItemChangeData& data){QQuickItem::itemChange(change,data);if(change==ItemVisibleHasChanged||change==ItemEnabledHasChanged||change==ItemDevicePixelRatioHasChanged)syncGeometry();}
void SystemBrowser::setSuspended(bool value){if(value==suspended_)return;suspended_=value;syncGeometry();emit suspendedChanged();}
void SystemBrowser::updateState(){
#ifdef Q_OS_WIN
    if(!native_->view)return;
    LPWSTR source=nullptr,title=nullptr;BOOL back=FALSE,forward=FALSE;
    native_->view->get_Source(&source);native_->view->get_DocumentTitle(&title);
    if(source)url_=QUrl(QString::fromWCharArray(source));if(title)title_=QString::fromWCharArray(title);
    CoTaskMemFree(source);CoTaskMemFree(title);native_->view->get_CanGoBack(&back);native_->view->get_CanGoForward(&forward);back_=back;forward_=forward;
#endif
    emit changed();
}
void SystemBrowser::goBack(){
#ifdef Q_OS_WIN
    if(native_->view)native_->view->GoBack();
#endif
}
void SystemBrowser::goForward(){
#ifdef Q_OS_WIN
    if(native_->view)native_->view->GoForward();
#endif
}
void SystemBrowser::reload(){
#ifdef Q_OS_WIN
    if(native_->view){error_.clear();native_->view->Reload();emit changed();}else {error_.clear();initialize();}
#endif
}
void SystemBrowser::stop(){
#ifdef Q_OS_WIN
    if(native_->view)native_->view->Stop();
#endif
}
void SystemBrowser::openExternal(){QDesktopServices::openUrl(url_);}
}
