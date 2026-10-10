#include "StrokeEraser.h"
namespace scalar {
namespace {
using Interval=std::pair<double,double>;

void disk(std::vector<Interval>& result,Point a,Point d,Point center,double radius){
    const auto q=a-center;
        const double aa=d.x*d.x+d.y*d.y,bb=2*(q.x*d.x+q.y*d.y),cc=q.x*q.x+q.y*q.y-radius*radius;

    if(aa<1e-14){if(cc<=0)result.push_back({0,1});
            return;
            }
    const double discriminant=bb*bb-4*aa*cc;
        if(discriminant<0)return;

    const double lo=std::max(0.,(-bb-std::sqrt(discriminant))/(2*aa)),hi=std::min(1.,(-bb+std::sqrt(discriminant))/(2*aa));
        if(hi>lo)result.push_back({lo,hi});

}
bool clip(double origin,double direction,double low,double high,double& from,double& to){
    if(std::abs(direction)<1e-12)return origin>=low&&origin<=high;

    double a=(low-origin)/direction,b=(high-origin)/direction;
        if(a>b)std::swap(a,b);
        from=std::max(from,a);
        to=std::min(to,b);
        return to>from;

}
std::vector<Interval> intersections(Point a,Point b,Point from,Point to,double radius){
    std::vector<Interval> cuts;
        const auto d=b-a;
        disk(cuts,a,d,from,radius);
        disk(cuts,a,d,to,radius);

    const auto axis=to-from;
        const double len=length(axis);

    if(len>1e-9){const auto u=axis*(1/len);
            const Point n{-u.y,u.x};
            const auto q=a-from;
            double lo=0,hi=1;

        if(clip(q.x*u.x+q.y*u.y,d.x*u.x+d.y*u.y,0,len,lo,hi)&&clip(q.x*n.x+q.y*n.y,d.x*n.x+d.y*n.y,-radius,radius,lo,hi))cuts.push_back({lo,hi});
            }
    std::sort(cuts.begin(),cuts.end());
        std::vector<Interval> merged;

    for(auto interval:cuts){if(!merged.empty()&&interval.first<=merged.back().second+1e-10)merged.back().second=std::max(merged.back().second,interval.second);
            else merged.push_back(interval);
            }return merged;

}
// Clip a stroke segment to a convex image quadrilateral, including rotated images.
std::optional<Interval> protectedInterval(Point a,Point b,const std::array<Point,4>& polygon){
    double area=0;
    for(std::size_t i=0;i<4;++i){const auto p=polygon[i],q=polygon[(i+1)%4];area+=p.x*q.y-p.y*q.x;}
    if(std::abs(area)<1e-12)return {};
    const double sign=area>0?1.:-1.;
    double lo=0,hi=1;
    for(std::size_t i=0;i<4;++i){
        const auto edge=polygon[(i+1)%4]-polygon[i],q=a-polygon[i],d=b-a;
        const double origin=sign*(edge.x*q.y-edge.y*q.x),direction=sign*(edge.x*d.y-edge.y*d.x);
        if(std::abs(direction)<1e-12){if(origin<0)return {};}
        else {const double crossing=-origin/direction;if(direction>0)lo=std::max(lo,crossing);else hi=std::min(hi,crossing);}
        if(hi<=lo)return {};
    }
    return Interval{lo,hi};
}
std::vector<Interval> subtractProtected(std::vector<Interval> cuts,Point a,Point b,
    std::span<const std::array<Point,4>> areas){
    for(const auto& polygon:areas){
        const auto shield=protectedInterval(a,b,polygon);if(!shield)continue;
        std::vector<Interval> next;
        for(const auto& [lo,hi]:cuts){
            if(hi<=shield->first||lo>=shield->second)next.emplace_back(lo,hi);
            else {if(lo<shield->first)next.emplace_back(lo,shield->first);if(hi>shield->second)next.emplace_back(shield->second,hi);}
        }
        cuts=std::move(next);
    }
    return cuts;
}
PointerSample interpolate(const PointerSample& a,const PointerSample& b,double t){
    auto p=a;
        p.position=a.position+(b.position-a.position)*t;
        p.pressure=a.pressure+(b.pressure-a.pressure)*t;
        p.tiltX=a.tiltX+(b.tiltX-a.tiltX)*t;
        p.tiltY=a.tiltY+(b.tiltY-a.tiltY)*t;
        p.rotation=a.rotation+(b.rotation-a.rotation)*t;
        p.timestamp=std::uint64_t(double(a.timestamp)*(1-t)+double(b.timestamp)*t);
        return p;

}
}
static std::vector<StrokeObject> splitStroke(const StrokeObject& stroke,Point from,Point to,double radius,std::span<const std::array<Point,4>> protectedAreas,bool keepInside,std::vector<StrokeObject>* removed=nullptr){
    if(stroke.samples.empty()||radius<=0)return keepInside?std::vector<StrokeObject>{}:std::vector<StrokeObject>{stroke};
    radius+=stroke.style.maxWidthMm/2;
    double left=stroke.samples.front().position.x,right=left,top=stroke.samples.front().position.y,bottom=top;
    for(const auto& sample:stroke.samples){left=std::min(left,sample.position.x);right=std::max(right,sample.position.x);top=std::min(top,sample.position.y);bottom=std::max(bottom,sample.position.y);}
    if(right<std::min(from.x,to.x)-radius||left>std::max(from.x,to.x)+radius||bottom<std::min(from.y,to.y)-radius||top>std::max(from.y,to.y)+radius)
        return keepInside?std::vector<StrokeObject>{}:std::vector<StrokeObject>{stroke};

    if(stroke.samples.size()==1){
        bool inside=distanceToSegment(stroke.samples[0].position,from,to)<=radius;
        for(const auto& area:protectedAreas){auto image=ImageObject{};image.corners.assign(area.begin(),area.end());if(hitTest(image,stroke.samples[0].position,0))inside=false;}
        if(inside&&removed){auto dot=stroke;dot.id=newId();removed->push_back(std::move(dot));}
        if(inside!=keepInside)return {};
        auto dot=stroke;if(keepInside)dot.id=newId();return {dot};
    }

    std::vector<StrokeObject> result;
    const auto emptyRun=[&]{return StrokeObject{stroke.id,stroke.style,{},stroke.properties,stroke.marker,stroke.erasedRegions};};
    StrokeObject run=emptyRun(),removedRun=emptyRun();
    bool changed=false;

    const auto finish=[&]{if(!run.samples.empty()){run.id=newId();
            run.properties.revision++;
            result.push_back(std::move(run));
            run=emptyRun();
            }};

    for(std::size_t i=1;i<stroke.samples.size();++i){const auto& a=stroke.samples[i-1];
        const auto& b=stroke.samples[i];
        // Most samples of a long stroke are outside the small swept footprint.
        // Reject them before square roots, interval allocation or image clipping.
        const bool nearby=std::max(a.position.x,b.position.x)>=std::min(from.x,to.x)-radius&&std::min(a.position.x,b.position.x)<=std::max(from.x,to.x)+radius
            &&std::max(a.position.y,b.position.y)>=std::min(from.y,to.y)-radius&&std::min(a.position.y,b.position.y)<=std::max(from.y,to.y)+radius;
        auto cuts=nearby?subtractProtected(intersections(a.position,b.position,from,to,radius),a.position,b.position,protectedAreas):std::vector<Interval>{};
        // Roundoff at an already cut boundary must not create microscopic ink
        // fragments on repeated erase/restore passes.
        std::erase_if(cuts,[](const auto& interval){return interval.second-interval.first<1e-10;});
        if(removed){
            const auto finishRemoved=[&]{if(!removedRun.samples.empty()){removedRun.id=newId();++removedRun.properties.revision;removed->push_back(std::move(removedRun));removedRun=emptyRun();}};
            if(cuts.empty())finishRemoved();
            for(const auto& [lo,hi]:cuts){
                if(lo>1e-10)finishRemoved();
                const auto first=lo==0?a:interpolate(a,b,lo),last=hi==1?b:interpolate(a,b,hi);
                if(removedRun.samples.empty()||length(removedRun.samples.back().position-first.position)>1e-8)removedRun.samples.push_back(first);
                removedRun.samples.push_back(last);if(hi<1-1e-10)finishRemoved();
            }
        }
        if(keepInside){std::vector<Interval> complement;double position=0;for(auto [lo,hi]:cuts){if(lo>position)complement.emplace_back(position,lo);position=hi;}if(position<1)complement.emplace_back(position,1);cuts=std::move(complement);}

        double cursor=0;

        const auto keep=[&](double lo,double hi){if(hi-lo<1e-10)return;
            const auto first=lo==0?a:interpolate(a,b,lo),last=hi==1?b:interpolate(a,b,hi);
            if(run.samples.empty()||length(run.samples.back().position-first.position)>1e-8)run.samples.push_back(first);
            run.samples.push_back(last);
            };

        for(auto [lo,hi]:cuts){changed=true;
            keep(cursor,lo);
            finish();
            cursor=hi;
            }keep(cursor,1);

    }
    finish();
    if(removed&&!removedRun.samples.empty()){removedRun.id=newId();++removedRun.properties.revision;removed->push_back(std::move(removedRun));}
    return (keepInside||changed)?result:std::vector<StrokeObject>{stroke};

}
StrokeCut cutStroke(const StrokeObject& stroke,Point from,Point to,double radius,std::span<const std::array<Point,4>> protectedAreas){
    StrokeCut result;result.visible=splitStroke(stroke,from,to,radius,protectedAreas,false,&result.removed);return result;
}
std::vector<StrokeObject> eraseStroke(const StrokeObject& stroke,Point from,Point to,double radius,std::span<const std::array<Point,4>> protectedAreas){return splitStroke(stroke,from,to,radius,protectedAreas,false);}
std::vector<StrokeObject> inkInsideEraser(const StrokeObject& stroke,Point from,Point to,double radius,std::span<const std::array<Point,4>> protectedAreas){return splitStroke(stroke,from,to,radius,protectedAreas,true);}
}
