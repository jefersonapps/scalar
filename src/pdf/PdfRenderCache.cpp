#include "PdfRenderCache.h"
#include <QtConcurrent>
namespace scalar {
PdfRenderCache::PdfRenderCache(QObject* parent):QObject(parent){
    connect(&watcher_,&QFutureWatcher<Result>::finished,this,[this]{
        running_=false;
        const auto result=watcher_.result();
        if(!result.image.isNull())cache_.insert(result.key,new QImage(result.image),int(result.image.sizeInBytes()/1024)+1);
        if(result.key==desired_){
            if(result.error.isEmpty()){image_=result.image;++revision_;emit imageChanged();}
            else emit failed(result.error);
        }
        start();
    });
}
PdfRenderCache::~PdfRenderCache(){watcher_.waitForFinished();}
void PdfRenderCache::clear(){pending_.reset();desired_.clear();pageKey_.clear();cache_.clear();image_={};++revision_;emit imageChanged();}
void PdfRenderCache::request(const std::optional<PdfPageObject>& pdf,PageSize physical,double scale){
    if(!pdf){if(!desired_.isEmpty())clear();return;}
    const auto pageKey=QString::fromStdString(pdf->assetId)+":"+QString::number(pdf->pageIndex);
    const auto size=pdfRenderSize(physical,scale);const auto key=pageKey+":"+QString::number(size.width())+"x"+QString::number(size.height());
    if(key==desired_)return;
    if(pageKey!=pageKey_){image_={};++revision_;emit imageChanged();}
    pageKey_=pageKey;desired_=key;pending_.reset();
    if(const auto* cached=cache_.object(key)){image_=*cached;++revision_;emit imageChanged();return;}
    pending_=Request{*pdf,size,key};start();
}
void PdfRenderCache::start(){
    if(running_||!pending_)return;
    const auto request=*pending_;pending_.reset();
    running_=true;
    watcher_.setFuture(QtConcurrent::run([request]{Result r;r.key=request.key;r.image=renderPdf(request.page,request.size,&r.error);return r;}));
}
}
