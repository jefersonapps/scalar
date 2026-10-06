#include "ShapeMesh.h"
#include "StrokeMesh.h"
#include "geometry/Geometry.h"
#include <numbers>
namespace scalar {
ShapeMeshClipper::ShapeMeshClipper(std::vector<Point> mesh){
    for(std::size_t i=0;i+2<mesh.size();i+=3){
        std::vector<Point> original{mesh[i],mesh[i+1],mesh[i+2]};
        const double left=std::min({mesh[i].x,mesh[i+1].x,mesh[i+2].x}),right=std::max({mesh[i].x,mesh[i+1].x,mesh[i+2].x});
        const double top=std::min({mesh[i].y,mesh[i+1].y,mesh[i+2].y}),bottom=std::max({mesh[i].y,mesh[i+1].y,mesh[i+2].y});
        cells_.push_back({original,{original},{},left,right,top,bottom});
    }
}
void ShapeMeshClipper::erase(std::span<const ErasedRegion> regions){
    const auto cross=[](Point a,Point b){return a.x*b.y-a.y*b.x;};
    const auto area=[&](std::span<const Point> polygon){double result=0;for(std::size_t i=0;i<polygon.size();++i)result+=cross(polygon[i],polygon[(i+1)%polygon.size()]);return result;};
    const auto separated=[&](std::span<const Point> a,std::span<const Point> b,double winding){
        for(std::size_t i=0;i<a.size();++i){const auto origin=a[i],edge=a[(i+1)%a.size()]-origin;if(length(edge)<1e-10)continue;
            bool outside=true;for(auto p:b)if(winding*cross(edge,p-origin)>1e-10){outside=false;break;}
            if(outside)return true;
        }return false;
    };
    const auto clip=[&](const std::vector<Point>& polygon,Point a,Point b,bool inside){
        std::vector<Point> result;if(polygon.empty())return result;
        auto previous=polygon.back();double pd=cross(b-a,previous-a);bool pin=inside?pd>=0:pd<=0;
        for(auto p:polygon){const double d=cross(b-a,p-a);const bool in=inside?d>=0:d<=0;
            if(in!=pin)result.push_back(previous+(p-previous)*(pd/(pd-d)));
            if(in)result.push_back(p);previous=p;pd=d;pin=in;
        }return result;
    };
    for(const auto& erased:regions){
        std::vector<Point> capsule;const auto delta=erased.to-erased.from;const double angle=std::atan2(delta.y,delta.x);
        for(int half=0;half<2;++half)for(int i=0;i<=16;++i){const double a=angle+std::numbers::pi*(.5+half+i/16.);const auto center=half?erased.to:erased.from;capsule.push_back(center+Point{erased.radius*std::cos(a),erased.radius*std::sin(a)});}
        const double left=std::min(erased.from.x,erased.to.x)-erased.radius,right=std::max(erased.from.x,erased.to.x)+erased.radius;
        const double top=std::min(erased.from.y,erased.to.y)-erased.radius,bottom=std::max(erased.from.y,erased.to.y)+erased.radius;
        for(auto& cell:cells_){
            if(cell.right<left||cell.left>right||cell.bottom<top||cell.top>bottom)continue;
            // Transfer only geometry that actually changes visibility. A restore
            // never subdivides already visible fill, and repeated erases skip holes.
            auto& source=erased.restore?cell.hidden:cell.visible;
            auto& destination=erased.restore?cell.visible:cell.hidden;
            if(source.empty())continue;
            std::vector<std::vector<Point>> output;output.reserve(source.size());
            for(auto& polygon:source){
                double minX=1e99,maxX=-1e99,minY=1e99,maxY=-1e99;
                for(auto p:polygon){minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);}
                if(maxX<left||minX>right||maxY<top||minY>bottom){output.push_back(std::move(polygon));continue;}
                const double winding=area(polygon);if(std::abs(winding)<1e-10)continue;
                if(separated(capsule,polygon,1)||separated(polygon,capsule,winding>0?1:-1)){output.push_back(std::move(polygon));continue;}
                auto remainder=std::move(polygon);
                for(std::size_t edge=0;edge<capsule.size()&&!remainder.empty();++edge){
                    if(length(capsule[(edge+1)%capsule.size()]-capsule[edge])<1e-10)continue;
                    auto outside=clip(remainder,capsule[edge],capsule[(edge+1)%capsule.size()],false);
                    if(outside.size()>=3&&std::abs(area(outside))>1e-10)output.push_back(std::move(outside));
                    remainder=clip(remainder,capsule[edge],capsule[(edge+1)%capsule.size()],true);
                }
                if(remainder.size()>=3&&std::abs(area(remainder))>1e-10)destination.push_back(std::move(remainder));
            }
            source=std::move(output);
            // Recover the original triangle whenever a cell becomes uniform.
            // This discards old cut boundaries rather than accumulating them
            // through erase/restore cycles.
            if(cell.hidden.empty())cell.visible={cell.original};
            else if(cell.visible.empty())cell.hidden={cell.original};
        }
    }
}
std::vector<Point> ShapeMeshClipper::triangles() const {
    std::vector<Point> result;
    for(const auto& cell:cells_)for(const auto& polygon:cell.visible)for(std::size_t i=1;i+1<polygon.size();++i){
        const auto a=polygon[i]-polygon[0],b=polygon[i+1]-polygon[0];
        if(std::abs(a.x*b.y-a.y*b.x)>1e-10)result.insert(result.end(),{polygon[0],polygon[i],polygon[i+1]});
    }return result;
}
std::vector<Point> eraseShapeMesh(std::vector<Point> mesh,std::span<const ErasedRegion> regions){
    if(regions.empty())return mesh;
    ShapeMeshClipper clipper(std::move(mesh));clipper.erase(regions);return clipper.triangles();
}
std::vector<Point> shapeBorderMesh(const ShapeObject& shape){
    // Smooth closed contours need a strip, not a round cap at every sample.
    // The latter overlaps hundreds of disks and forces unnecessary blending at
    // high magnification. Retain the general stroke path for sharp/thick shapes.
    const double halfWidth=shape.style.width(1)/2;
    const double minor=std::min(shape.radiusX,shape.radiusY),major=std::max(shape.radiusX,shape.radiusY);
    if((shape.kind==ShapeKind::Circle||shape.kind==ShapeKind::Ellipse)
        &&shape.style.pattern==LinePattern::Solid&&major>0&&minor*minor/major>halfWidth*2){
        const auto outline=shapeOutline(shape);
        std::vector<Point> inner,outer,mesh;
        inner.reserve(outline.size());outer.reserve(outline.size());mesh.reserve(outline.size()*6);
        for(std::size_t i=0;i<outline.size();++i){
            const auto incoming=outline[i]-outline[(i+outline.size()-1)%outline.size()];
            const auto outgoing=outline[(i+1)%outline.size()]-outline[i];
            const auto n1=Point{-incoming.y,incoming.x}*(1/length(incoming));
            const auto n2=Point{-outgoing.y,outgoing.x}*(1/length(outgoing));
            const double denominator=1+n1.x*n2.x+n1.y*n2.y;
            const auto offset=(n1+n2)*(halfWidth/denominator);
            inner.push_back(outline[i]+offset);outer.push_back(outline[i]-offset);
        }
        for(std::size_t i=0;i<outline.size();++i){const auto next=(i+1)%outline.size();
            mesh.insert(mesh.end(),{outer[i],outer[next],inner[i],inner[i],outer[next],inner[next]});}
        return eraseShapeMesh(std::move(mesh),shape.erasedRegions);
    }
    StrokeObject s{shape.id,shape.style,{}};
    for(auto p:shapeOutline(shape))s.samples.push_back({p});

    if(shape.kind!=ShapeKind::Line&&shape.kind!=ShapeKind::CircularArc&&!s.samples.empty())s.samples.push_back(s.samples.front());
    auto mesh=strokeMesh(s);
    if(shape.kind==ShapeKind::RightAngle&&shape.vertices.size()==4){
        Point center;for(auto p:shape.vertices)center=center+p;center=center*.25;
        const double side=std::min(length(shape.vertices[1]-shape.vertices[0]),length(shape.vertices[3]-shape.vertices[0]));
        const double radius=std::min(side*.10,std::max(.15,shape.style.maxWidthMm*.65));
        for(int i=0;i<32;++i){const double a=2*std::numbers::pi*i/32,b=2*std::numbers::pi*(i+1)/32;
            mesh.insert(mesh.end(),{center,center+Point{radius*std::cos(a),radius*std::sin(a)},center+Point{radius*std::cos(b),radius*std::sin(b)}});}
    }
    return eraseShapeMesh(std::move(mesh),shape.erasedRegions);

}
std::vector<Point> shapeFillMesh(const ShapeObject& shape){
    if(shape.kind==ShapeKind::Line||shape.kind==ShapeKind::CircularArc||shape.fillOpacity<=0)return {};

    const auto p=shapeOutline(shape);
    if(p.size()<3)return {};

    if(shape.kind!=ShapeKind::Polygon){
        std::vector<Point> mesh;for(std::size_t i=1;i+1<p.size();++i)mesh.insert(mesh.end(),{p[0],p[i],p[i+1]});return eraseShapeMesh(std::move(mesh),shape.erasedRegions);
    }
    // Ear clipping supports concave polygons; a triangle fan would fill their notches.
    if(!isSimplePolygon(p))return {};
    const auto cross=[](Point a,Point b){return a.x*b.y-a.y*b.x;};
    double area=0;for(std::size_t i=0;i<p.size();++i)area+=cross(p[i],p[(i+1)%p.size()]);
    const double sign=area>0?1.:-1.;
    std::vector<std::size_t> indices;for(std::size_t i=0;i<p.size();++i)indices.push_back(i);
    std::vector<Point> vertices;
    while(indices.size()>3){
        bool clipped=false;
        for(std::size_t i=0;i<indices.size();++i){
            const auto a=p[indices[(i+indices.size()-1)%indices.size()]],b=p[indices[i]],c=p[indices[(i+1)%indices.size()]];
            if(sign*cross(b-a,c-b)<=1e-10)continue;
            bool occupied=false;
            for(std::size_t j=0;j<indices.size();++j){
                if(j==i||j==(i+1)%indices.size()||j==(i+indices.size()-1)%indices.size())continue;
                const auto v=p[indices[j]];
                if(sign*cross(b-a,v-a)>=-1e-10&&sign*cross(c-b,v-b)>=-1e-10&&sign*cross(a-c,v-c)>=-1e-10){occupied=true;break;}
            }
            if(occupied)continue;
            vertices.insert(vertices.end(),{a,b,c});indices.erase(indices.begin()+std::ptrdiff_t(i));clipped=true;break;
        }
        if(!clipped)return {}; // degenerate or crossing handles never produce invalid fill
    }
    vertices.insert(vertices.end(),{p[indices[0]],p[indices[1]],p[indices[2]]});
    return eraseShapeMesh(std::move(vertices),shape.erasedRegions);

}
}
