#include "AppController.h"
#include "tools/RegionFill.h"
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
#include <QFontDatabase>
#include <QFont>
#include <QFontInfo>
#include "rendering/TextFormatting.h"
#include <numbers>
Q_LOGGING_CATEGORY(appLog,"scalar.app")
namespace scalar {
namespace { QString now(){return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);} }
AppController::AppController(QObject* parent,const QString& dataDirectory):QObject(parent) {
    registerTextFonts();
    dataDir_=dataDirectory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation):dataDirectory;
    QDir().mkpath(dataDir_+"/projects"); library_=std::make_unique<Library>(dataDir_+"/library.sqlite");
    recognitionEnabled_=library_->setting("recognitionEnabled",true).toBool();holdDelay_=std::clamp(library_->setting("holdDelay",1000).toInt(),250,1500);
    qGuiApp->installEventFilter(this);
    connect(&exportWatcher_,&QFutureWatcher<QString>::finished,this,[this]{exportPending_=false;const auto error=exportWatcher_.result();status_=error.isEmpty()?"Arquivo exportado":error;emit changed();});
    thumbnailsTimer_.setSingleShot(true);thumbnailsTimer_.setInterval(700);
    connect(&thumbnailsTimer_,&QTimer::timeout,this,&AppController::beginThumbnails);
    connect(&thumbnailsWatcher_,&QFutureWatcher<QHash<QString,QString>>::finished,this,[this]{
        thumbnailsPending_=false;
        if(thumbnailsRevision_!=revision_){scheduleThumbnails();return;}
        const auto result=thumbnailsWatcher_.result();for(auto i=result.begin();i!=result.end();++i)thumbnails_.insert(i.key(),i.value());
        for(const auto& id:renderingThumbnails_)dirtyThumbnails_.remove(id);emit pagesChanged();if(!dirtyThumbnails_.isEmpty())scheduleThumbnails();
    });
    connect(&pdfCache_,&PdfRenderCache::imageChanged,this,[this]{emit documentChanged();});
    connect(&pdfCache_,&PdfRenderCache::failed,this,[this](const QString& error){status_=error;emit changed();});
    connect(&pdfWatcher_,&QFutureWatcher<PdfInfo>::finished,this,[this]{pdfPending_=false;pdfInfo_=pdfWatcher_.result();status_=pdfInfo_.error.isEmpty()?"Escolha as páginas para importar":pdfInfo_.error;emit changed();});
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
    connect(qGuiApp->styleHints(),&QStyleHints::colorSchemeChanged,this,[this]{emit preferencesChanged();});
