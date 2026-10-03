#include "BackgroundMesh.h"
#include <numbers>
namespace scalar {
std::vector<Point> backgroundMesh(const BackgroundStyle& s,Bounds b,double minimum){
    std::vector<Point> mesh;
    if(!s.valid()||s.gridType==GridType::None||s.opacity==0||b.width()<=0||b.height()<=0)return mesh;
    const auto line=[&](Point a,Point c,double width){
        const auto d=c-a;const double len=length(d);if(len<1e-9)return;
        const Point n{-d.y/len*width/2,d.x/len*width/2};
        mesh.insert(mesh.end(),{a+n,a-n,c+n,c+n,a-n,c-n});
    };
    const double factor=std::max(1.,std::ceil(minimum/std::min(s.spacingX,s.spacingY)));
    const double sx=s.spacingX*factor,sy=s.spacingY*factor;
    const auto horizontal=[&](double step,double width){for(double y=std::ceil(b.top/step)*step;y<=b.bottom;y+=step)line({b.left,y},{b.right,y},width);};
    const auto vertical=[&](double step,double width){for(double x=std::ceil(b.left/step)*step;x<=b.right;x+=step)line({x,b.top},{x,b.bottom},width);};
    if(s.gridType==GridType::Dots){
        for(double y=std::ceil(b.top/sy)*sy;y<=b.bottom;y+=sy)for(double x=std::ceil(b.left/sx)*sx;x<=b.right;x+=sx){
            const double r=s.thicknessMm*1.5;
            for(int i=0;i<8;++i){const auto a=2*std::numbers::pi*i/8,c=2*std::numbers::pi*(i+1)/8;mesh.insert(mesh.end(),{{x,y},{x+r*std::cos(a),y+r*std::sin(a)},{x+r*std::cos(c),y+r*std::sin(c)}});}
        }
    }else if(s.gridType==GridType::Isometric){
        horizontal(sy,s.thicknessMm);
        const double slope=std::sqrt(3.);
        for(double sign:{-1.,1.}){
            const double lo=b.top-std::max(sign*slope*b.left,sign*slope*b.right),hi=b.bottom-std::min(sign*slope*b.left,sign*slope*b.right);
            for(double k=std::ceil(lo/sy)*sy;k<=hi;k+=sy){
                double left=b.left,right=b.right;
                const double a=(b.top-k)/(sign*slope),c=(b.bottom-k)/(sign*slope);
                left=std::max(left,std::min(a,c));right=std::min(right,std::max(a,c));
                if(right>left)line({left,sign*slope*left+k},{right,sign*slope*right+k},s.thicknessMm);
            }
        }
    }else{
        horizontal(sy,s.thicknessMm);if(s.gridType!=GridType::Ruled)vertical(sx,s.thicknessMm);
        if(s.gridType==GridType::Millimetric&&minimum<=1){horizontal(1,s.thicknessMm*.35);vertical(1,s.thicknessMm*.35);}
    }
    return mesh;
}
}
