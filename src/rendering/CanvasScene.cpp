#include "canvas/CanvasItem.h"
#include "StrokeMesh.h"
#include "ShapeMesh.h"
#include "ShapeRasterCache.h"
#include "BackgroundMesh.h"
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>
#include <QSGTransformNode>
#include <QSGClipNode>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include <QMatrix4x4>
#include <QSet>
#include <QPainter>
#include <QPainterPath>
#include <numbers>
#include <unordered_map>
namespace scalar {
namespace {
QColor fromRgba(std::uint32_t c){return QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);}
struct SceneRoot : QSGTransformNode {
    QSGSimpleRectNode* paper=new QSGSimpleRectNode;
    QSGClipNode* clip=new QSGClipNode;
    struct Entry {QSGNode* node;std::uint64_t revision;std::size_t kind;bool editing=false;double rasterScale=0;QRectF rasterBounds;};
    std::unordered_map<std::string,Entry> strokes;
    struct RasterShape {
        ShapeRasterCache raster;
        struct Texture {std::unique_ptr<QSGTexture> texture;std::uint64_t revision=0;};
        std::map<ShapeRasterCache::Key,Texture> textures;
    };
    std::unordered_map<std::string,RasterShape> rasterShapes;
    QSGNode* live=nullptr;
    QSGNode* grid=nullptr;
    QSGNode* pdf=nullptr;
    quint64 pdfRevision=0;
    std::string pdfPageId;
    BackgroundStyle gridStyle;
    QRectF gridBounds;
    double gridScale=0;
    SceneRoot(){appendChildNode(paper);appendChildNode(clip);clip->setIsRectangular(true);}
};
QSGGeometryNode* meshNode(std::span<const Point> vertices,QColor color) {
    auto* node=new QSGGeometryNode;
    auto* geometry=new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),int(vertices.size()));
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* data=geometry->vertexDataAsPoint2D();for(std::size_t i=0;i<vertices.size();++i)data[i].set(float(vertices[i].x),float(vertices[i].y));
    node->setGeometry(geometry);node->setFlag(QSGNode::OwnsGeometry);
    auto* material=new QSGFlatColorMaterial;material->setColor(color);node->setMaterial(material);node->setFlag(QSGNode::OwnsMaterial);return node;
}
QSGNode* objectNode(const StrokeObject& stroke,QQuickWindow* window){
    auto* root=new QSGNode;const auto color=fromRgba(stroke.style.rgba);const auto mesh=strokeMesh(stroke);
    if(color.alpha()==255||!window){root->appendChildNode(meshNode(mesh,color));return root;}
    // Flatten translucent ink once so overlapping triangles do not darken a stroke.
    const auto b=bounds(CanvasObject(stroke));const double margin=stroke.style.maxWidthMm/2+.2;
    const QRectF rect(b.left-margin,b.top-margin,b.width()+2*margin,b.height()+2*margin);
    const double scale=std::min({12.,2048/rect.width(),2048/rect.height(),std::sqrt(2000000./(rect.width()*rect.height()))});
    QImage bitmap(std::max(1,int(std::ceil(rect.width()*scale))),std::max(1,int(std::ceil(rect.height()*scale))),QImage::Format_ARGB32_Premultiplied);bitmap.fill(Qt::transparent);
    QPainter painter(&bitmap);painter.setRenderHint(QPainter::Antialiasing);painter.scale(scale,scale);painter.translate(-rect.x(),-rect.y());
    QPainterPath path;path.setFillRule(Qt::WindingFill);
    for(std::size_t i=0;i+2<mesh.size();i+=3){auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];
        if((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)<0)std::swap(b,c);
        path.moveTo(a.x,a.y);path.lineTo(b.x,b.y);path.lineTo(c.x,c.y);path.closeSubpath();}
    painter.fillPath(path,color);painter.end();
    auto* texture=window->createTextureFromImage(bitmap);if(!texture)return root;
    auto* node=new QSGSimpleTextureNode;node->setTexture(texture);node->setOwnsTexture(true);node->setFiltering(QSGTexture::Linear);node->setRect(rect);root->appendChildNode(node);return root;
}
QSGNode* objectNode(const ShapeObject& shape){
    auto* root=new QSGNode;auto color=fromRgba(shape.fillColor());color.setAlphaF(shape.fillOpacity);
    root->appendChildNode(meshNode(shapeFillMesh(shape),color));root->appendChildNode(meshNode(shapeBorderMesh(shape),fromRgba(shape.style.rgba)));return root;
}
template<class Object>
QSGNode* rasterShapeNode(const Object& shape,QQuickWindow* window,SceneRoot::RasterShape& cached,double scale,QRectF visible,const QImage& bitmap={}){
    if constexpr(std::is_same_v<Object,ImageObject>)cached.raster.update(shape,bitmap,scale,visible);else cached.raster.update(shape,scale,visible);
    const auto& tiles=cached.raster.tiles();
    std::erase_if(cached.textures,[&](const auto& entry){return !tiles.contains(entry.first);});
    auto* root=new QSGNode;
    for(const auto& [key,tile]:tiles){
        auto& texture=cached.textures[key];
        if(texture.revision!=tile.revision){texture.texture.reset(window->createTextureFromImage(tile.image));texture.revision=tile.revision;}
        if(!texture.texture)continue;
        auto* node=new QSGSimpleTextureNode;node->setTexture(texture.texture.get());node->setFiltering(QSGTexture::Linear);
        node->setSourceRect(QRectF(1,1,ShapeRasterCache::tilePixels,ShapeRasterCache::tilePixels));node->setRect(tile.worldRect);root->appendChildNode(node);
    }
    return root;
}
template<class Object> QSGNode* imageNode(const Object& image,QQuickWindow* window,const QImage& bitmap){
    auto* root=new QSGTransformNode;if(image.corners.size()!=4||!window||bitmap.isNull())return root;
    const auto origin=image.corners[0],edge=image.corners[1]-origin;
    QMatrix4x4 matrix;matrix.translate(float(origin.x),float(origin.y));matrix.rotate(float(std::atan2(edge.y,edge.x)*180/std::numbers::pi),0,0,1);root->setMatrix(matrix);
    auto* node=new QSGSimpleTextureNode;auto* texture=window->createTextureFromImage(bitmap);if(!texture)return root;
    node->setTexture(texture);node->setOwnsTexture(true);node->setFiltering(QSGTexture::Linear);node->setRect(0,0,length(edge),length(image.corners[3]-origin));root->appendChildNode(node);return root;
}


}
QSGNode* CanvasItem::updatePaintNode(QSGNode* old,UpdatePaintNodeData*){
    auto* root=static_cast<SceneRoot*>(old);if(!root)root=new SceneRoot;
    QMatrix4x4 matrix;matrix.translate(float(view_.pan.x),float(view_.pan.y));matrix.scale(float(view_.pixelsPerMm*view_.zoom));root->setMatrix(matrix);
    const Page* page=controller_?controller_->page():nullptr;
    const auto a=view_.screenToWorld({0,0}),b=view_.screenToWorld({width(),height()});
    const QRectF viewport(a.x,a.y,b.x-a.x,b.y-a.y);
    const auto paper=page&&page->size.infinite?viewport.adjusted(-viewport.width()*.5,-viewport.height()*.5,viewport.width()*.5,viewport.height()*.5):QRectF(0,0,page?page->size.widthMm:0,page?page->size.heightMm:0);
    root->paper->setRect(paper);root->paper->setColor(page?fromRgba(page->background):QColor(Qt::transparent));
    root->clip->setClipRect(paper);
    const QRectF visible=viewport.intersected(paper);
    const double scale=view_.pixelsPerMm*view_.zoom;
    const bool navigating=viewRefinementTimer_.isActive();
    // Resolution bands avoid rebuilding masks for every 5% wheel step. While
    // navigating, existing textures are transformed; refinement runs at rest.
    const double physicalScale=scale*(window()?window()->devicePixelRatio():1.);
    const double targetRasterScale=std::pow(2.,std::ceil(std::log2(physicalScale)*2)/2);
    const auto bufferedVisible=visible.adjusted(-visible.width()*.25,-visible.height()*.25,visible.width()*.25,visible.height()*.25).intersected(root->paper->rect());
    const double gridScale=navigating&&root->grid?root->gridScale:targetRasterScale;
    const auto gridBounds=root->grid&&root->gridScale==gridScale&&root->gridBounds.contains(visible)?root->gridBounds:bufferedVisible;
    if(!page||root->pdfPageId!=page->id||root->pdfRevision!=(controller_?controller_->pdfImageRevision():0)){
        if(root->pdf){root->clip->removeChildNode(root->pdf);delete root->pdf;root->pdf=nullptr;}
        if(page&&page->pdf){const auto size=page->pdf->size.valid()?page->pdf->size:page->size;ImageObject base;base.corners={{0,0},{size.widthMm,0},{size.widthMm,size.heightMm},{0,size.heightMm}};
            root->pdf=imageNode(base,window(),controller_->pdfImage());root->clip->prependChildNode(root->pdf);}
        root->pdfPageId=page?page->id:"";root->pdfRevision=controller_?controller_->pdfImageRevision():0;
    }
    if(!page||!root->grid||root->gridStyle!=page->backgroundStyle||root->gridBounds!=gridBounds||root->gridScale!=gridScale){
        if(root->grid){root->clip->removeChildNode(root->grid);delete root->grid;root->grid=nullptr;}
        if(page&&page->backgroundStyle.gridType!=GridType::None){auto color=fromRgba(page->backgroundStyle.gridColor);color.setAlphaF(color.alphaF()*page->backgroundStyle.opacity);
            root->grid=meshNode(backgroundMesh(page->backgroundStyle,{gridBounds.left(),gridBounds.top(),gridBounds.right(),gridBounds.bottom()},4/gridScale),color);
            if(root->pdf)root->clip->insertChildNodeAfter(root->grid,root->pdf);else root->clip->prependChildNode(root->grid);root->gridStyle=page->backgroundStyle;root->gridBounds=gridBounds;root->gridScale=gridScale;}
    }

    QSet<QString> present,rasterPresent;
    if(page){
        // Cache completed meshes; changing a selected object invalidates only its node.
        auto all=objectViews(*page);
        std::erase_if(all,[&](const auto& object){const auto id=std::visit([](const auto* s){return s->id;},object);
            return (state_==State::Erasing&&std::any_of(eraseBefore_.begin(),eraseBefore_.end(),[&](const auto& s){return s.id==id;}))
                ||(state_==State::Erasing&&std::any_of(eraseShapesBefore_.begin(),eraseShapesBefore_.end(),[&](const auto& s){return s.id==id;}))
                ||(state_==State::Erasing&&std::any_of(eraseFillsBefore_.begin(),eraseFillsBefore_.end(),[&](const auto& s){return s.id==id;}))
                ||(!editPreview_.empty()&&selection_.contains(id));});
        for(const auto& stroke:erasePreview_)all.emplace_back(&stroke);
        for(const auto& shape:eraseShapesPreview_)all.emplace_back(&shape);
        for(const auto& fill:eraseFillsPreview_)all.emplace_back(&fill);
        for(const auto& object:editPreview_)std::visit([&](const auto& s){all.emplace_back(&s);},object);
        std::stable_sort(all.begin(),all.end(),[](const auto& a,const auto& b){
            return std::visit([](const auto* s){return s->properties.zIndex;},a)<std::visit([](const auto* s){return s->properties.zIndex;},b);});
        QSGNode* previous=root->grid?root->grid:root->pdf;
        for(const auto& object:all){const auto& props=std::visit([](const auto* s)->const ObjectProperties&{return s->properties;},object);const auto id=std::visit([](const auto* s){return s->id;},object);
            if(!props.visible||QString::fromStdString(id)==editingTextId_)continue;
            const bool editing=!editPreview_.empty()&&selection_.contains(id);
            const bool raster=window()&&std::visit([](const auto* s){if constexpr(std::is_same_v<std::decay_t<decltype(*s)>,ShapeObject>||std::is_same_v<std::decay_t<decltype(*s)>,StrokeObject>||std::is_same_v<std::decay_t<decltype(*s)>,ImageObject>)return !s->erasedRegions.empty();else return false;},object);
            if(raster)rasterPresent.insert(QString::fromStdString(id));
            present.insert(QString::fromStdString(id));auto it=root->strokes.find(id);
            const double rasterScale=navigating&&it!=root->strokes.end()&&it->second.rasterScale>0?std::min(it->second.rasterScale,targetRasterScale*2):targetRasterScale;
            const auto rasterBounds=it!=root->strokes.end()&&it->second.rasterScale==rasterScale&&it->second.rasterBounds.contains(visible)?it->second.rasterBounds:bufferedVisible;
            if(it!=root->strokes.end()&&(it->second.revision!=props.revision||it->second.kind!=object.index()||it->second.editing||editing||(raster&&(it->second.rasterScale!=rasterScale||it->second.rasterBounds!=rasterBounds)))){root->clip->removeChildNode(it->second.node);delete it->second.node;root->strokes.erase(it);it=root->strokes.end();}
            if(it==root->strokes.end()){auto* node=std::visit([&](const auto* s)->QSGNode*{
                using T=std::decay_t<decltype(*s)>;
                if constexpr(std::is_same_v<T,TextObject>){
                    auto* node=imageNode(*s,window(),controller_->image(s->id));
                    const auto vertices=controller_->mathGeometry(s->id);if(!vertices.empty()&&s->corners.size()==4){
                        const auto origin=s->corners[0];const auto edge=s->corners[1]-origin;
                        // Meshes are in local mm; resizing is a geometry transform.
                        auto* transform=new QSGTransformNode;QMatrix4x4 m;
                        const auto natural=controller_->textSize(s->id);m.scale(float(length(edge)/natural.width()),float(length(s->corners[3]-origin)/natural.height()));transform->setMatrix(m);
                        transform->appendChildNode(meshNode(vertices,fromRgba(s->style.rgba)));node->appendChildNode(transform);
                    }return node;
                }else if constexpr(std::is_same_v<T,ImageObject>){if(raster)return rasterShapeNode(*s,window(),root->rasterShapes[s->id],rasterScale,rasterBounds,controller_->image(s->id));return imageNode(*s,window(),controller_->image(s->id));}else if constexpr(std::is_same_v<T,StrokeObject>){if(raster)return rasterShapeNode(*s,window(),root->rasterShapes[s->id],rasterScale,rasterBounds);return objectNode(*s,window());}else {
                    if(raster)return rasterShapeNode(*s,window(),root->rasterShapes[s->id],rasterScale,rasterBounds);
                    return objectNode(*s);
                }
},object);it=root->strokes.emplace(id,SceneRoot::Entry{node,props.revision,object.index(),editing}).first;root->clip->appendChildNode(node);it->second.rasterScale=rasterScale;it->second.rasterBounds=rasterBounds;}
            // A view transform does not change stacking. Detaching every object
            // here invalidates the renderer's batches on every wheel event.
            // Touch the child list only when an edit really changes its order.
            auto* node=it->second.node;
            if(node->previousSibling()!=previous){
                root->clip->removeChildNode(node);
                if(previous)root->clip->insertChildNodeAfter(node,previous);
                else root->clip->prependChildNode(node);
            }
            previous=node;
        }
    }
    for(auto it=root->strokes.begin();it!=root->strokes.end();){if(!present.contains(QString::fromStdString(it->first))){root->clip->removeChildNode(it->second.node);delete it->second.node;it=root->strokes.erase(it);}else ++it;}
    std::erase_if(root->rasterShapes,[&](const auto& entry){return !rasterPresent.contains(QString::fromStdString(entry.first));});
    if(root->live){root->clip->removeChildNode(root->live);delete root->live;root->live=nullptr;}
    if(drawing()){
        root->live=new QSGNode;
        if(previewShape_)root->live->appendChildNode(objectNode(*previewShape_));else root->live->appendChildNode(objectNode(current_,window()));
        root->clip->appendChildNode(root->live);
    }
    return root;
}
}
