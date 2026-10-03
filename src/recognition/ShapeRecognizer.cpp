#include "ShapeRecognizer.h"
#include <array>
#include <numbers>
#include <functional>
#include <unordered_set>
namespace scalar {
namespace {
double pathLength(const std::vector<Point>& p){double len=0;
        for(std::size_t i=1;i<p.size();++i)len+=length(p[i]-p[i-1]);
        return len;
        }
void rdp(const std::vector<Point>& p,std::size_t first,std::size_t last,double epsilon,std::vector<Point>& out){
    if(last<=first+1){out.push_back(p[first]);
            return;
            }
    double max=0;
        std::size_t split=first;

    for(std::size_t i=first+1;i<last;++i){const auto d=distanceToSegment(p[i],p[first],p[last]);
            if(d>max){max=d;
                split=i;
                }}
    if(max>epsilon){rdp(p,first,split,epsilon,out);
            rdp(p,split,last,epsilon,out);
            }else out.push_back(p[first]);

}
std::optional<Point> circleCenter(const std::vector<Point>& p,Point mean){
    double a[3][4]{};

    for(auto v:p){v=v-mean;
            const std::array<double,3> row{v.x,v.y,1};
            const auto rhs=-(v.x*v.x+v.y*v.y);

        for(int i=0;i<3;++i){for(int j=0;j<3;++j)a[i][j]+=row[i]*row[j];
                a[i][3]+=row[i]*rhs;
                }}
    for(int i=0;i<3;++i){int pivot=i;
            for(int j=i+1;j<3;++j)if(std::abs(a[j][i])>std::abs(a[pivot][i]))pivot=j;

        if(std::abs(a[pivot][i])<1e-9)return {};

        for(int k=i;k<4;++k)std::swap(a[i][k],a[pivot][k]);
            const auto div=a[i][i];
            for(int k=i;k<4;++k)a[i][k]/=div;

        for(int j=0;j<3;++j)if(j!=i){const auto f=a[j][i];
                for(int k=i;k<4;++k)a[j][k]-=f*a[i][k];
                }}
    return mean+Point{-a[0][3]/2,-a[1][3]/2};

}
}
std::vector<Point> resample(const std::vector<Point>& points,std::size_t count){
    if(points.empty()||count<2)return points;

    const auto len=pathLength(points);
    if(len<1e-8)return {points.front()};

    std::vector<Point> out;
    out.reserve(count);
    out.push_back(points.front());

    const auto step=len/(count-1);
    double distance=0;
    std::size_t segment=1;

    for(std::size_t i=1;i+1<count;++i){const double target=i*step;

        while(segment<points.size()&&distance+length(points[segment]-points[segment-1])<target){distance+=length(points[segment]-points[segment-1]);
            ++segment;
            }
        if(segment>=points.size())break;
        const auto d=points[segment]-points[segment-1];
        const auto l=length(d);

        out.push_back(l>1e-9?points[segment-1]+d*((target-distance)/l):points[segment]);
        }
    out.push_back(points.back());
    return out;

}
std::vector<Point> simplifyRdp(const std::vector<Point>& p,double epsilon){
    if(p.size()<3)return p;
    std::vector<Point> result;
    rdp(p,0,p.size()-1,epsilon,result);
    result.push_back(p.back());
    return result;

}
static RecognitionResult recognizeConfidentShape(const StrokeObject& stroke){
    if(stroke.samples.size()<4)return {};

    std::vector<Point> raw;
    raw.reserve(stroke.samples.size());
    for(const auto& sample:stroke.samples)raw.push_back(sample.position);

    auto p=resample(raw,512);
    if(p.size()<4)return {};

    const double len=pathLength(p),chord=length(p.back()-p.front());

    ShapeObject shape;
    shape.id=stroke.id;
    shape.style=stroke.style;
    shape.properties=stroke.properties;
    shape.properties.revision++;

    if(chord>=3&&len/chord<1.15){
        double residual=0;
        for(auto v:p)residual=std::max(residual,distanceToSegment(v,p.front(),p.back()));

        if(residual/chord<0.045){shape.kind=ShapeKind::Line;
            shape.vertices={p.front(),p.back()};
            shape.fillOpacity=0;
            return {shape,1-residual/chord*4};
            }}
    const auto b=bounds(CanvasObject(stroke));
    const double diagonal=std::hypot(b.width(),b.height());

    if(diagonal<4||chord>diagonal*0.18||len<diagonal*1.7)return {};

    Point mean;
    for(auto v:p)mean=mean+v;
    mean=mean*(1./p.size());

    // Split a closed path far from its start;
    // Merge collinear vertices at the seam.
    std::size_t far=1;
    for(std::size_t i=2;i+1<p.size();++i)if(length(p[i]-p[0])>length(p[far]-p[0]))far=i;

    // Smooth circles have gradual turns. Several localized sharp turns indicate
    // a polygon rather than a circle or ellipse.
    const auto count=p.size()-1;
    std::vector<Point> smooth(count);
    for(std::size_t i=0;i<count;++i){
        for(int k=-3;k<=3;++k){const auto index=std::size_t((int(i)+k+int(count))%int(count));smooth[i]=smooth[i]+p[index];}
        smooth[i]=smooth[i]*(1./7);
    }
    std::vector<double> turns(count);
    for(std::size_t i=0;i<count;++i){
        const auto incoming=smooth[i]-smooth[(i+count-10)%count];
        const auto outgoing=smooth[(i+10)%count]-smooth[i];
        turns[i]=std::abs(std::atan2(incoming.x*outgoing.y-incoming.y*outgoing.x,
                                   incoming.x*outgoing.x+incoming.y*outgoing.y));
    }
    // Count contiguous corner regions rather than several peaks in the same
    // curved end of an ellipse. Bridge short gaps introduced by input jitter.
    std::size_t seam=0;while(seam<count&&turns[seam]>0.38)++seam;
    std::vector<std::size_t> corners;
    std::size_t last=count;
    for(std::size_t step=0;step<count&&seam<count;++step){
        const auto index=(seam+step)%count;if(turns[index]<=0.38)continue;
        if(last==count||step-last>4)corners.push_back(index);
        last=step;
    }

    RecognitionResult polygonFallback,regularPolygon;
    double regularScore=1e9;
    // Try progressively larger simplification scales for bowed handwritten sides.
    // Keep a relaxed polygon candidate while checking for smooth round shapes.
    for (const double tolerance : {0.003, 0.006, 0.012, 0.022, 0.035, 0.055, 0.08, 0.11, 0.15}) {
    shape.vertices.clear();
    auto left=simplifyRdp(std::vector<Point>(p.begin(),p.begin()+std::ptrdiff_t(far+1)),diagonal*tolerance);

    auto right=simplifyRdp(std::vector<Point>(p.begin()+std::ptrdiff_t(far),p.end()),diagonal*tolerance);

    left.pop_back();
    left.insert(left.end(),right.begin(),right.end());
    left.pop_back();

    bool changed=true;

    while(changed&&left.size()>3){changed=false;
        for(std::size_t i=0;i<left.size();++i){
        if(distanceToSegment(left[i],left[(i+left.size()-1)%left.size()],left[(i+1)%left.size()])<diagonal*tolerance){left.erase(left.begin()+std::ptrdiff_t(i));
                changed=true;
                break;
                }}}
    // A gentle change in direction belongs to the same bowed side. Keep only
    // meaningful corners (about 29 degrees), independent of RDP distance.
    changed=true;
    while(changed&&left.size()>3){changed=false;
        for(std::size_t i=0;i<left.size();++i){
            const auto incoming=left[i]-left[(i+left.size()-1)%left.size()];
            const auto outgoing=left[(i+1)%left.size()]-left[i];
            const auto turn=std::abs(std::atan2(incoming.x*outgoing.y-incoming.y*outgoing.x,incoming.x*outgoing.x+incoming.y*outgoing.y));
            if(turn<0.50){left.erase(left.begin()+std::ptrdiff_t(i));changed=true;break;}
        }
    }
    if(left.size()>=3&&isSimplePolygon(left)){
        bool convex=true;
        double sign=0,perimeter=0;

        for(std::size_t i=0;i<left.size();++i){const auto a=left[(i+1)%left.size()]-left[i],c=left[(i+2)%left.size()]-left[(i+1)%left.size()];

            const auto cross=a.x*c.y-a.y*c.x;
            if(std::abs(cross)<diagonal*diagonal*0.01||sign*cross<0)convex=false;
            sign=cross;
            perimeter+=length(a);
            }
        double error=0, maximumError=0;
        for(auto v:p){double nearest=diagonal;
            for(std::size_t i=0;i<left.size();++i)nearest=std::min(nearest,distanceToSegment(v,left[i],left[(i+1)%left.size()]));
            error+=nearest*nearest;
            maximumError=std::max(maximumError,nearest/diagonal);
            }
        error=std::sqrt(error/p.size())/diagonal;

        const double score=error+0.05*std::abs(double(left.size())-double(corners.size()));
        if(convex&&left.size()>=5&&left.size()<=12&&error<0.035&&maximumError<0.12&&len/perimeter<1.22&&score<regularScore){
            Point center;for(auto v:left)center=center+v;center=center*(1./left.size());
            double area=0;for(std::size_t i=0;i<left.size();++i){const auto a=left[i]-center,b=left[(i+1)%left.size()]-center;area+=a.x*b.y-a.y*b.x;}
            const double direction=area>0?1.:-1.;Point correlation;
            for(std::size_t i=0;i<left.size();++i){const double a=direction*2*std::numbers::pi*i/left.size();const auto v=left[i]-center;
                correlation=correlation+Point{v.x*std::cos(a)+v.y*std::sin(a),v.y*std::cos(a)-v.x*std::sin(a)};}
            const double angle=std::atan2(correlation.y,correlation.x),radius=length(correlation)/left.size();
            std::vector<Point> regular;double deviation=0;
            for(std::size_t i=0;i<left.size();++i){const double a=angle+direction*2*std::numbers::pi*i/left.size();regular.push_back(center+Point{radius*std::cos(a),radius*std::sin(a)});deviation+=std::pow(length(left[i]-regular.back()),2);}
            deviation=std::sqrt(deviation/left.size())/std::max(radius,0.01);
            if(deviation<0.22){auto polygon=shape;polygon.kind=ShapeKind::Polygon;polygon.vertices=std::move(regular);
                regularPolygon={polygon,1-error*4};regularScore=score;}
        }
        if(convex&&error<0.065&&maximumError<0.17&&len/perimeter<1.22){
            if(left.size()==3&&corners.size()==3){shape.kind=ShapeKind::Triangle;
                shape.vertices=left;
                if(error<0.035) return {shape,1-error*4};
                if(!polygonFallback.shape) polygonFallback={shape,1-error*4};
                continue;
                }
            if(left.size()!=4||corners.size()!=4)continue;
            bool rightAngles=true;
            for(int i=0;i<4;++i){const auto a=left[(i+1)%4]-left[i],c=left[(i+2)%4]-left[(i+1)%4];
                if(std::abs(a.x*c.x+a.y*c.y)/(length(a)*length(c))>0.32)rightAngles=false;
                }
            if(rightAngles){
                Point center;
                for(auto v:left)center=center+v;
                center=center*0.25;

                const auto edge=left[1]-left[0];
                const auto angle=std::atan2(edge.y,edge.x);

                double w=(length(left[1]-left[0])+length(left[3]-left[2]))/2,h=(length(left[2]-left[1])+length(left[0]-left[3]))/2;

                shape.kind=std::max(w,h)/std::min(w,h)<1.15?ShapeKind::Square:ShapeKind::Rectangle;

                if(shape.kind==ShapeKind::Square)w=h=(w+h)/2;

                for(auto v:std::vector<Point>{{-w/2,-h/2},{w/2,-h/2},{w/2,h/2},{-w/2,h/2}})shape.vertices.push_back(rotatePoint(center+v,center,angle));

                if(error<0.035) return {shape,1-error*4};
                if(!polygonFallback.shape) polygonFallback={shape,1-error*4};
                shape.vertices.clear();

            }
        }
    }
    }
    if(!polygonFallback.shape)polygonFallback=regularPolygon;
    const bool hasStrongCorners=corners.size()>=3&&regularPolygon.shape
        &&std::abs(double(corners.size())-double(regularPolygon.shape->vertices.size()))<=2;
    if(const auto center=circleCenter(p,mean)){
        double radius=0;
        for(auto v:p)radius+=length(v-*center);
        radius/=p.size();

        double residual=0;
        for(auto v:p)residual+=std::pow(length(v-*center)-radius,2);
        residual=std::sqrt(residual/p.size())/radius;

        if(!hasStrongCorners&&radius>1&&residual<0.10&&std::abs(len/(2*std::numbers::pi*radius)-1)<0.20){shape.kind=ShapeKind::Circle;
            shape.center=*center;
            shape.radiusX=shape.radiusY=radius;
            return {shape,1-residual*3};
            }
    }
    double xx=0,yy=0,xy=0;
    for(auto v:p){v=v-mean;
        xx+=v.x*v.x;
        yy+=v.y*v.y;
        xy+=v.x*v.y;
        }
    const double angle=0.5*std::atan2(2*xy,xx-yy);
    double minX=1e9,maxX=-1e9,minY=1e9,maxY=-1e9;

    for(auto v:p){v=rotatePoint(v,mean,-angle)-mean;
        minX=std::min(minX,v.x);
        maxX=std::max(maxX,v.x);
        minY=std::min(minY,v.y);
        maxY=std::max(maxY,v.y);
        }
    const Point center=rotatePoint(mean+Point{(minX+maxX)/2,(minY+maxY)/2},mean,angle);

    const double rx=(maxX-minX)/2,ry=(maxY-minY)/2;
    if(rx<0.2||ry<0.2||std::max(rx,ry)/std::min(rx,ry)>50)return polygonFallback;

    double residual=0;
    for(auto v:p){v=rotatePoint(v,center,-angle)-center;
        const auto r=std::hypot(v.x/rx,v.y/ry);
        residual+=(r-1)*(r-1);
        }residual=std::sqrt(residual/p.size());

    const double perimeter=std::numbers::pi*(3*(rx+ry)-std::sqrt((3*rx+ry)*(rx+3*ry)));

    const double ellipseTolerance=std::min(0.20,0.09+0.03*stroke.style.maxWidthMm/std::min(rx,ry));
    if(!hasStrongCorners&&residual<ellipseTolerance&&std::abs(len/perimeter-1)<0.20){shape.kind=ShapeKind::Ellipse;
        shape.center=center;
        shape.radiusX=rx;
        shape.radiusY=ry;
        shape.rotation=angle;
        return {shape,0.95-0.20*residual/ellipseTolerance};
        }
    return polygonFallback;

}
RecognitionResult recognizeShape(const StrokeObject& stroke){
    if(stroke.samples.empty())return {};
    if(auto result=recognizeConfidentShape(stroke);result.shape)return result;
    // Hold is an explicit request to straighten the gesture, including hesitant
    // endings. PCA keeps terminal jitter from defeating nearly straight lines.
    ShapeObject shape;shape.id=stroke.id;shape.style=stroke.style;shape.properties=stroke.properties;++shape.properties.revision;
    Point mean;for(const auto& s:stroke.samples)mean=mean+s.position;mean=mean*(1./stroke.samples.size());
    double xx=0,xy=0,yy=0;for(const auto& s:stroke.samples){const auto p=s.position-mean;xx+=p.x*p.x;xy+=p.x*p.y;yy+=p.y*p.y;}
    const double angle=.5*std::atan2(2*xy,xx-yy);const Point direction{std::cos(angle),std::sin(angle)};
    double lo=1e9,hi=-1e9,top=1e9,bottom=-1e9;
    for(const auto& s:stroke.samples){const auto p=rotatePoint(s.position,mean,-angle)-mean;lo=std::min(lo,p.x);hi=std::max(hi,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
    const auto extent=hi-lo,thickness=bottom-top;const auto chord=length(stroke.samples.back().position-stroke.samples.front().position);
    if(extent<1||thickness<extent*.18||chord>std::hypot(extent,thickness)*.35){
        shape.kind=ShapeKind::Line;shape.vertices={mean+direction*lo,mean+direction*hi};shape.fillOpacity=0;
        if(length(shape.vertices[1]-shape.vertices[0])<.2)shape.vertices[1]=shape.vertices[0]+Point{.2,0};
        if(length(shape.vertices[0]-stroke.samples.front().position)>length(shape.vertices[1]-stroke.samples.front().position))std::reverse(shape.vertices.begin(),shape.vertices.end());
    }else{
        shape.kind=std::max(extent,thickness)/std::max(.1,std::min(extent,thickness))<1.2?ShapeKind::Circle:ShapeKind::Ellipse;
        shape.center=rotatePoint(mean+Point{(lo+hi)/2,(top+bottom)/2},mean,angle);shape.radiusX=std::max(.2,extent/2);shape.radiusY=std::max(.2,thickness/2);shape.rotation=angle;
        if(shape.kind==ShapeKind::Circle)shape.radiusX=shape.radiusY=(shape.radiusX+shape.radiusY)/2;
    }
    return {shape,.76};
}
std::optional<ClosedLines> closeConnectedLines(const Page& page,const std::string& newestId,double tolerance){
    std::vector<const ShapeObject*> lines;const ShapeObject* newest=nullptr;
    for(const auto& shape:page.shapes)if(shape.kind==ShapeKind::Line&&shape.vertices.size()==2&&shape.properties.visible&&!shape.properties.locked){
        if(shape.id==newestId)newest=&shape;else lines.push_back(&shape);
    }
    if(!newest||!std::isfinite(tolerance)||tolerance<=0)return {};
    std::sort(lines.begin(),lines.end(),[](const auto* a,const auto* b){return a->properties.zIndex>b->properties.zIndex;});
    // Limit exploration to nearby endpoints, so unrelated lines do not slow input.
    std::vector<std::string> ids{newestId};std::vector<Point> vertices{newest->vertices[0]};
    std::unordered_set<std::string> used{newestId};std::optional<ClosedLines> result;int budget=4096;
    std::function<bool(Point)> search=[&](Point end){
        if(--budget<=0)return false;
        if(ids.size()>=3&&length(end-newest->vertices[0])<=tolerance){
            auto polygon=vertices;polygon[0]=(end+newest->vertices[0])*.5;
            if(!isSimplePolygon(polygon))return false;ShapeObject shape=*newest;shape.id=newId();shape.kind=ids.size()==3?ShapeKind::Triangle:ShapeKind::Polygon;shape.vertices=std::move(polygon);shape.fillOpacity=.10;++shape.properties.revision;
            result=ClosedLines{shape,ids};return true;
        }
        if(ids.size()>=12)return false;
        for(const auto* line:lines)if(!used.contains(line->id))for(int first=0;first<2;++first){
            if(length(line->vertices[first]-end)>tolerance)continue;
            const auto next=line->vertices[1-first];if(length(next-end)<tolerance*.5)continue;
            used.insert(line->id);ids.push_back(line->id);vertices.push_back((line->vertices[first]+end)*.5);
            if(search(next))return true;vertices.pop_back();ids.pop_back();used.erase(line->id);
        }return false;
    };
    search(newest->vertices[1]);return result;
}
std::string shapeName(ShapeKind kind){switch(kind){case ShapeKind::Line:return "Linha";
        case ShapeKind::Circle:return "Círculo";
        case ShapeKind::Ellipse:return "Elipse";
        case ShapeKind::Triangle:return "Triângulo";
        case ShapeKind::Rectangle:return "Retângulo";
        case ShapeKind::Square:return "Quadrado";
        case ShapeKind::Polygon:return "Polígono";
        }return {};
    }
}
