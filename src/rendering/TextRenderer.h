#pragma once
#include "documents/Document.h"
#include <QImage>
#include <QString>
#include <QSizeF>
namespace scalar {
struct PreparedText {TextObject object;QImage image;QString error;std::vector<Point> geometry{};QSizeF naturalSize{};};
struct TextVisual {QImage text;std::vector<Point> math;QString error;QSizeF naturalSize;};
QSizeF textNaturalSize(const TextObject& object);
TextVisual textVisual(const TextObject& object,double pixelsPerMm);
PreparedText prepareText(TextObject object);
QImage textBitmap(const TextObject& object,double pixelsPerMm);
bool svgRenderingAvailable();
}
