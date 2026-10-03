#pragma once
#include "geometry/Geometry.h"
namespace scalar {
struct RecognitionResult {std::optional<ShapeObject> shape;double confidence=0;};
std::vector<Point> resample(const std::vector<Point>& points,std::size_t count);
std::vector<Point> simplifyRdp(const std::vector<Point>& points,double epsilon);
RecognitionResult recognizeShape(const StrokeObject& stroke);
std::string shapeName(ShapeKind kind);
}
