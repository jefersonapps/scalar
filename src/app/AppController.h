#pragma once
#include "documents/Document.h"
#include "commands/History.h"
#include "persistence/Library.h"
#include "persistence/ProjectStore.h"
#include "clipboard/ImageImporter.h"
#include "rendering/TextRenderer.h"
#include "persistence/TrashStore.h"
#include "recognition/ShapeRecognizer.h"
#include "pdf/PdfRenderCache.h"
#include <QObject>
#include <QTimer>
#include <QFutureWatcher>
#include <QColor>
#include <QUrl>
#include <memory>
#include <span>
#include <unordered_map>
#include <QSet>
#include <QHash>
namespace scalar {
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList trashedProjects READ trashedProjects NOTIFY recentChanged)
    Q_PROPERTY(QVariantMap background READ background NOTIFY changed)
    Q_PROPERTY(QVariantList backgroundPresets READ backgroundPresets NOTIFY preferencesChanged)
    Q_PROPERTY(QVariantList gridPresets READ gridPresets NOTIFY preferencesChanged)
    Q_PROPERTY(bool textBusy READ textBusy NOTIFY changed)
    Q_PROPERTY(QStringList fontFamilies READ fontFamilies CONSTANT)
    Q_PROPERTY(QString defaultTextFont READ defaultTextFont CONSTANT)
    Q_PROPERTY(QString textError READ textError NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString projectName READ projectName NOTIFY changed)
    Q_PROPERTY(bool pageInfinite READ pageInfinite NOTIFY changed)
    Q_PROPERTY(double pageWidth READ pageWidth NOTIFY changed)
    Q_PROPERTY(double pageHeight READ pageHeight NOTIFY changed)
    Q_PROPERTY(QColor pageColor READ pageColor NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool documentBusy READ documentBusy NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY preferencesChanged)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY recentChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY recentChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY preferencesChanged)
    Q_PROPERTY(bool reducedEffects READ reducedEffects WRITE setReducedEffects NOTIFY preferencesChanged)
    Q_PROPERTY(bool disableToolbarBlur READ disableToolbarBlur WRITE setDisableToolbarBlur NOTIFY preferencesChanged)
    Q_PROPERTY(QString defaultSize READ defaultSize WRITE setDefaultSize NOTIFY preferencesChanged)
    Q_PROPERTY(bool defaultLandscape READ defaultLandscape WRITE setDefaultLandscape NOTIFY preferencesChanged)
    Q_PROPERTY(QString defaultBackground READ defaultBackground WRITE setDefaultBackground NOTIFY preferencesChanged)
    Q_PROPERTY(bool recognitionEnabled READ recognitionEnabled WRITE setRecognitionEnabled NOTIFY preferencesChanged)
    Q_PROPERTY(int holdDelay READ holdDelay WRITE setHoldDelay NOTIFY preferencesChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY changed)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY pageChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pagesChanged)
    Q_PROPERTY(QVariantList pages READ pages NOTIFY pagesChanged)
    Q_PROPERTY(bool pdfSupported READ pdfSupported CONSTANT)
    Q_PROPERTY(bool pdfBusy READ pdfBusy NOTIFY changed)
    Q_PROPERTY(int pdfPageCount READ pdfPageCount NOTIFY changed)
    Q_PROPERTY(QString pdfName READ pdfName NOTIFY changed)
    Q_PROPERTY(QString pdfError READ pdfError NOTIFY changed)
    Q_PROPERTY(bool exporting READ exporting NOTIFY changed)
