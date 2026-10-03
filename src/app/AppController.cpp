#include "AppController.h"
#include "documents/Backgrounds.h"
#include "persistence/Thumbnail.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QGuiApplication>
#include <QPalette>
#include <QEvent>
#include <QClipboard>
#include <QMimeData>
#include <QStyleHints>
#include <QtConcurrent>
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(appLog,"scalar.app")
namespace scalar {
namespace { QString now(){return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);} }
AppController::AppController(QObject* parent,const QString& dataDirectory):QObject(parent) {
    dataDir_=dataDirectory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation):dataDirectory;
    QDir().mkpath(dataDir_+"/projects"); library_=std::make_unique<Library>(dataDir_+"/library.sqlite");
    recognitionEnabled_=library_->setting("recognitionEnabled",true).toBool();holdDelay_=std::clamp(library_->setting("holdDelay",500).toInt(),250,1500);
    qGuiApp->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
    connect(qGuiApp->styleHints(),&QStyleHints::colorSchemeChanged,this,[this]{emit preferencesChanged();});
#endif
    connect(&imageWatcher_,&QFutureWatcher<ImportedImage>::finished,this,[this]{
        imageImportPending_=false;const auto imported=imageWatcher_.result();if(importProjectId_!=QString::fromStdString(project_.id))return;
        if(!imported.error.isEmpty()){status_=imported.error;emit changed();return;}
        auto object=imported.object;object.properties.zIndex=nextZIndex();images_.insert(QString::fromStdString(object.id),imported.image);
        history_.apply(project_.pages[0],{{{},CanvasObject(std::move(object))}},CommandKind::AddObject);mutate();
    });
    connect(&closedLinesWatcher_,&QFutureWatcher<std::optional<ClosedLines>>::finished,this,[this]{
        if(closeLinesRevision_!=revision_){if(active())closeLines(latestLineId_);return;}
        const auto result=closedLinesWatcher_.result();if(!result||!active())return;std::vector<ObjectChange> changes;
        for(const auto& id:result->lineIds){const auto before=findObject(*page(),id);if(!before||!std::holds_alternative<ShapeObject>(*before))return;changes.push_back({before,{}});}
        changes.push_back({{},CanvasObject(result->polygon)});changeObjects(std::move(changes),CommandKind::ConvertStrokeToShape);
    });
    connect(&trashWatcher_,&QFutureWatcher<TrashResult>::finished,this,[this]{
        const auto r=trashWatcher_.result();bool success=r.error.isEmpty();
        if(r.action==TrashAction::Move&&!success)library_->forgetTrash(r.entry.value("id").toString(),false);
        if(r.action==TrashAction::Restore&&success)success=library_->forgetTrash(r.entry.value("id").toString(),false);
        if(r.action==TrashAction::Delete&&success)success=library_->forgetTrash(r.entry.value("id").toString(),true);
        for(const auto& entry:r.expired)if(!library_->forgetTrash(entry.toMap().value("id").toString(),true))success=false;
        status_=!success?(!r.error.isEmpty()?r.error:QString("Não foi possível atualizar a lixeira.")):r.action==TrashAction::Move?"Quadro movido para a lixeira por 30 dias":r.action==TrashAction::Restore?"Quadro restaurado":r.action==TrashAction::Delete?"Quadro excluído permanentemente":"Pronto para suas ideias";
        trashPending_=false;emit recentChanged();emit changed();
    });
    trashExpiry_.setInterval(60*60*1000);connect(&trashExpiry_,&QTimer::timeout,this,&AppController::maintainTrashedProjects);trashExpiry_.start();
    QTimer::singleShot(0,this,&AppController::maintainTrashedProjects);
    connect(&textWatcher_,&QFutureWatcher<PreparedText>::finished,this,[this]{
        textPending_=false;const auto prepared=textWatcher_.result();textError_=prepared.error;
        if(textProjectId_!=QString::fromStdString(project_.id)){emit changed();return;}
        if(!prepared.error.isEmpty()){status_=prepared.error;emit changed();return;}
        auto object=prepared.object;images_.insert(QString::fromStdString(object.id),prepared.image);textMeshes_.insert(QString::fromStdString(object.id),prepared.geometry);textSizes_.insert(QString::fromStdString(object.id),prepared.naturalSize);
        history_.apply(project_.pages[0],{{textBefore_,CanvasObject(object)}},textBefore_?CommandKind::ChangeStyle:CommandKind::AddObject);textRasterRevision_=0;mutate();emit textCommitted(QString::fromStdString(object.id));
    });
    connect(&textRasterWatcher_,&QFutureWatcher<QHash<QString,TextVisual>>::finished,this,[this]{
        if(textRasterRevision_!=revision_){textRasterRevision_=0;refreshTextTextures(textRasterScale_);return;}
        const auto results=textRasterWatcher_.result();for(auto i=results.begin();i!=results.end();++i){images_.insert(i.key(),i.value().text);textMeshes_.insert(i.key(),i.value().math);textSizes_.insert(i.key(),i.value().naturalSize);if(!i.value().error.isEmpty())status_=i.value().error;}
        if(active())for(auto& object:project_.pages[0].texts)++object.properties.revision;
        emit documentChanged();
    });
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
            install(result.project,target);images_=result.images;refreshTextTextures(textRasterScale_);emit documentChanged();library_->remember(QString::fromStdString(result.project.id),QString::fromStdString(result.project.name),target,QString::fromStdString(result.project.updatedAt));emit recentChanged();status_=recovered?"Quadro recuperado":"Projeto aberto";
            if(recovered){mutate();beginSave();}
        } else {status_=result.error;qCWarning(appLog)<<status_;}
        emit changed();
    });
}
bool AppController::eventFilter(QObject* watched,QEvent* event){
    if(watched==qGuiApp && event->type()==QEvent::ApplicationPaletteChange)emit preferencesChanged();
    return QObject::eventFilter(watched,event);
}
AppController::~AppController(){ if(!closed_)flush(); loadWatcher_.waitForFinished();imageWatcher_.waitForFinished();textWatcher_.waitForFinished();textRasterWatcher_.waitForFinished();trashWatcher_.waitForFinished();closedLinesWatcher_.waitForFinished(); }
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
void AppController::setDefaultBackground(const QString& v){bool found=false;for(const auto& preset:backgroundPresets())if(preset.toMap().value("name").toString()==v)found=true;if(!found)return;library_->setSetting("defaultBackground",v);emit preferencesChanged();}
void AppController::install(Project p,const QString& path){project_=std::move(p);path_=path;images_.clear();textMeshes_.clear();textSizes_.clear();history_.clear();dirty_=false;++revision_;emit documentChanged();emit changed();}
void AppController::newProject(const QString& name,const QString& preset,double width,double height,bool landscape,const QString& background) {
    if(loading_)return;
    PageSize size=preset=="A4"?PageSize::a4():preset=="Carta"?PageSize::letter():PageSize{width,height};
    if(!size.valid()){status_="Use dimensões entre 10 e 5000 mm.";emit changed();return;}
    if(landscape&&size.widthMm<size.heightMm)std::swap(size.widthMm,size.heightMm);
    if(!landscape&&size.widthMm>size.heightMm)std::swap(size.widthMm,size.heightMm);
    if(!flush())return;
    Project p{newId(),(name.trimmed().isEmpty()?QString("Quadro sem título"):name.trimmed()).toStdString(),now().toStdString(),now().toStdString(),{}};
    p.pages.push_back({newId(),size,background=="Preto"?0x18221effu:background=="Verde"?0x214f43ffu:0xffffffffu,{}});
    for(const auto& preset:backgroundPresets())if(preset.toMap().value("name").toString()==background)parseBackground(preset.toMap(),p.pages[0].background,p.pages[0].backgroundStyle);
    const QString path=dataDir_+"/projects/"+QString::fromStdString(p.id)+".board";
    install(std::move(p),path); mutate(); beginSave();
}
void AppController::newDefault(){newProject("Novo quadro",defaultSize(),210,297,defaultLandscape(),defaultBackground());}
void AppController::mutate(){dirty_=true;++revision_;project_.updatedAt=now().toStdString();autosave_.start();refreshTextTextures(textRasterScale_);emit documentChanged();emit changed();}
void AppController::addStroke(StrokeObject s){if(!active()||loading_||s.samples.empty())return;s.properties.zIndex=nextZIndex();history_.add(project_.pages[0],std::move(s));mutate();}
std::int64_t AppController::nextZIndex() const{
    std::int64_t value=0;if(page()){for(const auto& s:page()->strokes)value=std::max(value,s.properties.zIndex+1);for(const auto& s:page()->shapes)value=std::max(value,s.properties.zIndex+1);for(const auto& s:page()->images)value=std::max(value,s.properties.zIndex+1);for(const auto& s:page()->texts)value=std::max(value,s.properties.zIndex+1);}return value;
}
void AppController::addRecognizedStroke(StrokeObject stroke,ShapeObject shape){
    const auto id=shape.id;const bool line=shape.kind==ShapeKind::Line;
    if(!active()||loading_)return;stroke.properties.zIndex=nextZIndex();shape.properties.zIndex=stroke.properties.zIndex;
    history_.add(project_.pages[0],stroke);history_.apply(project_.pages[0],{{CanvasObject(stroke),CanvasObject(std::move(shape))}},CommandKind::ConvertStrokeToShape);mutate();if(line)closeLines(id);
}
void AppController::importImage(const QUrl& url,QPointF center){
    if(!active()||loading_||imageImportPending_||!url.isLocalFile())return;importProjectId_=QString::fromStdString(project_.id);const auto size=page()->size;const auto path=url.toLocalFile();
    imageImportPending_=true;imageWatcher_.setFuture(QtConcurrent::run([path,center,size]{return importImageFile(path,{center.x(),center.y()},size);}));status_="Importando imagem…";emit changed();
}
void AppController::pasteImage(QPointF center){
    if(!active()||loading_||imageImportPending_)return;
    const auto* mime=QGuiApplication::clipboard()->mimeData();
    if(mime->hasImage()){const auto image=QGuiApplication::clipboard()->image();const auto size=page()->size;importProjectId_=QString::fromStdString(project_.id);
        imageImportPending_=true;imageWatcher_.setFuture(QtConcurrent::run([image,center,size]{return encodeImage(image,{center.x(),center.y()},size);}));status_="Colando imagem…";emit changed();}
    else if(mime->hasUrls()&&!mime->urls().isEmpty())importImage(mime->urls().first(),center);
    else if(mime->hasText())upsertText({},center,{{"source",mime->text()},{"color",pageColor().lightness()<128?"#f4f4f5":"#263345"}});
    else {status_="A área de transferência não contém imagem ou texto.";emit changed();}
}
void AppController::addShape(ShapeObject shape){const auto id=shape.id;const bool line=shape.kind==ShapeKind::Line;if(!active()||loading_)return;shape.properties.zIndex=nextZIndex();history_.apply(project_.pages[0],{{{},CanvasObject(std::move(shape))}},CommandKind::AddObject);mutate();if(line)closeLines(id);}
void AppController::changeObjects(std::vector<ObjectChange> changes,CommandKind kind){if(!active()||loading_||changes.empty())return;history_.apply(project_.pages[0],std::move(changes),kind);mutate();}
bool AppController::recognitionEnabled() const{return recognitionEnabled_;}
void AppController::setRecognitionEnabled(bool value){recognitionEnabled_=value;library_->setSetting("recognitionEnabled",value);emit preferencesChanged();}
int AppController::holdDelay() const{return holdDelay_;}
void AppController::setHoldDelay(int value){holdDelay_=std::clamp(value,250,1500);library_->setSetting("holdDelay",holdDelay_);emit preferencesChanged();}
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
    if(textPending_){status_="Aguarde a conversão do texto.";emit changed();return false;}
    if(imageImportPending_){status_="Aguarde a importação da imagem.";emit changed();return false;}
    autosave_.stop();
    if(saveWatcher_.isRunning()){saveWatcher_.waitForFinished();finishSave();}
    if(active()&&dirty_){beginSave();saveWatcher_.waitForFinished();finishSave();}
    return !dirty_;
}
void AppController::open(const QUrl& url){if(url.isLocalFile())openPath(url.toLocalFile());}
void AppController::openPath(const QString& path){
    for(const auto& value:library_->trash()){const auto e=value.toMap();if(QFileInfo(e["originalPath"].toString()).absoluteFilePath()==QFileInfo(path).absoluteFilePath()||QFileInfo(e["trashPath"].toString()).absoluteFilePath()==QFileInfo(path).absoluteFilePath()){status_="Restaure o quadro na lixeira antes de abri-lo.";emit changed();return;}}
    if(trashPending_||loading_||!flush())return;loading_=true;pendingOpenPath_=path;status_="Abrindo projeto…";
    loadWatcher_.setFuture(QtConcurrent::run([path]{return ProjectStore::load(path);}));emit changed();
}
void AppController::home(){if(loading_||!flush())return;project_={};images_.clear();textMeshes_.clear();textSizes_.clear();history_.clear();++revision_;emit documentChanged();emit changed();}
void AppController::recover(){if(!recovery_)return;recovery_=false;openPath(dataDir_+"/session.board");}
void AppController::discardRecovery(){recovery_=false;QFile::remove(dataDir_+"/session.board");emit changed();}
bool AppController::shutdown(){
    if(loading_||trashPending_)return false;
    if(!flush())return false;
    QFile::remove(dataDir_+"/session.board");library_->setSetting("cleanShutdown","true");closed_=true;return true;
}
QVariantMap AppController::background() const {return backgroundValues(page()?page()->background:0xffffffff,page()?page()->backgroundStyle:BackgroundStyle{});}
QVariantList AppController::backgroundPresets() const {auto presets=builtinBackgrounds();presets.append(library_->backgroundPresets());return presets;}
bool AppController::validBackground(const QVariantMap& values) const {std::uint32_t color;BackgroundStyle style;return parseBackground(values,color,style);}
void AppController::setBackground(const QVariantMap& values){
    std::uint32_t color;BackgroundStyle style;
    if(!active()||loading_||!parseBackground(values,color,style))return;
    if(page()->background==color&&page()->backgroundStyle==style)return;
    history_.background(project_.pages[0],color,style);mutate();
}
void AppController::applyBackgroundPreset(const QString& name){for(const auto& preset:backgroundPresets())if(preset.toMap().value("name").toString()==name){setBackground(preset.toMap());return;}}
void AppController::saveBackgroundPreset(const QString& name,const QVariantMap& values){
    std::uint32_t color;BackgroundStyle style;const auto label=name.trimmed();
    if(label.isEmpty()||label.size()>80||!parseBackground(values,color,style))return;
    for(const auto& builtin:builtinBackgrounds())if(builtin.toMap().value("name").toString()==label){status_="Escolha um nome diferente dos presets padrão.";emit changed();return;}
    if(library_->saveBackgroundPreset(label,backgroundValues(color,style)))emit preferencesChanged();
}

