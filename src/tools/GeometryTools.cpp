#include "GeometryTools.h"
namespace scalar {
Point projectPointOntoLine(Point p,Point a,Point b){
    const auto d=b-a;const double squared=d.x*d.x+d.y*d.y;
    if(squared<1e-12)return a;
    return a+d*(((p.x-a.x)*d.x+(p.y-a.y)*d.y)/squared);
}
Point RulerGeometry::local(Point p) const {return rotatePoint(p,origin,-angle)-origin;}
Point RulerGeometry::world(Point p) const {return rotatePoint(origin+p,origin,angle);}
std::optional<int> RulerGeometry::nearEdge(Point p,double tolerance) const {
    // The snap distance applies across the edge, never beyond the ruler's ends.
    const auto q=local(p);if(q.x < -1e-9||q.x > lengthMm+1e-9)return {};
    const int edge=std::abs(q.y)<std::abs(q.y-widthMm)?0:1;
    if(std::abs(q.y-edge*widthMm)>tolerance)return {};
    return edge;
}
Point RulerGeometry::project(Point p,int edge) const {return projectPointOntoLine(p,world({0,edge*widthMm}),world({lengthMm,edge*widthMm}));}
std::optional<Point> RulerGeometry::entryPoint(Point start,Point end) const {
    const auto a=local(start),d=local(end)-a;
    double enter=0,exit=1;
    const auto clip=[&](double p,double delta,double limit){
        if(std::abs(delta)<1e-9)return p>1e-9&&p<limit-1e-9;
        double first=-p/delta,last=(limit-p)/delta;
        if(first>last)std::swap(first,last);
        enter=std::max(enter,first);exit=std::min(exit,last);
        return exit-enter>1e-10;
    };
    if(!clip(a.x,d.x,lengthMm)||!clip(a.y,d.y,widthMm))return {};
    return start+(end-start)*enter;
}
Point CompassGeometry::pencil() const {return center+Point{std::cos(angle),std::sin(angle)}*radiusMm;}
Point CompassGeometry::hinge() const {
    // Keep the legs fixed for normal openings; scale for very large constructions.
    const double leg=std::max(40.,radiusMm*.6);
    const double height=std::sqrt(leg*leg-radiusMm*radiusMm*.25);
    return center+Point{std::cos(angle)*radiusMm*.5+std::sin(angle)*height,
                        std::sin(angle)*radiusMm*.5-std::cos(angle)*height};
}
Point CompassGeometry::openingHandle() const {return hinge()*.2+pencil()*.8;}
double CompassSweep::advance(double angle){const double delta=std::remainder(angle-last_,2*std::numbers::pi);last_=angle;sweep_+=delta;return delta;}
std::optional<ShapeObject> constructLine(const ShapeObject& source,LineConstruction kind){
    if(source.kind!=ShapeKind::Line||source.vertices.size()!=2)return {};
    const auto a=source.vertices[0],b=source.vertices[1],d=b-a;const double size=length(d);if(size<0.1)return {};
    ShapeObject out;out.id=newId();out.kind=ShapeKind::Line;out.style=source.style;out.fillOpacity=0;
    const Point normal{-d.y/size,d.x/size};
    if(kind==LineConstruction::Parallel)out.vertices={a+normal*10,b+normal*10};
    else {const auto center=kind==LineConstruction::Bisector?(a+b)*.5:a;out.vertices={center-normal*(size*.5),center+normal*(size*.5)};}
    return out;
}
std::optional<ShapeObject> incircle(const ShapeObject& source){
    if(source.vertices.size()!=3||(source.kind!=ShapeKind::Triangle&&source.kind!=ShapeKind::Polygon))return {};
    const auto a=source.vertices[0],b=source.vertices[1],c=source.vertices[2];
    const double oppositeA=length(c-b),oppositeB=length(c-a),oppositeC=length(b-a),perimeter=oppositeA+oppositeB+oppositeC;
    const auto u=b-a,v=c-a;const double areaTwice=std::abs(u.x*v.y-u.y*v.x);
    if(perimeter<=0||areaTwice<1e-6*std::max(1.,length(u)*length(v)))return {};
    const double radius=areaTwice/perimeter;
    const auto center=(a*oppositeA+b*oppositeB+c*oppositeC)*(1/perimeter);
    if(!std::isfinite(radius)||radius<.1||radius>5000)return {};
    ShapeObject out;out.id=newId();out.kind=ShapeKind::Circle;out.center=center;out.radiusX=out.radiusY=radius;out.style=source.style;out.fillOpacity=0;return out;
}
std::optional<ShapeObject> circumcircle(const ShapeObject& source){
    if(source.vertices.size()!=3||(source.kind!=ShapeKind::Triangle&&source.kind!=ShapeKind::Polygon))return {};
    const auto a=source.vertices[0],b=source.vertices[1],c=source.vertices[2];
    const auto u=b-a,v=c-a;const double cross=u.x*v.y-u.y*v.x;
    if(std::abs(cross)<1e-6*std::max(1.,length(u)*length(v)))return {};
    const double uu=u.x*u.x+u.y*u.y,vv=v.x*v.x+v.y*v.y;
    const auto center=a+Point{(uu*v.y-vv*u.y)/(2*cross),(u.x*vv-v.x*uu)/(2*cross)};
    const double radius=length(center-a);if(!std::isfinite(radius)||radius<.1||radius>5000)return {};
    ShapeObject out;out.id=newId();out.kind=ShapeKind::Circle;out.center=center;out.radiusX=out.radiusY=radius;out.style=source.style;return out;
}
}
