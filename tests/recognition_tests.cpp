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
            if((x>=0&&y>=0&&z>=0)||(x<=0&&y<=0&&z<=0))return true;}
        return false;
    };
    ShapeObject patterned;patterned.kind=ShapeKind::Line;patterned.vertices={{0,0},{20,0}};
    patterned.style.maxWidthMm=1;patterned.style.pattern=LinePattern::Dashed;
    auto mesh=shapeBorderMesh(patterned);
    check(covers(mesh,{1,0})&&!covers(mesh,{4,0})&&covers(mesh,{6,0}),"vector dash/gap geometry");
    patterned.style.pattern=LinePattern::Dotted;mesh=shapeBorderMesh(patterned);
    check(covers(mesh,{0,0})&&!covers(mesh,{1.25,0})&&covers(mesh,{2.5,0}),"vector dots/gap geometry");
    patterned.kind=ShapeKind::Rectangle;patterned.vertices={{0,0},{20,0},{20,10},{0,10}};
    check(!shapeBorderMesh(patterned).empty()&&!shapeFillMesh(patterned).empty(),"patterned polygon fill lost");
    auto line=path({{2,3},{40,30}},false);const auto fitted=recognizeShape(line);check(fitted.shape&&fitted.shape->kind==ShapeKind::Line,"line fit");
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
    for(const int seam:{0,70,155}){
        StrokeObject round{newId(),{}, {}};
        for(int i=0;i<240;++i){const double a=2*std::numbers::pi*i/240;
            const double r=25+1.2*std::sin(3*a)+0.25*std::sin(17*a);
            round.samples.push_back({{60+r*std::cos(a),70+r*std::sin(a)}});
        }
        std::rotate(round.samples.begin(),round.samples.begin()+seam,round.samples.end());round.samples.push_back(round.samples.front());
        const auto result=recognizeShape(round);check(result.shape&&result.shape->kind==ShapeKind::Circle,"wobbly circle recognized as polygon");
    }
    for(const double bow:{1.0,2.5,4.0}){
        const std::vector<Point> vertices{{15,0},{40,0},{25,35},{0,35}};
        StrokeObject parallelogram{newId(),{}, {}};
        for(std::size_t edge=0;edge<4;++edge)for(int i=0;i<80;++i){
            const double t=i/80.;const auto a=vertices[edge],d=vertices[(edge+1)%4]-a;
            const Point n{-d.y/length(d),d.x/length(d)};
            parallelogram.samples.push_back({a+d*t+n*(edge==1?bow*std::sin(std::numbers::pi*t):0)});
        }
        parallelogram.samples.push_back(parallelogram.samples.front());
        const auto result=recognizeShape(parallelogram);
        check(result.shape&&result.shape->kind==ShapeKind::Polygon&&result.shape->vertices.size()==4,"bowed parallelogram gained a side");
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
    std::vector<Point> star;
    for(int i=0;i<24;++i){const double a=2*std::numbers::pi*i/24,r=i%2?20:40;star.push_back({r*std::cos(a),r*std::sin(a)});}
    const auto starFit=recognizeShape(path(star,true));
    check(starFit.shape&&starFit.shape->kind==ShapeKind::Polygon&&starFit.shape->vertices.size()==24,"24 sharp corners lost");
    for(auto polygon:std::vector<std::vector<Point>>{
        {{10,0},{30,0},{45,40},{0,40}}, // trapezoid
        {{0,0},{40,0},{40,40},{30,40},{30,10},{10,10},{10,40},{0,40}}, // concave U
        {{0,0},{25,0},{40,15},{30,30},{5,40},{-10,20}},
        {{0,10},{25,10},{25,0},{45,20},{25,40},{25,30},{0,30}}}){
        for(auto& point:polygon)point=rotatePoint(point,{20,20},0.35)+Point{30,50};
        for(const bool reversed:{false,true}){
            if(reversed)std::reverse(polygon.begin(),polygon.end());
            const auto result=recognizeShape(path(polygon,true));
            check(result.shape&&result.shape->kind==ShapeKind::Polygon,"generic polygon recognition");
            check(result.shape->vertices.size()==polygon.size(),"generic polygon lost vertices");
        }
    }
    ShapeObject concave;concave.kind=ShapeKind::Polygon;
    concave.vertices={{0,0},{40,0},{40,40},{30,40},{30,10},{10,10},{10,40},{0,40}};
    const auto concaveMesh=shapeFillMesh(concave);
    check(covers(concaveMesh,{5,25})&&!covers(concaveMesh,{20,25}),"concave fill covers notch");
    double meshArea=0;for(std::size_t i=0;i+2<concaveMesh.size();i+=3){const auto a=concaveMesh[i+1]-concaveMesh[i],b=concaveMesh[i+2]-concaveMesh[i];meshArea+=std::abs(a.x*b.y-a.y*b.x)/2;}
    check(std::abs(meshArea-1000)<1e-6,"concave triangulation area");
    check(!recognizeShape(path({{0,0},{40,40},{0,40},{40,0}},true)).shape,"crossing polygon recognized");
    check(!recognizeShape(path({{0,0},{10,30},{20,0},{30,30},{40,0},{0,0}},false)).shape,"scribble recognized");
    check(simplifyRdp({{0,0},{1,0.01},{2,0}},0.1).size()==2,"RDP collinear");
    auto samples=resample({{0,0},{0,0},{10,0}},11);check(samples.size()==11&&std::abs(samples[5].x-5)<1e-8,"resampling duplicate");
    Page page;History history;history.add(page,line);history.apply(page,{{CanvasObject(line),CanvasObject(*fitted.shape)}},CommandKind::ConvertStrokeToShape);
    check(page.strokes.empty()&&page.shapes.size()==1,"conversion");history.undo(page);check(page.shapes.empty()&&page.strokes.size()==1&&page.strokes[0].samples.size()==line.samples.size(),"undo restore stroke");history.redo(page);check(page.shapes.size()==1,"redo shape");
    auto moved=transformed(*fitted.shape,{0,0},{5,8});history.apply(page,{{CanvasObject(*fitted.shape),moved}},CommandKind::TransformObject);history.undo(page);
    check(length(page.shapes[0].vertices[0]-fitted.shape->vertices[0])<1e-8,"undo move");
    check(hitTest(*fitted.shape,{20,17},2),"line hit");check(!hitTest(*fitted.shape,{100,100},2),"line miss");
    std::cout<<"PASS: line, circle, ellipse, rotated and bowed triangle/rectangle/square, seam independence, generic convex/concave polygons, concave fill, scribble rejection, RDP, resampling, conversion undo, transform, hit test\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
