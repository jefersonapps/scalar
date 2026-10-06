#include "tools/GeometryTools.h"
#include <iostream>
#include <stdexcept>
using namespace scalar;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    require(length(projectPointOntoLine({3,7},{0,0},{10,0})-Point{3,0})<1e-9,"line projection");
    RulerGeometry ruler;ruler.origin={40,30};ruler.angle=.7;
    for(int edge:{0,1}){const auto point=ruler.world({45,edge*RulerGeometry::widthMm+1});require(ruler.nearEdge(point,2)==edge,"rotated snap");require(length(ruler.local(ruler.project(point,edge))-Point{45,edge*RulerGeometry::widthMm})<1e-9,"exact ruler edge");}
    require(!ruler.nearEdge(ruler.world({-20,0}),2),"end tolerance");require(!ruler.nearEdge(ruler.world({40,6}),2),"snap distance");
    for(double angle:{0.,.7,1.57,-2.}){
        ruler.angle=angle;
        for(int edge:{0,1}){
            const double y=edge*RulerGeometry::widthMm;
            require(ruler.nearEdge(ruler.world({0,y}),3)==edge,"snap at first endpoint");
            require(ruler.nearEdge(ruler.world({ruler.lengthMm,y}),3)==edge,"snap at last endpoint");
            require(!ruler.nearEdge(ruler.world({-.01,y+1}),3),"release snap immediately before ruler");
            require(!ruler.nearEdge(ruler.world({ruler.lengthMm+.01,y+1}),3),"release snap immediately after ruler");
        }
        const auto entry=ruler.entryPoint(ruler.world({30,-5}),ruler.world({30,20}));require(bool(entry),"fast crossing blocked");require(length(ruler.local(*entry)-Point{30,0})<1e-8,"clip to first ruler edge");
        require(!ruler.entryPoint(ruler.world({20,0}),ruler.world({80,0})),"allow drawing along boundary");
        require(!ruler.entryPoint(ruler.world({20,-5}),ruler.world({80,-5})),"allow drawing outside ruler");
        require(!ruler.entryPoint(ruler.world({20,0}),ruler.world({20,-5})),"allow moving away from boundary");
        require(bool(ruler.entryPoint(ruler.world({20,0}),ruler.world({20,6}))),"block entering from boundary");
    }
    CompassGeometry compass;for(double radius:{1.,25.,60.,80.,500.}){compass.radiusMm=radius;compass.angle=.7;const auto hinge=compass.hinge();const double leg=std::max(40.,radius*.6);require(std::abs(length(hinge-compass.center)-leg)<1e-9,"dry leg length");require(std::abs(length(hinge-compass.pencil())-leg)<1e-9,"pencil leg length");require(length(compass.openingHandle()-(hinge*.2+compass.pencil()*.8))<1e-9,"opening grip follows pencil leg");}
    CompassSweep sweep;sweep.begin(2.9);for(int i=1;i<=360;++i)sweep.advance(std::remainder(2.9+i*std::numbers::pi/180,2*std::numbers::pi));require(sweep.complete(),"circle across angle seam");require(std::abs(sweep.sweep()-2*std::numbers::pi)<1e-8,"full turn");
    sweep.begin(0);sweep.advance(.5);sweep.advance(0);require(!sweep.complete(),"backtracking is not a full circle");
    ShapeObject line;line.kind=ShapeKind::Line;line.vertices={{10,20},{60,50}};
    const auto parallel=constructLine(line,LineConstruction::Parallel),perp=constructLine(line,LineConstruction::Perpendicular),bisector=constructLine(line,LineConstruction::Bisector);require(parallel&&perp&&bisector,"line construction");
    const auto d=line.vertices[1]-line.vertices[0],p=perp->vertices[1]-perp->vertices[0],q=parallel->vertices[1]-parallel->vertices[0];require(std::abs(d.x*p.x+d.y*p.y)<1e-8,"perpendicular");require(length(d-q)<1e-9,"parallel");require(length((bisector->vertices[0]+bisector->vertices[1])*.5-(line.vertices[0]+line.vertices[1])*.5)<1e-9,"bisector midpoint");
    ShapeObject triangle;triangle.kind=ShapeKind::Triangle;triangle.vertices={{0,0},{40,0},{0,30}};auto circle=circumcircle(triangle);require(bool(circle),"circumcircle");for(auto vertex:triangle.vertices)require(std::abs(length(vertex-circle->center)-circle->radiusX)<1e-9,"equidistant vertices");triangle.vertices={{0,0},{1,1},{2,2}};require(!circumcircle(triangle),"collinear triangle");
    triangle.vertices={{0,0},{40,0},{0,30}};const auto inscribed=incircle(triangle);require(bool(inscribed),"incircle");
    require(length(inscribed->center-Point{10,10})<1e-9&&std::abs(inscribed->radiusX-10)<1e-9,"right triangle incenter and radius");
    for(int i=0;i<3;++i)require(std::abs(distanceToSegment(inscribed->center,triangle.vertices[i],triangle.vertices[(i+1)%3])-inscribed->radiusX)<1e-9,"incircle tangent to all sides");
    std::reverse(triangle.vertices.begin(),triangle.vertices.end());require(length(incircle(triangle)->center-inscribed->center)<1e-9,"incircle independent of winding");
    triangle.vertices={{0,0},{1,1},{2,2}};require(!incircle(triangle),"collinear incircle rejected");
    std::cout<<"PASS: ruler projection, rotated snap, compass sweep, parallel, perpendicular, bisector, circumcircle, incircle\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
