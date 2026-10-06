#pragma once
#include "geometry/Geometry.h"
#include <array>
#include <span>
namespace scalar {
// Split at exact intersections with a swept circular eraser (capsule).
std::vector<StrokeObject> eraseStroke(const StrokeObject& stroke,Point from,Point to,double radiusMm,
    std::span<const std::array<Point,4>> protectedAreas = {});
std::vector<StrokeObject> inkInsideEraser(const StrokeObject& stroke,Point from,Point to,double radiusMm,
    std::span<const std::array<Point,4>> protectedAreas = {});
}
