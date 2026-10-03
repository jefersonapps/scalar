#pragma once
#include "documents/Document.h"
namespace scalar {
// Triangle mesh in millimetres, shared by every graphics backend.
std::vector<Point> strokeMesh(const StrokeObject& stroke);
}