QVariantMap AppController::textValues(const QString& id) const {
    if(page())if(auto object=findObject(*page(),id.toStdString()))if(const auto* t=std::get_if<TextObject>(&*object)){
        const auto c=t->style.rgba;return {{"source",QString::fromStdString(t->source)},{"fontFamily",QString::fromStdString(t->fontFamily)},{"fontSizePt",t->fontSizePt},{"bold",t->bold},{"italic",t->italic},{"alignment",t->alignment},{"color",QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255).name()}};
    }return {};
}
void AppController::upsertText(const QString& id,QPointF position,const QVariantMap& v){
    if(!active()||loading_||textPending_)return;TextObject t;textBefore_.reset();
    if(!id.isEmpty()){
        auto original=findObject(*page(),id.toStdString());if(!original||!std::holds_alternative<TextObject>(*original))return;
        textBefore_=original;t=std::get<TextObject>(*original);++t.properties.revision;
    }else{t.id=newId();t.properties.zIndex=nextZIndex();t.corners={{position.x(),position.y()}};}
    const auto source=v.value("source").toString();const QColor c(v.value("color","#263345").toString());
    t.source=source.toStdString();t.fontFamily=v.value("fontFamily").toString().toStdString();t.fontSizePt=v.value("fontSizePt",18).toDouble();t.bold=v.value("bold",false).toBool();t.italic=v.value("italic",false).toBool();t.alignment=v.value("alignment",0).toInt();
    if(source.trimmed().isEmpty()||t.source.size()>32768||!c.isValid()||!std::isfinite(t.fontSizePt)||t.fontSizePt<6||t.fontSizePt>144||t.alignment<0||t.alignment>2){textError_="Use texto de até 32 KiB e tamanho de 6 a 144 pt.";emit changed();return;}
    t.style.rgba=(std::uint32_t(c.red())<<24)|(std::uint32_t(c.green())<<16)|(std::uint32_t(c.blue())<<8)|255;
    textProjectId_=QString::fromStdString(project_.id);textPending_=true;textError_.clear();textWatcher_.setFuture(QtConcurrent::run([t]{return prepareText(t);}));emit changed();
}
void AppController::refreshTextTextures(double scale){
    if(!active()||page()->texts.empty()||textRasterWatcher_.isRunning())return;
    const double bucket=std::pow(2.,std::ceil(std::log2(std::clamp(scale,1.,64.))));
    if(bucket==textRasterScale_&&textRasterRevision_==revision_)return;
    textRasterScale_=bucket;textRasterRevision_=revision_;const auto texts=page()->texts;
    textRasterWatcher_.setFuture(QtConcurrent::run([texts,bucket]{QHash<QString,TextVisual> result;for(const auto& t:texts)result.insert(QString::fromStdString(t.id),textVisual(t,bucket));return result;}));
}

