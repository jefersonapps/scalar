#include "AppController.h"
#include "persistence/Thumbnail.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QEvent>
#include <QStyleHints>
#include <QtConcurrent>
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(appLog,"scalar.app")
namespace scalar {
namespace { QString now(){return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);} }
AppController::AppController(QObject* parent,const QString& dataDirectory):QObject(parent) {
    dataDir_=dataDirectory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation):dataDirectory;
    QDir().mkpath(dataDir_+"/projects"); library_=std::make_unique<Library>(dataDir_+"/library.sqlite");
    qGuiApp->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
    connect(qGuiApp->styleHints(),&QStyleHints::colorSchemeChanged,this,[this]{emit preferencesChanged();});
#endif
    recovery_=library_->setting("cleanShutdown","true").toString()=="false"&&QFile::exists(dataDir_+"/session.board");
    library_->setSetting("cleanShutdown","false");
    status_=library_->error().isEmpty()?"Pronto para suas ideias":library_->error();
    autosave_.setSingleShot(true); autosave_.setInterval(1600);
    connect(&autosave_,&QTimer::timeout,this,&AppController::beginSave);
    connect(&saveWatcher_,&QFutureWatcher<SaveResult>::finished,this,&AppController::finishSave);
    connect(&loadWatcher_,&QFutureWatcher<LoadResult>::finished,this,[this]{
        loading_=false; const auto result=loadWatcher_.result();
        if(result){
            const bool recovered=pendingOpenPath_==dataDir_+"/session.board";
            const auto target=recovered ? dataDir_+"/projects/"+QString::fromStdString(result.project.id)+".board" : pendingOpenPath_;
            install(result.project,target);library_->remember(QString::fromStdString(result.project.id),QString::fromStdString(result.project.name),target,QString::fromStdString(result.project.updatedAt));emit recentChanged();status_=recovered?"Quadro recuperado":"Projeto aberto";
            if(recovered){mutate();beginSave();}
        } else {status_=result.error;qCWarning(appLog)<<status_;}
        emit changed();
    });
}
bool AppController::eventFilter(QObject* watched,QEvent* event){
    if(watched==qGuiApp && event->type()==QEvent::ApplicationPaletteChange)emit preferencesChanged();
    return QObject::eventFilter(watched,event);
}
AppController::~AppController(){ if(!closed_)flush(); loadWatcher_.waitForFinished(); }
QColor AppController::pageColor() const { const auto c=page()?page()->background:0xffffffff;return QColor(int((c>>24)&255),int((c>>16)&255),int((c>>8)&255),int(c&255)); }
bool AppController::systemDark() const {
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
    const auto scheme=qGuiApp->styleHints()->colorScheme();
    if(scheme!=Qt::ColorScheme::Unknown)return scheme==Qt::ColorScheme::Dark;
#endif
    return QGuiApplication::palette().color(QPalette::Window).lightness()<128;
}
QString AppController::theme() const { return library_->setting("theme","System").toString(); }
void AppController::setTheme(const QString& v){if(v!="Light"&&v!="Dark"&&v!="System")return;library_->setSetting("theme",v);emit preferencesChanged();}
bool AppController::reducedEffects() const {return library_->setting("reducedEffects",false).toBool();}
void AppController::setReducedEffects(bool v){library_->setSetting("reducedEffects",v);emit preferencesChanged();}
QString AppController::defaultSize() const{return library_->setting("defaultSize","A4").toString();}
void AppController::setDefaultSize(const QString& v){if(v!="A4"&&v!="Carta")return;library_->setSetting("defaultSize",v);emit preferencesChanged();}
bool AppController::defaultLandscape() const{return library_->setting("defaultLandscape",false).toBool();}
void AppController::setDefaultLandscape(bool v){library_->setSetting("defaultLandscape",v);emit preferencesChanged();}
QString AppController::defaultBackground() const{return library_->setting("defaultBackground","Branco").toString();}
void AppController::setDefaultBackground(const QString& v){if(v!="Branco"&&v!="Preto"&&v!="Verde")return;library_->setSetting("defaultBackground",v);emit preferencesChanged();}
void AppController::install(Project p,const QString& path){project_=std::move(p);path_=path;history_.clear();dirty_=false;++revision_;emit documentChanged();emit changed();}
void AppController::newProject(const QString& name,const QString& preset,double width,double height,bool landscape,const QString& background) {
    if(loading_)return;
    PageSize size=preset=="A4"?PageSize::a4():preset=="Carta"?PageSize::letter():PageSize{width,height};
    if(!size.valid()){status_="Use dimensões entre 10 e 5000 mm.";emit changed();return;}
    if(landscape&&size.widthMm<size.heightMm)std::swap(size.widthMm,size.heightMm);
    if(!landscape&&size.widthMm>size.heightMm)std::swap(size.widthMm,size.heightMm);
    if(!flush())return;
    Project p{newId(),(name.trimmed().isEmpty()?QString("Quadro sem título"):name.trimmed()).toStdString(),now().toStdString(),now().toStdString(),{}};
    p.pages.push_back({newId(),size,background=="Preto"?0x18221effu:background=="Verde"?0x214f43ffu:0xffffffffu,{}});
    const QString path=dataDir_+"/projects/"+QString::fromStdString(p.id)+".board";
    install(std::move(p),path); mutate(); beginSave();
}
void AppController::newDefault(){newProject("Novo quadro",defaultSize(),210,297,defaultLandscape(),defaultBackground());}
void AppController::mutate(){dirty_=true;++revision_;project_.updatedAt=now().toStdString();autosave_.start();emit documentChanged();emit changed();}
void AppController::addStroke(StrokeObject s){if(!active()||loading_||s.samples.empty())return;history_.add(project_.pages[0],std::move(s));mutate();}
void AppController::undo(){if(active()&&history_.undo(project_.pages[0]))mutate();}
void AppController::redo(){if(active()&&history_.redo(project_.pages[0]))mutate();}
void AppController::save(){if(active()){dirty_=true;beginSave();}}
void AppController::saveAs(const QUrl& url){if(!active()||!url.isLocalFile()||!flush())return;path_=url.toLocalFile();if(!path_.endsWith(".board",Qt::CaseInsensitive))path_+=".board";dirty_=true;beginSave();}
void AppController::beginSave(){
    if(!active()||!dirty_||loading_)return;
    if(saveWatcher_.isRunning()){autosave_.start();return;}
    const auto snapshot=project_;const auto path=path_;const auto recovery=dataDir_+"/session.board";const auto rev=revision_;
    status_="Salvando…";
    saveWatcher_.setFuture(QtConcurrent::run([snapshot,path,recovery,rev]{
        auto error=ProjectStore::save(recovery,snapshot);
        if(error.isEmpty())error=ProjectStore::save(path,snapshot);
        if(error.isEmpty())saveThumbnail(snapshot,path+".png");
        return SaveResult{error,QString::fromStdString(snapshot.id),QString::fromStdString(snapshot.name),path,QString::fromStdString(snapshot.updatedAt),rev};
    }));emit changed();
}
void AppController::finishSave(){
    if(!saveWatcher_.future().isValid()||saveWatcher_.isRunning())return;
    const auto r=saveWatcher_.result();
    if(r.error.isEmpty()) {
        library_->remember(r.id,r.name,r.path,r.updated);emit recentChanged();
        if(r.revision==revision_ && r.id==QString::fromStdString(project_.id)){dirty_=false;status_="Salvo neste dispositivo";} else {status_="Alterações pendentes";autosave_.start();}
    } else {status_="Falha ao salvar: "+r.error;qCWarning(appLog)<<status_;}
    emit changed();
}
bool AppController::flush(){
    autosave_.stop();
    if(saveWatcher_.isRunning()){saveWatcher_.waitForFinished();finishSave();}
    if(active()&&dirty_){beginSave();saveWatcher_.waitForFinished();finishSave();}
    return !dirty_;
}
void AppController::open(const QUrl& url){if(url.isLocalFile())openPath(url.toLocalFile());}
void AppController::openPath(const QString& path){
    if(loading_||!flush())return;loading_=true;pendingOpenPath_=path;status_="Abrindo projeto…";
    loadWatcher_.setFuture(QtConcurrent::run([path]{return ProjectStore::load(path);}));emit changed();
}
void AppController::home(){if(loading_||!flush())return;project_={};history_.clear();++revision_;emit documentChanged();emit changed();}
void AppController::recover(){if(!recovery_)return;recovery_=false;openPath(dataDir_+"/session.board");}
void AppController::discardRecovery(){recovery_=false;QFile::remove(dataDir_+"/session.board");emit changed();}
bool AppController::shutdown(){
    if(loading_)return false;
    if(!flush())return false;
    QFile::remove(dataDir_+"/session.board");library_->setSetting("cleanShutdown","true");closed_=true;return true;
}
}
