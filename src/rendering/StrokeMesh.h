#pragma once
#include "documents/Document.h"
namespace scalar {
// Triangle mesh in millimetres, shared by every graphics backend.
std::vector<Point> strokeMesh(const StrokeObject& stroke);
// Compact display geometry; document samples and export remain lossless.
std::vector<Point> strokeDisplayMesh(const StrokeObject& stroke);
std::vector<PointerSample> strokeDisplaySamples(const StrokeObject& stroke,double tolerance=.004);
// Materialize curves so rendering, erasing, saving and PDF use the same path.
std::vector<PointerSample> smoothStrokeSamples(const std::vector<PointerSample>& samples);
void appendSmoothStrokeSegment(const std::vector<PointerSample>& samples,std::size_t segment,std::vector<PointerSample>& output);
}
