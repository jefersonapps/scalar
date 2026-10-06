#include "Geometry.h"
#include <limits>
#include <numbers>
namespace scalar {
double distanceToSegment(Point p,Point a,Point b){
    const auto d=b-a;
    const auto l=d.x*d.x+d.y*d.y;

    if(l<1e-12)return length(p-a);

    const auto t=std::clamp(((p-a).x*d.x+(p-a).y*d.y)/l,0.,1.);
    return length(p-(a+d*t));

}
bool isSimplePolygon(const std::vector<Point>& vertices){
    if(vertices.size()<3)return false;
    const auto cross=[](Point a,Point b){return a.x*b.y-a.y*b.x;};
    double area=0;
    for(std::size_t i=0;i<vertices.size();++i){
        const auto a=vertices[i],b=vertices[(i+1)%vertices.size()];
        if(length(b-a)<1e-8)return false;
        area+=cross(a,b);
        for(std::size_t j=i+1;j<vertices.size();++j){
            if(j==i+1||(i==0&&j+1==vertices.size()))continue;
            const auto c=vertices[j],d=vertices[(j+1)%vertices.size()];
            const auto ab1=cross(b-a,c-a),ab2=cross(b-a,d-a),cd1=cross(d-c,a-c),cd2=cross(d-c,b-c);
            if(ab1*ab2<0&&cd1*cd2<0)return false;
            if(distanceToSegment(a,c,d)<1e-8||distanceToSegment(b,c,d)<1e-8||distanceToSegment(c,a,b)<1e-8||distanceToSegment(d,a,b)<1e-8)return false;
        }
    }
    return std::abs(area)>1e-8;
}
Point rotatePoint(Point p,Point center,double angle){p=p-center;
    return center+Point{p.x*std::cos(angle)-p.y*std::sin(angle),p.x*std::sin(angle)+p.y*std::cos(angle)};
    }
std::vector<Point> shapeOutline(const ShapeObject& s,int segments){
    if(s.kind!=ShapeKind::Circle&&s.kind!=ShapeKind::Ellipse)return s.vertices;

    std::vector<Point> points;
    points.reserve(segments);

    for(int i=0;i<segments;++i){const double a=2*std::numbers::pi*i/segments;
        points.push_back(rotatePoint(s.center+Point{s.radiusX*std::cos(a),s.radiusY*std::sin(a)},s.center,s.rotation));
        }
    return points;

}
std::string objectId(const CanvasObject& o){return std::visit([](const auto& s){return s.id;},o);
    }
ObjectProperties& properties(CanvasObject& o){return std::visit([](auto& s)->ObjectProperties&{return s.properties;},o);
    }
const ObjectProperties& properties(const CanvasObject& o){return std::visit([](const auto& s)->const ObjectProperties&{return s.properties;},o);
    }
Bounds bounds(const CanvasObject& o){
    std::vector<Point> p;

    if(const auto* s=std::get_if<StrokeObject>(&o)){for(const auto& sample:s->samples)p.push_back(sample.position);
        }else if(const auto* shape=std::get_if<ShapeObject>(&o))p=shapeOutline(*shape);
    else std::visit([&](const auto& obj){if constexpr(requires {obj.corners;})p=obj.corners;},o);

    if(p.empty())return {};

    Bounds b{p[0].x,p[0].y,p[0].x,p[0].y};
    for(auto v:p){b.left=std::min(b.left,v.x);
        b.top=std::min(b.top,v.y);
        b.right=std::max(b.right,v.x);
        b.bottom=std::max(b.bottom,v.y);
        }return b;

}
Bounds pageRenderBounds(const Page& page,double marginMm){
    if(!page.size.infinite)return {0,0,page.size.widthMm,page.size.heightMm};
    std::optional<Bounds> extent;
    const auto include=[&](Bounds b){if(!extent)extent=b;else {extent->left=std::min(extent->left,b.left);extent->top=std::min(extent->top,b.top);extent->right=std::max(extent->right,b.right);extent->bottom=std::max(extent->bottom,b.bottom);}};
    for(const auto& view:objectViews(page))std::visit([&](const auto* object){
        if(!object->properties.visible)return;
        // Include the full pen/border width, not just its centerline.
        const auto b=bounds(CanvasObject(*object));
        const double padding=[&]{if constexpr(std::is_same_v<std::decay_t<decltype(*object)>,StrokeObject>||std::is_same_v<std::decay_t<decltype(*object)>,ShapeObject>)return object->style.maxWidthMm*.5;else return 0.;}();
        include({b.left-padding,b.top-padding,b.right+padding,b.bottom+padding});
    },view);
    if(page.pdf){const auto size=page.pdf->size.valid()?page.pdf->size:page.size;include({0,0,size.widthMm,size.heightMm});}
    if(!extent)return {0,0,page.size.widthMm,page.size.heightMm};
    const auto b=*extent;return {b.left-marginMm,b.top-marginMm,b.right+marginMm,b.bottom+marginMm};
}
bool hitTest(const CanvasObject& o,Point p,double tolerance){
    if(!properties(o).visible||properties(o).locked)return false;

    if(const auto* image=std::get_if<ImageObject>(&o)){
        bool erased=false;for(const auto& region:image->erasedRegions)if(distanceToSegment(p,region.from,region.to)<region.radius)erased=!region.restore;
        if(erased)return false;
    }
    const auto width=std::visit([](const auto& s){if constexpr(std::is_same_v<std::decay_t<decltype(s)>,ImageObject>)return 0.;else return s.style.maxWidthMm/2;},o);
    tolerance+=width;

    if(!bounds(o).contains(p,tolerance))return false;

    if(const auto* stroke=std::get_if<StrokeObject>(&o)){
        bool erased=false;for(const auto& region:stroke->erasedRegions)if(distanceToSegment(p,region.from,region.to)<region.radius)erased=!region.restore;if(erased)return false;
        if(stroke->samples.size()==1)return length(p-stroke->samples[0].position)<=tolerance;

        for(std::size_t i=1;i<stroke->samples.size();++i)if(distanceToSegment(p,stroke->samples[i-1].position,stroke->samples[i].position)<=tolerance)return true;

        return false;

    }
    const auto* shape=std::get_if<ShapeObject>(&o);
    if(shape){bool erasedPoint=false;for(const auto& erased:shape->erasedRegions)if(distanceToSegment(p,erased.from,erased.to)<erased.radius)erasedPoint=!erased.restore;if(erasedPoint)return false;}

    std::vector<Point> polygon;if(shape)polygon=shapeOutline(*shape);else std::visit([&](const auto& obj){if constexpr(requires {obj.corners;})polygon=obj.corners;},o);

    const bool line=shape&&(shape->kind==ShapeKind::Line||shape->kind==ShapeKind::CircularArc);

    if(polygon.empty())return false;

    bool inside=false;

    for(std::size_t i=0,j=polygon.size()-1;i<polygon.size();j=i++){
        if(!line||i>0)if(distanceToSegment(p,polygon[j],polygon[i])<=tolerance)return true;

        const auto a=polygon[i],b=polygon[j];
        if((a.y>p.y)!=(b.y>p.y)&&p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x)inside=!inside;

    }
    return !line&&(!shape||shape->fillOpacity>0)&&inside;

}
bool shapeTouchesEraser(const ShapeObject& shape,Point from,Point to,double radius){
    // Contact uses the swept disk, not selection hit testing. A center inside an
    // earlier erased hole can still overlap intact geometry along its rim.
    const auto polygon=shapeOutline(shape);if(polygon.empty())return false;
    const bool closed=shape.kind!=ShapeKind::Line&&shape.kind!=ShapeKind::CircularArc;
    const auto cross=[](Point a,Point b){return a.x*b.y-a.y*b.x;};
    bool insideFrom=false,insideTo=false;
    for(std::size_t i=0;i<polygon.size();++i){
        if(!closed&&i+1==polygon.size())break;
        const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
        const double distance=std::min({distanceToSegment(from,a,b),distanceToSegment(to,a,b),distanceToSegment(a,from,to),distanceToSegment(b,from,to)});
        if(distance<=radius+shape.style.maxWidthMm*.5)return true;
        if(cross(to-from,a-from)*cross(to-from,b-from)<0&&cross(b-a,from-a)*cross(b-a,to-a)<0)return true;
        const auto inside=[&](Point p){return (a.y>p.y)!=(b.y>p.y)&&p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x;};
        if(inside(from))insideFrom=!insideFrom;if(inside(to))insideTo=!insideTo;
    }
    return closed&&shape.fillOpacity>0&&(insideFrom||insideTo);
}
CanvasObject transformed(CanvasObject o,Point center,Point translation,double sx,double sy,double rotation){
    sx=std::max(0.01,sx);
    sy=std::max(0.01,sy);

    const auto move=[&](Point p){p=p-center;
        p={p.x*sx,p.y*sy};
        return rotatePoint(center+p,center,rotation)+translation;
        };

    if(auto* s=std::get_if<StrokeObject>(&o)){for(auto& sample:s->samples)sample.position=move(sample.position);
        for(auto& region:s->erasedRegions){region.from=move(region.from);region.to=move(region.to);region.radius*=std::min(sx,sy);}
        }
    else if(auto* image=std::get_if<ImageObject>(&o)){for(auto& p:image->corners)p=move(p);
        for(auto& region:image->erasedRegions){region.from=move(region.from);region.to=move(region.to);region.radius*=std::min(sx,sy);}
        }
    else if(auto* text=std::get_if<TextObject>(&o)){for(auto& p:text->corners)p=move(p);}
    else {
        auto& shape=std::get<ShapeObject>(o);
        for(auto& erased:shape.erasedRegions){erased.from=move(erased.from);erased.to=move(erased.to);erased.radius*=std::min(sx,sy);}

        if(shape.kind==ShapeKind::Circle||shape.kind==ShapeKind::Ellipse){
            // Nonuniform screen-axis resize of a rotated ellipse requires conic decomposition.
            // Keep radial objects uniformly scaled so they remain parametric and distortion-free.
            const double scale=std::min(sx,sy);
            shape.center=move(shape.center);
            shape.radiusX*=scale;
            shape.radiusY*=scale;
            shape.rotation+=rotation;

        }else {
            for(auto& p:shape.vertices)p=move(p);
            if(shape.kind==ShapeKind::CircularArc||shape.kind==ShapeKind::CircularSector){shape.center=move(shape.center);shape.radiusX*=std::min(sx,sy);shape.radiusY*=std::min(sx,sy);shape.rotation+=rotation;}
        }

    }
    ++properties(o).revision;
    return o;

}
std::vector<CanvasObjectView> objectViews(const Page& page){
    std::vector<CanvasObjectView> result;
    result.reserve(page.strokes.size()+page.shapes.size());

    for(const auto& s:page.strokes)result.emplace_back(&s);
    for(const auto& s:page.shapes)result.emplace_back(&s);
    for(const auto& s:page.images)result.emplace_back(&s);
    for(const auto& s:page.texts)result.emplace_back(&s);

    const auto z=[](const CanvasObjectView& o){return std::visit([](const auto* s){return s->properties.zIndex;},o);
        };

    std::stable_sort(result.begin(),result.end(),[&](const auto& a,const auto& b){return z(a)<z(b);});
    return result;

}
std::vector<CanvasObject> objects(const Page& page){
    std::vector<CanvasObject> result;
    result.reserve(page.strokes.size()+page.shapes.size());

    for(const auto& s:page.strokes)result.emplace_back(s);
    for(const auto& s:page.shapes)result.emplace_back(s);
    for(const auto& s:page.images)result.emplace_back(s);
    for(const auto& s:page.texts)result.emplace_back(s);

    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return properties(a).zIndex<properties(b).zIndex;});
    return result;

}
std::optional<CanvasObject> findObject(const Page& page,const std::string& id){
    for(const auto& s:page.strokes)if(s.id==id)return s;
    for(const auto& s:page.shapes)if(s.id==id)return s;
    for(const auto& s:page.images)if(s.id==id)return s;
    for(const auto& s:page.texts)if(s.id==id)return s;
    return {};

}
void replaceObject(Page& page,const std::string& id,const std::optional<CanvasObject>& object){
    std::erase_if(page.strokes,[&](const auto& s){return s.id==id;});
    std::erase_if(page.shapes,[&](const auto& s){return s.id==id;});

    std::erase_if(page.images,[&](const auto& s){return s.id==id;});
    std::erase_if(page.texts,[&](const auto& s){return s.id==id;});

    if(object)std::visit([&](const auto& s){using T=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<T,StrokeObject>)page.strokes.push_back(s);else if constexpr(std::is_same_v<T,ShapeObject>)page.shapes.push_back(s);else if constexpr(std::is_same_v<T,ImageObject>)page.images.push_back(s);else page.texts.push_back(s);},*object);

}
}
