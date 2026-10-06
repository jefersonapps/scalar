#pragma once
#include "clipboard/ImageImporter.h"
#include <QColor>
namespace scalar {
// Only ink and shape outlines are barriers; paper, grids and images never close a region.
ImportedImage fillInkRegion(const Page& page,Point seed,QColor color,double gapMm=2.0,double opacity=.10);
}
