#pragma once
#include "documents/Document.h"
#include <QImage>
#include <QString>
#include <QSizeF>
#include <QFont>
class QPainter;
namespace scalar {
void registerTextFonts();
QFont documentFont(const QString& family,int pixelSize,bool bold=false,bool italic=false);
struct PreparedText {TextObject object;QImage image;QString error;std::vector<Point> geometry{};QSizeF naturalSize{};};
struct TextVisual {QImage text;std::vector<Point> math;QString error;QSizeF naturalSize;};
QSizeF textNaturalSize(const TextObject& object);
TextVisual textVisual(const TextObject& object,double pixelsPerMm);
PreparedText prepareText(TextObject object);
QImage textBitmap(const TextObject& object,double pixelsPerMm);
bool svgRenderingAvailable();
QString paintVectorText(QPainter& painter,const TextObject& object);
}
