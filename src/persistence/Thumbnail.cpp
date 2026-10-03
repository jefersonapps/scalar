#include "Thumbnail.h"
#include "rendering/PageRenderer.h"
#include <QImage>
#include <QPainter>
#include <QSaveFile>
namespace scalar {
QImage pageThumbnail(const Page& page,QSize size){
    const int width=size.width(),height=size.height();
    QImage image(width,height,QImage::Format_ARGB32_Premultiplied);image.fill(QColor("#e9eef1"));
    QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);
    const double scale=std::min((width-24)/page.size.widthMm,(height-24)/page.size.heightMm);
    painter.translate((width-scale*page.size.widthMm)/2,(height-scale*page.size.heightMm)/2);painter.scale(scale,scale);
    paintPage(painter,page,scale);painter.end();return image;
}

bool saveThumbnail(const Project& project,const QString& path){
    if(project.pages.empty())return false;const auto image=pageThumbnail(project.pages.front());
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
    if(!image.save(&file,"PNG"))return false;return file.commit();
}
}
