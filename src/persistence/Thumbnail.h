#pragma once
#include "documents/Document.h"
#include <QString>
#include <QImage>
namespace scalar {
QImage pageThumbnail(const Page&,QSize size={640,400});
bool saveThumbnail(const Project&,const QString& path);
}
