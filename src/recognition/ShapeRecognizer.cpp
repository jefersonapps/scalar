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
static RecognitionResult recognizeConfidentShape(const StrokeObject& stroke,bool allowClosedSectors=true,bool contextualArc=false){
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

    // Angle marks are open circular arcs: require a constant radius, one turn
    // direction and distributed curvature, rather than a bent line or an S curve.
    if(chord>=3&&len/chord>1.005&&len/chord<2.0){
        Point mean;for(auto v:p)mean=mean+v;mean=mean*(1./p.size());
        if(const auto fitted=circleCenter(p,mean)){
            const auto middle=(p.front()+p.back())*.5,delta=p.back()-p.front();
            const Point normal{-delta.y/chord,delta.x/chord};
            const auto offset=*fitted-middle;
            const auto center=middle+normal*(offset.x*normal.x+offset.y*normal.y);
            const double radius=length(p.front()-center);
            double error=0,maxError=0,sweep=0,backtracking=0;
            double previous=std::atan2(p.front().y-center.y,p.front().x-center.x);
            for(auto v:p){
                const double residual=std::abs(length(v-center)-radius)/std::max(radius,1e-9);
                error+=residual*residual;maxError=std::max(maxError,residual);
                const double angle=std::atan2(v.y-center.y,v.x-center.x);
                const double step=std::remainder(angle-previous,2*std::numbers::pi);
                sweep+=step;backtracking+=std::abs(step);previous=angle;
            }
            error=std::sqrt(error/p.size());
            bool smooth=true;int curvedParts=0,parts=0;
            // Reject corners and curves with a long straight tail.
            for(std::size_t i=64;i+64<p.size();i+=64){
                const auto a=p[i]-p[i-64],b=p[i+64]-p[i];
                const double turn=std::atan2(a.x*b.y-a.y*b.x,a.x*b.x+a.y*b.y);
                ++parts;
                if(turn*sweep>0&&std::abs(turn)>(std::abs(sweep)<.65?.008:std::abs(sweep)*.045))++curvedParts;
                if(turn*sweep<-.03||std::abs(turn)>(contextualArc?1.05:.80))smooth=false;
            }
            if(smooth&&curvedParts>=parts-(std::abs(sweep)<.65?2:1)&&radius>=2&&radius<=5000&&error<(contextualArc?.085:.045)&&maxError<(contextualArc?.18:.10)
                &&std::abs(sweep)>=.40&&std::abs(sweep)<=3.05
                &&backtracking-std::abs(sweep)<.06&&std::abs(len/(radius*std::abs(sweep))-1)<.30){
                shape.kind=ShapeKind::CircularSector;shape.fillOpacity=.10;shape.center=center;
                shape.radiusX=shape.radiusY=radius;
                shape.vertices.push_back(center);
                const double start=std::atan2(p.front().y-center.y,p.front().x-center.x);
                for(int i=0;i<=96;++i){const double angle=start+sweep*i/96.;shape.vertices.push_back(center+Point{radius*std::cos(angle),radius*std::sin(angle)});}
                shape.vertices[1]=p.front();shape.vertices.back()=p.back();
                return {shape,1-error*4};
            }
        }
    }

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

    // Also accept a pizza slice drawn as one closed gesture: two radial sides
    // and one circular arc. The arc fit must agree with their common vertex.
    if(allowClosedSectors)for(std::size_t apex=0;apex+1<p.size();apex+=8){
        const auto candidate=p[apex];std::vector<Point> loop;double radius=0;
        for(std::size_t i=0;i+1<p.size();++i){const auto point=p[(apex+i)%(p.size()-1)];loop.push_back(point);radius=std::max(radius,length(point-candidate));}
        if(radius<2)continue;
        std::size_t first=0,last=loop.size()-1;
        while(first<loop.size()&&length(loop[first]-candidate)<radius*.97)++first;
        while(last>first&&length(loop[last]-candidate)<radius*.97)--last;
        if(last<=first+30||first<8||loop.size()-last<8)continue;
        StrokeObject arc=stroke;arc.samples.clear();
        for(std::size_t i=first;i<=last;++i)arc.samples.push_back({loop[i]});
        const auto result=recognizeConfidentShape(arc,false);
        if(!result.shape||result.shape->kind!=ShapeKind::CircularSector||length(result.shape->center-candidate)>radius*.08)continue;
        bool radial=true;
        for(std::size_t i=0;i<first;++i)if(distanceToSegment(loop[i],result.shape->center,loop[first])>radius*.035)radial=false;
        for(std::size_t i=last+1;i<loop.size();++i)if(distanceToSegment(loop[i],result.shape->center,loop[last])>radius*.035)radial=false;
        if(radial)return result;
    }

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

            }else{
                // A parallelogram has parallel, similarly sized opposite sides,
                // but its adjacent sides need not meet at right angles.
                const auto a=left[1]-left[0],b=left[2]-left[1];
                const auto c=left[3]-left[2],d=left[0]-left[3];
                const auto oppositeMatch=[](Point u,Point v){
                    const double product=length(u)*length(v);
                    return product>1e-8 && u.x*v.x+u.y*v.y<0
                        && std::abs(u.x*v.y-u.y*v.x)/product<0.22
                        && std::min(length(u),length(v))/std::max(length(u),length(v))>0.72;
                };
                if(oppositeMatch(a,c)&&oppositeMatch(b,d)){
                    const auto center=(left[0]+left[1]+left[2]+left[3])*0.25;
                    const auto halfA=(a-c)*0.25,halfB=(b-d)*0.25;
                    shape.kind=ShapeKind::Polygon;
                    shape.vertices={center-halfA-halfB,center+halfA-halfB,
                                    center+halfA+halfB,center-halfA+halfB};
                    if(error<0.035)return {shape,1-error*4};
                    if(!polygonFallback.shape)polygonFallback={shape,1-error*4};
                    shape.vertices.clear();
                }
            }
        }
    }
    }
    if(!polygonFallback.shape)polygonFallback=regularPolygon;
    const bool hasParallelogramCorners=corners.size()==4&&polygonFallback.shape
        &&polygonFallback.shape->kind==ShapeKind::Polygon&&polygonFallback.shape->vertices.size()==4;
    const bool hasStrongCorners=hasParallelogramCorners||(corners.size()>=3&&regularPolygon.shape
        &&std::abs(double(corners.size())-double(regularPolygon.shape->vertices.size()))<=2);
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
    // endings. PCA classifies the gesture; it must not move the pen endpoints.
    ShapeObject shape;shape.id=stroke.id;shape.style=stroke.style;shape.properties=stroke.properties;++shape.properties.revision;
    Point mean;for(const auto& s:stroke.samples)mean=mean+s.position;mean=mean*(1./stroke.samples.size());
    double xx=0,xy=0,yy=0;for(const auto& s:stroke.samples){const auto p=s.position-mean;xx+=p.x*p.x;xy+=p.x*p.y;yy+=p.y*p.y;}
    const double angle=.5*std::atan2(2*xy,xx-yy);
    double lo=1e9,hi=-1e9,top=1e9,bottom=-1e9;
    for(const auto& s:stroke.samples){const auto p=rotatePoint(s.position,mean,-angle)-mean;lo=std::min(lo,p.x);hi=std::max(hi,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
    const auto extent=hi-lo,thickness=bottom-top;const auto chord=length(stroke.samples.back().position-stroke.samples.front().position);
    if(extent<1||thickness<extent*.18||chord>std::hypot(extent,thickness)*.35){
        shape.kind=ShapeKind::Line;shape.vertices={stroke.samples.front().position,stroke.samples.back().position};shape.fillOpacity=0;
        if(length(shape.vertices[1]-shape.vertices[0])<.2)shape.vertices[1]=shape.vertices[0]+Point{.2,0};
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
std::optional<ShapeObject> recognizeRightAngle(const Page& page,const StrokeObject& stroke){
    if(stroke.marker||stroke.samples.size()<4)return {};
    const auto first=stroke.samples.front().position,last=stroke.samples.back().position;
    const double chord=length(last-first);if(chord<1||chord>40)return {};
    struct Edge{Point a,b;};std::vector<Edge> starts,ends;
    const double reach=std::min(1.5,chord*.20);
    const auto add=[&](Point a,Point b){
        if(length(b-a)<chord*.6)return;
        if(distanceToSegment(first,a,b)<=reach&&starts.size()<64)starts.push_back({a,b});
        if(distanceToSegment(last,a,b)<=reach&&ends.size()<64)ends.push_back({a,b});
    };
    for(const auto& shape:page.shapes){
        if(!shape.properties.visible||shape.id==stroke.id||(shape.style.rgba&255)==0)continue;
        if(shape.kind==ShapeKind::Line&&shape.vertices.size()==2)add(shape.vertices[0],shape.vertices[1]);
        else if(shape.kind==ShapeKind::Triangle||shape.kind==ShapeKind::Rectangle||shape.kind==ShapeKind::Square||shape.kind==ShapeKind::Polygon)
            for(std::size_t i=0;i<shape.vertices.size();++i)add(shape.vertices[i],shape.vertices[(i+1)%shape.vertices.size()]);
    }
    for(const auto& ink:page.strokes){
        if(ink.id==stroke.id||ink.marker||!ink.properties.visible||(ink.style.rgba&255)==0||ink.samples.size()<2)continue;
        const auto a=ink.samples.front().position,b=ink.samples.back().position;const double extent=length(b-a);
        if(extent<chord*.6)continue;
        double error=0;for(const auto& sample:ink.samples)error=std::max(error,distanceToSegment(sample.position,a,b));
        if(error<=std::max(.15,extent*.02))add(a,b);
    }
    double best=1e9;std::optional<ShapeObject> result;
    for(const auto& start:starts)for(const auto& end:ends){
        const auto a=start.b-start.a,b=end.b-end.a;const double cross=a.x*b.y-a.y*b.x;
        // Only contextual right angles: within 10 degrees of perpendicular.
        if(std::abs(a.x*b.x+a.y*b.y)>length(a)*length(b)*.174)continue;
        const auto offset=end.a-start.a;const auto corner=start.a+a*((offset.x*b.y-offset.y*b.x)/cross);
        if(distanceToSegment(corner,start.a,start.b)>reach||distanceToSegment(corner,end.a,end.b)>reach)continue;
        const auto project=[&](Point p,const Edge& edge){const auto d=edge.b-edge.a,v=p-edge.a;return edge.a+d*((v.x*d.x+v.y*d.y)/(d.x*d.x+d.y*d.y));};
        const auto rayA=project(first,start)-corner,rayB=project(last,end)-corner;
        const double ra=length(rayA),rb=length(rayB),side=(ra+rb)*.5;
        if(side<.5||side>25||std::min(ra,rb)<side*.7||std::max(ra,rb)>side*1.3)continue;
        const auto u=rayA*(side/ra),v=rayB*(side/rb),p=corner+u,q=corner+v,kink=corner+u+v;
        std::size_t split=0;double closest=1e9;
        for(std::size_t i=0;i<stroke.samples.size();++i){const double d=length(stroke.samples[i].position-kink);if(d<closest){closest=d;split=i;}}
        if(split==0||split+1==stroke.samples.size()||closest>side*.25)continue;
        double error=0;
        for(std::size_t i=0;i<stroke.samples.size();++i)error=std::max(error,distanceToSegment(stroke.samples[i].position,i<=split?p:kink,i<=split?kink:q));
        if(error>std::max(.15,side*.14))continue; // curved arcs and extra corners are not square marks
        const double score=error+closest+distanceToSegment(first,start.a,start.b)+distanceToSegment(last,end.a,end.b);
        if(score>=best)continue;
        ShapeObject shape;shape.id=stroke.id;shape.style=stroke.style;shape.properties=stroke.properties;++shape.properties.revision;
        shape.kind=ShapeKind::RightAngle;shape.vertices={corner,p,kink,q};shape.fillOpacity=.10;
        result=shape;best=score;
    }
    return result;
}
static std::optional<ShapeObject> matchedCircularSector(const Page& page,ShapeObject sector,double tolerance){
    if(sector.kind!=ShapeKind::CircularSector||sector.vertices.size()<4||!std::isfinite(tolerance)||tolerance<=0)return {};
    const Point first=sector.vertices[1],last=sector.vertices.back();
    const double radius=sector.radiusX;
    if(radius<.2)return {};
    // A tight hand-drawn bend can fit a much smaller circle than the intended
    // corner. Use the endpoint span as well, so a small gap to the second side
    // does not exclude it before the full-arc/wedge checks below can run.
    const double chord=length(last-first);
    const double reach=std::min(tolerance,std::max({.6,radius*.30,chord*.25}));
    struct Edge{Point a,b;};std::vector<Edge> starts,ends;
    const auto add=[&](Point a,Point b){
        if(length(b-a)<1)return;
        if(distanceToSegment(first,a,b)<=reach&&starts.size()<64)starts.push_back({a,b});
        if(distanceToSegment(last,a,b)<=reach&&ends.size()<64)ends.push_back({a,b});
    };
    for(const auto& shape:page.shapes){
        if(!shape.properties.visible||shape.id==sector.id||(shape.style.rgba&255)==0)continue;
        if(shape.kind==ShapeKind::Line){if(shape.vertices.size()==2)add(shape.vertices[0],shape.vertices[1]);}
        else if(shape.kind==ShapeKind::Triangle||shape.kind==ShapeKind::Rectangle||shape.kind==ShapeKind::Square||shape.kind==ShapeKind::Polygon)
            for(std::size_t i=0;i<shape.vertices.size();++i)add(shape.vertices[i],shape.vertices[(i+1)%shape.vertices.size()]);
    }
    for(const auto& stroke:page.strokes){
        if(!stroke.properties.visible||stroke.marker||stroke.id==sector.id||(stroke.style.rgba&255)==0||stroke.samples.size()<2)continue;
        const auto a=stroke.samples.front().position,b=stroke.samples.back().position;const double extent=length(b-a);
        if(extent<1||std::max(distanceToSegment(first,a,b),distanceToSegment(last,a,b))>extent+reach)continue;
        double error=0;for(const auto& sample:stroke.samples)error=std::max(error,distanceToSegment(sample.position,a,b));
        if(error<=std::max(.2,extent*.025))add(a,b);
    }
    double originalSweep=0;
    for(std::size_t i=2;i<sector.vertices.size();++i){const auto a=sector.vertices[i-1]-sector.center,b=sector.vertices[i]-sector.center;originalSweep+=std::atan2(a.x*b.y-a.y*b.x,a.x*b.x+a.y*b.y);}
    double best=1e9;std::optional<ShapeObject> aligned;
    for(const auto& start:starts)for(const auto& end:ends){
        const auto a=start.b-start.a,b=end.b-end.a;
        const double cross=a.x*b.y-a.y*b.x;
        if(std::abs(cross)<length(a)*length(b)*.12)continue;
        const auto offset=end.a-start.a;
        const auto center=start.a+a*((offset.x*b.y-offset.y*b.x)/cross);
        const double centerError=length(center-sector.center);
        if(length(center-first)>chord*3||length(center-last)>chord*3||distanceToSegment(center,start.a,start.b)>reach||distanceToSegment(center,end.a,end.b)>reach)continue;
        const auto project=[&](Point p,const Edge& edge){const auto d=edge.b-edge.a,v=p-edge.a;return edge.a+d*((v.x*d.x+v.y*d.y)/(d.x*d.x+d.y*d.y));};
        const auto rayA=project(first,start)-center,rayB=project(last,end)-center;
        const double ra=length(rayA),rb=length(rayB),r=(ra+rb)*.5;
        if(r<.2||r>chord*3||std::abs(ra-rb)>std::max(reach,r*.45))continue;
        const double angle=std::atan2(rayA.y,rayA.x),sweep=std::atan2(rayA.x*rayB.y-rayA.y*rayB.x,rayA.x*rayB.x+rayA.y*rayB.y);
        if(sweep*originalSweep<=0||std::abs(sweep)<.35||std::abs(sweep)>3.05)continue;
        // For shallow arcs the unconstrained circle center is unreliable. Prefer
        // a nearby contour corner only when the entire arc supports its wedge.
        double radialError=0,maxRadialError=0;bool inside=true;
        for(std::size_t i=1;i<sector.vertices.size();++i){
            const auto v=sector.vertices[i]-center;const double error=std::abs(length(v)-r);
            radialError+=error*error;maxRadialError=std::max(maxRadialError,error);
            const double progress=std::atan2(rayA.x*v.y-rayA.y*v.x,rayA.x*v.x+rayA.y*v.y)/sweep;
            if(progress<-.15||progress>1.15){inside=false;break;}
        }
        radialError=std::sqrt(radialError/(sector.vertices.size()-1));
        if(!inside||radialError>r*.30||maxRadialError>r*.45)continue;
        const double score=distanceToSegment(first,start.a,start.b)+distanceToSegment(last,end.a,end.b)+radialError+centerError*.2+std::abs(ra-rb);
        if(score>=best)continue;
        auto candidate=sector;candidate.center=center;candidate.radiusX=candidate.radiusY=r;candidate.vertices={center};
        for(int i=0;i<=96;++i){const double t=angle+sweep*i/96.;candidate.vertices.push_back(center+Point{r*std::cos(t),r*std::sin(t)});}
        aligned=std::move(candidate);best=score;
    }
    return aligned;
}
ShapeObject alignCircularSector(const Page& page,ShapeObject sector,double tolerance){
    return matchedCircularSector(page,sector,tolerance).value_or(sector);
}
RecognitionResult recognizeShape(const StrokeObject& stroke,const Page& page){
    auto result=recognizeShape(stroke);
    // A small hand-drawn angle can be uneven enough to fail an isolated circle
    // fit. Relax that fit only when both nearby sides validate the whole arc.
    if(!result.shape||result.shape->kind==ShapeKind::Line){
        auto contextual=recognizeConfidentShape(stroke,false,true);
        if(contextual.shape&&contextual.shape->kind==ShapeKind::CircularSector){
            if(auto aligned=matchedCircularSector(page,*contextual.shape,3))
                return {std::move(aligned),contextual.confidence};
        }
    }
    if(result.shape&&result.shape->kind==ShapeKind::CircularSector){
        result.shape=matchedCircularSector(page,std::move(*result.shape),3);
        if(!result.shape){
            // Holding an open gesture asks to straighten it. Without a nearby
            // angle, a curved line candidate must still become a segment.
            const auto first=stroke.samples.front().position,last=stroke.samples.back().position;
            if(length(last-first)>=.2){
                ShapeObject line;line.id=stroke.id;line.style=stroke.style;line.properties=stroke.properties;++line.properties.revision;
                line.kind=ShapeKind::Line;line.vertices={first,last};line.fillOpacity=0;
                result={std::move(line),.76};
            }else result.confidence=0;
        }
    }
    return result;
}
std::string shapeName(ShapeKind kind){switch(kind){case ShapeKind::Line:return "Linha";
        case ShapeKind::Circle:return "Círculo";
        case ShapeKind::Ellipse:return "Elipse";
        case ShapeKind::Triangle:return "Triângulo";
        case ShapeKind::Rectangle:return "Retângulo";
        case ShapeKind::Square:return "Quadrado";
        case ShapeKind::Polygon:return "Polígono";
        case ShapeKind::CircularArc:return "Arco circular";
        case ShapeKind::CircularSector:return "Setor circular";
        case ShapeKind::RightAngle:return "Ângulo reto";
        }return {};
    }
}
