#pragma once
#include "documents/Document.h"
#include "commands/History.h"
#include "persistence/Library.h"
#include "persistence/ProjectStore.h"
#include "clipboard/ImageImporter.h"
#include "rendering/TextRenderer.h"
#include "persistence/TrashStore.h"
#include "recognition/ShapeRecognizer.h"
#include <QObject>
#include <QTimer>
#include <QFutureWatcher>
#include <QColor>
#include <QUrl>
#include <memory>
#include <span>
namespace scalar {
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList trashedProjects READ trashedProjects NOTIFY recentChanged)
    Q_PROPERTY(QVariantMap background READ background NOTIFY changed)
    Q_PROPERTY(QVariantList backgroundPresets READ backgroundPresets NOTIFY preferencesChanged)
    Q_PROPERTY(bool textBusy READ textBusy NOTIFY changed)
    Q_PROPERTY(QString textError READ textError NOTIFY changed)
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
    Q_PROPERTY(bool recognitionEnabled READ recognitionEnabled WRITE setRecognitionEnabled NOTIFY preferencesChanged)
    Q_PROPERTY(int holdDelay READ holdDelay WRITE setHoldDelay NOTIFY preferencesChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY changed)
public:
    explicit AppController(QObject* parent=nullptr,const QString& dataDirectory={});
    ~AppController() override;
    bool textBusy() const {return textPending_;}
    QString textError() const {return textError_;}
    Q_INVOKABLE QVariantMap textValues(const QString& id) const;
    Q_INVOKABLE void upsertText(const QString& id,QPointF position,const QVariantMap& values);
    void refreshTextTextures(double pixelsPerMm);
    bool active() const { return !project_.pages.empty(); }
    QString projectName() const { return QString::fromStdString(project_.name); }
    double pageWidth() const { return active()?project_.pages[0].size.widthMm:210; }
    double pageHeight() const { return active()?project_.pages[0].size.heightMm:297; }
    QColor pageColor() const;
    QVariantMap background() const;
    QVariantList backgroundPresets() const;
    Q_INVOKABLE bool validBackground(const QVariantMap& values) const;
    Q_INVOKABLE void setBackground(const QVariantMap& values);
    Q_INVOKABLE void applyBackgroundPreset(const QString& name);
    Q_INVOKABLE void saveBackgroundPreset(const QString& name,const QVariantMap& values);
    bool canUndo() const { return history_.canUndo(); }
    bool canRedo() const { return history_.canRedo(); }
    bool dirty() const { return dirty_; }
    bool loading() const { return loading_; }
    bool systemDark() const;
    bool busy() const { return loading_||saveWatcher_.isRunning()||imageImportPending_||textPending_||trashPending_; }
    QString status() const { return status_; }
    QVariantList trashedProjects() const {return library_->trash();}
    Q_INVOKABLE void trashProject(const QString& id);
    Q_INVOKABLE void restoreProject(const QString& id);
    Q_INVOKABLE void deleteProjectPermanently(const QString& id);
    void maintainTrashedProjects();
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
    void addRecognizedStroke(StrokeObject stroke,ShapeObject shape);
    void addShape(ShapeObject shape);
    Q_INVOKABLE void importImage(const QUrl& url,QPointF center);
    Q_INVOKABLE void pasteImage(QPointF center);
    void aliasImage(const std::string& from,const std::string& to){images_.insert(QString::fromStdString(to),image(from));textMeshes_.insert(QString::fromStdString(to),textMeshes_.value(QString::fromStdString(from)));textSizes_.insert(QString::fromStdString(to),textSize(from));}
    std::span<const Point> mathGeometry(const std::string& id) const {const auto i=textMeshes_.constFind(QString::fromStdString(id));return i==textMeshes_.cend()?std::span<const Point>{}:std::span<const Point>{i.value()};}
    QSizeF textSize(const std::string& id) const {return textSizes_.value(QString::fromStdString(id));}
    QImage image(const std::string& id) const {return images_.value(QString::fromStdString(id));}
    void changeObjects(std::vector<ObjectChange> changes,CommandKind kind);
    bool recognitionEnabled() const;
    void setRecognitionEnabled(bool value);
    int holdDelay() const;
    void setHoldDelay(int value);
    std::int64_t nextZIndex() const;
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
    void textCommitted(const QString& id);
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    struct SaveResult { QString error,id,name,path,updated; quint64 revision=0; };
    void mutate();
    void closeLines(const std::string& newest);
    QFutureWatcher<std::optional<ClosedLines>> closedLinesWatcher_;
    quint64 closeLinesRevision_=0;
    std::string latestLineId_;
    void beginSave();
    bool flush();
    void finishSave();
    void install(Project project,const QString& path);
    Project project_;
    History history_;
    std::unique_ptr<Library> library_;
    QString dataDir_,path_,status_,pendingOpenPath_;
    bool trashPending_=false;
    QTimer autosave_,trashExpiry_;
    QFutureWatcher<TrashResult> trashWatcher_;
    QFutureWatcher<SaveResult> saveWatcher_;
    QFutureWatcher<LoadResult> loadWatcher_;
    QFutureWatcher<ImportedImage> imageWatcher_;
    QFutureWatcher<PreparedText> textWatcher_;
    QFutureWatcher<QHash<QString,TextVisual>> textRasterWatcher_;
    QString textProjectId_,textError_;
    std::optional<CanvasObject> textBefore_;
    bool textPending_=false;
    double textRasterScale_=96./25.4;
    quint64 textRasterRevision_=0;
    QString importProjectId_;
    QHash<QString,QImage> images_;
    QHash<QString,std::vector<Point>> textMeshes_;
    QHash<QString,QSizeF> textSizes_;
    quint64 revision_=0;
    bool recognitionEnabled_=true;
    int holdDelay_=500;
    bool imageImportPending_=false;
    bool dirty_=false,loading_=false,recovery_=false,closed_=false;
};
}
