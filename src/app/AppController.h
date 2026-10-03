#pragma once
#include "documents/Document.h"
#include "commands/History.h"
#include "persistence/Library.h"
#include "persistence/ProjectStore.h"
#include <QObject>
#include <QTimer>
#include <QFutureWatcher>
#include <QColor>
#include <QUrl>
#include <memory>
namespace scalar {
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString projectName READ projectName NOTIFY changed)
    Q_PROPERTY(double pageWidth READ pageWidth NOTIFY changed)
    Q_PROPERTY(double pageHeight READ pageHeight NOTIFY changed)
    Q_PROPERTY(QColor pageColor READ pageColor NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY preferencesChanged)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY recentChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY preferencesChanged)
    Q_PROPERTY(bool reducedEffects READ reducedEffects WRITE setReducedEffects NOTIFY preferencesChanged)
    Q_PROPERTY(QString defaultSize READ defaultSize WRITE setDefaultSize NOTIFY preferencesChanged)
    Q_PROPERTY(bool defaultLandscape READ defaultLandscape WRITE setDefaultLandscape NOTIFY preferencesChanged)
    Q_PROPERTY(QString defaultBackground READ defaultBackground WRITE setDefaultBackground NOTIFY preferencesChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY changed)
public:
    explicit AppController(QObject* parent=nullptr,const QString& dataDirectory={});
    ~AppController() override;
    bool active() const { return !project_.pages.empty(); }
    QString projectName() const { return QString::fromStdString(project_.name); }
    double pageWidth() const { return active()?project_.pages[0].size.widthMm:210; }
    double pageHeight() const { return active()?project_.pages[0].size.heightMm:297; }
    QColor pageColor() const;
    bool canUndo() const { return history_.canUndo(); }
    bool canRedo() const { return history_.canRedo(); }
    bool dirty() const { return dirty_; }
    bool loading() const { return loading_; }
    bool systemDark() const;
    bool busy() const { return loading_||saveWatcher_.isRunning(); }
    QString status() const { return status_; }
    QVariantList recentProjects() const { return library_->recent(); }
    QString theme() const;
    void setTheme(const QString& theme);
    bool reducedEffects() const;
    void setReducedEffects(bool value);
    QString defaultSize() const;
    void setDefaultSize(const QString& value);
    bool defaultLandscape() const;
    void setDefaultLandscape(bool value);
    QString defaultBackground() const;
    void setDefaultBackground(const QString& value);
    bool recoveryAvailable() const { return recovery_; }
    const Page* page() const { return active()?&project_.pages[0]:nullptr; }
    void addStroke(StrokeObject stroke);
    Q_INVOKABLE void newProject(const QString& name,const QString& preset,double width,double height,bool landscape,const QString& background);
    Q_INVOKABLE void newDefault();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs(const QUrl& url);
    Q_INVOKABLE void open(const QUrl& url);
    Q_INVOKABLE void openPath(const QString& path);
    Q_INVOKABLE void home();
    Q_INVOKABLE void recover();
    Q_INVOKABLE void discardRecovery();
    Q_INVOKABLE bool shutdown();
signals:
    void changed();
    void documentChanged();
    void recentChanged();
    void preferencesChanged();
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    struct SaveResult { QString error,id,name,path,updated; quint64 revision=0; };
    void mutate();
    void beginSave();
    bool flush();
    void finishSave();
    void install(Project project,const QString& path);
    Project project_;
    History history_;
    std::unique_ptr<Library> library_;
    QString dataDir_,path_,status_,pendingOpenPath_;
    QTimer autosave_;
    QFutureWatcher<SaveResult> saveWatcher_;
    QFutureWatcher<LoadResult> loadWatcher_;
    quint64 revision_=0;
    bool dirty_=false,loading_=false,recovery_=false,closed_=false;
};
}
