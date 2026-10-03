#include "ShapeMesh.h"
#include "StrokeMesh.h"
#include "geometry/Geometry.h"
namespace scalar {
std::vector<Point> shapeBorderMesh(const ShapeObject& shape){
    StrokeObject s{shape.id,shape.style,{}};
    for(auto p:shapeOutline(shape))s.samples.push_back({p});

    if(shape.kind!=ShapeKind::Line&&!s.samples.empty())s.samples.push_back(s.samples.front());
    return strokeMesh(s);

}
std::vector<Point> shapeFillMesh(const ShapeObject& shape){
    if(shape.kind==ShapeKind::Line||shape.fillOpacity<=0)return {};

    const auto p=shapeOutline(shape);
    if(p.size()<3)return {};

    if(shape.kind!=ShapeKind::Polygon){
        std::vector<Point> mesh;for(std::size_t i=1;i+1<p.size();++i)mesh.insert(mesh.end(),{p[0],p[i],p[i+1]});return mesh;
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
    return vertices;

}
}
