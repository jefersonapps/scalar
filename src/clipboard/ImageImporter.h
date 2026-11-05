#pragma once
#include "documents/Document.h"
#include <QImage>
#include <QString>
#include <QUrl>
#include <QVariantList>
namespace scalar {
struct ImportedImage {ImageObject object;QImage image;QString error;};
ImportedImage importImageFile(const QString& path,Point center,PageSize page);
// Call on an import worker: HTTP requests and image decoding never block input.
ImportedImage importImageUrl(const QUrl& url,Point center,PageSize page);
QVariantList imageDropSources(const QVariantList& urls,const QString& html);
ImportedImage encodeImage(QImage image,Point center,PageSize page);
}
