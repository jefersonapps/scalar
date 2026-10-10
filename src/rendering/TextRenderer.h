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
struct MathColorRun {std::size_t start=0,count=0;std::uint32_t rgba=0;};
struct PreparedText {TextObject object;QImage image;QString error;std::vector<Point> geometry{};QSizeF naturalSize{};std::vector<MathColorRun> mathColors;};
struct TextVisual {QImage text;std::vector<Point> math;QString error;QSizeF naturalSize;std::vector<MathColorRun> mathColors;};
QSizeF textNaturalSize(const TextObject& object);
TextVisual textVisual(const TextObject& object,double pixelsPerMm);
PreparedText prepareText(TextObject object);
QImage textBitmap(const TextObject& object,double pixelsPerMm);
bool svgRenderingAvailable();
QString paintVectorText(QPainter& painter,const TextObject& object);
}
