#include "MagneticGeometry.h"
#include <set>
namespace scalar {
namespace {
constexpr double cellSize=10,eps=1e-8;
bool overlaps(Bounds a,Bounds b){return a.left<=b.right+eps&&b.left<=a.right+eps&&a.top<=b.bottom+eps&&b.top<=a.bottom+eps;}
double cross(Point a,Point b){return a.x*b.y-a.y*b.x;}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
int cell(double v){return int(std::floor(std::clamp(v/cellSize,-1e8,1e8)));}
}
void MagneticGeometry::reset(const Page& page){
    owners_.clear();primitives_.clear();endpoints_.clear();cells_.clear();large_.clear();intersections_.clear();
    std::map<std::size_t,Point> straightOwners;
    const auto addPath=[&](const std::vector<Point>& samples,std::size_t owner,bool closed){
        // Repeated pointer samples must not turn adjacent segments into a
        // self-intersection at an ordinary point along the stroke.
        std::vector<Point> points;points.reserve(samples.size());
        for(const auto p:samples)if(points.empty()||length(p-points.back())>=eps)points.push_back(p);
        if(closed&&points.size()>1&&length(points.front()-points.back())<eps)points.pop_back();
        if(points.empty())return;
        // A ruler stroke can reverse many times. Its visible ends are the
        // extrema along the line, rather than the first/last pointer samples.
        if(!closed&&points.size()>1){
            const Point origin=points.front();Point direction{};
            for(const auto p:points)if(dot(p-origin,p-origin)>dot(direction,direction))direction=p-origin;
            const double span=length(direction);
            if(span>eps&&std::all_of(points.begin(),points.end(),[&](Point p){return std::abs(cross(direction,p-origin))/span<=1e-6;})){
                const auto ends=std::minmax_element(points.begin(),points.end(),[&](Point a,Point b){return dot(a-origin,direction)<dot(b-origin,direction);});
                const Point first=*ends.first,last=*ends.second;
                std::erase_if(endpoints_,[&](const auto& endpoint){return endpoint.second==owner;});
                endpoints_.push_back({first,owner});endpoints_.push_back({last,owner});
                points={first,last};straightOwners[owner]=last-first;
            }
        }
        // A retraced circle has no isolated self-crossings. Different sampling
        // phases produce crossing chords, but they represent the same curve.
        bool circularPath=false;
        if(points.size()>=3){
            const Point origin=points.front();Point u{},v{};
            for(const auto p:points)if(dot(p-origin,p-origin)>dot(u,u))u=p-origin;
            for(const auto p:points)if(std::abs(cross(u,p-origin))>std::abs(cross(u,v)))v=p-origin;
            const double determinant=2*cross(u,v);
            if(std::abs(determinant)>1e-10){
                const Point center=origin+Point{(dot(u,u)*v.y-dot(v,v)*u.y)/determinant,(u.x*dot(v,v)-v.x*dot(u,u))/determinant};
                const double radius=length(center-origin);
                circularPath=std::all_of(points.begin(),points.end(),[&](Point p){return std::abs(length(p-center)-radius)<=1e-6;});
                // Sparse polygon vertices on a circle may form a real crossing.
                for(std::size_t i=1;circularPath&&i<points.size();++i)
                    circularPath=length(points[i]-points[i-1])<=2*radius*std::sin(std::numbers::pi/24);
                if(circularPath&&!closed){
                    double angle=0,low=0,high=0;Point first=points.front(),last=first;
                    for(std::size_t i=1;i<points.size();++i){
                        const auto previous=points[i-1]-center,current=points[i]-center;
                        angle+=std::atan2(cross(previous,current),dot(previous,current));
                        if(angle<low){low=angle;first=points[i];}
                        if(angle>high){high=angle;last=points[i];}
                    }
                    // After reversing, the input's first/last sample can lie
                    // inside the visible arc. Only the swept ends are targets.
                    std::erase_if(endpoints_,[&](const auto& endpoint){return endpoint.second==owner;});
                    if(high-low<2*std::numbers::pi-1e-6){endpoints_.push_back({first,owner});endpoints_.push_back({last,owner});}
                }
            }
        }
        const auto count=closed?points.size():points.size()-1;
        for(std::size_t i=0;i<count;++i){const auto a=points[i],b=points[(i+1)%points.size()];if(length(b-a)<eps)continue;
            Primitive p;p.a=a;p.b=b;p.owner=owner;p.step=i;p.count=count;p.closed=closed;p.circularPath=circularPath;p.box={std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y)};primitives_.push_back(p);
        }
    };
    for(const auto view:objectViews(page))std::visit([&](const auto* object){
        using T=std::decay_t<decltype(*object)>;
        if constexpr(std::is_same_v<T,StrokeObject>||std::is_same_v<T,ShapeObject>){
            if(!object->properties.visible)return;
            const auto owner=owners_.size();owners_.push_back(view);
            if constexpr(std::is_same_v<T,StrokeObject>){
                if(object->samples.empty())return;
                std::vector<Point> path;path.reserve(object->samples.size());for(const auto& sample:object->samples)path.push_back(sample.position);
                endpoints_.push_back({path.front(),owner});endpoints_.push_back({path.back(),owner});addPath(path,owner,false);
            }else {
                if(object->kind==ShapeKind::Circle||object->kind==ShapeKind::Ellipse){
                    if(object->radiusX<=0||object->radiusY<=0)return;
                    Primitive p;p.owner=owner;p.ellipse=true;p.a=object->center;p.rx=object->radiusX;p.ry=object->radiusY;p.angle=object->rotation;
                    const double c=std::cos(p.angle),s=std::sin(p.angle),w=std::hypot(p.rx*c,p.ry*s),h=std::hypot(p.rx*s,p.ry*c);
                    p.box={p.a.x-w,p.a.y-h,p.a.x+w,p.a.y+h};primitives_.push_back(p);
                }else {
                    const auto path=shapeOutline(*object);const bool closed=object->kind!=ShapeKind::Line&&object->kind!=ShapeKind::CircularArc;
                    if(object->kind==ShapeKind::CircularSector){
                        // Sampled points along the curved edge are not corners.
                        if(path.size()>=3){endpoints_.push_back({path[0],owner});endpoints_.push_back({path[1],owner});endpoints_.push_back({path.back(),owner});}
                    }else if(closed)for(const auto p:path)endpoints_.push_back({p,owner});
                    else if(!path.empty()){endpoints_.push_back({path.front(),owner});endpoints_.push_back({path.back(),owner});}
                    addPath(path,owner,closed);
                }
            }
        }
    },view);
    for(std::size_t i=0;i<primitives_.size();++i){const auto b=primitives_[i].box;const auto l=cell(b.left),r=cell(b.right),t=cell(b.top),d=cell(b.bottom);
        if(double(r-l+1)*(d-t+1)>512){large_.push_back(i);continue;}
        for(int x=l;x<=r;++x)for(int y=t;y<=d;++y)cells_[{x,y}].push_back(i);
    }
    // Collinear strokes drawn over one another form one visible interval.
    // Hide an endpoint covered by the interior of another straight stroke.
    std::erase_if(endpoints_,[&](const auto& endpoint){
        const auto own=straightOwners.find(endpoint.second);if(own==straightOwners.end())return false;
        const auto covered=[&](std::size_t id){const auto& segment=primitives_[id];
            if(segment.owner==endpoint.second||!straightOwners.contains(segment.owner))return false;
            const auto d=segment.b-segment.a;const double size=length(d);
            if(size<eps||std::abs(cross(own->second,d))>1e-8*length(own->second)*size)return false;
            const double t=dot(endpoint.first-segment.a,d)/dot(d,d);
            return t>eps&&t<1-eps&&distanceToSegment(endpoint.first,segment.a,segment.b)<=1e-6&&visibleAt(segment.owner,endpoint.first);
        };
        const auto it=cells_.find({cell(endpoint.first.x),cell(endpoint.first.y)});
        return (it!=cells_.end()&&std::any_of(it->second.begin(),it->second.end(),covered))||std::any_of(large_.begin(),large_.end(),covered);
    });
}
bool MagneticGeometry::visibleAt(std::size_t owner,Point point) const {
    return std::visit([&](const auto* item){
        if constexpr(requires{item->erasedRegions;}){
            bool erased=false;for(const auto& region:item->erasedRegions)if(distanceToSegment(point,region.from,region.to)<region.radius)erased=!region.restore;
            if(erased)return false;
        }
        if constexpr(requires{item->eraseMask;})if(item->eraseMask&&item->eraseMask->alpha&&item->eraseMask->corners.size()==4){
            const auto& mask=*item->eraseMask;const auto a=mask.corners[1]-mask.corners[0],b=mask.corners[3]-mask.corners[0],p=point-mask.corners[0];const double determinant=cross(a,b);
            if(std::abs(determinant)>eps){const double u=cross(p,b)/determinant,v=cross(a,p)/determinant;
                if(u>=0&&u<1&&v>=0&&v<1){const auto offset=std::size_t(int(v*mask.pixelHeight))*mask.pixelWidth+int(u*mask.pixelWidth);if(offset<mask.alpha->size()&&(*mask.alpha)[offset]>=254)return false;}
            }
        }
        return true;
    },owners_[owner]);
}
std::vector<Point> MagneticGeometry::intersect(const Primitive& a,const Primitive& b){
    std::vector<Point> out;
    const auto append=[&](Point p){if(std::isfinite(p.x)&&std::isfinite(p.y)&&std::none_of(out.begin(),out.end(),[&](Point q){return length(p-q)<1e-6;}))out.push_back(p);};
    if(!a.ellipse&&!b.ellipse){
        const auto u=a.b-a.a,v=b.b-b.a;const double det=cross(u,v);
        if(std::abs(det)<=eps*length(u)*length(v))return out;
        const auto d=b.a-a.a;const double t=cross(d,v)/det,s=cross(d,u)/det;
        if(t>=-eps&&t<=1+eps&&s>=-eps&&s<=1+eps)append(a.a+u*std::clamp(t,0.,1.));return out;
    }
    if(!a.ellipse)return intersect(b,a);
    const auto local=[](const Primitive& curve,Point p){const auto q=rotatePoint(p-curve.a,{},-curve.angle);return Point{q.x/curve.rx,q.y/curve.ry};};
    if(!b.ellipse){
        const auto p=local(a,b.a),q=local(a,b.b)-p;const double A=dot(q,q),B=2*dot(p,q),C=dot(p,p)-1;
        if(A<1e-20)return out;
        double disc=B*B-4*A*C;const double tolerance=1e-12*(B*B+std::abs(4*A*C)+1);
        if(disc<-tolerance)return out;disc=std::max(0.,disc);
        for(double t:{(-B-std::sqrt(disc))/(2*A),(-B+std::sqrt(disc))/(2*A)})if(t>=-eps&&t<=1+eps)append(b.a+(b.b-b.a)*std::clamp(t,0.,1.));return out;
    }
    const auto point=[&](double t){return a.a+rotatePoint({a.rx*std::cos(t),a.ry*std::sin(t)},{},a.angle);};
    const auto value=[&](double t){const auto p=local(b,point(t));return dot(p,p)-1;};
    const auto derivative=[&](double t){const auto p=local(b,point(t));const auto d=rotatePoint(rotatePoint({-a.rx*std::sin(t),a.ry*std::cos(t)},{},a.angle),{},-b.angle);return 2*(p.x*d.x/b.rx+p.y*d.y/b.ry);};
    // Coincident curves do not define isolated construction points.
    if(std::abs(value(0))+std::abs(value(.7))+std::abs(value(1.8))+std::abs(value(3.1))+std::abs(value(4.8))<1e-10)return out;
    const auto bisect=[](auto f,double l,double r){double left=f(l);for(int i=0;i<55;++i){const auto m=(l+r)/2,v=f(m);if((v<0)==(left<0)){l=m;left=v;}else r=m;}return (l+r)/2;};
    std::vector<double> extrema{0};
    for(int i=0;i<720;++i){const double l=2*std::numbers::pi*i/720,r=2*std::numbers::pi*(i+1)/720,dl=derivative(l),dr=derivative(r);
        if(std::abs(dl)<1e-12)extrema.push_back(l);
        if((dl<0)!=(dr<0))extrema.push_back(bisect(derivative,l,r));
    }
    extrema.push_back(2*std::numbers::pi);std::sort(extrema.begin(),extrema.end());
    for(std::size_t i=0;i<extrema.size();++i){const double t=extrema[i],v=value(t);if(std::abs(v)<1e-9)append(point(t));
        if(i&&((value(extrema[i-1])<0)!=(v<0)))append(point(bisect(value,extrema[i-1],t)));
    }
    return out;
}
std::vector<MagneticGeometry::Feature> MagneticGeometry::featuresIn(Bounds area){
    std::vector<Feature> out;
    const auto append=[&](Point p,bool intersection=false){
        if(!area.contains(p,eps))return;
        const auto it=std::find_if(out.begin(),out.end(),[&](const Feature& q){return length(p-q.point)<1e-6;});
        if(it==out.end())out.push_back({p,intersection});
        else it->intersection|=intersection; // A shared endpoint can also be a crossing.
    };
    for(const auto& [point,owner]:endpoints_)if(area.contains(point,eps)&&visibleAt(owner,point))append(point);
    std::set<std::size_t> candidates(large_.begin(),large_.end());
    const int l=cell(area.left),r=cell(area.right),t=cell(area.top),d=cell(area.bottom);
    if(double(r-l+1)*(d-t+1)>8192){for(std::size_t i=0;i<primitives_.size();++i)candidates.insert(i);}
    else for(int x=l;x<=r;++x)for(int y=t;y<=d;++y){const auto it=cells_.find({x,y});if(it!=cells_.end())candidates.insert(it->second.begin(),it->second.end());}
    std::vector<std::size_t> ids;for(auto id:candidates)if(overlaps(area,primitives_[id].box))ids.push_back(id);
    std::sort(ids.begin(),ids.end(),[&](auto a,auto b){return primitives_[a].box.left<primitives_[b].box.left;});
    for(std::size_t i=0;i<ids.size();++i)for(std::size_t j=i+1;j<ids.size();++j){
        const auto& a=primitives_[ids[i]];const auto& b=primitives_[ids[j]];if(b.box.left>a.box.right+eps)break;
        if(!overlaps(a.box,b.box))continue;
        if(a.owner==b.owner&&(a.circularPath||a.ellipse||b.ellipse||std::max(a.step,b.step)-std::min(a.step,b.step)<=1||(a.closed&&std::max(a.step,b.step)-std::min(a.step,b.step)+1==a.count)))continue;
        const auto key=std::minmax(ids[i],ids[j]);auto it=intersections_.find(key);if(it==intersections_.end())it=intersections_.emplace(key,intersect(a,b)).first;
        for(const auto p:it->second)if(visibleAt(a.owner,p)&&visibleAt(b.owner,p))append(p,true);
    }
    return out;
}
std::vector<Point> MagneticGeometry::pointsIn(Bounds area){
    std::vector<Point> out;for(const auto& feature:featuresIn(area))out.push_back(feature.point);return out;
}
std::optional<Point> MagneticGeometry::nearest(Point p,double tolerance){
    std::optional<Point> best;double distance=tolerance;
    for(auto q:pointsIn({p.x-tolerance,p.y-tolerance,p.x+tolerance,p.y+tolerance})){const auto d=length(q-p);if(d<=distance){best=q;distance=d;}}
    return best;
}
bool MagneticGeometry::alignRuler(RulerGeometry& ruler,double tolerance,std::optional<RulerSnap>& latch,int edge,std::optional<Point> pivot){
    edge=std::clamp(edge,0,1);
    const auto fit=[&](RulerSnap snap,double radius)->std::optional<RulerGeometry>{
        if(pivot&&length(snap.first-*pivot)>1e-6&&length(snap.second-*pivot)>1e-6)return {};
        auto a=ruler.local(snap.first),b=ruler.local(snap.second);const double y=snap.edge*RulerGeometry::widthMm;
        if(std::abs(a.y-y)>radius||std::abs(b.y-y)>radius||a.x < -radius||b.x < -radius||a.x>ruler.lengthMm+radius||b.x>ruler.lengthMm+radius)return {};
        if(a.x>b.x){std::swap(a,b);std::swap(snap.first,snap.second);}
        const auto delta=snap.second-snap.first;const double distance=length(delta);
        // Two targets within the same capture neighborhood do not define a
        // reliable alignment: tiny retraced loops can create several crossings.
        if(distance<std::max(1.,tolerance*2)||distance>ruler.lengthMm)return {};
        const double angle=std::atan2(delta.y,delta.x);
        if(std::abs(std::remainder(angle-ruler.angle,2*std::numbers::pi))>std::numbers::pi/12)return {};
        auto result=ruler;result.angle=angle;
        const double offset=std::clamp(a.x,0.,ruler.lengthMm-distance);
        result.origin=snap.first-rotatePoint({offset,y},{},angle);
        if(pivot){result.origin=*pivot-rotatePoint({ruler.local(*pivot).x,y},{},angle);const auto x1=result.local(snap.first).x,x2=result.local(snap.second).x;if(x1<-eps||x2<-eps||x1>ruler.lengthMm+eps||x2>ruler.lengthMm+eps)return {};}
        return result;
    };
    latch.reset();const auto a=ruler.world({0,edge*RulerGeometry::widthMm}),b=ruler.world({ruler.lengthMm,edge*RulerGeometry::widthMm});
    // Crossings get a modest capture margin as well as ranking priority.
    const double intersectionTolerance=tolerance*2;
    auto points=featuresIn({std::min(a.x,b.x)-intersectionTolerance,std::min(a.y,b.y)-intersectionTolerance,std::max(a.x,b.x)+intersectionTolerance,std::max(a.y,b.y)+intersectionTolerance});
    int priority=-1;double score=2*tolerance+eps;
    {std::vector<std::pair<double,Feature>> near;
        for(const auto& p:points){const auto q=ruler.local(p.point);const double radius=p.intersection?intersectionTolerance:tolerance;if(q.x>=-radius&&q.x<=ruler.lengthMm+radius&&std::abs(q.y-edge*RulerGeometry::widthMm)<=radius&&std::abs(q.y-edge*RulerGeometry::widthMm)<=std::abs(q.y-(1-edge)*RulerGeometry::widthMm))near.push_back({std::abs(q.y-edge*RulerGeometry::widthMm),p});}
        std::stable_sort(near.begin(),near.end(),[](const auto& a,const auto& b){return a.first<b.first;});
        for(std::size_t i=0;i<near.size();++i)for(std::size_t j=i+1;j<near.size();++j){
            const int candidatePriority=int(near[i].second.intersection)+int(near[j].second.intersection);
            const double candidate=near[i].first+near[j].first;
            if(candidatePriority<priority||(candidatePriority==priority&&candidate>=score))continue;
            RulerSnap snap{near[i].second.point,near[j].second.point,edge,false,near[i].second.intersection,near[j].second.intersection};
            if(fit(snap,intersectionTolerance)){priority=candidatePriority;score=candidate;latch=snap;}
        }
        if(!latch){
            const Feature* best=nullptr;double distance=0;
            for(const auto& candidate:near){const auto& feature=candidate.second;if(pivot&&length(feature.point-*pivot)>1e-6)continue;
                if(!best||feature.intersection>best->intersection||(feature.intersection==best->intersection&&candidate.first<distance)){best=&feature;distance=candidate.first;}}
            if(best){const auto local=ruler.local(best->point);ruler.origin=ruler.origin+best->point-ruler.world({std::clamp(local.x,0.,ruler.lengthMm),edge*RulerGeometry::widthMm});latch=RulerSnap{best->point,best->point,edge,true,best->intersection,best->intersection};return true;}
        }
    }
    if(latch){ruler=*fit(*latch,intersectionTolerance);return true;}return false;
}
}
