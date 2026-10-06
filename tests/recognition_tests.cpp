#include "recognition/ShapeRecognizer.h"
#include "commands/History.h"
#include "rendering/ShapeMesh.h"
#include <iostream>
#include <stdexcept>
#include <numbers>
using namespace scalar;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
StrokeObject path(std::vector<Point> vertices,bool closed){
    StrokeObject s{newId(),{}, {}};if(closed)vertices.push_back(vertices.front());
    for(std::size_t i=1;i<vertices.size();++i)for(int j=0;j<20;++j){auto p=vertices[i-1]+(vertices[i]-vertices[i-1])*(j/20.);s.samples.push_back({p});}s.samples.push_back({vertices.back()});return s;
}
int main(){try{
    const auto covers=[](const std::vector<Point>& mesh,Point p){
        const auto cross=[](Point a,Point b){return a.x*b.y-a.y*b.x;};
        for(std::size_t i=0;i+2<mesh.size();i+=3){const auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];
            const auto x=cross(b-a,p-a),y=cross(c-b,p-b),z=cross(a-c,p-c);
            if((x>=-1e-9&&y>=-1e-9&&z>=-1e-9)||(x<=1e-9&&y<=1e-9&&z<=1e-9))return true;}
        return false;
    };
    ShapeObject smooth;smooth.kind=ShapeKind::Circle;smooth.center={50,50};smooth.radiusX=smooth.radiusY=20;
    smooth.style.minWidthMm=smooth.style.maxWidthMm=1;
    const auto ring=shapeBorderMesh(smooth);
    check(ring.size()==128*6,"circle border must remain a bounded strip at high zoom");
    check(covers(ring,{70,50})&&covers(ring,{69.6,50})&&covers(ring,{70.4,50}),"circle border thickness lost");
    check(!covers(ring,{50,50})&&!covers(ring,{69,50})&&!covers(ring,{71,50}),"circle border fills interior or exceeds width");
    double ringArea=0;for(std::size_t i=0;i<ring.size();i+=3){const auto a=ring[i+1]-ring[i],b=ring[i+2]-ring[i];ringArea+=std::abs(a.x*b.y-a.y*b.x)*.5;}
    check(std::abs(ringArea-40*std::numbers::pi)<.1,"circle strip contains overlapping translucent triangles");
    smooth.erasedRegions={{{70,50},{70,50},1}};
    check(!covers(shapeBorderMesh(smooth),{70,50})&&covers(shapeBorderMesh(smooth),{30,50}),"optimized circle border ignores erasure");
    smooth.erasedRegions.clear();smooth.kind=ShapeKind::Ellipse;smooth.radiusY=10;smooth.rotation=.7;
    const auto ellipseRing=shapeBorderMesh(smooth);
    check(ellipseRing.size()==128*6&&covers(ellipseRing,rotatePoint({70,50},smooth.center,.7))&&!covers(ellipseRing,smooth.center),"rotated ellipse strip invalid");
    smooth.style.pattern=LinePattern::Dashed;check(shapeBorderMesh(smooth).size()!=128*6,"ellipse strip discards border pattern");
    smooth.style.pattern=LinePattern::Solid;smooth.radiusY=.2;
    for(auto p:shapeBorderMesh(smooth))check(std::isfinite(p.x)&&std::isfinite(p.y),"thick narrow ellipse produces invalid border");
    ShapeObject patterned;patterned.kind=ShapeKind::Line;patterned.vertices={{0,0},{20,0}};
    patterned.style.maxWidthMm=1;patterned.style.pattern=LinePattern::Dashed;
    auto mesh=shapeBorderMesh(patterned);
    check(covers(mesh,{1,0})&&!covers(mesh,{4,0})&&covers(mesh,{6,0}),"vector dash/gap geometry");
    patterned.style.pattern=LinePattern::Dotted;mesh=shapeBorderMesh(patterned);
    check(covers(mesh,{0,0})&&!covers(mesh,{1.25,0})&&covers(mesh,{2.5,0}),"vector dots/gap geometry");
    patterned.kind=ShapeKind::Rectangle;patterned.vertices={{0,0},{20,0},{20,10},{0,10}};
    check(!shapeBorderMesh(patterned).empty()&&!shapeFillMesh(patterned).empty(),"patterned polygon fill lost");
    patterned.style.pattern=LinePattern::Solid;patterned.erasedRegions={{{10,-2},{10,12},2}};
    check(!covers(shapeFillMesh(patterned),{10,5})&&covers(shapeFillMesh(patterned),{5,5})&&covers(shapeFillMesh(patterned),{15,5}),"shape eraser removes whole fill or leaves swept region");
    check(!covers(shapeBorderMesh(patterned),{10,0})&&covers(shapeBorderMesh(patterned),{5,0}),"shape eraser border clipping");
    check(!hitTest(patterned,{10,5},.1)&&hitTest(patterned,{5,5},.1),"erased shape region still selectable");
    const auto erasedMoved=std::get<ShapeObject>(transformed(patterned,{0,0},{4,5}));
    check(length(erasedMoved.erasedRegions[0].from-Point{14,3})<1e-8&&!covers(shapeFillMesh(erasedMoved),{14,10}),"shape erasure does not follow transforms");
    patterned.erasedRegions={{{5,5},{5,5},1}};
    check(!covers(shapeFillMesh(patterned),{5,5})&&covers(shapeFillMesh(patterned),{8,5}),"shape eraser tap clipping");
    double remainingArea=0;const auto clipped=shapeFillMesh(patterned);
    for(std::size_t i=0;i+2<clipped.size();i+=3){const auto a=clipped[i+1]-clipped[i],b=clipped[i+2]-clipped[i];remainingArea+=std::abs(a.x*b.y-a.y*b.x)*.5;}
    check(std::abs(remainingArea-(200-std::numbers::pi))<.03,"eraser duplicates translucent fill triangles");
    check(shapeTouchesEraser(patterned,{5,5},{5.5,5},1),"eraser rim contact lost inside previous hole");
    check(shapeTouchesEraser(patterned,{10,-1.5},{10,-1.5},1.1),"eraser contact ignores visible border width");
    check(!shapeTouchesEraser(patterned,{10,-5},{10,-5},1),"eraser contacts remote shape");
    auto restoredShape=patterned;restoredShape.erasedRegions.push_back({{5,5},{5,5},.5,true});
    auto restoredMesh=shapeFillMesh(restoredShape);
    check(covers(restoredMesh,{5,5})&&!covers(restoredMesh,{5.8,5})&&hitTest(restoredShape,{5,5},.1),"local restore does not preserve surrounding erasure");
    double restoredArea=0;for(std::size_t i=0;i+2<restoredMesh.size();i+=3){const auto a=restoredMesh[i+1]-restoredMesh[i],b=restoredMesh[i+2]-restoredMesh[i];restoredArea+=std::abs(a.x*b.y-a.y*b.x)*.5;}
    check(std::abs(restoredArea-(200-std::numbers::pi*.75))<.03,"restore duplicates translucent geometry");
    restoredShape.erasedRegions.push_back({{5,5},{5,5},.25,false});
    check(!covers(shapeFillMesh(restoredShape),{5,5})&&covers(shapeFillMesh(restoredShape),{5.4,5}),"erase after restore ignores operation order");
    auto incremental=clipped;const std::vector<ErasedRegion> more{{{5,5},{5.5,5},1},{{5.5,5},{6,5.5},1}};
    for(const auto& region:more)incremental=eraseShapeMesh(std::move(incremental),std::span<const ErasedRegion>(&region,1));
    patterned.erasedRegions.insert(patterned.erasedRegions.end(),more.begin(),more.end());const auto complete=shapeFillMesh(patterned);
    for(double y=3.1;y<7.9;y+=.2)for(double x=3.1;x<8.9;x+=.2)check(covers(incremental,{x,y})==covers(complete,{x,y}),"live incremental erasing differs from committed shape");
    ShapeObject stress;stress.kind=ShapeKind::Circle;stress.center={60,60};stress.radiusX=stress.radiusY=12;
    const auto originalMesh=shapeFillMesh(stress);ShapeMeshClipper clipper(originalMesh);Point previous{60,48};
    std::vector<ErasedRegion> trail;
    for(int i=0;i<400;++i){const double angle=i*.012;const Point p{60+12*std::sin(angle),60-12*std::cos(angle)};
        const ErasedRegion region{previous,p,3};std::vector<ErasedRegion> pending;if(!trail.empty())pending.push_back(trail.back());pending.push_back(region);
        clipper.erase(pending);trail.push_back(region);previous=p;
    }
    const auto liveMesh=clipper.triangles();check(liveMesh.size()<50000,"continuous eraser causes excessive mesh growth");
    check(covers(liveMesh,{60,60}),"continuous eraser removes distant circle center");
    for(double y=50;y<=70;y+=2)for(double x=50;x<=70;x+=2){const Point p{x,y};bool touched=false;
        for(const auto& erased:trail)if(distanceToSegment(p,erased.from,erased.to)<=erased.radius+.02){touched=true;break;}
        if(!touched)check(covers(liveMesh,p)==covers(originalMesh,p),"continuous eraser removes area outside its path");
    }
    auto restoreTrail=trail;for(auto& region:restoreTrail)region.restore=true;
    for(int cycle=0;cycle<3;++cycle){
        clipper.erase(restoreTrail);
        check(clipper.triangles().size()==originalMesh.size(),"restore leaves accumulated cut geometry after a full cycle");
        for(double y=50.1;y<=70;y+=2)for(double x=50.1;x<=70;x+=2)
            check(covers(clipper.triangles(),{x,y})==covers(originalMesh,{x,y}),"restore cycle changes original coverage");
        const auto cleanSize=clipper.triangles().size();
        const ErasedRegion noOp{{60,60},{60,60},3,true};
        for(int i=0;i<20;++i)clipper.erase(std::span<const ErasedRegion>(&noOp,1));
        check(clipper.triangles().size()==cleanSize,"restoring visible geometry creates extra cuts");
        clipper.erase(trail);
        check(clipper.triangles().size()<50000,"erase after restore causes excessive mesh growth");
    }
    auto line=path({{2,3},{40,30}},false);const auto fitted=recognizeShape(line);check(fitted.shape&&fitted.shape->kind==ShapeKind::Line,"line fit");
    for(double rotation:{0.,.7,2.4})for(double tilt:{0.,.12})for(bool reverse:{false,true})for(bool ink:{false,true}){
        const Point corner{60,70};const auto point=[&](Point p){return rotatePoint(p,{0,0},rotation)+corner;};
        Page page;const auto a=point({30,0}),b=point({30*std::sin(tilt),30*std::cos(tilt)});
        if(ink)page.strokes={path({corner,a},false),path({corner,b},false)};
        else {ShapeObject triangle;triangle.id=newId();triangle.kind=ShapeKind::Triangle;triangle.vertices={corner,a,b};page.shapes={triangle};}
        auto mark=path({point({0,4}),point({4.2,3.8}),point({4,0})},false);
        if(reverse)std::reverse(mark.samples.begin(),mark.samples.end());
        const auto result=recognizeRightAngle(page,mark);check(result&&result->kind==ShapeKind::RightAngle,"contextual square angle mark absent");
        check(result->vertices.size()==4&&length(result->vertices[0]-corner)<1e-8&&result->fillOpacity==.10,"right angle closure or fill incorrect");
        const auto center=(result->vertices[0]+result->vertices[2])*.5;
        check(covers(shapeBorderMesh(*result),center),"right angle central dot absent");
        check(covers(shapeFillMesh(*result),center)&&hitTest(*result,center,.1),"right angle fill or hit test absent");
        check(!recognizeRightAngle(Page{},mark),"isolated L recognized as right angle");
        StrokeObject arc;arc.id=newId();for(int i=0;i<=100;++i){const auto t=std::numbers::pi*i/200.;arc.samples.push_back({point({4*std::cos(t),4*std::sin(t)})});}
        check(!recognizeRightAngle(page,arc),"rounded arc recognized as square mark");
        page.shapes.clear();page.strokes={path({corner,a},false)};
        check(!recognizeRightAngle(page,mark),"single reference segment closes right angle");
        page.strokes.push_back(path({corner,point({15,26})},false));
        check(!recognizeRightAngle(page,mark),"nonperpendicular references recognized as right angle");
    }
    for(double sweep:{.5,.8,1.2,1.57,2.3,3.0})for(double direction:{-1.,1.})for(double rotation:{0.,.7}){
        StrokeObject arc{newId(),{}, {}};const Point center{60,70};
        for(int i=0;i<=120;++i){const double a=rotation+direction*sweep*i/120.;
            const double r=20+.04*std::sin(i*.7);arc.samples.push_back({center+Point{r*std::cos(a),r*std::sin(a)}});}
        const auto result=recognizeShape(arc);
        if(!result.shape||result.shape->kind!=ShapeKind::CircularSector)std::cerr<<"arc sweep "<<sweep<<" direction "<<direction<<" rotation "<<rotation<<" -> "<<(result.shape?shapeName(result.shape->kind):"none")<<'\n';
        check(result.shape&&result.shape->kind==ShapeKind::CircularSector,"circular angle mark not recognized");
        check(length(result.shape->vertices[1]-arc.samples.front().position)<1e-8&&length(result.shape->vertices.back()-arc.samples.back().position)<1e-8,"arc endpoints moved");
        check(!shapeFillMesh(*result.shape).empty()&&result.shape->fillOpacity==.10,"sector has no translucent fill");
        const auto inside=(result.shape->center+result.shape->vertices[49])*.5;
        check(hitTest(*result.shape,inside,.1)&&covers(shapeFillMesh(*result.shape),inside),"sector interior not filled");
        const auto moved=transformed(*result.shape,{0,0},{4,5});
        check(length(std::get<ShapeObject>(moved).vertices.front()-result.shape->vertices.front()-Point{4,5})<1e-8,"arc transform");
        check(length(std::get<ShapeObject>(moved).center-result.shape->center-Point{4,5})<1e-8,"sector center transform");
    }
    // An angle mark must close against the existing corner, not its noisy fitted center.
    for(double rotation:{0.,.8})for(bool reverse:{false,true})for(bool ink:{false,true}){
        const Point corner{60,70};
        const auto point=[&](Point p){return rotatePoint(p,{0,0},rotation)+corner;};
        const auto a=point({40,0}),b=point({-12,38});
        Page page;
        if(ink){page.strokes={path({a,corner},false),path({corner,b},false)};}
        else {ShapeObject polygon;polygon.id=newId();polygon.kind=ShapeKind::Triangle;polygon.vertices={corner,a,b};page.shapes.push_back(polygon);}
        StrokeObject arc{newId(),{}, {}};
        const double sweep=std::atan2(38.,-12.);
        for(int i=0;i<=120;++i){const double t=sweep*i/120.;arc.samples.push_back({point({1.0+12*std::cos(t),-.7+12*std::sin(t)})});}
        if(reverse)std::reverse(arc.samples.begin(),arc.samples.end());
        const auto recognized=recognizeShape(arc);check(recognized.shape&&recognized.shape->kind==ShapeKind::CircularSector,"corner arc recognition");
        const auto aligned=alignCircularSector(page,*recognized.shape);
        check(length(aligned.center-corner)<1e-8,"sector center misses existing corner");
        const auto contextual=recognizeShape(arc,page);
        check(contextual.shape&&contextual.shape->kind==ShapeKind::CircularSector&&length(contextual.shape->center-corner)<1e-8,"contextual sector missing with two nearby sides");
        for(const auto& points:std::vector<std::vector<Point>>{
            {point({1503,537}),point({1549,579})},
            {point({1503,537}),point({1549,537}),point({1549,579})}}){
            const auto ordinary=recognizeShape(path(points,false),page);
            check(!ordinary.shape||ordinary.shape->kind!=ShapeKind::CircularSector,"straight or bent segment becomes an angle near triangle");
        }
        const auto isolated=recognizeShape(arc,Page{});
        check(isolated.shape&&isolated.shape->kind==ShapeKind::Line,"isolated curved stroke must straighten to a line");
        check(length(isolated.shape->vertices[0]-arc.samples.front().position)<1e-8&&length(isolated.shape->vertices[1]-arc.samples.back().position)<1e-8,"straightening an isolated curve moves endpoints");
        check(isolated.shape->id==arc.id&&isolated.shape->style.rgba==arc.style.rgba&&isolated.shape->fillOpacity==0,"straightened curve loses identity or style");
        check(distanceToSegment(aligned.vertices[1],corner,reverse?b:a)<1e-8&&distanceToSegment(aligned.vertices.back(),corner,reverse?a:b)<1e-8,"sector endpoints miss straight edges");
        check(aligned.id==recognized.shape->id&&aligned.fillOpacity==.10&&aligned.style.rgba==recognized.shape->style.rgba,"aligned sector style changed");
        for(std::size_t i=1;i<aligned.vertices.size();++i)check(std::abs(length(aligned.vertices[i]-corner)-aligned.radiusX)<1e-8,"aligned sector is not circular");
        const auto repeated=alignCircularSector(page,aligned);
        check(length(repeated.center-aligned.center)<1e-8&&length(repeated.vertices[1]-aligned.vertices[1])<1e-8,"sector alignment not stable");
        const auto unchanged=alignCircularSector(Page{},*recognized.shape);
        check(length(unchanged.center-recognized.shape->center)<1e-8,"isolated sector moved");
        ShapeObject parallel;parallel.kind=ShapeKind::Line;parallel.vertices={point({-20,0}),point({40,0})};page.shapes={parallel};page.strokes.clear();
        check(length(alignCircularSector(page,*recognized.shape).center-recognized.shape->center)<1e-8,"single straight edge invents corner");
        check(recognizeShape(arc,page).shape->kind==ShapeKind::Line,"one nearby side permits a sector");
        auto other=parallel;other.id=newId();other.vertices={point({-20,8}),point({40,8})};page.shapes.push_back(other);
        check(recognizeShape(arc,page).shape->kind==ShapeKind::Line,"parallel sides permit a sector without a corner");
    }
    // A shallow hand-drawn arc can fit a circle centered outside an obtuse corner.
    // Its endpoints and the nearby contour provide the intended angle instead.
    for(double rotation:{0.,.6})for(bool reverse:{false,true}){
        const auto point=[&](Point p){return rotatePoint(p,{52.8,28.3},rotation);};
        const auto corner=point({52.8,28.3}),a=point({85.8,28.1}),b=point({35.7,55.7});
        Page page;ShapeObject polygon;polygon.id=newId();polygon.kind=ShapeKind::Triangle;polygon.vertices={corner,a,b};page.shapes={polygon};
        StrokeObject arc{newId(),{}, {}};
        for(int i=0;i<=120;++i){const double angle=.45+(std::numbers::pi/2-.45)*i/120.;arc.samples.push_back({point({49.4+8.5*std::cos(angle),24.5+8.5*std::sin(angle)})});}
        if(reverse)std::reverse(arc.samples.begin(),arc.samples.end());
        const auto result=recognizeShape(arc);check(result.shape&&result.shape->kind==ShapeKind::CircularSector,"shallow corner arc recognition");
        const auto aligned=alignCircularSector(page,*result.shape);
        check(length(aligned.center-corner)<1e-8,"shallow arc closes outside obtuse corner");
        check(distanceToSegment(aligned.vertices[1],corner,reverse?b:a)<1e-8&&distanceToSegment(aligned.vertices.back(),corner,reverse?a:b)<1e-8,"shallow arc misses contour");
    }
    for(double rotation:{0.,.6})for(bool reverse:{false,true}){
        const Point pivot{143.6,49.};const auto point=[&](Point p){return rotatePoint(p,pivot,rotation);};
        Page page;ShapeObject triangle;triangle.id=newId();triangle.kind=ShapeKind::Triangle;
        triangle.vertices={pivot,point({110.8,20.8}),point({96.2,49.4})};page.shapes={triangle};
        StrokeObject arc{newId(),{}, {}};
        for(int i=0;i<=120;++i){const double angle=-1.85-1.64*i/120.;arc.samples.push_back({point({138.8+3.95*std::cos(angle),47.8+3.95*std::sin(angle)})});}
        if(reverse)std::reverse(arc.samples.begin(),arc.samples.end());
        const auto result=recognizeShape(arc);check(result.shape&&result.shape->kind==ShapeKind::CircularSector,"acute corner arc recognition");
        const auto aligned=alignCircularSector(page,*result.shape);
        check(length(aligned.center-pivot)<1e-8,"acute sector fails to use triangle vertex");
        check(distanceToSegment(aligned.vertices[1],pivot,triangle.vertices[reverse?2:1])<1e-8&&distanceToSegment(aligned.vertices.back(),pivot,triangle.vertices[reverse?1:2])<1e-8,"acute sector fails to close on triangle edges");
    }
    // A tightly curved hand-drawn mark in an acute corner has a small fitted
    // radius. Its short endpoint gap must be judged against the mark's span,
    // not just that radius (regression from the rotated triangle screenshot).
    for(double rotation:{0.,.8,2.4})for(bool reverse:{false,true})for(bool ink:{false,true}){
        const Point corner{19.65,35.25};const auto point=[&](Point p){return rotatePoint(p,corner,rotation);};
        const auto apex=point({41.25,10.10}),base=point({70.85,32.55});
        Page page;
        if(ink)page.strokes={path({point(corner),apex},false),path({point(corner),base},false)};
        else {ShapeObject triangle;triangle.id=newId();triangle.kind=ShapeKind::Triangle;triangle.vertices={point(corner),apex,base};page.shapes={triangle};}
        StrokeObject arc{newId(),{}, {}};
        for(int i=0;i<=120;++i){const double angle=-1.77+2.29*i/120.;arc.samples.push_back({point({25.3+3.01*std::cos(angle),32.7+3.01*std::sin(angle)})});}
        if(reverse)std::reverse(arc.samples.begin(),arc.samples.end());
        const auto result=recognizeShape(arc);check(result.shape&&result.shape->kind==ShapeKind::CircularSector,"tight acute angle arc recognition");
        const auto aligned=alignCircularSector(page,*result.shape);
        check(length(aligned.center-corner)<1e-8,"small endpoint gap leaves acute sector centered inside triangle");
        const auto contextual=recognizeShape(arc,page);
        check(contextual.shape&&length(contextual.shape->center-corner)<1e-8,"context requirement rejects gapped acute angle");
        check(distanceToSegment(aligned.vertices[1],corner,reverse?base:apex)<1e-8&&distanceToSegment(aligned.vertices.back(),corner,reverse?apex:base)<1e-8,"gapped acute angle fails to close on contour");
        check(covers(shapeFillMesh(aligned),(corner+aligned.vertices[49])*.5),"aligned acute angle loses fill");
        // The extra allowance must not use an edge that is actually remote.
        page.shapes.clear();page.strokes={path({corner,apex},false),path({point({19.65,39.25}),point({70.85,36.55})},false)};
        check(length(alignCircularSector(page,*result.shape).center-result.shape->center)<1e-8,"remote edge closes unrelated angle mark");
        check(recognizeShape(arc,page).shape->kind==ShapeKind::Line,"remote second side permits an isolated sector");
    }
    // Uneven small angle mark with a short gap to the lower triangle side.
    for(double rotation:{0.,.8,2.4})for(bool reverse:{false,true}){
        const Point corner{40,70};
        const auto point=[&](Point p){return rotatePoint(corner+(p-Point{1488,603})*.083,corner,rotation);};
        Page page;ShapeObject triangle;triangle.id=newId();triangle.kind=ShapeKind::Triangle;
        triangle.vertices={point({1488,603}),point({1548,334}),point({1768,555})};page.shapes={triangle};
        auto arc=path({point({1503,537}),point({1515,538}),point({1528,542}),point({1540,551}),point({1548,562}),point({1552,573}),point({1551,577}),point({1549,579})},false);
        if(reverse)std::reverse(arc.samples.begin(),arc.samples.end());
        const auto result=recognizeShape(arc,page);
        check(result.shape&&result.shape->kind==ShapeKind::CircularSector,"uneven lower-left angle is not recognized");
        check(length(result.shape->center-corner)<1e-8,"uneven angle misses triangle vertex");
        check(distanceToSegment(result.shape->vertices[1],corner,triangle.vertices[reverse?2:1])<1e-8&&distanceToSegment(result.shape->vertices.back(),corner,triangle.vertices[reverse?1:2])<1e-8,"uneven angle does not close on both sides");
        check(!shapeFillMesh(*result.shape).empty(),"uneven angle loses translucent fill");
        const auto isolated=recognizeShape(arc,Page{});
        check(!isolated.shape||isolated.shape->kind!=ShapeKind::CircularSector,"uneven isolated curve invents a sector");
    }
    for(const auto& vertices:std::vector<std::vector<Point>>{{{0,0},{2,8},{6,14},{14,18},{50,24}},{{0,0},{10,5},{20,-5},{30,0}},{{0,0},{10,0},{10,10}},{{69,57.1},{70.2,58},{73,59.1},{78,60.5},{83,62},{89,64},{95,66.5},{101,69.5},{106,72},{109,75.5},{112.4,79.1}}}){
        const auto result=recognizeShape(path(vertices,false));check(result.shape&&result.shape->kind!=ShapeKind::CircularSector,"bent segment or free curve recognized as arc");
    }
    for(const double sweep:{1.2,2.3})for(const int seam:{0,35}){
        std::vector<Point> points{{60,70}};
        for(int i=0;i<=100;++i){const double a=.6+sweep*i/100.;points.push_back({60+20*std::cos(a),70+20*std::sin(a)});}
        auto sector=path(points,true);sector.samples.pop_back();std::rotate(sector.samples.begin(),sector.samples.begin()+seam,sector.samples.end());sector.samples.push_back(sector.samples.front());
        const auto result=recognizeShape(sector);
        check(result.shape&&result.shape->kind==ShapeKind::CircularSector,"closed pizza slice not recognized");
        check(result.shape->fillOpacity==.10,"closed sector opacity");
    }
    for(auto [rx,ry]:std::vector<std::pair<double,double>>{{20,20},{30,12},{40,4},{40,2},{40,1}}){
        StrokeObject s{newId(),{}, {}};
        for(int i=0;i<=180;++i){double a=2*std::numbers::pi*i/180;auto p=Point{80+rx*std::cos(a),90+ry*std::sin(a)};p=rotatePoint(p,{80,90},0.4);p.x+=0.12*std::sin(i*5.);s.samples.push_back({p});}
        const auto r=recognizeShape(s);check(r.shape.has_value(),"ellipse/circle absent");if(r.shape->kind!=(rx==ry?ShapeKind::Circle:ShapeKind::Ellipse))std::cerr<<"round kind "<<rx<<","<<ry<<" -> "<<shapeName(r.shape->kind)<<"\n";check(r.shape->kind==(rx==ry?ShapeKind::Circle:ShapeKind::Ellipse),"ellipse/circle kind");
    }
    for(auto [vertices,kind]:std::vector<std::pair<std::vector<Point>,ShapeKind>>{{{{0,0},{40,0},{40,20},{0,20}},ShapeKind::Rectangle},{{{0,0},{20,0},{20,20},{0,20}},ShapeKind::Square},{{{0,30},{20,0},{40,30}},ShapeKind::Triangle}}){
        for(auto& p:vertices)p=rotatePoint(p,{20,20},0.43)+Point{30,40};const auto r=recognizeShape(path(vertices,true));check(r.shape&&r.shape->kind==kind,"polygon recognition");
    }
    // Bowed sides, seam in the middle of an edge, rotation and uneven sampling.
    for(const auto kind : {ShapeKind::Square,ShapeKind::Triangle}) {
        const std::vector<Point> corners=kind==ShapeKind::Square
            ?std::vector<Point>{{0,0},{40,0},{40,40},{0,40}}
            :std::vector<Point>{{0,0},{45,35},{5,45}};
        StrokeObject curved{newId(),{}, {}};
        for(std::size_t edge=0;edge<corners.size();++edge){
            const auto a=corners[edge],b=corners[(edge+1)%corners.size()];
            const auto delta=b-a;const Point normal{-delta.y/length(delta),delta.x/length(delta)};
            for(int i=0;i<80;++i){const double t=i/80.;
                const double bow=edge==0 ? (kind==ShapeKind::Square?5.:9.) : 0.8;
                auto p=a+delta*t+normal*(bow*std::sin(std::numbers::pi*t));
                curved.samples.push_back({rotatePoint(p,{20,20},0.35)+Point{40,50}});
            }
        }
        curved.samples.push_back(curved.samples.front());
        for(const int seam : {0,35,100}) {
            auto shifted=curved;shifted.samples.pop_back();
            std::rotate(shifted.samples.begin(),shifted.samples.begin()+seam,shifted.samples.end());
            shifted.samples.push_back(shifted.samples.front());
            const auto result=recognizeShape(shifted);
            if(!result.shape||result.shape->kind!=kind)std::cerr<<"Expected "<<shapeName(kind)<<" seam "<<seam<<" got "<<(result.shape?shapeName(result.shape->kind):"none")<<" confidence "<<result.confidence<<"\n";
            check(result.shape&&result.shape->kind==kind&&result.confidence>=0.70,"bowed polygon recognition");
        }
    }
    // Slanted quadrilaterals with bowed sides must retain their four corners.
    for(const double bowScale : {1.,2.5})for(const double rotation : {0.,0.65})for(const int seam : {0,35,110})for(const bool reverse : {false,true}){
        const std::vector<Point> vertices{{0,15},{38,0},{38,24},{0,39}};
        StrokeObject parallelogram{newId(),{}, {}};
        for(std::size_t edge=0;edge<vertices.size();++edge){
            const auto a=vertices[edge],delta=vertices[(edge+1)%vertices.size()]-a;
            const Point normal{-delta.y/length(delta),delta.x/length(delta)};
            for(int i=0;i<80;++i){const double t=i/80.;
                const double bow=bowScale*(edge%2==0?1.3:0.6);
                const auto p=a+delta*t+normal*(bow*std::sin(std::numbers::pi*t)+0.08*std::sin(i*2.1));
                parallelogram.samples.push_back({rotatePoint(p,{20,20},rotation)+Point{50,60}});
            }
        }
        std::rotate(parallelogram.samples.begin(),parallelogram.samples.begin()+seam,parallelogram.samples.end());
        if(reverse)std::reverse(parallelogram.samples.begin(),parallelogram.samples.end());
        parallelogram.samples.push_back(parallelogram.samples.front());
        const auto result=recognizeShape(parallelogram);
        check(result.shape&&result.shape->kind==ShapeKind::Polygon&&result.shape->vertices.size()==4,"parallelogram misclassified as round shape");
        const auto& fitted=result.shape->vertices;
        check(length((fitted[1]-fitted[0])+(fitted[3]-fitted[2]))<1e-8,"parallelogram opposite sides not parallel");
        check(result.confidence>=0.70,"parallelogram confidence");
    }
    for(const int seam:{0,70,155}){
        StrokeObject round{newId(),{}, {}};
        for(int i=0;i<240;++i){const double a=2*std::numbers::pi*i/240;
            const double r=25+1.2*std::sin(3*a)+0.25*std::sin(17*a);
            round.samples.push_back({{60+r*std::cos(a),70+r*std::sin(a)}});
        }
        std::rotate(round.samples.begin(),round.samples.begin()+seam,round.samples.end());round.samples.push_back(round.samples.front());
        const auto result=recognizeShape(round);check(result.shape&&result.shape->kind==ShapeKind::Circle,"wobbly circle recognized as polygon");
    }
    auto shaky=path({{0,0},{40,0},{39,2},{41,-1},{40,1}},false);
    const auto straightened=recognizeShape(shaky);check(straightened.shape&&straightened.shape->kind==ShapeKind::Line,"terminal tremor prevents line recognition");
    auto bent=path({{0,0},{2,8},{6,14},{14,18},{50,24}},false);
    for(const bool reverse : {false,true}){
        if(reverse)std::reverse(bent.samples.begin(),bent.samples.end());
        const auto result=recognizeShape(bent);
        check(result.shape&&result.shape->kind==ShapeKind::Line,"bent open stroke not straightened");
        check(length(result.shape->vertices[0]-bent.samples.front().position)<1e-8,"straightened line origin moved");
        check(length(result.shape->vertices[1]-bent.samples.back().position)<1e-8,"straightened line endpoint moved");
    }
    std::vector<Point> pentagon;
    for(int i=0;i<5;++i){const auto angle=2*std::numbers::pi*i/5;pentagon.push_back({20*std::cos(angle),20*std::sin(angle)});}
    const auto pentagonFit=recognizeShape(path(pentagon,true));
    check(pentagonFit.shape&&pentagonFit.shape->kind==ShapeKind::Polygon&&pentagonFit.shape->vertices.size()==5,"pentagon recognition");
    for(const int sides:{6,8,10,12}){
        std::vector<Point> polygon;for(int i=0;i<sides;++i){const auto angle=2*std::numbers::pi*i/sides;polygon.push_back({40*std::cos(angle),40*std::sin(angle)});}
        const auto result=recognizeShape(path(polygon,true));
        if(!result.shape||result.shape->kind!=ShapeKind::Polygon)std::cerr<<"sides "<<sides<<" kind "<<(result.shape?shapeName(result.shape->kind):"none")<<"\n";
        check(result.shape&&result.shape->kind==ShapeKind::Polygon,"many-sided polygon recognition");
        check(result.shape->vertices.size()==std::size_t(sides),"many-sided polygon lost corners");
    }
    for(auto polygon:std::vector<std::vector<Point>>{{{10,0},{30,0},{45,40},{0,40}},{{0,0},{40,0},{40,40},{30,40},{30,10},{10,10},{10,40},{0,40}}}){
        const auto result=recognizeShape(path(polygon,true));check(result.shape.has_value(),"hold produced no shape");
        check(result.shape->kind!=ShapeKind::Polygon,"arbitrary polygon recognition retained");
    }
    Page assembled;
    for(const auto& vertices:std::vector<std::vector<Point>>{{{0,0},{40,0}},{{40.8,0.5},{40,30}},{{40,30.8},{0,30}},{{0.6,30},{0.2,0.8}}}){
        ShapeObject edge;edge.id=newId();edge.kind=ShapeKind::Line;edge.vertices=vertices;assembled.shapes.push_back(edge);
    }
    const auto closed=closeConnectedLines(assembled,assembled.shapes.back().id);check(closed&&closed->lineIds.size()==4&&closed->polygon.vertices.size()==4&&closed->polygon.fillOpacity>0,"nearby lines not closed");
    auto disconnected=assembled;disconnected.shapes[0].vertices[0]={10,10};check(!closeConnectedLines(disconnected,disconnected.shapes.back().id),"disconnected lines joined");
    ShapeObject concave;concave.kind=ShapeKind::Polygon;
    concave.vertices={{0,0},{40,0},{40,40},{30,40},{30,10},{10,10},{10,40},{0,40}};
    const auto concaveMesh=shapeFillMesh(concave);
    check(covers(concaveMesh,{5,25})&&!covers(concaveMesh,{20,25}),"concave fill covers notch");
    double meshArea=0;for(std::size_t i=0;i+2<concaveMesh.size();i+=3){const auto a=concaveMesh[i+1]-concaveMesh[i],b=concaveMesh[i+2]-concaveMesh[i];meshArea+=std::abs(a.x*b.y-a.y*b.x)/2;}
    check(std::abs(meshArea-1000)<1e-6,"concave triangulation area");
    check(recognizeShape(path({{0,0},{40,40},{0,40},{40,0}},true)).shape.has_value(),"hold must produce fallback for crossing gesture");
    check(recognizeShape(path({{0,0},{10,30},{20,0},{30,30},{40,0},{0,0}},false)).shape.has_value(),"hold must produce fallback for ambiguous gesture");
    check(simplifyRdp({{0,0},{1,0.01},{2,0}},0.1).size()==2,"RDP collinear");
    auto samples=resample({{0,0},{0,0},{10,0}},11);check(samples.size()==11&&std::abs(samples[5].x-5)<1e-8,"resampling duplicate");
    Page page;History history;history.add(page,line);history.apply(page,{{CanvasObject(line),CanvasObject(*fitted.shape)}},CommandKind::ConvertStrokeToShape);
    check(page.strokes.empty()&&page.shapes.size()==1,"conversion");history.undo(page);check(page.shapes.empty()&&page.strokes.size()==1&&page.strokes[0].samples.size()==line.samples.size(),"undo restore stroke");history.redo(page);check(page.shapes.size()==1,"redo shape");
    auto moved=transformed(*fitted.shape,{0,0},{5,8});history.apply(page,{{CanvasObject(*fitted.shape),moved}},CommandKind::TransformObject);history.undo(page);
    check(length(page.shapes[0].vertices[0]-fitted.shape->vertices[0])<1e-8,"undo move");
    check(hitTest(*fitted.shape,{20,17},2),"line hit");check(!hitTest(*fitted.shape,{100,100},2),"line miss");
    std::cout<<"PASS: line, circle, ellipse, rotated and bowed triangle/rectangle/square/parallelogram, seam independence, regular polygons, concave stored fill, explicit hold fallback, connected lines, RDP, resampling, conversion undo, transform, hit test\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
