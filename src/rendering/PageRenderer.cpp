#include "PageRenderer.h"
#include "rendering/StrokeMesh.h"
#include "rendering/ShapeMesh.h"
#include "rendering/ShapeRasterCache.h"
#include "rendering/BackgroundMesh.h"
#include "geometry/Geometry.h"
#include "rendering/TextRenderer.h"
#include "pdf/PdfService.h"
#include <QImage>
#include <QPainter>
#include <QPainterPath>
namespace scalar {
QString paintPage(QPainter& painter,const Page& page,double scale){
    painter.save();painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QString error;
    const auto color=[](std::uint32_t c){return QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);};
    const auto area=pageRenderBounds(page);const QRectF pageRect(area.left,area.top,area.width(),area.height());
    painter.fillRect(pageRect,color(page.background));
    painter.setClipRect(pageRect);painter.setPen(Qt::NoPen);
    if(page.pdf){const auto size=page.pdf->size.valid()?page.pdf->size:page.size;const auto pdf=renderPdf(*page.pdf,pdfRenderSize(size,scale),&error);if(pdf.isNull()){painter.restore();return error;}painter.drawImage(QRectF(0,0,size.widthMm,size.heightMm),pdf);}
    const auto grid=backgroundMesh(page.backgroundStyle,area,1/scale);
    QPainterPath gridPath;gridPath.setFillRule(Qt::WindingFill);
    for(std::size_t i=0;i+2<grid.size();i+=3){gridPath.moveTo(grid[i].x,grid[i].y);gridPath.lineTo(grid[i+1].x,grid[i+1].y);gridPath.lineTo(grid[i+2].x,grid[i+2].y);gridPath.closeSubpath();}
    auto gridColor=color(page.backgroundStyle.gridColor);gridColor.setAlphaF(gridColor.alphaF()*page.backgroundStyle.opacity);painter.fillPath(gridPath,gridColor);
    const auto paintMasked=[&]<class T>(const T& value){const auto* object=&value;
        const auto outline=[&]{if constexpr(std::is_same_v<T,ShapeObject>)return shapeOutline(*object);else if constexpr(std::is_same_v<T,ImageObject>)return object->corners;else {std::vector<Point> points;for(const auto& sample:object->samples)points.push_back(sample.position);return points;}}();if(outline.empty())return;
        double left=outline.front().x,right=left,top=outline.front().y,bottom=top;
        for(auto p:outline){left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
        const double maskScale=std::clamp(std::ceil(scale*4)/4,.25,128.);
        const double margin=[&]{if constexpr(std::is_same_v<T,ImageObject>)return 2/maskScale;else return object->style.maxWidthMm*.5+2/maskScale;}();
        const auto visible=QRectF(QPointF(left-margin,top-margin),QPointF(right+margin,bottom+margin)).intersected(pageRect);
        ShapeRasterCache raster;
        const auto bitmap=[&]{if constexpr(std::is_same_v<T,ImageObject>){if(object->png)return QImage::fromData(reinterpret_cast<const uchar*>(object->png->data()),int(object->png->size()),"PNG");}return QImage{};}();
        if constexpr(std::is_same_v<T,ImageObject>)if(bitmap.isNull()){error="Não foi possível renderizar uma imagem incorporada.";return;}
        const auto paintTiles=[&](QRectF area,std::optional<ShapeRasterCache::Key> only={}){if constexpr(std::is_same_v<T,ImageObject>)raster.update(*object,bitmap,maskScale,area);else raster.update(*object,maskScale,area);for(const auto& [key,tile]:raster.tiles())if(!only||key==*only)painter.drawImage(tile.worldRect,tile.image,QRectF(1,1,ShapeRasterCache::tilePixels,ShapeRasterCache::tilePixels));};
        if(visible.width()*visible.height()*maskScale*maskScale<=4000000)paintTiles(visible);
        else {
            // Stream large exports rather than retaining an entire page
            // of original, mask and composited bitmaps in memory.
            const double step=ShapeRasterCache::tilePixels/maskScale;
            for(int y=int(std::floor(visible.top()/step));y<int(std::ceil(visible.bottom()/step));++y)
                for(int x=int(std::floor(visible.left()/step));x<int(std::ceil(visible.right()/step));++x)paintTiles(QRectF(x*step,y*step,step,step).intersected(visible),ShapeRasterCache::Key{x,y});
        }

    };
    for(const auto& view:objectViews(page))std::visit([&](const auto* object){
        if(!object->properties.visible)return;
        using T=std::decay_t<decltype(*object)>;
        if constexpr(std::is_same_v<T,StrokeObject>){
            if((!object->erasedRegions.empty()||object->eraseMask.has_value())){paintMasked(*object);return;}
            if(object->marker&&!object->samples.empty()&&object->style.pattern==LinePattern::Solid){
                QPainterPath centerline;const auto first=object->samples.front().position;centerline.moveTo(first.x,first.y);
                for(std::size_t i=1;i<object->samples.size();++i){const auto p=object->samples[i].position;centerline.lineTo(p.x,p.y);}
                painter.save();painter.setPen(QPen(color(object->style.rgba),object->style.maxWidthMm,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                if(object->samples.size()==1)painter.drawPoint(QPointF(first.x,first.y));else painter.drawPath(centerline);
                painter.restore();return;
            }
            // Retain vector ink while removing redundant samples and overlapping
            // per-sample disks (the display mesh bounds error to 0.004 mm).
            const auto mesh=strokeDisplayMesh(*object);QPainterPath triangles;triangles.setFillRule(Qt::WindingFill);
            for(std::size_t i=0;i+2<mesh.size();i+=3){auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];if((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)<0)std::swap(b,c);triangles.moveTo(a.x,a.y);triangles.lineTo(b.x,b.y);triangles.lineTo(c.x,c.y);triangles.closeSubpath();}
            painter.fillPath(triangles,color(object->style.rgba));
        }else if constexpr(std::is_same_v<T,ShapeObject>){
            if((!object->erasedRegions.empty()||object->eraseMask.has_value())){paintMasked(*object);return;}
            const auto points=shapeOutline(*object);if(points.empty())return;QPainterPath path;path.moveTo(points[0].x,points[0].y);
            for(std::size_t i=1;i<points.size();++i)path.lineTo(points[i].x,points[i].y);
            if(object->kind!=ShapeKind::Line&&object->kind!=ShapeKind::CircularArc){
                QPainterPath fillPath;fillPath.setFillRule(Qt::WindingFill);const auto mesh=shapeFillMesh(*object);
                for(std::size_t i=0;i+2<mesh.size();i+=3){auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];if((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)<0)std::swap(b,c);fillPath.moveTo(a.x,a.y);fillPath.lineTo(b.x,b.y);fillPath.lineTo(c.x,c.y);fillPath.closeSubpath();}
                auto fill=color(object->fillColor());fill.setAlphaF(object->fillOpacity);painter.fillPath(fillPath,fill);
            }
            const auto mesh=shapeBorderMesh(*object);QPainterPath border;border.setFillRule(Qt::WindingFill);
            for(std::size_t i=0;i+2<mesh.size();i+=3){border.moveTo(mesh[i].x,mesh[i].y);border.lineTo(mesh[i+1].x,mesh[i+1].y);border.lineTo(mesh[i+2].x,mesh[i+2].y);border.closeSubpath();}
            painter.fillPath(border,color(object->style.rgba));
        }else if constexpr(std::is_same_v<T,TextObject>){
            if(object->corners.size()!=4)return;const auto origin=object->corners[0],edge=object->corners[1]-origin;
            const auto natural=textNaturalSize(*object);
            painter.save();painter.translate(origin.x,origin.y);painter.rotate(std::atan2(edge.y,edge.x)*180/3.141592653589793);painter.scale(length(edge)/natural.width(),length(object->corners[3]-origin)/natural.height());
            const auto failure=paintVectorText(painter,*object);if(!failure.isEmpty())error=failure;painter.restore();
        }else {
            if((!object->erasedRegions.empty()||object->eraseMask.has_value())){paintMasked(*object);return;}
            if(object->corners.size()!=4)return;
            if(!object->png){error="A imagem não possui dados incorporados.";return;}
            const auto bitmap=QImage::fromData(reinterpret_cast<const uchar*>(object->png->data()),int(object->png->size()),"PNG");
            if(bitmap.isNull()){error="Não foi possível renderizar uma imagem incorporada.";return;}
            const auto origin=object->corners[0],edge=object->corners[1]-origin;
            painter.save();painter.translate(origin.x,origin.y);painter.rotate(std::atan2(edge.y,edge.x)*180/3.141592653589793);painter.drawImage(QRectF(0,0,length(edge),length(object->corners[3]-origin)),bitmap);painter.restore();
        }
    },view);
    painter.restore();return error;
}
}