#endif
    connect(&imageWatcher_,&QFutureWatcher<std::vector<ImportedImage>>::finished,this,[this]{
        imageImportPending_=false;const auto imported=imageWatcher_.result();if(importProjectId_!=QString::fromStdString(project_.id)||!page()||importPageId_!=QString::fromStdString(page()->id)){emit changed();return;}
        std::vector<ObjectChange> changes;QStringList errors;auto z=nextZIndex();
        for(const auto& item:imported){if(!item.error.isEmpty()){errors.append(item.error);continue;}
            auto object=item.object;object.properties.zIndex=z++;images_.insert(QString::fromStdString(object.id),item.image);changes.push_back({{},CanvasObject(std::move(object))});}
        if(!changes.empty()){history().apply(current(),std::move(changes),CommandKind::AddObject);mutate();}
        if(!errors.isEmpty()){status_=errors.join(" · ");emit changed();}else if(imported.empty())emit changed();
    });
    connect(&fillWatcher_,&QFutureWatcher<ImportedImage>::finished,this,[this]{
        fillPending_=false;
        if(fillRevision_!=revision_||!page()||page()->id!=fillPageId_){emit changed();return;}
        auto result=fillWatcher_.result();
        if(!result.error.isEmpty()){status_=result.error;emit changed();return;}
        result.object.properties.zIndex=nextZIndex();images_.insert(QString::fromStdString(result.object.id),result.image);
        history().apply(current(),{{{},CanvasObject(result.object)}},CommandKind::AddObject);mutate();
        status_="Região preenchida · Ctrl+Z para desfazer";emit changed();
    });
    connect(&closedLinesWatcher_,&QFutureWatcher<std::optional<ClosedLines>>::finished,this,[this]{
        if(closeLinesRevision_!=revision_){if(active())closeLines(latestLineId_);return;}
        const auto result=closedLinesWatcher_.result();if(!result||!active())return;std::vector<ObjectChange> changes;
        for(const auto& id:result->lineIds){const auto before=findObject(*page(),id);if(!before||!std::holds_alternative<ShapeObject>(*before))return;changes.push_back({before,{}});}
        changes.push_back({{},CanvasObject(result->polygon)});changeObjects(std::move(changes),CommandKind::ConvertStrokeToShape);
    });
    connect(&trashWatcher_,&QFutureWatcher<TrashResult>::finished,this,[this]{
        const auto r=trashWatcher_.result();bool success=r.error.isEmpty();
        // Keep a folder's durable manifest after a partial move; maintenance
        // resumes it and restoration can recover files from either location.
        if(r.action==TrashAction::Move&&!success&&!r.entry.value("isFolder").toBool())library_->forgetTrash(r.entry.value("id").toString(),false);
        if(r.action==TrashAction::Restore&&success)success=library_->forgetTrash(r.entry.value("id").toString(),false);
        if(r.action==TrashAction::Delete&&success)success=library_->forgetTrash(r.entry.value("id").toString(),true);
        for(const auto& entry:r.expired)if(!library_->forgetTrash(entry.toMap().value("id").toString(),true))success=false;
        const bool folder=r.entry.value("isFolder").toBool();
        status_=!success?(!r.error.isEmpty()?r.error:QString("Não foi possível atualizar a lixeira.")):r.action==TrashAction::Move?(folder?"Pasta movida para a lixeira por 30 dias":"Quadro movido para a lixeira por 30 dias"):r.action==TrashAction::Restore?(folder?"Pasta restaurada":"Quadro restaurado"):r.action==TrashAction::Delete?(folder?"Pasta excluída permanentemente":"Quadro excluído permanentemente"):"Pronto para suas ideias";
        trashPending_=false;emit recentChanged();emit changed();
    });
    trashExpiry_.setInterval(60*60*1000);connect(&trashExpiry_,&QTimer::timeout,this,&AppController::maintainTrashedProjects);trashExpiry_.start();
    QTimer::singleShot(0,this,&AppController::maintainTrashedProjects);
    connect(&textWatcher_,&QFutureWatcher<PreparedText>::finished,this,[this]{
        textPending_=false;const auto prepared=textWatcher_.result();textError_=prepared.error;
        if(textProjectId_!=QString::fromStdString(project_.id)||!page()||textPageId_!=QString::fromStdString(page()->id)){emit changed();return;}
        if(!prepared.error.isEmpty()){status_=prepared.error;emit changed();return;}
        auto object=prepared.object;images_.insert(QString::fromStdString(object.id),prepared.image);textMeshes_.insert(QString::fromStdString(object.id),prepared.geometry);textSizes_.insert(QString::fromStdString(object.id),prepared.naturalSize);
        history().apply(current(),{{textBefore_,CanvasObject(object)}},textBefore_?CommandKind::ChangeStyle:CommandKind::AddObject);textRasterRevision_=0;mutate();emit textCommitted(QString::fromStdString(object.id));
    });
    connect(&textRasterWatcher_,&QFutureWatcher<QHash<QString,TextVisual>>::finished,this,[this]{
        textRasterPending_=false;
        if(textRasterRevision_!=revision_){textRasterRevision_=0;refreshTextTextures(textRasterRequestedScale_);return;}
        const auto results=textRasterWatcher_.result();for(auto i=results.begin();i!=results.end();++i){images_.insert(i.key(),i.value().text);textMeshes_.insert(i.key(),i.value().math);textSizes_.insert(i.key(),i.value().naturalSize);if(!i.value().error.isEmpty())status_=i.value().error;}
        if(active())for(auto& object:current().texts)++object.properties.revision;
        emit documentChanged();
        refreshTextTextures(textRasterRequestedScale_);
    });
    recovery_=library_->setting("cleanShutdown","true").toString()=="false"&&QFile::exists(dataDir_+"/session.board");
    library_->setSetting("cleanShutdown","false");
    status_=library_->error().isEmpty()?"Pronto para suas ideias":library_->error();
    autosave_.setSingleShot(true); autosave_.setInterval(1600);
    connect(&autosave_,&QTimer::timeout,this,&AppController::beginSave);
    connect(&saveWatcher_,&QFutureWatcher<SaveResult>::finished,this,&AppController::finishSave);
    connect(&renameWatcher_,&QFutureWatcher<SaveResult>::finished,this,&AppController::finishRename);
    connect(&loadWatcher_,&QFutureWatcher<LoadResult>::finished,this,[this]{
        loading_=false; const auto result=loadWatcher_.result();
        if(result){
            const bool recovered=pendingOpenPath_==dataDir_+"/session.board";
            const auto target=recovered ? dataDir_+"/projects/"+QString::fromStdString(result.project.id)+".board" : pendingOpenPath_;
            install(result.project,target);images_=result.images;refreshTextTextures(textRasterRequestedScale_);emit documentChanged();library_->remember(QString::fromStdString(result.project.id),QString::fromStdString(result.project.name),target,QString::fromStdString(result.project.updatedAt));emit recentChanged();status_=recovered?"Quadro recuperado":"Projeto aberto";
            if(recovered||result.compacted){mutate();beginSave();}
        } else {status_=result.error;qCWarning(appLog)<<status_;}
        emit changed();
    });
}
bool AppController::eventFilter(QObject* watched,QEvent* event){
    if(watched==qGuiApp && event->type()==QEvent::ApplicationPaletteChange)emit preferencesChanged();
    return QObject::eventFilter(watched,event);
}
AppController::~AppController(){ if(!closed_)flush(); loadWatcher_.waitForFinished();imageWatcher_.waitForFinished();textWatcher_.waitForFinished();textRasterWatcher_.waitForFinished();trashWatcher_.waitForFinished();closedLinesWatcher_.waitForFinished();pdfWatcher_.waitForFinished();thumbnailsWatcher_.waitForFinished();exportWatcher_.waitForFinished(); }
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
bool AppController::disableToolbarBlur() const {return library_->setting("disableToolbarBlur",false).toBool();}
void AppController::setDisableToolbarBlur(bool value){if(value==disableToolbarBlur())return;library_->setSetting("disableToolbarBlur",value);emit preferencesChanged();}
QString AppController::defaultSize() const{return library_->setting("defaultSize","A4").toString();}
void AppController::setDefaultSize(const QString& v){if(v!="A4"&&v!="Carta")return;library_->setSetting("defaultSize",v);emit preferencesChanged();}
bool AppController::defaultLandscape() const{return library_->setting("defaultLandscape",false).toBool();}
void AppController::setDefaultLandscape(bool v){library_->setSetting("defaultLandscape",v);emit preferencesChanged();}
QString AppController::defaultBackground() const{const auto value=library_->setting("defaultBackground","Sem grade").toString();return value=="Branco"||value=="Preto"||value=="Verde"?"Sem grade":value;}
void AppController::setDefaultBackground(const QString& v){bool found=false;for(const auto& preset:gridPresets())if(preset.toMap().value("name").toString()==v)found=true;if(!found)return;library_->setSetting("defaultBackground",v);emit preferencesChanged();}
void AppController::install(Project p,const QString& path){project_=std::move(p);currentPage_=0;path_=path;images_.clear();textMeshes_.clear();textSizes_.clear();histories_.clear();thumbnails_.clear();dirtyThumbnails_.clear();for(const auto& page:project_.pages)dirtyThumbnails_.insert(QString::fromStdString(page.id));pdfCache_.clear();dirty_=false;++revision_;textRasterRevision_=0;scheduleThumbnails();refreshPdf(textRasterScale_);emit pagesChanged();emit pageChanged();emit documentChanged();emit changed();}
void AppController::newProject(const QString& name,const QString& preset,double width,double height,bool landscape,const QString& background,const QString& pageColor) {
    if(loading_)return;
    PageSize size=preset=="A4"?PageSize::a4():preset=="Carta"?PageSize::letter():PageSize{width,height};
    if(preset=="Infinito"){size=PageSize::a4();size.infinite=true;}
    if(!size.valid()){status_="Use dimensões entre 10 e 5000 mm.";emit changed();return;}
    if(landscape&&size.widthMm<size.heightMm)std::swap(size.widthMm,size.heightMm);
    if(!landscape&&size.widthMm>size.heightMm)std::swap(size.widthMm,size.heightMm);
    if(!flush())return;
    Project p{newId(),(name.trimmed().isEmpty()?QString("Quadro sem título"):name.trimmed()).toStdString(),now().toStdString(),now().toStdString(),{}};
    const bool dark=theme()=="Dark"||(theme()=="System"&&systemDark());
    const QColor chosen=pageColor.isEmpty()?QColor(dark?"#000000":"#ffffff"):QColor(pageColor);
    if(!chosen.isValid()){status_="Cor de página inválida.";emit changed();return;}
    const auto rgba=(std::uint32_t(chosen.red())<<24)|(std::uint32_t(chosen.green())<<16)|(std::uint32_t(chosen.blue())<<8)|255;
    p.pages.push_back({newId(),size,rgba,{}});
    for(const auto& preset:backgroundPresets())if(preset.toMap().value("name").toString()==background){std::uint32_t ignored;parseBackground(preset.toMap(),ignored,p.pages[0].backgroundStyle);}
    const QString path=dataDir_+"/projects/"+QString::fromStdString(p.id)+".board";
    install(std::move(p),path); mutate(); beginSave();
}
void AppController::newDefault(){newProject("Novo quadro",defaultSize(),210,297,defaultLandscape(),defaultBackground());}
bool AppController::renameProject(const QString& id,const QString& name){
    const auto label=name.trimmed();
    if(label.isEmpty()||label.size()>120||renamePending_||loading_)return false;
    if(active()&&id==QString::fromStdString(project_.id)){
        if(projectName()!=label){project_.name=label.toStdString();mutate();}
        return true;
    }
    if(busy())return false;
    const auto entry=library_->project(id);if(entry.isEmpty())return false;
    const auto path=entry.value("originalPath").toString(),updated=now();
    renamePending_=true;status_="Salvando nome do quadro…";emit changed();
    renameWatcher_.setFuture(QtConcurrent::run([id,label,path,updated]{
        auto loaded=ProjectStore::load(path);
        QString error=loaded.error;
        if(loaded){
            if(QString::fromStdString(loaded.project.id)!=id)error="O arquivo não corresponde ao quadro.";
            else {loaded.project.name=label.toStdString();loaded.project.updatedAt=updated.toStdString();error=ProjectStore::save(path,loaded.project);}
        }
        return SaveResult{error,id,label,path,updated,0};
    }));
    return true;
}
void AppController::finishRename(){
    if(!renamePending_||renameWatcher_.isRunning())return;
    const auto result=renameWatcher_.result();renamePending_=false;
    if(result.error.isEmpty()){library_->remember(result.id,result.name,result.path,result.updated);emit recentChanged();status_="Nome do quadro atualizado";}
    else status_="Não foi possível renomear: "+result.error;
    emit changed();
}
QVariantList AppController::searchProjects(const QString& query,const QString& folderId,const QString& scope) const{
    const auto normalize=[](const QString& input){
        QString result;
        for(const auto character:input.normalized(QString::NormalizationForm_D).toCaseFolded())
            if(character.category()!=QChar::Mark_NonSpacing&&character.category()!=QChar::Mark_SpacingCombining)result+=character;
        return result;
    };
    const auto term=normalize(query.trimmed());QVariantList result;
    for(const auto& value:library_->recent()){
        const auto entry=value.toMap();
        if(term.isEmpty()&&scope=="folder"&&entry.value("folderId").toString()!=folderId)continue;
        if(!normalize(entry.value("name").toString()).contains(term))continue;
        result.append(value);
        if(term.isEmpty()&&scope=="recent"&&result.size()==12)break;
    }
    return result;
}
void AppController::newDefaultInFolder(const QString& folderId){
    if(folderId.isEmpty()) { newDefault(); return; }
    bool exists=false;
    for(const auto& entry:folders())if(entry.toMap().value("id").toString()==folderId){exists=true;break;}
    if(!exists){status_="A pasta não está mais disponível.";emit changed();return;}
    pendingFolderId_=folderId;
    newDefault();
}
void AppController::mutate(){dirty_=true;++revision_;project_.updatedAt=now().toStdString();autosave_.start();refreshTextTextures(textRasterRequestedScale_);if(page())dirtyThumbnails_.insert(QString::fromStdString(page()->id));scheduleThumbnails();emit documentChanged();emit changed();}
void AppController::addStroke(StrokeObject s){if(!active()||loading_||s.samples.empty())return;s.properties.zIndex=nextZIndex();history().add(current(),std::move(s));mutate();}
void AppController::fillRegion(Point seed,QColor color,double opacity){
    if(!active()||documentBusy())return;
    fillPending_=true;fillRevision_=revision_;fillPageId_=current().id;status_="Identificando região fechada…";emit changed();
    const auto snapshot=current();fillWatcher_.setFuture(QtConcurrent::run([snapshot,seed,color,opacity]{return fillInkRegion(snapshot,seed,color,2.,opacity);}));
}
std::int64_t AppController::nextZIndex() const{
    std::int64_t value=0;if(page()){for(const auto& s:page()->strokes)value=std::max(value,s.properties.zIndex+1);for(const auto& s:page()->shapes)value=std::max(value,s.properties.zIndex+1);for(const auto& s:page()->images)value=std::max(value,s.properties.zIndex+1);for(const auto& s:page()->texts)value=std::max(value,s.properties.zIndex+1);}return value;
}
void AppController::addRecognizedStroke(StrokeObject stroke,ShapeObject shape){
    const auto id=shape.id;const bool line=shape.kind==ShapeKind::Line;
    if(!active()||loading_)return;stroke.properties.zIndex=nextZIndex();shape.properties.zIndex=stroke.properties.zIndex;
    shape=alignCircularSector(current(),std::move(shape));
    history().add(current(),stroke);history().apply(current(),{{CanvasObject(stroke),CanvasObject(std::move(shape))}},CommandKind::ConvertStrokeToShape);mutate();if(line)closeLines(id);
}
void AppController::importImage(const QUrl& url,QPointF center){
    importImages({url},center);
}
void AppController::importImages(const QVariantList& urls,QPointF center){
    if(!active()||loading_||imageImportPending_||urls.isEmpty())return;QStringList paths;
    for(const auto& value:urls){const auto url=value.toUrl();if(url.isLocalFile())paths.append(url.toLocalFile());}
    if(paths.isEmpty())return;if(paths.size()>32){status_="Importe até 32 imagens por vez.";emit changed();return;}
    importProjectId_=QString::fromStdString(project_.id);importPageId_=QString::fromStdString(page()->id);const auto size=page()->size;
    imageImportPending_=true;imageWatcher_.setFuture(QtConcurrent::run([paths,center,size]{std::vector<ImportedImage> result;qint64 memory=0;int offset=0;
        for(const auto& path:paths){auto image=importImageFile(path,{center.x()+offset,center.y()+offset},size);memory+=image.image.sizeInBytes();
            if(memory>256*1024*1024){result.push_back({{},{},"O lote excede 256 MiB de imagens decodificadas."});break;}
            if(image.error.isEmpty())offset+=3;result.push_back(std::move(image));}return result;
    }));status_="Importando imagens…";emit changed();
}
void AppController::pasteImage(QPointF center){
    if(!active()||loading_||imageImportPending_)return;
    const auto* mime=QGuiApplication::clipboard()->mimeData();
    if(mime->hasImage()){const auto image=QGuiApplication::clipboard()->image();const auto size=page()->size;importProjectId_=QString::fromStdString(project_.id);importPageId_=QString::fromStdString(page()->id);
        imageImportPending_=true;imageWatcher_.setFuture(QtConcurrent::run([image,center,size]{return std::vector<ImportedImage>{encodeImage(image,{center.x(),center.y()},size)};}));status_="Colando imagem…";emit changed();}
    else if(mime->hasUrls()&&!mime->urls().isEmpty()){const auto url=mime->urls().first();if(QFileInfo(url.toLocalFile()).suffix().compare("pdf",Qt::CaseInsensitive)==0)inspectPdfFile(url);else {QVariantList urls;for(const auto& u:mime->urls())urls.append(u);importImages(urls,center);}}
    else if(mime->hasText())upsertText({},center,{{"source",mime->text()},{"color",pageColor().lightness()<128?"#f4f4f5":"#263345"}});
    else {status_="A área de transferência não contém imagem ou texto.";emit changed();}
}
void AppController::addShape(ShapeObject shape){const auto id=shape.id;const bool line=shape.kind==ShapeKind::Line;if(!active()||loading_)return;shape.properties.zIndex=nextZIndex();history().apply(current(),{{{},CanvasObject(std::move(shape))}},CommandKind::AddObject);mutate();if(line)closeLines(id);}
void AppController::changeObjects(std::vector<ObjectChange> changes,CommandKind kind){if(!active()||loading_||changes.empty())return;history().apply(current(),std::move(changes),kind);mutate();}
void AppController::eraseObjects(std::vector<ObjectChange> changes,std::vector<StrokeObject> recoverable){if(!active()||loading_||changes.empty())return;history().erase(current(),std::move(changes),std::move(recoverable));mutate();}
bool AppController::recognitionEnabled() const{return recognitionEnabled_;}
void AppController::setRecognitionEnabled(bool value){recognitionEnabled_=value;library_->setSetting("recognitionEnabled",value);emit preferencesChanged();}
int AppController::holdDelay() const{return holdDelay_;}
void AppController::setHoldDelay(int value){holdDelay_=std::clamp(value,250,1500);library_->setSetting("holdDelay",holdDelay_);emit preferencesChanged();}
void AppController::undo(){if(active()){const auto size=current().size;if(history().undo(current())){mutate();if(size.widthMm!=pageWidth()||size.heightMm!=pageHeight()||size.infinite!=pageInfinite())emit pagesChanged();}}}
void AppController::redo(){if(active()){const auto size=current().size;if(history().redo(current())){mutate();if(size.widthMm!=pageWidth()||size.heightMm!=pageHeight()||size.infinite!=pageInfinite())emit pagesChanged();}}}
void AppController::save(){if(active()){dirty_=true;beginSave();}}
void AppController::saveAs(const QUrl& url){if(!active()||!url.isLocalFile()||!flush())return;path_=url.toLocalFile();if(!path_.endsWith(".board",Qt::CaseInsensitive))path_+=".board";dirty_=true;beginSave();}
void AppController::beginSave(){
    if(!active()||!dirty_||loading_)return;
    if(saveWatcher_.isRunning()){autosave_.start();return;}
    const auto snapshot=project_;const auto path=path_;const auto recovery=dataDir_+"/session.board";const auto rev=revision_;
    status_="Salvando…";
    saveWatcher_.setFuture(QtConcurrent::run([snapshot=Project(snapshot),path,recovery,rev]() mutable {
        auto error=ProjectStore::discardErasureHistory(snapshot);
        if(error.isEmpty())error=ProjectStore::save(recovery,snapshot);
        if(error.isEmpty())error=ProjectStore::save(path,snapshot);
        if(error.isEmpty())saveThumbnail(snapshot,path+".png");
        return SaveResult{error,QString::fromStdString(snapshot.id),QString::fromStdString(snapshot.name),path,QString::fromStdString(snapshot.updatedAt),rev};
    }));emit changed();
}
void AppController::finishSave(){
    if(!saveWatcher_.future().isValid()||saveWatcher_.isRunning())return;
    const auto r=saveWatcher_.result();
    if(r.error.isEmpty()) {
        library_->remember(r.id,r.name,r.path,r.updated);
        if(r.id==QString::fromStdString(project_.id) && !pendingFolderId_.isEmpty()){
            library_->moveToFolder(r.id,pendingFolderId_);
            pendingFolderId_.clear();
        }
        emit recentChanged();
        if(r.revision==revision_ && r.id==QString::fromStdString(project_.id)){dirty_=false;status_="Salvo neste dispositivo";} else {status_="Alterações pendentes";autosave_.start();}
    } else {status_="Falha ao salvar: "+r.error;qCWarning(appLog)<<status_;}
    emit changed();
}
bool AppController::flush(){
    if(renamePending_){renameWatcher_.waitForFinished();finishRename();}
    if(pdfPending_){status_="Aguarde a leitura do PDF.";emit changed();return false;}
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
void AppController::home(){if(loading_||!flush())return;project_={};currentPage_=0;images_.clear();textMeshes_.clear();textSizes_.clear();histories_.clear();thumbnails_.clear();pdfCache_.clear();thumbnailsTimer_.stop();++revision_;emit pagesChanged();emit pageChanged();emit documentChanged();emit changed();}
void AppController::recover(){if(!recovery_)return;recovery_=false;openPath(dataDir_+"/session.board");}
void AppController::discardRecovery(){recovery_=false;QFile::remove(dataDir_+"/session.board");emit changed();}
bool AppController::shutdown(){
    if(loading_||trashPending_||fillPending_)return false;
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
    history().background(current(),color,style);mutate();
}
void AppController::applyBackgroundPreset(const QString& name){for(const auto& preset:gridPresets())if(preset.toMap().value("name").toString()==name){auto value=background();for(const auto* key:{"gridType","spacingX","spacingY"})value[key]=preset.toMap().value(key);setBackground(value);return;}}
void AppController::saveBackgroundPreset(const QString& name,const QVariantMap& values){
    std::uint32_t color;BackgroundStyle style;const auto label=name.trimmed();
    if(label.isEmpty()||label.size()>80||!parseBackground(values,color,style))return;
    for(const auto& builtin:builtinBackgrounds())if(builtin.toMap().value("name").toString()==label){status_="Escolha um nome diferente dos presets padrão.";emit changed();return;}
    if(library_->saveBackgroundPreset(label,backgroundValues(color,style)))emit preferencesChanged();
}

QStringList AppController::fontFamilies() const {return QFontDatabase::families();}
QString AppController::defaultTextFont() const {return QStringLiteral("Lobster Two");}
QFont AppController::textFont(const QString& family,int pixelSize,bool bold,bool italic) const {return documentFont(family,pixelSize,bold,italic);}
QVariantMap AppController::textEmphasis(QObject* document,int start,int end) const {return scalar::textEmphasis(document,start,end);}
void AppController::formatTextSelection(QObject* document,int start,int end,bool bold,bool enabled){scalar::formatTextSelection(document,start,end,bold,enabled);}
QVariantList AppController::textFormats(QObject* document,bool bold,bool italic) const {return scalar::textFormats(document,bold,italic);}
void AppController::restoreTextFormats(QObject* document,const QVariantList& formats){scalar::restoreTextFormats(document,formats);}
QVariantMap AppController::textValues(const QString& id) const {
    if(page())if(auto object=findObject(*page(),id.toStdString()))if(const auto* t=std::get_if<TextObject>(&*object)){
        const auto c=t->style.rgba;const auto origin=t->corners[0],edge=t->corners[1]-origin;
        QVariantList formats;for(const auto& f:t->formats)formats.append(QVariantMap{{"start",int(f.start)},{"length",int(f.length)},{"bold",f.bold},{"italic",f.italic}});
        return {{"source",QString::fromStdString(t->source)},{"fontFamily",QString::fromStdString(t->fontFamily)},{"fontSizePt",t->fontSizePt},{"bold",t->bold},{"italic",t->italic},{"alignment",t->alignment},{"color",QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255).name()},
            {"formats",formats},{"x",origin.x},{"y",origin.y},{"width",length(edge)},{"height",length(t->corners[3]-origin)},{"rotation",std::atan2(edge.y,edge.x)*180/std::numbers::pi},{"boxWidthMm",t->boxWidthMm},{"boxHeightMm",t->boxHeightMm}};
    }return {};
}
void AppController::upsertText(const QString& id,QPointF position,const QVariantMap& v){
    if(!active()||loading_||textPending_)return;TextObject t;textBefore_.reset();
    if(!id.isEmpty()){
        auto original=findObject(*page(),id.toStdString());if(!original||!std::holds_alternative<TextObject>(*original))return;
        textBefore_=original;t=std::get<TextObject>(*original);++t.properties.revision;
    }else{t.id=newId();t.properties.zIndex=nextZIndex();t.corners={{position.x(),position.y()}};}
    const auto source=v.value("source").toString();const QColor c(v.value("color","#263345").toString());
    t.source=source.toStdString();t.fontFamily=v.value("fontFamily",defaultTextFont()).toString().toStdString();t.fontSizePt=v.value("fontSizePt",18).toDouble();t.bold=v.value("bold",false).toBool();t.italic=v.value("italic",false).toBool();t.alignment=v.value("alignment",0).toInt();
    t.boxWidthMm=v.value("boxWidthMm",t.boxWidthMm).toDouble();t.boxHeightMm=v.value("boxHeightMm",t.boxHeightMm).toDouble();
    if(t.fontFamily.empty())t.fontFamily=defaultTextFont().toStdString();
    if(!std::isfinite(t.boxWidthMm)||!std::isfinite(t.boxHeightMm)||t.boxWidthMm<0||t.boxWidthMm>10000||t.boxHeightMm<0||t.boxHeightMm>10000){textError_="Dimensões da caixa de texto inválidas.";emit changed();return;}
    if(source.trimmed().isEmpty()||t.source.size()>32768||!c.isValid()||!std::isfinite(t.fontSizePt)||t.fontSizePt<6||t.fontSizePt>144||t.alignment<0||t.alignment>2){textError_="Use texto de até 32 KiB e tamanho de 6 a 144 pt.";emit changed();return;}
    if(v.contains("formats")){
        t.formats.clear();int previous=0;
        for(const auto& value:v["formats"].toList()){const auto f=value.toMap();const int start=f["start"].toInt(),length=f["length"].toInt();
            if(start<previous||length<=0||start>source.size()||length>source.size()-start){textError_="Formatação de texto inválida.";emit changed();return;}
            t.formats.push_back({std::size_t(start),std::size_t(length),f["bold"].toBool(),f["italic"].toBool()});previous=start+length;
        }
    }
    t.style.rgba=(std::uint32_t(c.red())<<24)|(std::uint32_t(c.green())<<16)|(std::uint32_t(c.blue())<<8)|255;
    textProjectId_=QString::fromStdString(project_.id);textPageId_=QString::fromStdString(page()->id);textPending_=true;textError_.clear();textWatcher_.setFuture(QtConcurrent::run([t]{return prepareText(t);}));emit changed();
}
void AppController::refreshTextTextures(double scale){
    if(std::isfinite(scale)&&scale>0)textRasterRequestedScale_=scale;
    if(!active()||page()->texts.empty()||textRasterPending_)return;
    const double bucket=std::pow(2.,std::ceil(std::log2(std::clamp(textRasterRequestedScale_*1.5,1.,64.))));
    if(bucket==textRasterScale_&&textRasterRevision_==revision_)return;
    textRasterScale_=bucket;textRasterRevision_=revision_;textRasterPending_=true;const auto texts=page()->texts;
    textRasterWatcher_.setFuture(QtConcurrent::run([texts,bucket]{QHash<QString,TextVisual> result;for(const auto& t:texts){
        const auto natural=textNaturalSize(t);double gain=1;
        if(t.corners.size()==4)gain=std::max({1.,length(t.corners[1]-t.corners[0])/natural.width(),length(t.corners[3]-t.corners[0])/natural.height()});
        result.insert(QString::fromStdString(t.id),textVisual(t,bucket*gain));
    }return result;}));
}

bool AppController::deleteFolder(const QString& id){
    if(active()||busy()||!library_->deleteFolder(id,dataDir_+"/trash"))return false;
    for(const auto& value:library_->trash()){
        const auto entry=value.toMap();if(entry.value("isFolder").toBool()&&entry.value("id").toString()==id){
            trashPending_=true;trashWatcher_.setFuture(QtConcurrent::run([entry]{return moveProjectToTrash(entry);}));emit changed();emit recentChanged();return true;
        }
    }
    return false;
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
