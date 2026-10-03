#include "AppController.h"
#include "persistence/Thumbnail.h"
#include "documents/Backgrounds.h"
#include "export/PdfExporter.h"
#include <QtConcurrent>
#include <QBuffer>
#include <QDateTime>
namespace scalar {
void AppController::exportPdf(const QUrl& url){
    if(!active()||loading_||imageImportPending_||textPending_||pdfPending_||exportPending_||!url.isLocalFile())return;
    auto path=url.toLocalFile();if(!path.endsWith(".pdf",Qt::CaseInsensitive))path+=".pdf";
    const auto snapshot=project_;exportPending_=true;status_="Exportando todas as páginas…";emit changed();
    exportWatcher_.setFuture(QtConcurrent::run([snapshot,path]{return exportProjectPdf(snapshot,path);}));
}
QVariantList AppController::gridPresets() const {
    QVariantList result;auto none=backgroundValues(0xffffffff,BackgroundStyle{});none["name"]="Sem grade";none.remove("color");result.append(none);
    for(const auto& value:backgroundPresets()){auto preset=value.toMap();const auto name=preset["name"].toString();if(name=="Branco"||name=="Preto"||name=="Verde")continue;preset.remove("color");result.append(preset);}return result;
}
QVariantList AppController::pages() const {
    QVariantList result;
    for(int i=0;i<pageCount();++i){const auto& p=project_.pages[i];const auto id=QString::fromStdString(p.id);
        result.append(QVariantMap{{"id",id},{"index",i},{"widthMm",p.size.widthMm},{"heightMm",p.size.heightMm},{"pdf",bool(p.pdf)},{"thumbnail",thumbnails_.value(id)}});
    }return result;
}
void AppController::selectPage(int index){
    if(index<0||index>=pageCount()||index==currentPage_||loading_||imageImportPending_||textPending_||pdfPending_)return;
    currentPage_=index;++revision_;textRasterRevision_=0;latestLineId_.clear();
    refreshTextTextures(textRasterScale_);refreshPdf(textRasterScale_);emit pageChanged();emit documentChanged();emit changed();
}
void AppController::addPage(){
    if(!page()||loading_||imageImportPending_||textPending_||pdfPending_||pageCount()>=1000)return;
    Page blank;blank.id=newId();blank.size=page()->size;blank.background=page()->background;blank.backgroundStyle=page()->backgroundStyle;
    project_.pages.push_back(std::move(blank));selectPage(pageCount()-1);mutate();emit pagesChanged();
}
void AppController::duplicatePage(){
    if(!page()||loading_||imageImportPending_||textPending_||pdfPending_||pageCount()>=1000)return;
    auto copy=*page();copy.id=newId();
    for(const auto& before:objects(copy)){
        auto after=before;const auto id=newId();if(std::holds_alternative<ImageObject>(after)||std::holds_alternative<TextObject>(after))aliasImage(objectId(before),id);
        std::visit([&](auto& object){object.id=id;object.properties.revision=0;},after);replaceObject(copy,objectId(before),after);
    }
    project_.pages.push_back(std::move(copy));selectPage(pageCount()-1);mutate();emit pagesChanged();
}
void AppController::inspectPdfFile(const QUrl& url){
    if(loading_||imageImportPending_||textPending_||pdfPending_||!url.isLocalFile())return;
    pdfInfo_={};pdfPending_=true;const auto path=url.toLocalFile();
    pdfWatcher_.setFuture(QtConcurrent::run([path]{return inspectPdf(path);}));status_="Lendo PDF…";emit changed();emit pdfImportRequested();
}
void AppController::cancelPdfImport(){if(pdfPending_)return;pdfInfo_={};emit changed();}
void AppController::importPdfPages(const QString& range){
    if(pdfPending_||loading_||pdfInfo_.sizes.empty())return;QString error;const auto selected=pdfPageRange(range,pdfPageCount(),error);
    if(!error.isEmpty()){pdfInfo_.error=error;emit changed();return;}
    if(pageCount()+int(selected.size())>1000){pdfInfo_.error="O projeto comporta até 1000 páginas.";emit changed();return;}
    std::vector<Page> imported;for(int index:selected){Page p;p.id=newId();p.size=pdfInfo_.sizes[index];p.pdf=PdfPageObject{pdfInfo_.assetId,pdfInfo_.data,index,pdfPageCount()};imported.push_back(std::move(p));}
    const bool fresh=!active();const int first=pageCount();
    if(fresh){Project p;p.id=newId();p.name=pdfInfo_.name.toStdString();if(p.name.empty())p.name="PDF importado";p.createdAt=p.updatedAt=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();p.pages=std::move(imported);const auto path=dataDir_+"/projects/"+QString::fromStdString(p.id)+".board";install(std::move(p),path);}
    else {for(auto& p:imported){dirtyThumbnails_.insert(QString::fromStdString(p.id));project_.pages.push_back(std::move(p));}selectPage(first);}
    mutate();refreshPdf(textRasterScale_);emit pagesChanged();pdfInfo_={};status_="PDF importado · anote sobre as páginas";emit changed();emit pdfImported();
}
void AppController::scheduleThumbnails(){if(active())thumbnailsTimer_.start();}
void AppController::beginThumbnails(){
    if(!active()||dirtyThumbnails_.isEmpty())return;
    if(thumbnailsPending_){scheduleThumbnails();return;}
    std::vector<Page> snapshot;renderingThumbnails_.clear();for(const auto& page:project_.pages)if(dirtyThumbnails_.contains(QString::fromStdString(page.id))){snapshot.push_back(page);renderingThumbnails_.insert(QString::fromStdString(page.id));if(snapshot.size()>=16)break;}
    thumbnailsRevision_=revision_;thumbnailsPending_=true;
    thumbnailsWatcher_.setFuture(QtConcurrent::run([snapshot]{QHash<QString,QString> result;
        for(const auto& page:snapshot){auto image=pageThumbnail(page,{192,160});QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"PNG");result.insert(QString::fromStdString(page.id),"data:image/png;base64,"+QString::fromLatin1(png.toBase64()));}return result;
    }));
}
}
