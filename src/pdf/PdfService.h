#pragma once
#include "documents/Document.h"
#include <QString>
#include <QImage>
#include <QSize>
namespace scalar {
struct PdfInfo {
    std::string assetId;
    std::shared_ptr<const std::vector<std::uint8_t>> data;
    std::vector<PageSize> sizes;
    QString name,error;
    explicit operator bool() const {return error.isEmpty()&&!sizes.empty();}
};
PdfInfo inspectPdf(const QString& path);
std::vector<int> pdfPageRange(const QString& range,int count,QString& error);
QImage renderPdf(const PdfPageObject& page,QSize size,QString* error=nullptr);
QSize pdfRenderSize(PageSize page,double pixelsPerMm);
bool pdfAvailable();
}