void AppController::trashProject(const QString& id){
    if(active()||busy())return;auto entry=library_->project(id);if(entry.isEmpty())return;
    entry["trashPath"]=dataDir_+"/trash/"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".board";entry["deletedAt"]=now();
    if(!library_->recordTrash(entry)){status_="Não foi possível registrar o quadro na lixeira.";emit changed();return;}
    trashPending_=true;trashWatcher_.setFuture(QtConcurrent::run([entry]{return moveProjectToTrash(entry);}));emit changed();emit recentChanged();
}
void AppController::restoreProject(const QString& id){
    if(active()||busy())return;for(const auto& value:library_->trash()){const auto e=value.toMap();if(e["id"].toString()==id){trashPending_=true;trashWatcher_.setFuture(QtConcurrent::run([e]{return restoreTrashedProject(e);}));emit changed();return;}}
}
void AppController::deleteProjectPermanently(const QString& id){
    if(active()||busy())return;for(const auto& value:library_->trash()){const auto e=value.toMap();if(e["id"].toString()==id){trashPending_=true;trashWatcher_.setFuture(QtConcurrent::run([e]{return deleteTrashedProject(e);}));emit changed();return;}}
}
void AppController::maintainTrashedProjects(){
    if(busy()||library_->trash().isEmpty())return;const auto entries=library_->trash();trashPending_=true;trashWatcher_.setFuture(QtConcurrent::run([entries]{return maintainTrash(entries);}));emit changed();
}

void AppController::closeLines(const std::string& newest){
    latestLineId_=newest;if(!active()||closedLinesWatcher_.isRunning())return;
    Page snapshot;for(const auto& s:page()->shapes)if(s.kind==ShapeKind::Line)snapshot.shapes.push_back(s);
    if(snapshot.shapes.size()<3)return;closeLinesRevision_=revision_;
    closedLinesWatcher_.setFuture(QtConcurrent::run([snapshot,newest]{return closeConnectedLines(snapshot,newest);}));
}

}
