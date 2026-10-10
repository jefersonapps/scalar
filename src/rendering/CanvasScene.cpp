#include "canvas/CanvasItem.h"
#include "StrokeMesh.h"
#include "ShapeMesh.h"
#include "ShapeRasterCache.h"
#include "BackgroundMesh.h"
#include "geometry/Geometry.h"
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>
#include <QSGTransformNode>
#include <QSGClipNode>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include <QSGOpacityNode>
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
    struct Entry {QSGNode* node;std::uint64_t revision;std::size_t kind;bool editing=false;double rasterScale=0;QRectF rasterBounds;QRectF objectBounds;};
    std::unordered_map<std::string,Entry> strokes;
    struct RasterShape {
        ShapeRasterCache raster;
        struct Texture {std::unique_ptr<QSGTexture> texture;std::uint64_t revision=0;};
        std::map<ShapeRasterCache::Key,Texture> textures;
        QFuture<ShapeRasterCache> pending;
        std::uint64_t pendingRevision=0;
        double pendingScale=0;
        QRectF pendingBounds;
        bool hasPending=false,prepared=false;
    };
    std::unordered_map<std::string,RasterShape> rasterShapes;
    QSGNode* live=nullptr;
    struct LiveMarker {
        struct Tile {QImage image;std::unique_ptr<QSGTexture> texture;QSGSimpleTextureNode* node=nullptr;};
        std::map<std::pair<int,int>,Tile> tiles;
        std::string id;
        std::size_t samples=0;
        double scale=0;
    } liveMarker;
    QSGNode* grid=nullptr;
    QSGNode* pdf=nullptr;
    quint64 pdfRevision=0;
    std::string pdfPageId;
    BackgroundStyle gridStyle;
    QRectF gridBounds;
    double gridScale=0;
    SceneRoot(){appendChildNode(paper);appendChildNode(clip);clip->setIsRectangular(true);}
};
class InkGeometryNode : public QSGGeometryNode {
public:
    QRectF bounds;
    bool blocked=false;
    bool isSubtreeBlocked() const override{return blocked;}
    void setVisibleIn(QRectF visible){const bool next=!bounds.intersects(visible);if(next!=blocked){blocked=next;markDirty(DirtySubtreeBlocked);}}
};
template<class Node=QSGGeometryNode> Node* meshNode(std::span<const Point> vertices,QColor color) {
    auto* node=new Node;
    auto* geometry=new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),int(vertices.size()));
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* data=geometry->vertexDataAsPoint2D();for(std::size_t i=0;i<vertices.size();++i)data[i].set(float(vertices[i].x),float(vertices[i].y));
    node->setGeometry(geometry);node->setFlag(QSGNode::OwnsGeometry);
    auto* material=new QSGFlatColorMaterial;material->setColor(color);node->setMaterial(material);node->setFlag(QSGNode::OwnsMaterial);return node;
}
void appendMeshNodes(QSGNode& root,std::span<const Point> vertices,QColor color){
    // Qt Quick may generate 16-bit indices when batching even non-indexed
    // geometry. Split on triangle boundaries so no node can wrap those indices.
    constexpr std::size_t maxVertices=60000;
    for(std::size_t offset=0;offset<vertices.size();offset+=maxVertices){
        const auto chunk=vertices.subspan(offset,std::min(maxVertices,vertices.size()-offset));
        auto* node=meshNode<InkGeometryNode>(chunk,color);
        double left=chunk.front().x,right=left,top=chunk.front().y,bottom=top;for(auto p:chunk){left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}node->bounds=QRectF(QPointF(left,top),QPointF(right,bottom));root.appendChildNode(node);
    }
}
QSGNode* objectNode(const StrokeObject& stroke,QQuickWindow* window,double rasterScale=12.){
    auto* root=new QSGNode;const auto color=fromRgba(stroke.style.rgba);
    if(color.alpha()==255||!window){appendMeshNodes(*root,strokeDisplayMesh(stroke),color);return root;}
    // Flatten translucent ink once so overlapping triangles do not darken a stroke.
    const auto b=bounds(CanvasObject(stroke));const double margin=stroke.style.maxWidthMm/2+.2;
    const QRectF rect(b.left-margin,b.top-margin,b.width()+2*margin,b.height()+2*margin);
    const double scale=std::min({rasterScale,2048/rect.width(),2048/rect.height(),std::sqrt(2000000./(rect.width()*rect.height()))});
    QImage bitmap(std::max(1,int(std::ceil(rect.width()*scale))),std::max(1,int(std::ceil(rect.height()*scale))),QImage::Format_ARGB32_Premultiplied);bitmap.fill(Qt::transparent);
    QPainter painter(&bitmap);painter.setRenderHint(QPainter::Antialiasing);painter.scale(scale,scale);painter.translate(-rect.x(),-rect.y());
    if(stroke.marker&&stroke.style.pattern==LinePattern::Solid&&!stroke.samples.empty()){
        // Paint opaque coverage first, then apply opacity once. A centerline
        // avoids an expensive union of thousands of overlapping triangles.
        auto opaque=color;opaque.setAlpha(255);
        painter.setPen(QPen(opaque,stroke.style.maxWidthMm,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        QPainterPath centerline;const auto first=stroke.samples.front().position;centerline.moveTo(first.x,first.y);
        for(std::size_t i=1;i<stroke.samples.size();++i){const auto p=stroke.samples[i].position;centerline.lineTo(p.x,p.y);}
        if(stroke.samples.size()==1)painter.drawPoint(QPointF(first.x,first.y));else painter.drawPath(centerline);
        painter.resetTransform();painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);painter.fillRect(bitmap.rect(),QColor(0,0,0,color.alpha()));
    }else{
    const auto mesh=strokeMesh(stroke);QPainterPath path;path.setFillRule(Qt::WindingFill);
    for(std::size_t i=0;i+2<mesh.size();i+=3){auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];
        if((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)<0)std::swap(b,c);
        path.moveTo(a.x,a.y);path.lineTo(b.x,b.y);path.lineTo(c.x,c.y);path.closeSubpath();}
    painter.fillPath(path,color);
    }
    painter.end();
    auto* texture=window->createTextureFromImage(bitmap);if(!texture)return root;
    auto* node=new QSGSimpleTextureNode;node->setTexture(texture);node->setOwnsTexture(true);node->setFiltering(QSGTexture::Linear);node->setRect(rect);root->appendChildNode(node);return root;
}
void appendLiveMarker(SceneRoot& root,const StrokeObject& stroke,QQuickWindow* window,double scale){
    constexpr int pixels=256;
    auto& cache=root.liveMarker;
    if(cache.id!=stroke.id||cache.scale!=scale||cache.samples>stroke.samples.size()){
        if(root.live){root.clip->removeChildNode(root.live);delete root.live;root.live=nullptr;}
        cache={};cache.id=stroke.id;cache.scale=scale;
        auto* opacity=new QSGOpacityNode;opacity->setOpacity(fromRgba(stroke.style.rgba).alphaF());root.live=opacity;root.clip->appendChildNode(root.live);
    }
    const double side=pixels/scale,margin=stroke.style.maxWidthMm/2+2/scale;
    auto color=fromRgba(stroke.style.rgba);color.setAlpha(255);
    std::map<std::pair<int,int>,QPainterPath> paths;
    for(std::size_t i=cache.samples;i<stroke.samples.size();++i){
        const auto a=stroke.samples[i?i-1:0].position,b=stroke.samples[i].position;
        const int left=int(std::floor((std::min(a.x,b.x)-margin)/side)),right=int(std::floor((std::max(a.x,b.x)+margin)/side));
        const int top=int(std::floor((std::min(a.y,b.y)-margin)/side)),bottom=int(std::floor((std::max(a.y,b.y)+margin)/side));
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){auto& path=paths[{x,y}];path.moveTo(a.x,a.y);path.lineTo(b.x,b.y);}
    }
    for(const auto& [key,path]:paths){
        auto& tile=cache.tiles[key];const QRectF rect(key.first*side,key.second*side,side,side);
        if(tile.image.isNull()){
            tile.image=QImage(pixels+2,pixels+2,QImage::Format_ARGB32_Premultiplied);tile.image.fill(Qt::transparent);
            tile.node=new QSGSimpleTextureNode;tile.node->setFiltering(QSGTexture::Linear);tile.node->setRect(rect);tile.node->setSourceRect(QRectF(1,1,pixels,pixels));
        }
        QPainter painter(&tile.image);painter.setRenderHint(QPainter::Antialiasing);painter.translate(1,1);painter.scale(scale,scale);painter.translate(-rect.x(),-rect.y());
        painter.setPen(QPen(color,stroke.style.maxWidthMm,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPath(path);
        if(cache.samples==0&&!stroke.samples.empty()){const auto p=stroke.samples.front().position;painter.drawPoint(QPointF(p.x,p.y));}
        painter.end();
        auto texture=std::unique_ptr<QSGTexture>(window->createTextureFromImage(tile.image));
        if(texture){tile.node->setTexture(texture.get());tile.texture=std::move(texture);if(!tile.node->parent())root.live->appendChildNode(tile.node);}
        else if(!tile.node->parent()){delete tile.node;tile.node=nullptr;tile.image={};}
    }
    cache.samples=stroke.samples.size();
}
QSGNode* objectNode(const ShapeObject& shape){
    auto* root=new QSGNode;auto color=fromRgba(shape.fillColor());color.setAlphaF(shape.fillOpacity);
    root->appendChildNode(meshNode(shapeFillMesh(shape),color));root->appendChildNode(meshNode(shapeBorderMesh(shape),fromRgba(shape.style.rgba)));return root;
}
template<class Object>
QSGNode* rasterShapeNode(const Object& shape,QQuickWindow* window,SceneRoot::RasterShape& cached,double scale,QRectF visible,const QImage& bitmap={}){
    if(!cached.prepared){if constexpr(std::is_same_v<Object,ImageObject>)cached.raster.update(shape,bitmap,scale,visible);else cached.raster.update(shape,scale,visible);}
    cached.prepared=false;
    const auto& tiles=cached.raster.tiles();
    std::erase_if(cached.textures,[&](const auto& entry){return !tiles.contains(entry.first);});
    auto* root=new QSGNode;
    for(const auto& [key,tile]:tiles){
        if(tile.inkRect.isEmpty())continue;
        auto& texture=cached.textures[key];
        if(texture.revision!=tile.revision){texture.texture.reset(window->createTextureFromImage(tile.image));texture.revision=tile.revision;}
        if(!texture.texture)continue;
        auto* node=new QSGSimpleTextureNode;node->setTexture(texture.texture.get());node->setFiltering(QSGTexture::Linear);
        const double pixels=ShapeRasterCache::tilePixels/tile.worldRect.width();const auto ink=tile.inkRect;
        node->setSourceRect(QRectF(1+(ink.left()-tile.worldRect.left())*pixels,1+(ink.top()-tile.worldRect.top())*pixels,ink.width()*pixels,ink.height()*pixels));node->setRect(ink);root->appendChildNode(node);
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
    QElapsedTimer refinementBudget;refinementBudget.start();
    bool refined=false,refinementPending=false;
    int markerJobs=0;for(const auto& [id,cache]:root->rasterShapes)if(cache.hasPending)++markerJobs;
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
            const bool raster=window()&&std::visit([](const auto* s){if constexpr(std::is_same_v<std::decay_t<decltype(*s)>,StrokeObject>)return s->marker||(!s->erasedRegions.empty()||s->eraseMask.has_value());else if constexpr(std::is_same_v<std::decay_t<decltype(*s)>,ShapeObject>||std::is_same_v<std::decay_t<decltype(*s)>,ImageObject>)return (!s->erasedRegions.empty()||s->eraseMask.has_value());else return false;},object);
            const auto* stroke=std::get_if<const StrokeObject*>(&object);
            const bool marker=raster&&stroke&&(*stroke)->marker;
            if(raster)rasterPresent.insert(QString::fromStdString(id));
            present.insert(QString::fromStdString(id));auto it=root->strokes.find(id);
            const bool unchanged=it!=root->strokes.end()&&it->second.revision==props.revision&&it->second.kind==object.index()&&!it->second.editing&&!editing;
            double rasterScale=targetRasterScale;
            if(raster&&!marker&&unchanged&&it->second.rasterScale>0){
                if(navigating)rasterScale=it->second.rasterScale;
                else if(it->second.rasterScale!=targetRasterScale){
                    // Refinement must not stall every object in the same frame.
                    // Always permit one object, then respect a small CPU budget.
                    if(refined&&refinementBudget.elapsed()>=4){rasterScale=it->second.rasterScale;refinementPending=true;}
                    else refined=true;
                }
            }
            const auto needed=unchanged?visible.intersected(it->second.objectBounds):visible;
            if(marker&&navigating&&unchanged)rasterScale=it->second.rasterScale;
            auto rasterBounds=unchanged&&it->second.rasterScale==rasterScale&&(needed.isEmpty()||it->second.rasterBounds.contains(needed))?it->second.rasterBounds:bufferedVisible;
            if(marker&&unchanged&&needed.isEmpty()){rasterScale=it->second.rasterScale;rasterBounds=it->second.rasterBounds;}
            bool acceptMarker=false;
            if(marker){auto& cache=root->rasterShapes[id];
                if(!unchanged&&cache.hasPending){cache.pending.cancel();cache.pending={};cache.hasPending=false;--markerJobs;}
                if(unchanged&&cache.hasPending&&cache.pending.isFinished()){
                    const bool matches=cache.pendingRevision==props.revision&&cache.pendingScale==rasterScale&&(needed.isEmpty()||cache.pendingBounds.contains(needed));
                    if(!matches){cache.pending={};cache.hasPending=false;--markerJobs;}
                    else if(!refined||refinementBudget.elapsed()<4){acceptMarker=true;refined=true;rasterScale=cache.pendingScale;rasterBounds=cache.pendingBounds;}
                }
                if(unchanged&&!acceptMarker&&(rasterScale!=it->second.rasterScale||rasterBounds!=it->second.rasterBounds)){
                    if(!cache.hasPending&&markerJobs<2&&state_!=State::Erasing&&!drawing()){
                        cache.pendingRevision=props.revision;cache.pendingScale=rasterScale;cache.pendingBounds=rasterBounds;cache.hasPending=true;++markerJobs;
                        // Own the snapshot; the worker never reads the document,
                        // scene nodes or graphics resources. Stale results are
                        // discarded after edits, undo, page changes or new zoom.
                        cache.pending=QtConcurrent::run([snapshot=**stroke,rasterScale,rasterBounds]{ShapeRasterCache result;result.update(snapshot,rasterScale,rasterBounds);return result;});
                    }
                    rasterScale=it->second.rasterScale;rasterBounds=it->second.rasterBounds;refinementPending=true;
                }
            }
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
                }else if constexpr(std::is_same_v<T,ImageObject>){if(raster)return rasterShapeNode(*s,window(),root->rasterShapes[s->id],rasterScale,rasterBounds,controller_->image(s->id));return imageNode(*s,window(),controller_->image(s->id));}else if constexpr(std::is_same_v<T,StrokeObject>){if(raster){auto& cache=root->rasterShapes[s->id];if(acceptMarker){cache.raster=cache.pending.takeResult();cache.textures.clear();cache.hasPending=false;cache.prepared=true;--markerJobs;}return rasterShapeNode(*s,window(),cache,rasterScale,rasterBounds);}return objectNode(*s,window());}else {
                    if(raster)return rasterShapeNode(*s,window(),root->rasterShapes[s->id],rasterScale,rasterBounds);
                    return objectNode(*s);
                }
},object);it=root->strokes.emplace(id,SceneRoot::Entry{node,props.revision,object.index(),editing}).first;root->clip->appendChildNode(node);it->second.rasterScale=rasterScale;it->second.rasterBounds=rasterBounds;
                if(raster)it->second.objectBounds=std::visit([](const auto* s){const auto area=bounds(CanvasObject(*s));double padding=0;if constexpr(requires{s->style;})padding=s->style.maxWidthMm*.5;return QRectF(QPointF(area.left-padding,area.top-padding),QPointF(area.right+padding,area.bottom+padding));},object);
            }
            // A view transform does not change stacking. Detaching every object
            // here invalidates the renderer's batches on every wheel event.
            // Touch the child list only when an edit really changes its order.
            auto* node=it->second.node;
            if(!raster)for(auto* child=node->firstChild();child;child=child->nextSibling())if(auto* ink=dynamic_cast<InkGeometryNode*>(child))ink->setVisibleIn(visible.adjusted(-1/physicalScale,-1/physicalScale,1/physicalScale,1/physicalScale));
            if(node->previousSibling()!=previous){
                root->clip->removeChildNode(node);
                if(previous)root->clip->insertChildNodeAfter(node,previous);
                else root->clip->prependChildNode(node);
            }
            previous=node;
        }
    }
    if(refinementPending)QMetaObject::invokeMethod(this,[this]{QTimer::singleShot(16,this,[this]{update();});},Qt::QueuedConnection);
    for(auto it=root->strokes.begin();it!=root->strokes.end();){if(!present.contains(QString::fromStdString(it->first))){root->clip->removeChildNode(it->second.node);delete it->second.node;it=root->strokes.erase(it);}else ++it;}
    std::erase_if(root->rasterShapes,[&](const auto& entry){return !rasterPresent.contains(QString::fromStdString(entry.first));});
    const bool liveMarker=drawing()&&!previewShape_&&current_.marker&&window();
    if(liveMarker){
        appendLiveMarker(*root,current_,window(),std::clamp(physicalScale,1.,12.));
    }else{
    if(root->live){root->clip->removeChildNode(root->live);delete root->live;root->live=nullptr;}
    root->liveMarker={};
    if(drawing()){
        root->live=new QSGNode;
        if(previewShape_)root->live->appendChildNode(objectNode(*previewShape_));else root->live->appendChildNode(objectNode(current_,window(),std::clamp(physicalScale,1.,12.)));
        root->clip->appendChildNode(root->live);
    }
    }
    return root;
}
}
