#pragma once
#include "geometry/Geometry.h"
namespace scalar {
struct RecognitionResult {std::optional<ShapeObject> shape;double confidence=0;};
std::vector<Point> resample(const std::vector<Point>& points,std::size_t count);
std::vector<Point> simplifyRdp(const std::vector<Point>& points,double epsilon);
RecognitionResult recognizeShape(const StrokeObject& stroke);
std::string shapeName(ShapeKind kind);
struct ClosedLines {ShapeObject polygon;std::vector<std::string> lineIds;};
std::optional<ClosedLines> closeConnectedLines(const Page& page,const std::string& newestId,double toleranceMm=3);
}
