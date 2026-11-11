#include "tools/GeometryTools.h"
#include "tools/MagneticGeometry.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace scalar;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void magneticTests(){
    Page page;StrokeObject a;a.id=newId();a.samples={{{0,0},1},{{10,10},1}};page.strokes.push_back(a);
    auto b=a;b.id=newId();b.samples={{{0,10},1},{{10,0},1}};page.strokes.push_back(b);
    MagneticGeometry magnet;magnet.reset(page);
    auto p=magnet.nearest({5.1,4.9},.3);require(p&&length(*p-Point{5,5})<1e-8,"freehand intersection");
    require(magnet.nearest({.1,0},.2).has_value(),"stroke endpoint");require(!magnet.nearest({2,2},.1),"interior stroke sample is not an endpoint");
    page.strokes[0].properties.locked=true;magnet.reset(page);require(magnet.nearest({5,5},.1).has_value(),"locked geometry remains a target");
    page.strokes[0].erasedRegions.push_back({{4,4},{6,6},1});magnet.reset(page);require(!magnet.nearest({5,5},.1),"erased intersection excluded");
    page.strokes[0].erasedRegions.clear();page.strokes[1].properties.visible=false;magnet.reset(page);require(!magnet.nearest({5,5},.1),"hidden geometry excluded");
    page={};ShapeObject circle;circle.id=newId();circle.kind=ShapeKind::Circle;circle.center={0,0};circle.radiusX=circle.radiusY=10;page.shapes.push_back(circle);
    auto other=circle;other.id=newId();other.center={10,0};page.shapes.push_back(other);magnet.reset(page);
    p=magnet.nearest({5,8.66},.1);require(p&&std::abs(length(*p)-10)<1e-8&&std::abs(length(*p-other.center)-10)<1e-8,"exact circle intersections");
    page.shapes[1].center={20,0};magnet.reset(page);p=magnet.nearest({10,0},.1);require(p&&length(*p-Point{10,0})<1e-7,"circle tangency");
    page.shapes[1]=circle;magnet.reset(page);require(!magnet.nearest({10,0},.1),"coincident circles do not create arbitrary targets");
    page.shapes.resize(1);auto& ellipse=page.shapes[0];ellipse.kind=ShapeKind::Ellipse;ellipse.radiusY=5;ellipse.rotation=.5;
    ShapeObject line;line.id=newId();line.kind=ShapeKind::Line;line.vertices={rotatePoint({-20,0},{},.5),rotatePoint({20,0},{},.5)};page.shapes.push_back(line);magnet.reset(page);
    const auto expected=rotatePoint({10,0},{},.5);p=magnet.nearest(expected,.1);require(p&&length(*p-expected)<1e-8,"rotated ellipse-line intersection");
    page.shapes.resize(1);other=page.shapes[0];other.id=newId();other.center={8,0};other.rotation=-.2;page.shapes.push_back(other);magnet.reset(page);
    const auto crossings=magnet.pointsIn({-20,-20,20,20});require(crossings.size()>=2,"ellipse-ellipse intersections");
    for(const auto q:crossings)for(const auto& curve:page.shapes){const auto v=rotatePoint(q-curve.center,{},-curve.rotation);require(std::abs(v.x*v.x/(curve.radiusX*curve.radiusX)+v.y*v.y/(curve.radiusY*curve.radiusY)-1)<1e-8,"ellipse intersection residual");}
    page={};a.samples={{{20,10},1}};b.samples={{{80,12},1}};page.strokes={a,b};magnet.reset(page);
    RulerGeometry ruler;ruler.origin={0,10.2};ruler.lengthMm=100;std::optional<MagneticGeometry::RulerSnap> latch;
    require(magnet.alignRuler(ruler,3,latch),"ruler captures two points");require(std::abs(ruler.local({20,10}).y)<1e-8&&std::abs(ruler.local({80,12}).y)<1e-8,"both points on same ruler edge");
    ruler.origin={0,10.4};ruler.angle=0;require(magnet.alignRuler(ruler,3,latch),"stable ruler latch");
    ruler.origin={0,100};ruler.angle=0;require(!magnet.alignRuler(ruler,3,latch)&&!latch,"ruler releases when moved away");
    page.strokes[1].samples={{{80,10},1}};magnet.reset(page);ruler.origin={0,-2.2};require(!magnet.alignRuler(ruler,1,latch),"opposite ungraduated edge does not capture");require(magnet.alignRuler(ruler,1,latch,1),"flipped graduated edge captures");require(latch->edge==1,"correct ruler edge");
    ruler.origin={0,0};ruler.angle=0;latch.reset();require(!magnet.alignRuler(ruler,10,latch),"large zoom tolerance cannot capture opposite side");require(magnet.alignRuler(ruler,10,latch,1),"flipped edge captures at low zoom");
    page.strokes.resize(1);magnet.reset(page);latch.reset();const double beforeAngle=ruler.angle;require(magnet.alignRuler(ruler,1,latch,1)&&latch->single,"single endpoint becomes an anchor");require(ruler.angle==beforeAngle,"single anchor does not rotate ruler");
    page={};a.samples={{{20,10},1}};b.samples={{{20.5,10},1}};page.strokes={a,b};magnet.reset(page);ruler.origin={0,10.2};ruler.angle=0;latch.reset();
    require(magnet.alignRuler(ruler,3,latch)&&latch->single,"near-coincident targets count as a single anchor");
    // A sampled sector must only expose its center and the two arc ends.
    page={};ShapeObject sector;sector.id=newId();sector.kind=ShapeKind::CircularSector;sector.center={0,0};sector.vertices.push_back(sector.center);
    for(int i=0;i<=96;++i){const double t=i*std::numbers::pi/192;sector.vertices.push_back({10*std::cos(t),10*std::sin(t)});}page.shapes.push_back(sector);magnet.reset(page);
    require(magnet.pointsIn({-11,-11,11,11}).size()==3,"sector samples are not construction vertices");
    // Closer endpoints must not displace a valid pair of intersections.
    page={};a.samples={{{0,1},1},{{100,1},1}};page.strokes.push_back(a);
    b.samples={{{20,-20},1},{{20,20},1}};page.strokes.push_back(b);
    auto vertical=b;vertical.id=newId();vertical.samples={{{80,-20},1},{{80,20},1}};page.strokes.push_back(vertical);
    auto dot=a;dot.id=newId();dot.samples={{{5,0},1}};page.strokes.push_back(dot);dot.id=newId();dot.samples={{{95,0},1}};page.strokes.push_back(dot);
    dot.id=newId();dot.samples={{{20,1},1}};page.strokes.push_back(dot); // Endpoint coincides with a crossing.
    ruler.origin={0,0};ruler.angle=0;magnet.reset(page);latch.reset();require(magnet.alignRuler(ruler,3,latch),"intersection pair available");
    require((length(latch->first-Point{20,1})<1e-8&&length(latch->second-Point{80,1})<1e-8)||(length(latch->second-Point{20,1})<1e-8&&length(latch->first-Point{80,1})<1e-8),"two intersections outrank closer endpoints");
    auto approachPage=page;approachPage.strokes[0].samples={{{0,3.5},1},{{100,3.5},1}};magnet.reset(approachPage);ruler.origin={0,0};ruler.angle=0;latch.reset();
    require(magnet.alignRuler(ruler,3,latch),"nearby intersections get capture priority");require(std::abs(latch->first.y-3.5)<1e-8&&std::abs(latch->second.y-3.5)<1e-8,"intersection capture margin beats endpoints on edge");
    page.strokes[2].properties.visible=false;magnet.reset(page);ruler.origin={0,0};ruler.angle=0;require(magnet.alignRuler(ruler,3,latch),"mixed fallback available");
    require(length(latch->first-Point{20,1})<1e-8||length(latch->second-Point{20,1})<1e-8,"one intersection outranks endpoint-only pair");
    for(const auto handle:{latch->first,latch->second})require(length(latch->rotationAnchor(handle)-Point{20,1})<1e-8,"intersection is rotation anchor from either handle");
    auto reversed=*latch;std::swap(reversed.first,reversed.second);std::swap(reversed.firstIntersection,reversed.secondIntersection);
    require(length(reversed.rotationAnchor(reversed.first)-Point{20,1})<1e-8,"intersection pivot is independent of candidate order");
    page.strokes[1].properties.visible=false;magnet.reset(page);ruler.origin={0,0};ruler.angle=0;require(magnet.alignRuler(ruler,3,latch),"endpoint-only fallback available");require(std::abs(latch->first.y)<1e-8&&std::abs(latch->second.y)<1e-8,"closest endpoint pair fallback");
    page.strokes[1].properties.visible=true;page.strokes[2].properties.visible=true;page.strokes[2].samples={{{20.5,-20},1},{{20.5,20},1}};magnet.reset(page);ruler.origin={0,0};ruler.angle=0;latch.reset();require(magnet.alignRuler(ruler,3,latch),"invalid close intersection pair permits fallback");require(length(latch->first-latch->second)>=6,"priority does not bypass anchor separation");
    // Stationary pointer samples inside a curve are not construction points.
    page={};a.samples={{{0,0},1},{{5,1},1},{{5,1},1},{{5,1},1},{{10,0},1}};page.strokes={a};magnet.reset(page);
    require(!magnet.nearest({5,1},.1),"duplicate samples do not create false self intersections");
    // Prefer two crossings even when one is farther from the released edge.
    page={};a.samples={{{0,5.5},1},{{100,5.5},1}};page.strokes={a};
    b.samples={{{20,-20},1},{{20,20},1}};page.strokes.push_back(b);
    vertical.samples={{{80,-20},1},{{80,20},1}};page.strokes.push_back(vertical);
    dot.samples={{{5,0},1}};page.strokes.push_back(dot);dot.samples={{{95,0},1}};page.strokes.push_back(dot);
    magnet.reset(page);ruler.origin={0,0};ruler.angle=0;latch.reset();
    require(magnet.alignRuler(ruler,3,latch),"crossings in priority neighborhood capture");
    require(std::abs(latch->first.y-5.5)<1e-8&&std::abs(latch->second.y-5.5)<1e-8,"two crossings beat endpoints on edge in expanded neighborhood");
    // A compass reversal retraces the same arc with different polygon samples.
    // The chords cross numerically although the visible arc has no crossing.
    page={};a.samples.clear();
    for(double angle:{0.,.02,.04,.06,.08,.07,.05,.03,.01,-.01})a.samples.push_back({{50*std::cos(angle),50*std::sin(angle)},1});
    page.strokes={a};magnet.reset(page);
    require(magnet.pointsIn({49,-1,51,5}).size()==2,"retraced compass arc exposes only its endpoints");
    require(!magnet.nearest({50,0},.01),"input start inside retraced arc is not a visible endpoint");
    require(magnet.nearest({50*std::cos(.08),50*std::sin(.08)},.01).has_value(),"turnaround is the visible arc endpoint");
    // Ruler backtracking must expose only the two visible extremes.
    for(double angle:{0.,.7,1.57}){
        page={};a.samples.clear();
        for(double x:{30.,60.,10.,90.,50.,20.,70.,40.})a.samples.push_back({rotatePoint({x,0},{},angle),1});
        page.strokes={a};magnet.reset(page);
        require(magnet.pointsIn({-100,-100,100,100}).size()==2,"retraced ruler line has two endpoints");
        for(double x:{30.,40.,50.,60.,70.})require(!magnet.nearest(rotatePoint({x,0},{},angle),.01),"internal reversal sample is not an endpoint");
        for(double x:{10.,90.})require(magnet.nearest(rotatePoint({x,0},{},angle),.01).has_value(),"visible line extremity remains magnetic");
        b.samples={ {rotatePoint({55,-10},{},angle),1},{rotatePoint({55,10},{},angle),1}};page.strokes.push_back(b);magnet.reset(page);
        require(magnet.nearest(rotatePoint({55,0},{},angle),.01).has_value(),"actual crossing of retraced line remains magnetic");
    }
    page={};a.samples={{{0,0},1},{{60,0},1}};b.samples={{{30,0},1},{{90,0},1}};page.strokes={a,b};magnet.reset(page);
    require(magnet.pointsIn({-1,-1,100,1}).size()==2,"overlapping separate lines expose only outer ends");
    require(!magnet.nearest({30,0},.01)&&!magnet.nearest({60,0},.01),"covered line endpoints are not magnetic");
    // A freehand path crossing itself also provides a construction point.
    page={};a.samples={{{0,0},1},{{10,10},1},{{0,10},1},{{10,0},1}};page.strokes={a};magnet.reset(page);require(magnet.nearest({5,5},.01).has_value(),"self intersection");
    for(int n=0;n<500;++n){auto far=a;far.id=newId();far.samples.clear();for(int j=0;j<100;++j)far.samples.push_back({{double(j),1000.+n*10},1});page.strokes.push_back(std::move(far));}
    magnet.reset(page);const auto start=std::chrono::steady_clock::now();for(int i=0;i<1000;++i)require(magnet.nearest({5,5},.1).has_value(),"dense page local query");
    std::cout<<"1000 magnetic queries with 500 distant strokes: "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<" ms\n";
}
int main(){try{
    magneticTests();
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
    for(double angle:{0.,.7})for(int edge:{0,1}){ruler.angle=angle;require(length(ruler.local(ruler.project(ruler.world({-30,50}),edge))-Point{0,edge*RulerGeometry::widthMm})<1e-8,"ruler clamps before start");require(length(ruler.local(ruler.project(ruler.world({ruler.lengthMm+30,-50}),edge))-Point{ruler.lengthMm,edge*RulerGeometry::widthMm})<1e-8,"ruler clamps after end");}
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
