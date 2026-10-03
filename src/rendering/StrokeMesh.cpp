#include "StrokeMesh.h"
#include <numbers>
namespace scalar {
std::vector<Point> strokeMesh(const StrokeObject& stroke) {
    std::vector<Point> vertices;
    vertices.reserve(stroke.samples.size()*42);
    auto disk = [&](Point center,double radius) {
        constexpr int segments=12;
        for(int i=0;i<segments;++i) {
            const double a=2*std::numbers::pi*i/segments, b=2*std::numbers::pi*(i+1)/segments;
            vertices.insert(vertices.end(),{center,center+Point{std::cos(a),std::sin(a)}*radius,center+Point{std::cos(b),std::sin(b)}*radius});
        }
    };
    for(std::size_t i=0;i<stroke.samples.size();++i) {
        const auto& s=stroke.samples[i]; const double r=stroke.style.width(s.pressure)/2;
        disk(s.position,r);
        if(!i) continue;
        const auto& prev=stroke.samples[i-1]; const auto d=s.position-prev.position;
        const double len=length(d); if(len<1e-9) continue;
        const Point n{-d.y/len,d.x/len};
        const double pr=stroke.style.width(prev.pressure)/2;
        const auto a=prev.position+n*pr,b=prev.position-n*pr,c=s.position+n*r,e=s.position-n*r;
        vertices.insert(vertices.end(),{a,b,c,c,b,e});
    }
    return vertices;
}
}
