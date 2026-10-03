#pragma once
#include "documents/Document.h"
#include <QString>
class QPainter;
namespace scalar {
QString paintPage(QPainter& painter,const Page& page,double pixelsPerMm);
}