public:
    explicit AppController(QObject* parent=nullptr,const QString& dataDirectory={});
    ~AppController() override;
    bool textBusy() const {return textPending_;}
    QStringList fontFamilies() const;
    QString defaultTextFont() const;
    Q_INVOKABLE QFont textFont(const QString& family,int pixelSize,bool bold=false,bool italic=false) const;
    Q_INVOKABLE QVariantMap textEmphasis(QObject* document,int start,int end) const;
    Q_INVOKABLE void formatTextSelection(QObject* document,int start,int end,bool bold,bool enabled);
    Q_INVOKABLE QVariantList textFormats(QObject* document,bool defaultBold,bool defaultItalic) const;
    Q_INVOKABLE void restoreTextFormats(QObject* document,const QVariantList& formats);
    QString textError() const {return textError_;}
    Q_INVOKABLE QVariantMap textValues(const QString& id) const;
    Q_INVOKABLE void upsertText(const QString& id,QPointF position,const QVariantMap& values);
    void refreshTextTextures(double pixelsPerMm);
    bool active() const { return !project_.pages.empty(); }
    QString projectName() const { return QString::fromStdString(project_.name); }
    bool pageInfinite() const { return page()&&page()->size.infinite; }
    double pageWidth() const { return page()?page()->size.widthMm:210; }
    double pageHeight() const { return page()?page()->size.heightMm:297; }
    QColor pageColor() const;
    QVariantMap background() const;
    QVariantList backgroundPresets() const;
    QVariantList gridPresets() const;
    Q_INVOKABLE bool validBackground(const QVariantMap& values) const;
    Q_INVOKABLE void setBackground(const QVariantMap& values);
    Q_INVOKABLE void applyBackgroundPreset(const QString& name);
    Q_INVOKABLE void saveBackgroundPreset(const QString& name,const QVariantMap& values);
    bool canUndo() const {const auto i=histories_.find(page()?page()->id:"");return i!=histories_.end()&&i->second.canUndo();}
    bool canRedo() const {const auto i=histories_.find(page()?page()->id:"");return i!=histories_.end()&&i->second.canRedo();}
    bool dirty() const { return dirty_; }
    bool loading() const { return loading_; }
    bool systemDark() const;
    bool documentBusy() const { return loading_||imageImportPending_||textPending_||trashPending_||pdfPending_||renamePending_||fillPending_; }
    bool busy() const { return documentBusy()||saveWatcher_.isRunning(); }
    QString status() const { return status_; }
    QVariantList trashedProjects() const {return library_->trash();}
    Q_INVOKABLE void trashProject(const QString& id);
    Q_INVOKABLE void restoreProject(const QString& id);
    Q_INVOKABLE void deleteProjectPermanently(const QString& id);
    void maintainTrashedProjects();
    QVariantList recentProjects() const { return library_->recent(); }
    Q_INVOKABLE QVariantList searchProjects(const QString& query,const QString& folderId,const QString& scope) const;
    QVariantList folders() const { return library_->folders(); }
    Q_INVOKABLE QString saveFolder(const QString& id,const QString& name,const QString& color,const QString& parentId={}){const auto result=library_->saveFolder(id,name,color,parentId);if(!result.isEmpty())emit recentChanged();return result;}
    Q_INVOKABLE bool deleteFolder(const QString& id);
    Q_INVOKABLE bool moveProjectToFolder(const QString& projectId,const QString& folderId){if(!library_->moveToFolder(projectId,folderId))return false;emit recentChanged();return true;}
    QString theme() const;
    void setTheme(const QString& theme);
    bool reducedEffects() const;
    void setReducedEffects(bool value);
    bool disableToolbarBlur() const;
    void setDisableToolbarBlur(bool value);
    QString defaultSize() const;
    void setDefaultSize(const QString& value);
    bool defaultLandscape() const;
    void setDefaultLandscape(bool value);
    QString defaultBackground() const;
    void setDefaultBackground(const QString& value);
    bool recoveryAvailable() const { return recovery_; }
    const Page* page() const { return active()?&project_.pages[currentPage_]:nullptr; }
    // Session-only viewport state: deliberately excluded from Project/ProjectStore.
    struct PageView { double zoom; Point center; };
    std::optional<PageView> sessionPageView(const QString& id) const {
        const auto found=pageViews_.constFind(id);return found==pageViews_.cend()?std::nullopt:std::optional<PageView>{*found};
    }
    void rememberPageView(const QString& id,PageView view){pageViews_.insert(id,view);}
    int currentPage() const {return currentPage_;}
    int pageCount() const {return int(project_.pages.size());}
    QVariantList pages() const;
    Q_INVOKABLE void selectPage(int index);
    Q_INVOKABLE void addPage();
    Q_INVOKABLE void deletePage(int index);
    Q_INVOKABLE bool setPageSize(const QString& preset,double width,double height,bool landscape);
    Q_INVOKABLE void duplicatePage();
    bool pdfSupported() const {return pdfAvailable();}
    bool pdfBusy() const {return pdfPending_;}
    int pdfPageCount() const {return int(pdfInfo_.sizes.size());}
    QString pdfName() const {return pdfInfo_.name;}
    QString pdfError() const {return pdfInfo_.error;}
    Q_INVOKABLE void inspectPdfFile(const QUrl& url);
    Q_INVOKABLE void importPdfPages(const QString& range);
    Q_INVOKABLE void cancelPdfImport();
    bool exporting() const {return exportPending_;}
    Q_INVOKABLE void exportPdf(const QUrl& url);
    void exportSelection(std::vector<CanvasObject> selected,const QUrl& url,bool svg);
    void refreshPdf(double scale){if(page())pdfCache_.request(page()->pdf,page()->size,scale);}
    QImage pdfImage() const {return pdfCache_.image();}
    quint64 pdfImageRevision() const {return pdfCache_.revision();}
    void addStroke(StrokeObject stroke);
    void addRecognizedStroke(StrokeObject stroke,ShapeObject shape);
    void addShape(ShapeObject shape);
    Q_INVOKABLE void importImage(const QUrl& url,QPointF center);
    Q_INVOKABLE void importImages(const QVariantList& urls,QPointF center);
    Q_INVOKABLE void pasteImage(QPointF center);
    void aliasImage(const std::string& from,const std::string& to){images_.insert(QString::fromStdString(to),image(from));textMeshes_.insert(QString::fromStdString(to),textMeshes_.value(QString::fromStdString(from)));textSizes_.insert(QString::fromStdString(to),textSize(from));}
    std::span<const Point> mathGeometry(const std::string& id) const {const auto i=textMeshes_.constFind(QString::fromStdString(id));return i==textMeshes_.cend()?std::span<const Point>{}:std::span<const Point>{i.value()};}
    QSizeF textSize(const std::string& id) const {return textSizes_.value(QString::fromStdString(id));}
    QImage image(const std::string& id) const {return images_.value(QString::fromStdString(id));}
    void changeObjects(std::vector<ObjectChange> changes,CommandKind kind);
    void eraseObjects(std::vector<ObjectChange> changes,std::vector<StrokeObject> recoverable);
    void fillRegion(Point seed,QColor color,double opacity=.10);
    bool recognitionEnabled() const;
    void setRecognitionEnabled(bool value);
    int holdDelay() const;
    void setHoldDelay(int value);
    std::int64_t nextZIndex() const;
    Q_INVOKABLE void newProject(const QString& name,const QString& preset,double width,double height,bool landscape,const QString& background,const QString& color={});
    Q_INVOKABLE void newDefault();
    Q_INVOKABLE void newDefaultInFolder(const QString& folderId);
    Q_INVOKABLE bool renameProject(const QString& id,const QString& name);
    Q_INVOKABLE bool renameCurrentProject(const QString& name){return active() && renameProject(QString::fromStdString(project_.id),name);}
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
    void pagesChanged();
    void pageChanged();
    void pdfImportRequested();
    void pdfImported();
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    QFutureWatcher<ImportedImage> fillWatcher_;
    bool fillPending_=false;
    quint64 fillRevision_=0;
    std::string fillPageId_;
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
    Page& current(){return project_.pages[currentPage_];}
    History& history(){return histories_[current().id];}
    void scheduleThumbnails();
    void beginThumbnails();
    Project project_;
    std::unordered_map<std::string,History> histories_;
    int currentPage_=0;
    QHash<QString,PageView> pageViews_;
    QTimer thumbnailsTimer_;
    QFutureWatcher<QHash<QString,QString>> thumbnailsWatcher_;
    QHash<QString,QString> thumbnails_;
    QSet<QString> dirtyThumbnails_,renderingThumbnails_;
    quint64 thumbnailsRevision_=0;
    bool thumbnailsPending_=false;
    PdfRenderCache pdfCache_;
    QFutureWatcher<PdfInfo> pdfWatcher_;
    PdfInfo pdfInfo_;
    bool pdfPending_=false;
    QFutureWatcher<QString> exportWatcher_;
    bool exportPending_=false;
    std::unique_ptr<Library> library_;
    QString dataDir_,path_,status_,pendingOpenPath_,pendingFolderId_;
    bool trashPending_=false;
    QTimer autosave_,trashExpiry_;
    QFutureWatcher<TrashResult> trashWatcher_;
    QFutureWatcher<SaveResult> saveWatcher_;
    QFutureWatcher<SaveResult> renameWatcher_;
    bool renamePending_=false;
    void finishRename();
    QFutureWatcher<LoadResult> loadWatcher_;
    QFutureWatcher<std::vector<ImportedImage>> imageWatcher_;
    QFutureWatcher<PreparedText> textWatcher_;
    QFutureWatcher<QHash<QString,TextVisual>> textRasterWatcher_;
    QString textProjectId_,textError_;
    QString textPageId_;
    std::optional<CanvasObject> textBefore_;
    bool textPending_=false;
    bool textRasterPending_=false;
    double textRasterScale_=96./25.4;
    double textRasterRequestedScale_=96./25.4;
    quint64 textRasterRevision_=0;
    QString importProjectId_;
    QString importPageId_;
    QHash<QString,QImage> images_;
    QHash<QString,std::vector<Point>> textMeshes_;
    QHash<QString,QSizeF> textSizes_;
    quint64 revision_=0;
    bool recognitionEnabled_=true;
    int holdDelay_=1000;
    bool imageImportPending_=false;
    bool dirty_=false,loading_=false,recovery_=false,closed_=false;
};
}
