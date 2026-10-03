#pragma once
#include "documents/Document.h"
#include <optional>
namespace scalar {
struct Bounds {
    double left=0,top=0,right=0,bottom=0;
    double width() const {return right-left;}
    double height() const {return bottom-top;}
    Point center() const {return {(left+right)/2,(top+bottom)/2};}
    bool contains(Point p,double tolerance=0) const {return p.x>=left-tolerance&&p.x<=right+tolerance&&p.y>=top-tolerance&&p.y<=bottom+tolerance;}
};
double distanceToSegment(Point p,Point a,Point b);
bool isSimplePolygon(const std::vector<Point>& vertices);
Point rotatePoint(Point p,Point center,double radians);
std::vector<Point> shapeOutline(const ShapeObject& shape,int segments=128);
Bounds bounds(const CanvasObject& object);
bool hitTest(const CanvasObject& object,Point p,double tolerance);
CanvasObject transformed(CanvasObject object,Point center,Point translation,double scaleX=1,double scaleY=1,double rotation=0);
std::string objectId(const CanvasObject& object);
ObjectProperties& properties(CanvasObject& object);
const ObjectProperties& properties(const CanvasObject& object);
using CanvasObjectView=std::variant<const StrokeObject*,const ShapeObject*,const ImageObject*>;
std::vector<CanvasObjectView> objectViews(const Page& page);
std::vector<CanvasObject> objects(const Page& page);
std::optional<CanvasObject> findObject(const Page& page,const std::string& id);
void replaceObject(Page& page,const std::string& id,const std::optional<CanvasObject>& object);
}
