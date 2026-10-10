#pragma once
#include "documents/Document.h"
#include <QPainter>
#include <QPolygonF>
namespace scalar {
inline void prepareEraseMask(EraseMask& mask,const QImage& bitmap){
    mask.pixelWidth=bitmap.width();mask.pixelHeight=bitmap.height();auto alpha=std::make_shared<std::vector<std::uint8_t>>(std::size_t(bitmap.width())*bitmap.height());
    const auto image=bitmap.convertToFormat(QImage::Format_ARGB32);for(int y=0;y<image.height();++y){const auto* row=reinterpret_cast<const QRgb*>(image.constScanLine(y));for(int x=0;x<image.width();++x)(*alpha)[std::size_t(y)*image.width()+x]=qAlpha(row[x]);}mask.alpha=std::move(alpha);
}
inline QImage decodeEraseMask(const EraseMask& mask){
    return mask.png?QImage::fromData(mask.png->data(),int(mask.png->size()),"PNG"):QImage{};
}
inline void paintEraseMask(QPainter& painter,const EraseMask& mask,const QImage& bitmap){
    if(bitmap.isNull()||mask.corners.size()!=4)return;
    QPolygonF from{{0,0},{double(bitmap.width()),0},{double(bitmap.width()),double(bitmap.height())},{0,double(bitmap.height())}},to;
    for(auto point:mask.corners)to.append(QPointF(point.x,point.y));QTransform transform;
    if(!QTransform::quadToQuad(from,to,transform))return;
    painter.save();painter.setTransform(transform,true);painter.setRenderHint(QPainter::SmoothPixmapTransform);painter.drawImage(QPointF(0,0),bitmap);painter.restore();
}
}
