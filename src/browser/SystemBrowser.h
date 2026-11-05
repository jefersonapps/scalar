#pragma once
#include <QQuickItem>
#include <QUrl>
#include <memory>

namespace scalar {
// Native system web view: WebView2 on Windows, shared WebKitGTK on Linux.
// No private browser engine is shipped by Scalar.
class SystemBrowser : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY changed)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY changed)
    Q_PROPERTY(bool browserFocused READ browserFocused NOTIFY changed)
    Q_PROPERTY(bool suspended READ suspended WRITE setSuspended NOTIFY suspendedChanged)
public:
    explicit SystemBrowser(QQuickItem* parent=nullptr);
    ~SystemBrowser() override;
    QUrl url() const {return url_;}
    QString title() const {return title_;}
    QString error() const {return error_;}
    bool loading() const {return loading_;}
    bool ready() const {return ready_;}
    bool canGoBack() const {return back_;}
    bool canGoForward() const {return forward_;}
    bool browserFocused() const {return focused_;}
    bool suspended() const {return suspended_;}
    void setSuspended(bool value);
    Q_INVOKABLE void navigate(const QString& address);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void openExternal();
    static QUrl addressUrl(const QString& address);
signals:
    void changed();
    void suspendedChanged();
    void focusAddressRequested();
protected:
    void geometryChange(const QRectF& next,const QRectF& previous) override;
    void itemChange(ItemChange change,const ItemChangeData& data) override;
    bool eventFilter(QObject* object,QEvent* event) override;
private:
    void initialize();
    void syncGeometry();
    void updateState();
    void fail(const QString& message);
    struct Native;
    std::unique_ptr<Native> native_;
    QUrl url_{QStringLiteral("https://www.google.com")};
    QString title_,error_;
    bool loading_=false,ready_=false,back_=false,forward_=false,focused_=false,suspended_=false;
};
}
