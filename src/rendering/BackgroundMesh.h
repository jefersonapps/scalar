#pragma once
#include "geometry/Geometry.h"
namespace scalar {
// Visible-region geometry; physical spacing never changes with zoom.
std::vector<Point> backgroundMesh(const BackgroundStyle& style,Bounds visible,double minimumSpacingMm=0);
}
