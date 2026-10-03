#pragma once
#include "documents/Document.h"
#include <QImage>
#include <QString>
namespace scalar {
struct ImportedImage {ImageObject object;QImage image;QString error;};
ImportedImage importImageFile(const QString& path,Point center,PageSize page);
ImportedImage encodeImage(QImage image,Point center,PageSize page);
}
