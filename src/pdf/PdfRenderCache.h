#pragma once
#include "PdfService.h"
#include <QObject>
#include <QCache>
#include <QFutureWatcher>
namespace scalar {
class PdfRenderCache : public QObject {
    Q_OBJECT
public:
    explicit PdfRenderCache(QObject* parent=nullptr);
    ~PdfRenderCache() override;
    void request(const std::optional<PdfPageObject>& page,PageSize physical,double pixelsPerMm);
    void clear();
    QImage image() const {return image_;}
    quint64 revision() const {return revision_;}
signals:
    void imageChanged();
    void failed(QString error);
private:
    struct Request {PdfPageObject page;QSize size;QString key;};
    struct Result {QString key,error;QImage image;};
    void start();
    QCache<QString,QImage> cache_{96*1024}; // KiB; at most 96 MiB of completed renders
    QFutureWatcher<Result> watcher_;
    std::optional<Request> pending_;
    QString desired_,pageKey_;
    QImage image_;
    quint64 revision_=0;
    bool running_=false;
};
}
