#include "PageRenderer.h"
#include "rendering/StrokeMesh.h"
#include "rendering/ShapeMesh.h"
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
    painter.fillRect(QRectF(0,0,page.size.widthMm,page.size.heightMm),color(page.background));
    painter.setClipRect(QRectF(0,0,page.size.widthMm,page.size.heightMm));painter.setPen(Qt::NoPen);
    if(page.pdf){const auto pdf=renderPdf(*page.pdf,pdfRenderSize(page.size,scale),&error);if(pdf.isNull()){painter.restore();return error;}painter.drawImage(QRectF(0,0,page.size.widthMm,page.size.heightMm),pdf);}
    const auto grid=backgroundMesh(page.backgroundStyle,{0,0,page.size.widthMm,page.size.heightMm},1/scale);
    QPainterPath gridPath;gridPath.setFillRule(Qt::WindingFill);
    for(std::size_t i=0;i+2<grid.size();i+=3){gridPath.moveTo(grid[i].x,grid[i].y);gridPath.lineTo(grid[i+1].x,grid[i+1].y);gridPath.lineTo(grid[i+2].x,grid[i+2].y);gridPath.closeSubpath();}
    auto gridColor=color(page.backgroundStyle.gridColor);gridColor.setAlphaF(gridColor.alphaF()*page.backgroundStyle.opacity);painter.fillPath(gridPath,gridColor);
    for(const auto& view:objectViews(page))std::visit([&](const auto* object){
        if(!object->properties.visible)return;
        using T=std::decay_t<decltype(*object)>;
        if constexpr(std::is_same_v<T,StrokeObject>){
            const auto mesh=strokeMesh(*object);QPainterPath triangles;triangles.setFillRule(Qt::WindingFill);
            for(std::size_t i=0;i+2<mesh.size();i+=3){triangles.moveTo(mesh[i].x,mesh[i].y);triangles.lineTo(mesh[i+1].x,mesh[i+1].y);triangles.lineTo(mesh[i+2].x,mesh[i+2].y);triangles.closeSubpath();}
            painter.fillPath(triangles,color(object->style.rgba));
        }else if constexpr(std::is_same_v<T,ShapeObject>){
            const auto points=shapeOutline(*object);if(points.empty())return;QPainterPath path;path.moveTo(points[0].x,points[0].y);
            for(std::size_t i=1;i<points.size();++i)path.lineTo(points[i].x,points[i].y);
            if(object->kind!=ShapeKind::Line){path.closeSubpath();auto fill=color(object->fillColor());fill.setAlphaF(object->fillOpacity);painter.fillPath(path,fill);}
            const auto mesh=shapeBorderMesh(*object);QPainterPath border;border.setFillRule(Qt::WindingFill);
            for(std::size_t i=0;i+2<mesh.size();i+=3){border.moveTo(mesh[i].x,mesh[i].y);border.lineTo(mesh[i+1].x,mesh[i+1].y);border.lineTo(mesh[i+2].x,mesh[i+2].y);border.closeSubpath();}
            painter.fillPath(border,color(object->style.rgba));
        }else if constexpr(std::is_same_v<T,TextObject>){
            if(object->corners.size()!=4)return;const auto origin=object->corners[0],edge=object->corners[1]-origin;
            const auto natural=textNaturalSize(*object);
            painter.save();painter.translate(origin.x,origin.y);painter.rotate(std::atan2(edge.y,edge.x)*180/3.141592653589793);painter.scale(length(edge)/natural.width(),length(object->corners[3]-origin)/natural.height());
            const auto failure=paintVectorText(painter,*object);if(!failure.isEmpty())error=failure;painter.restore();
        }else {
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
