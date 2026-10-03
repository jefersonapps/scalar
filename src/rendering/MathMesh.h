#pragma once
#include "documents/Document.h"
#include <QString>
namespace scalar {
struct MathMesh {std::vector<Point> vertices;QString error;};
// Tessellates MathJax's self-contained glyph paths, retaining holes and transforms.
MathMesh svgMathMesh(const std::string& svg);
}
