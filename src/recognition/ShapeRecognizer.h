#pragma once
#include "geometry/Geometry.h"
namespace scalar {
struct RecognitionResult {std::optional<ShapeObject> shape;double confidence=0;};
std::vector<Point> resample(const std::vector<Point>& points,std::size_t count);
std::vector<Point> simplifyRdp(const std::vector<Point>& points,double epsilon);
RecognitionResult recognizeShape(const StrokeObject& stroke);
// A sector candidate is accepted only when two nearby sides close its angle.
RecognitionResult recognizeShape(const StrokeObject& stroke,const Page& page);
std::optional<ShapeObject> recognizeRightAngle(const Page& page,const StrokeObject& stroke);
ShapeObject alignCircularSector(const Page& page,ShapeObject sector,double toleranceMm=3);
std::string shapeName(ShapeKind kind);
struct ClosedLines {ShapeObject polygon;std::vector<std::string> lineIds;};
std::optional<ClosedLines> closeConnectedLines(const Page& page,const std::string& newestId,double toleranceMm=3);
}
