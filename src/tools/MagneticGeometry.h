#pragma once
#include "GeometryTools.h"
#include <map>
namespace scalar {
// A document-revision snapshot. Queries reuse local intersection results while
// a guide moves, rather than intersecting the entire page on every event.
class MagneticGeometry {
public:
    void reset(const Page& page);
    std::vector<Point> pointsIn(Bounds area);
    std::optional<Point> nearest(Point point,double tolerance);
    struct RulerSnap {
        Point first,second;int edge=0;bool single=false;
        bool firstIntersection=false,secondIntersection=false;
        Point rotationAnchor(Point handle) const {
            if(firstIntersection!=secondIntersection)return firstIntersection?first:second;
            return length(handle-first)>=length(handle-second)?first:second;
        }
    };
    bool alignRuler(RulerGeometry& ruler,double tolerance,std::optional<RulerSnap>& latch,int edge=0,std::optional<Point> pivot={});
private:
    struct Feature {Point point;bool intersection=false;};
    std::vector<Feature> featuresIn(Bounds area);
    struct Primitive {Point a,b;std::size_t owner=0,step=0,count=0;bool closed=false,ellipse=false,circularPath=false;double rx=0,ry=0,angle=0;Bounds box;};
    std::vector<CanvasObjectView> owners_;
    std::vector<Primitive> primitives_;
    std::vector<std::pair<Point,std::size_t>> endpoints_;
    std::map<std::pair<int,int>,std::vector<std::size_t>> cells_;
    std::vector<std::size_t> large_;
    std::map<std::pair<std::size_t,std::size_t>,std::vector<Point>> intersections_;
    bool visibleAt(std::size_t owner,Point point) const;
    static std::vector<Point> intersect(const Primitive& a,const Primitive& b);
};
}
