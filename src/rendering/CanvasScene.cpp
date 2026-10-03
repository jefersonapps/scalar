#include "canvas/CanvasItem.h"
#include "StrokeMesh.h"
#include "ShapeMesh.h"
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>
#include <QSGTransformNode>
#include <QSGClipNode>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include <QMatrix4x4>
#include <QSet>
#include <numbers>
#include <unordered_map>
namespace scalar {
namespace {
QColor fromRgba(std::uint32_t c){return QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);}
struct SceneRoot : QSGTransformNode {
    QSGSimpleRectNode* paper=new QSGSimpleRectNode;
    QSGClipNode* clip=new QSGClipNode;
    struct Entry {QSGNode* node;std::uint64_t revision;std::size_t kind;bool editing=false;};
    std::unordered_map<std::string,Entry> strokes;
    QSGNode* live=nullptr;
    SceneRoot(){appendChildNode(paper);appendChildNode(clip);clip->setIsRectangular(true);}
};
QSGGeometryNode* meshNode(const std::vector<Point>& vertices,QColor color) {
    auto* node=new QSGGeometryNode;
    auto* geometry=new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),int(vertices.size()));
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* data=geometry->vertexDataAsPoint2D();for(std::size_t i=0;i<vertices.size();++i)data[i].set(float(vertices[i].x),float(vertices[i].y));
    node->setGeometry(geometry);node->setFlag(QSGNode::OwnsGeometry);
    auto* material=new QSGFlatColorMaterial;material->setColor(color);node->setMaterial(material);node->setFlag(QSGNode::OwnsMaterial);return node;
}
QSGNode* objectNode(const StrokeObject& stroke){auto* root=new QSGNode;root->appendChildNode(meshNode(strokeMesh(stroke),fromRgba(stroke.style.rgba)));return root;}
QSGNode* objectNode(const ShapeObject& shape){
    auto* root=new QSGNode;auto color=fromRgba(shape.fillColor());color.setAlphaF(shape.fillOpacity);
    root->appendChildNode(meshNode(shapeFillMesh(shape),color));root->appendChildNode(meshNode(shapeBorderMesh(shape),fromRgba(shape.style.rgba)));return root;
}
QSGNode* imageNode(const ImageObject& image,QQuickWindow* window,const QImage& bitmap){
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
    root->paper->setRect(0,0,page?page->size.widthMm:0,page?page->size.heightMm:0);root->paper->setColor(page?fromRgba(page->background):QColor(Qt::transparent));
    root->clip->setClipRect(root->paper->rect());
    QSet<QString> present;
    if(page){
        // Cache completed meshes; changing a selected object invalidates only its node.
        auto all=objectViews(*page);
        std::erase_if(all,[&](const auto& object){const auto id=std::visit([](const auto* s){return s->id;},object);
            return (state_==State::Erasing&&std::any_of(eraseBefore_.begin(),eraseBefore_.end(),[&](const auto& s){return s.id==id;}))
                ||(!editPreview_.empty()&&selection_.contains(id));});
        for(const auto& stroke:erasePreview_)all.emplace_back(&stroke);
        for(const auto& object:editPreview_)std::visit([&](const auto& s){all.emplace_back(&s);},object);
        std::stable_sort(all.begin(),all.end(),[](const auto& a,const auto& b){
            return std::visit([](const auto* s){return s->properties.zIndex;},a)<std::visit([](const auto* s){return s->properties.zIndex;},b);});
        for(const auto& object:all){const auto& props=std::visit([](const auto* s)->const ObjectProperties&{return s->properties;},object);const auto id=std::visit([](const auto* s){return s->id;},object);
            if(!props.visible)continue;
            const bool editing=!editPreview_.empty()&&selection_.contains(id);
            present.insert(QString::fromStdString(id));auto it=root->strokes.find(id);
            if(it!=root->strokes.end()&&(it->second.revision!=props.revision||it->second.kind!=object.index()||it->second.editing||editing)){root->clip->removeChildNode(it->second.node);delete it->second.node;root->strokes.erase(it);it=root->strokes.end();}
            if(it==root->strokes.end()){auto* node=std::visit([&](const auto* s)->QSGNode*{if constexpr(std::is_same_v<std::decay_t<decltype(*s)>,ImageObject>)return imageNode(*s,window(),controller_->image(s->id));else return objectNode(*s);},object);it=root->strokes.emplace(id,SceneRoot::Entry{node,props.revision,object.index(),editing}).first;root->clip->appendChildNode(node);}
            else {root->clip->removeChildNode(it->second.node);root->clip->appendChildNode(it->second.node);}
        }
    }
    for(auto it=root->strokes.begin();it!=root->strokes.end();){if(!present.contains(QString::fromStdString(it->first))){root->clip->removeChildNode(it->second.node);delete it->second.node;it=root->strokes.erase(it);}else ++it;}
    if(root->live){root->clip->removeChildNode(root->live);delete root->live;root->live=nullptr;}
    if(drawing()){
        root->live=new QSGNode;
        if(previewShape_)root->live->appendChildNode(objectNode(*previewShape_));else root->live->appendChildNode(objectNode(current_));
        root->clip->appendChildNode(root->live);
    }
    return root;
}
}
