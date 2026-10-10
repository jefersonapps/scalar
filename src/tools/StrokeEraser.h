#pragma once
#include "geometry/Geometry.h"
#include <array>
#include <span>
namespace scalar {
struct StrokeCut {std::vector<StrokeObject> visible,removed;};
// Compute both halves once, retaining pressure and image protection boundaries.
StrokeCut cutStroke(const StrokeObject& stroke,Point from,Point to,double radiusMm,
    std::span<const std::array<Point,4>> protectedAreas = {});
// Split at exact intersections with a swept circular eraser (capsule).
std::vector<StrokeObject> eraseStroke(const StrokeObject& stroke,Point from,Point to,double radiusMm,
    std::span<const std::array<Point,4>> protectedAreas = {});
std::vector<StrokeObject> inkInsideEraser(const StrokeObject& stroke,Point from,Point to,double radiusMm,
    std::span<const std::array<Point,4>> protectedAreas = {});
}
