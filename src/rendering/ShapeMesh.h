#pragma once
#include "documents/Document.h"
namespace scalar {
std::vector<Point> shapeBorderMesh(const ShapeObject& shape);
std::vector<Point> shapeFillMesh(const ShapeObject& shape);
}
