#include "StrokeMesh.h"
#include <numbers>
namespace scalar {
namespace {
std::vector<Point> solidStrokeMesh(const StrokeObject& stroke) {
    std::vector<Point> vertices;
    vertices.reserve(stroke.samples.size()*42);
    auto disk = [&](Point center,double radius) {
        constexpr int segments=12;
        for(int i=0;i<segments;++i) {
            const double a=2*std::numbers::pi*i/segments, b=2*std::numbers::pi*(i+1)/segments;
            vertices.insert(vertices.end(),{center,center+Point{std::cos(a),std::sin(a)}*radius,center+Point{std::cos(b),std::sin(b)}*radius});
        }
    };
    for(std::size_t i=0;i<stroke.samples.size();++i) {
        const auto& s=stroke.samples[i]; const double r=stroke.style.width(s.pressure)/2;
        disk(s.position,r);
        if(!i) continue;
        const auto& prev=stroke.samples[i-1]; const auto d=s.position-prev.position;
        const double len=length(d); if(len<1e-9) continue;
        const Point n{-d.y/len,d.x/len};
        const double pr=stroke.style.width(prev.pressure)/2;
        const auto a=prev.position+n*pr,b=prev.position-n*pr,c=s.position+n*r,e=s.position-n*r;
        vertices.insert(vertices.end(),{a,b,c,c,b,e});
    }
    return vertices;
}
}
std::vector<Point> strokeMesh(const StrokeObject& stroke) {
    if(stroke.style.pattern==LinePattern::Solid||stroke.samples.size()<2)return solidStrokeMesh(stroke);
    std::vector<Point> vertices;
    StrokeObject run{stroke.id,stroke.style,{}};run.style.pattern=LinePattern::Solid;
    const auto flush=[&]{const auto mesh=solidStrokeMesh(run);vertices.insert(vertices.end(),mesh.begin(),mesh.end());run.samples.clear();};
    const auto sampleAt=[](const PointerSample& a,const PointerSample& b,double t){
        auto sample=a;sample.position=a.position+(b.position-a.position)*t;
        sample.pressure=a.pressure+(b.pressure-a.pressure)*t;return sample;
    };
    if(stroke.style.pattern==LinePattern::Dotted){
        const double spacing=std::max(stroke.style.dotSpacingMm,stroke.style.maxWidthMm*2);
        double next=0,distance=0;
        for(std::size_t i=1;i<stroke.samples.size();++i){
            const auto& a=stroke.samples[i-1];const auto& b=stroke.samples[i];const double len=length(b.position-a.position);
            if(len<1e-9)continue;
            while(next<=distance+len+1e-9){run.samples={sampleAt(a,b,std::clamp((next-distance)/len,0.,1.))};flush();next+=spacing;}
            distance+=len;
        }
        return vertices;
    }
    const double dash=std::max(0.1,stroke.style.dashLengthMm),gap=std::max(stroke.style.gapLengthMm,stroke.style.maxWidthMm*1.5);
    bool on=true;double remaining=dash;
    for(std::size_t i=1;i<stroke.samples.size();++i){
        const auto& a=stroke.samples[i-1];const auto& b=stroke.samples[i];const double len=length(b.position-a.position);
        if(len<1e-9)continue;
        double cursor=0;
        while(cursor<len-1e-9){
            const double step=std::min(remaining,len-cursor);
            if(on){if(run.samples.empty())run.samples.push_back(sampleAt(a,b,cursor/len));run.samples.push_back(sampleAt(a,b,(cursor+step)/len));}
            cursor+=step;remaining-=step;
            if(remaining<1e-9){if(on)flush();on=!on;remaining=on?dash:gap;}
        }
    }
    if(!run.samples.empty())flush();
    return vertices;
}

}
