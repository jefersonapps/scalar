#include "StrokeMesh.h"
#include <numbers>
#include "geometry/Geometry.h"
namespace scalar {
void appendSmoothStrokeSegment(const std::vector<PointerSample>& samples,std::size_t i,std::vector<PointerSample>& result){
    if(i+1>=samples.size())return;
    // A bounded five-point kernel removes hand jitter before interpolation.
    // Preserve endpoints and deliberate corners; never move ink by > 0.18 mm.
    const auto filtered=[&](std::size_t index){
        auto sample=samples[index];if(index==0||index+1==samples.size())return sample;
        const auto before=sample.position-samples[index-1].position,after=samples[index+1].position-sample.position;
        const double a=length(before),b=length(after);
        if(a>.4&&b>.4&&(before.x*after.x+before.y*after.y)/(a*b)<.707)return sample;
        Point sum{};double weight=0;
        for(int offset=-2;offset<=2;++offset){const auto neighbor=std::ptrdiff_t(index)+offset;if(neighbor<0||neighbor>=std::ptrdiff_t(samples.size()))continue;
            const double w=3-std::abs(offset);sum=sum+samples[std::size_t(neighbor)].position*w;weight+=w;}
        auto correction=sum*(1/weight)-sample.position;const double amount=length(correction);
        if(amount>.18)correction=correction*(.18/amount);
        sample.position=sample.position+correction;return sample;
    };
    const auto a=filtered(i),b=filtered(i+1);const auto delta=b.position-a.position;const double distance=length(delta);
    const double step=std::sqrt(std::max(distance,1e-12));
    auto start=delta,end=delta;
    if(i){const auto previousPoint=filtered(i-1).position;const auto before=a.position-previousPoint;const double previous=std::sqrt(std::max(length(before),1e-12));start=delta+(before*(1/previous)-(b.position-previousPoint)*(1/(previous+step)))*step;}
    if(i+2<samples.size()){const auto nextPoint=filtered(i+2).position;const auto after=nextPoint-b.position;const double next=std::sqrt(std::max(length(after),1e-12));end=delta+(after*(1/next)-(nextPoint-a.position)*(1/(next+step)))*step;}
    const auto limit=[distance](Point tangent){const double size=length(tangent);return size>distance&&size>0?tangent*(distance/size):tangent;};start=limit(start);end=limit(end);
    const double pressureDelta=b.pressure-a.pressure;
    double pressureStart=i?(b.pressure-samples[i-1].pressure)*.5:pressureDelta;
    double pressureEnd=i+2<samples.size()?(samples[i+2].pressure-a.pressure)*.5:pressureDelta;
    if(std::abs(pressureDelta)<1e-12)pressureStart=pressureEnd=0;
    else {if(pressureStart*pressureDelta<=0)pressureStart=0;if(pressureEnd*pressureDelta<=0)pressureEnd=0;const double norm=std::hypot(pressureStart/pressureDelta,pressureEnd/pressureDelta);if(norm>3){pressureStart*=3/norm;pressureEnd*=3/norm;}}
    struct Curve {Point p0,p1,p2,p3;double q0,q1,q2,q3,t0,t1;int depth;};
    std::vector<Curve> pending{{a.position,a.position+start*(1./3),b.position-end*(1./3),b.position,a.pressure,a.pressure+pressureStart/3,b.pressure-pressureEnd/3,b.pressure,0,1,0}};
    while(!pending.empty()){
        const auto c=pending.back();pending.pop_back();
        // The Bezier control polygon bounds the entire curve's deviation.
        // Include pressure flatness, so straight lines retain flowing widths.
        const double error=std::max(distanceToSegment(c.p1,c.p0,c.p3),distanceToSegment(c.p2,c.p0,c.p3));
        const double pressureError=std::max(std::abs(c.q1-(2*c.q0+c.q3)/3),std::abs(c.q2-(c.q0+2*c.q3)/3))*5;
        if((error<=.002&&pressureError<=.002)||c.depth>=12){auto sample=b;sample.position=c.p3;sample.pressure=std::clamp(c.q3,std::min(a.pressure,b.pressure),std::max(a.pressure,b.pressure));sample.timestamp=a.timestamp+std::uint64_t(double(b.timestamp>=a.timestamp?b.timestamp-a.timestamp:0)*c.t1);result.push_back(sample);continue;}
        const auto p01=(c.p0+c.p1)*.5,p12=(c.p1+c.p2)*.5,p23=(c.p2+c.p3)*.5,p012=(p01+p12)*.5,p123=(p12+p23)*.5,middle=(p012+p123)*.5;
        const double q01=(c.q0+c.q1)*.5,q12=(c.q1+c.q2)*.5,q23=(c.q2+c.q3)*.5,q012=(q01+q12)*.5,q123=(q12+q23)*.5,q=(q012+q123)*.5,t=(c.t0+c.t1)*.5;
        pending.push_back({middle,p123,p23,c.p3,q,q123,q23,c.q3,t,c.t1,c.depth+1});pending.push_back({c.p0,p01,p012,middle,c.q0,q01,q012,q,c.t0,t,c.depth+1});
    }
}
std::vector<PointerSample> smoothStrokeSamples(const std::vector<PointerSample>& samples){
    if(samples.size()<3)return samples;
    std::vector<PointerSample> result;result.reserve(samples.size()*4);result.push_back(samples.front());
    for(std::size_t i=0;i+1<samples.size();++i)appendSmoothStrokeSegment(samples,i,result);
    return result;
}
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

std::vector<PointerSample> strokeDisplaySamples(const StrokeObject& stroke,double tolerance){
    if(stroke.samples.size()<3)return stroke.samples;
    // Bound both the centerline and radius error to 4 micrometres. This is
    // below a quarter pixel even at 800% zoom on a 2x display. Process bounded
    // chunks so adversarial zigzags cannot make simplification quadratic.
    tolerance=std::max(tolerance,1e-6);
    std::vector<PointerSample> samples;samples.reserve(stroke.samples.size());
    const auto& source=stroke.samples;
    std::vector<double> radii;radii.reserve(source.size());for(const auto& sample:source)radii.push_back(stroke.style.width(sample.pressure)*.5);
    std::vector<bool> keep(source.size(),false);keep.front()=keep.back()=true;
    for(std::size_t begin=0;begin+1<source.size();begin+=128){
        const auto end=std::min(begin+128,source.size()-1);keep[begin]=keep[end]=true;
        std::vector<std::pair<std::size_t,std::size_t>> pending{{begin,end}};
        while(!pending.empty()){
            const auto [a,b]=pending.back();pending.pop_back();if(b<=a+1)continue;
            const auto delta=source[b].position-source[a].position;const double squared=delta.x*delta.x+delta.y*delta.y;
            double worst=tolerance;std::size_t split=b;
            for(auto i=a+1;i<b;++i){const auto relative=source[i].position-source[a].position;
                const double t=squared>1e-18?std::clamp((relative.x*delta.x+relative.y*delta.y)/squared,0.,1.):double(i-a)/(b-a);
                const double error=length(relative-delta*t)+std::abs(radii[i]-(radii[a]+(radii[b]-radii[a])*t));
                if(error>worst){worst=error;split=i;}
            }
            if(split!=b){keep[split]=true;pending.emplace_back(a,split);pending.emplace_back(split,b);}
        }
    }
    for(std::size_t i=0;i<source.size();++i)if(keep[i])samples.push_back(source[i]);
    return samples;
}
std::vector<Point> strokeDisplayMesh(const StrokeObject& stroke){
    if(stroke.style.pattern!=LinePattern::Solid||stroke.samples.size()<2)return strokeMesh(stroke);
    const auto samples=strokeDisplaySamples(stroke);
    std::vector<Point> vertices;vertices.reserve(samples.size()*12);
    const auto arc=[&](Point center,double radius,double angle,double sweep){
        const double angleStep=radius>.002?std::min(std::numbers::pi/6,2*std::acos(std::clamp(1-.002/radius,-1.,1.))):std::numbers::pi/6;
        const int steps=std::max(1,int(std::ceil(std::abs(sweep)/angleStep)));
        Point before=center+Point{std::cos(angle),std::sin(angle)}*radius;
        for(int step=1;step<=steps;++step){const double a=angle+sweep*step/steps;const auto after=center+Point{std::cos(a),std::sin(a)}*radius;vertices.insert(vertices.end(),{center,before,after});before=after;}
    };
    std::vector<Point> normals(samples.size()-1);
    for(std::size_t i=1;i<samples.size();++i){const auto delta=samples[i].position-samples[i-1].position;const double distance=length(delta);
        if(distance<1e-9){arc(samples[i].position,std::max(stroke.style.width(samples[i-1].pressure),stroke.style.width(samples[i].pressure))*.5,0,2*std::numbers::pi);continue;}
        const Point normal{-delta.y/distance,delta.x/distance};normals[i-1]=normal;
        const double before=stroke.style.width(samples[i-1].pressure)*.5,after=stroke.style.width(samples[i].pressure)*.5;
        const auto a=samples[i-1].position+normal*before,b=samples[i-1].position-normal*before,c=samples[i].position+normal*after,d=samples[i].position-normal*after;
        vertices.insert(vertices.end(),{a,b,c,c,b,d});
    }
    const auto first=normals.front(),last=normals.back();
    if(length(first)>0)arc(samples.front().position,stroke.style.width(samples.front().pressure)*.5,std::atan2(first.y,first.x),std::numbers::pi);
    if(length(last)>0)arc(samples.back().position,stroke.style.width(samples.back().pressure)*.5,std::atan2(-last.y,-last.x),std::numbers::pi);
    for(std::size_t i=1;i+1<samples.size();++i){const auto a=normals[i-1],b=normals[i];
        if(length(a)==0||length(b)==0){if(length(b)>0)arc(samples[i].position,stroke.style.width(samples[i].pressure)*.5,std::atan2(b.y,b.x),std::numbers::pi);if(length(a)>0)arc(samples[i].position,stroke.style.width(samples[i].pressure)*.5,std::atan2(-a.y,-a.x),std::numbers::pi);continue;}
        const double turn=std::atan2(a.x*b.y-a.y*b.x,a.x*b.x+a.y*b.y);
        if(std::abs(turn)<1e-9)continue;const double side=turn>0?-1.:1.;
        arc(samples[i].position,stroke.style.width(samples[i].pressure)*.5,std::atan2(a.y*side,a.x*side),turn);
    }
    return vertices;
}

}
