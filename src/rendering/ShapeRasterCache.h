#pragma once
#include "documents/Document.h"
#include <QImage>
#include <QRectF>
#include <map>
namespace scalar {
// Fixed-size visibility tiles: brush work depends on its pixel footprint,
// not on the number of triangles produced by earlier cuts.
class ShapeRasterCache {
public:
    static constexpr int tilePixels=256;
    using Key=std::pair<int,int>;
    struct Tile {
        QRectF worldRect;
        QImage original,mask,image;
        std::vector<std::uint8_t> coverage; // four fixed subpixel visibility samples
        std::size_t applied=0;
        std::uint64_t revision=0;
    };
    void update(const ShapeObject& shape,double pixelsPerMm,QRectF visible);
    void update(const StrokeObject& stroke,double pixelsPerMm,QRectF visible);
    void update(const ImageObject& image,const QImage& bitmap,double pixelsPerMm,QRectF visible);
    const std::map<Key,Tile>& tiles() const {return tiles_;}
    std::size_t updatedTiles() const {return updatedTiles_;}
private:
    void updateImpl(const ShapeObject& shape,double pixelsPerMm,QRectF visible,const StrokeObject* stroke,const QImage* bitmap=nullptr);
    std::map<Key,Tile> tiles_;
    std::vector<double> signature_;
    std::vector<ErasedRegion> applied_;
    double scale_=0;
    std::uint64_t generation_=0;
    std::size_t updatedTiles_=0;
};
}
