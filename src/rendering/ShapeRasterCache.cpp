#include "ShapeRasterCache.h"
#include "EraseMask.h"
#include "geometry/Geometry.h"
#include "rendering/StrokeMesh.h"
#include <QPainter>
#include <QPainterPath>
#include <bit>
namespace scalar {
namespace {
QColor color(std::uint32_t c){return QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);}
bool same(const ErasedRegion& a,const ErasedRegion& b){return a.from==b.from&&a.to==b.to&&a.radius==b.radius&&a.restore==b.restore;}
QRectF regionBounds(const ErasedRegion& region){return QRectF(QPointF(std::min(region.from.x,region.to.x)-region.radius,std::min(region.from.y,region.to.y)-region.radius),QPointF(std::max(region.from.x,region.to.x)+region.radius,std::max(region.from.y,region.to.y)+region.radius));}
void paintOriginal(QPainter& painter,const ShapeObject& shape,const std::vector<Point>& outline){
    if(outline.empty())return;
    QPainterPath path;path.moveTo(outline.front().x,outline.front().y);for(std::size_t i=1;i<outline.size();++i)path.lineTo(outline[i].x,outline[i].y);
    const bool closed=shape.kind!=ShapeKind::Line&&shape.kind!=ShapeKind::CircularArc;
    if(closed){path.closeSubpath();auto fill=color(shape.fillColor());fill.setAlphaF(shape.fillOpacity);painter.fillPath(path,fill);}
    const double width=shape.style.width(1);
    QPen pen(color(shape.style.rgba),width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
    if(width>0&&shape.style.pattern==LinePattern::Dashed)pen.setDashPattern({shape.style.dashLengthMm/width,shape.style.gapLengthMm/width});
    if(width>0&&shape.style.pattern==LinePattern::Dotted)pen.setDashPattern({.001,shape.style.dotSpacingMm/width});
    painter.setPen(pen);painter.drawPath(path);
    if(shape.kind==ShapeKind::RightAngle&&shape.vertices.size()==4){
        Point center;for(auto p:shape.vertices)center=center+p;center=center*.25;
        const double side=std::min(length(shape.vertices[1]-shape.vertices[0]),length(shape.vertices[3]-shape.vertices[0]));
        const double radius=std::min(side*.10,std::max(.15,shape.style.maxWidthMm*.65));
        painter.setPen(Qt::NoPen);painter.setBrush(color(shape.style.rgba));painter.drawEllipse(QPointF(center.x,center.y),radius,radius);
    }
}
}
void ShapeRasterCache::update(const ShapeObject& shape,double pixelsPerMm,QRectF visible){
    updateImpl(shape,pixelsPerMm,visible,nullptr);
}
void ShapeRasterCache::update(const StrokeObject& stroke,double pixelsPerMm,QRectF visible){
    ShapeObject proxy;proxy.kind=ShapeKind::CircularArc;proxy.style=stroke.style;proxy.erasedRegions=stroke.erasedRegions;proxy.eraseMask=stroke.eraseMask;
    for(const auto& sample:stroke.samples)proxy.vertices.push_back(sample.position);
    updateImpl(proxy,pixelsPerMm,visible,&stroke);
}
void ShapeRasterCache::update(const ImageObject& image,const QImage& bitmap,double pixelsPerMm,QRectF visible){
    ShapeObject proxy;proxy.kind=ShapeKind::Polygon;proxy.vertices=image.corners;proxy.erasedRegions=image.erasedRegions;
    proxy.eraseMask=image.eraseMask;
    proxy.style.minWidthMm=proxy.style.maxWidthMm=0;
    updateImpl(proxy,pixelsPerMm,visible,nullptr,&bitmap);
}
void ShapeRasterCache::updateImpl(const ShapeObject& shape,double pixelsPerMm,QRectF visible,const StrokeObject* stroke,const QImage* bitmap){
    updatedTiles_=0;
    const auto& style=shape.style;
    std::vector<double> signature{double(shape.kind),shape.center.x,shape.center.y,shape.radiusX,shape.radiusY,shape.rotation,shape.fillOpacity,double(shape.fillColor()),double(style.rgba),style.minWidthMm,style.maxWidthMm,style.gamma,style.sensitivity,double(style.pattern),style.dashLengthMm,style.gapLengthMm,style.dotSpacingMm};
    for(auto p:shape.vertices){signature.push_back(p.x);signature.push_back(p.y);}
    signature.push_back(shape.eraseMask?double(reinterpret_cast<std::uintptr_t>(shape.eraseMask->png.get())):0);
    if(shape.eraseMask)for(auto p:shape.eraseMask->corners){signature.push_back(p.x);signature.push_back(p.y);}
    signature.push_back(stroke?1:0);signature.push_back(stroke&&stroke->marker?1:0);if(stroke)for(const auto& sample:stroke->samples)signature.push_back(sample.pressure);
    signature.push_back(bitmap?double(bitmap->cacheKey()):0);
    const double scale=std::clamp(std::ceil(pixelsPerMm*4)/4,.25,128.);
    const bool geometryChanged=signature!=signature_||scale!=scale_;
    if(geometryChanged)permanentMask_=shape.eraseMask?decodeEraseMask(*shape.eraseMask):QImage{};
    if(stroke&&stroke->marker&&geometryChanged)displaySamples_=strokeDisplaySamples(*stroke,.002);
    bool reset=geometryChanged||applied_.size()>shape.erasedRegions.size();
    std::size_t start=applied_.size();
    std::optional<ErasedRegion> extension;
    if(!reset)for(std::size_t i=0;i<applied_.size();++i)if(!same(applied_[i],shape.erasedRegions[i])){
        const auto& old=applied_[i];const auto& current=shape.erasedRegions[i];
        // A straight gesture can extend its final capsule. Every earlier
        // operation must still match; undo or edits rebuild the visibility mask.
        if(i+1==applied_.size()&&old.from==current.from&&old.radius==current.radius&&old.restore==current.restore&&distanceToSegment(old.to,current.from,current.to)<1e-8){start=i;extension=ErasedRegion{old.to,current.to,current.radius,current.restore};}
        else reset=true;
        break;
    }
    const bool resetMasks=reset&&!geometryChanged;
    if(reset){
        if(geometryChanged)tiles_.clear();
        else for(auto& [key,tile]:tiles_){
            // Undo changes visibility, not the original ink. Retain its expensive
            // raster and rebuild only the small, fixed-size visibility masks.
            tile.mask.fill(Qt::white);std::fill(tile.coverage.begin(),tile.coverage.end(),15);tile.image=tile.original;tile.applied=0;tile.revision=++generation_;
        }
        start=0;signature_=std::move(signature);scale_=scale;
    }
    const auto outline=shapeOutline(shape);
    if(outline.empty()){tiles_.clear();applied_=shape.erasedRegions;return;}
    double left=outline.front().x,right=left,top=outline.front().y,bottom=top;
    for(auto p:outline){left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
    const double margin=style.maxWidthMm*.5+2/scale;
    visible=visible.intersected(QRectF(QPointF(left-margin,top-margin),QPointF(right+margin,bottom+margin)));
    if(visible.isEmpty()){tiles_.clear();applied_=shape.erasedRegions;return;}
    const int x0=int(std::floor(visible.left()*scale/tilePixels)),x1=int(std::ceil(visible.right()*scale/tilePixels))-1;
    const int y0=int(std::floor(visible.top()*scale/tilePixels)),y1=int(std::ceil(visible.bottom()*scale/tilePixels))-1;
    std::erase_if(tiles_,[&](const auto& tile){return tile.first.first<x0||tile.first.first>x1||tile.first.second<y0||tile.first.second>y1;});
    std::optional<QPainterPath> inkPath;
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
        auto [entry,created]=tiles_.try_emplace({x,y});auto& tile=entry->second;
        if(created){
            tile.worldRect=QRectF(x*tilePixels/scale,y*tilePixels/scale,tilePixels/scale,tilePixels/scale);
            tile.inkRect=tile.worldRect;
            tile.original=QImage(tilePixels+2,tilePixels+2,QImage::Format_ARGB32_Premultiplied);tile.original.fill(Qt::transparent);
            QPainter painter(&tile.original);painter.setRenderHint(QPainter::Antialiasing);painter.scale(scale,scale);painter.translate(-tile.worldRect.left()+1/scale,-tile.worldRect.top()+1/scale);const auto worldTransform=painter.worldTransform();
            if(bitmap){
              if(outline.size()==4&&!bitmap->isNull()){
                const auto origin=outline[0],a=(outline[1]-origin)*(1./bitmap->width()),b=(outline[3]-origin)*(1./bitmap->height());
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                painter.setWorldTransform(QTransform(a.x,a.y,b.x,b.y,origin.x,origin.y),true);
                painter.drawImage(QPointF(0,0),*bitmap);
              }
            }else if(stroke){
              if(stroke->marker&&stroke->style.pattern==LinePattern::Solid){
                // A marker has constant width. Rasterize its centerline rather
                // than unioning every overlapping triangle of the brush mesh.
                QPainterPath centerline;const double padding=stroke->style.maxWidthMm*.5+2/scale;
                const auto area=tile.worldRect.adjusted(-padding,-padding,padding,padding);std::size_t last=0;
                for(std::size_t i=1;i<displaySamples_.size();++i){const auto a=displaySamples_[i-1].position,b=displaySamples_[i].position;
                    if(std::max(a.x,b.x)<area.left()||std::min(a.x,b.x)>area.right()||std::max(a.y,b.y)<area.top()||std::min(a.y,b.y)>area.bottom())continue;
                    if(last!=i-1||centerline.isEmpty())centerline.moveTo(a.x,a.y);
                    centerline.lineTo(b.x,b.y);last=i;
                }
                auto opaque=color(stroke->style.rgba);const int alpha=opaque.alpha();opaque.setAlpha(255);
                painter.setPen(QPen(opaque,stroke->style.maxWidthMm,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                if(stroke->samples.size()==1){const auto p=stroke->samples.front().position;painter.drawPoint(QPointF(p.x,p.y));}else painter.drawPath(centerline);
                const auto ink=stroke->samples.size()==1?QRectF(QPointF(stroke->samples.front().position.x,stroke->samples.front().position.y),QSizeF(0,0)):centerline.boundingRect();
                tile.inkRect=(centerline.isEmpty()&&stroke->samples.size()!=1)?QRectF{}:ink.adjusted(-padding,-padding,padding,padding).intersected(tile.worldRect);
                painter.save();painter.resetTransform();painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);painter.fillRect(tile.original.rect(),QColor(0,0,0,alpha));painter.restore();
              }else{
                if(!inkPath){inkPath.emplace();inkPath->setFillRule(Qt::WindingFill);const auto mesh=strokeMesh(*stroke);
                    for(std::size_t i=0;i+2<mesh.size();i+=3){auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];if((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)<0)std::swap(b,c);
                        inkPath->moveTo(a.x,a.y);inkPath->lineTo(b.x,b.y);inkPath->lineTo(c.x,c.y);inkPath->closeSubpath();}}
                painter.fillPath(*inkPath,color(stroke->style.rgba));
              }
            }else paintOriginal(painter,shape,outline);
            if(shape.eraseMask){painter.setWorldTransform(worldTransform);painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);paintEraseMask(painter,*shape.eraseMask,permanentMask_);}
            painter.end();
            tile.mask=QImage(tile.original.size(),QImage::Format_ARGB32_Premultiplied);tile.mask.fill(Qt::white);tile.coverage.assign(std::size_t(tile.mask.width())*tile.mask.height(),15);tile.image=tile.original;tile.revision=++generation_;
        }
        QRect dirty;
        const auto expanded=tile.worldRect.adjusted(-1/scale,-1/scale,1/scale,1/scale);
        for(std::size_t i=created?0:start;i<shape.erasedRegions.size();++i){const auto& region=(!created&&extension&&i==start)?*extension:shape.erasedRegions[i];
            const auto affected=regionBounds(region).adjusted(-1/scale,-1/scale,1/scale,1/scale).intersected(expanded);if(affected.isEmpty())continue;
            const auto pixels=QRectF((affected.left()-tile.worldRect.left())*scale+1,(affected.top()-tile.worldRect.top())*scale+1,affected.width()*scale,affected.height()*scale).toAlignedRect().intersected(tile.mask.rect());
            const Point from{(region.from.x-tile.worldRect.left())*scale+1,(region.from.y-tile.worldRect.top())*scale+1},axis=(region.to-region.from)*scale;
            const double squared=axis.x*axis.x+axis.y*axis.y,radiusSquared=region.radius*region.radius*scale*scale;
            bool changed=false;
            for(int py=pixels.top();py<=pixels.bottom();++py){auto* row=reinterpret_cast<QRgb*>(tile.mask.scanLine(py));for(int px=pixels.left();px<=pixels.right();++px){
                auto& bits=tile.coverage[std::size_t(py)*tile.mask.width()+px];if(bits==(region.restore?15:0))continue;
                unsigned footprint=0;
                for(int sample=0;sample<4;++sample){const Point offset{px+.25+.5*(sample&1)-from.x,py+.25+.5*(sample>>1)-from.y};
                    const double t=squared>1e-16?std::clamp((offset.x*axis.x+offset.y*axis.y)/squared,0.,1.):0;
                    const auto delta=offset-axis*t;if(delta.x*delta.x+delta.y*delta.y<=radiusSquared)footprint|=1u<<sample;
                }
                const auto next=std::uint8_t(region.restore?bits|footprint:bits&~footprint);if(next==bits)continue;
                bits=next;const int alpha=(std::popcount(unsigned(bits))*255+2)/4;row[px]=qRgba(alpha,alpha,alpha,alpha);changed=true;
            }}
            if(changed)dirty=dirty.united(pixels);
        }
        tile.applied=shape.erasedRegions.size();
        dirty=dirty.intersected(tile.image.rect());
        if(!dirty.isEmpty()){
            QPainter painter(&tile.image);painter.setCompositionMode(QPainter::CompositionMode_Source);painter.drawImage(dirty,tile.original,dirty);painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);painter.drawImage(dirty,tile.mask,dirty);painter.end();tile.revision=++generation_;
        }
        if(created||resetMasks||!dirty.isEmpty())++updatedTiles_;
    }
    applied_=shape.erasedRegions;
}
}
