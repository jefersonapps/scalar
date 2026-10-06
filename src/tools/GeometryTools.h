#pragma once
#include "geometry/Geometry.h"
#include <numbers>
namespace scalar {
Point projectPointOntoLine(Point point,Point start,Point end);
struct RulerGeometry {
    Point origin{30,60};
    double lengthMm=120,angle=0;
    static constexpr double widthMm=12;
    Point local(Point point) const;
    Point world(Point point) const;
    std::optional<int> nearEdge(Point point,double tolerance) const;
    Point project(Point point,int edge) const;
    std::optional<Point> entryPoint(Point start,Point end) const;
};
struct CompassGeometry {
    Point center{100,100};
    double radiusMm=30,angle=0;
    Point pencil() const;
    Point hinge() const;
    Point openingHandle() const;
};
// Unwrap successive angles so a full turn survives the +/-pi seam.
class CompassSweep {
public:
    void begin(double angle){start_=last_=angle;sweep_=0;}
    double advance(double angle);
    double sweep() const {return sweep_;}
    double angle() const {return start_+sweep_;}
    bool complete() const {return std::abs(sweep_)>=2*std::numbers::pi-0.01;}
private:
    double start_=0,last_=0,sweep_=0;
};
enum class LineConstruction { Parallel,Perpendicular,Bisector };
std::optional<ShapeObject> constructLine(const ShapeObject& line,LineConstruction kind);
std::optional<ShapeObject> circumcircle(const ShapeObject& triangle);
std::optional<ShapeObject> incircle(const ShapeObject& triangle);
}
