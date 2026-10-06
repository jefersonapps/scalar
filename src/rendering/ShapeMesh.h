#pragma once
#include "documents/Document.h"
#include <span>
namespace scalar {
class ShapeMeshClipper {
    struct Cell {
        std::vector<Point> original;
        std::vector<std::vector<Point>> visible,hidden;
        double left,right,top,bottom;
    };
    std::vector<Cell> cells_;
public:
    ShapeMeshClipper()=default;
    explicit ShapeMeshClipper(std::vector<Point> triangles);
    void erase(std::span<const ErasedRegion> regions);
    std::vector<Point> triangles() const;
};
std::vector<Point> shapeBorderMesh(const ShapeObject& shape);
std::vector<Point> shapeFillMesh(const ShapeObject& shape);
std::vector<Point> eraseShapeMesh(std::vector<Point> mesh,std::span<const ErasedRegion> regions);
}
