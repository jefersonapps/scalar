#include "Thumbnail.h"
#include "rendering/StrokeMesh.h"
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
namespace scalar {
bool saveThumbnail(const Project& project,const QString& path){
    if(project.pages.empty())return false;
    const auto& page=project.pages.front();constexpr int width=640,height=400;
    QImage image(width,height,QImage::Format_ARGB32_Premultiplied);image.fill(QColor("#e9eef1"));
    QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);
    const double scale=std::min((width-24)/page.size.widthMm,(height-24)/page.size.heightMm);
    painter.translate((width-scale*page.size.widthMm)/2,(height-scale*page.size.heightMm)/2);painter.scale(scale,scale);
    const auto color=[](std::uint32_t c){return QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);};
    painter.fillRect(QRectF(0,0,page.size.widthMm,page.size.heightMm),color(page.background));
    painter.setClipRect(QRectF(0,0,page.size.widthMm,page.size.heightMm));painter.setPen(Qt::NoPen);
    for(const auto& stroke:page.strokes){
        const auto mesh=strokeMesh(stroke);QPainterPath triangles;triangles.setFillRule(Qt::WindingFill);
        for(std::size_t i=0;i+2<mesh.size();i+=3){triangles.moveTo(mesh[i].x,mesh[i].y);triangles.lineTo(mesh[i+1].x,mesh[i+1].y);triangles.lineTo(mesh[i+2].x,mesh[i+2].y);triangles.closeSubpath();}
        painter.fillPath(triangles,color(stroke.style.rgba));
    }
    painter.end();QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
    if(!image.save(&file,"PNG"))return false;return file.commit();
}
}
