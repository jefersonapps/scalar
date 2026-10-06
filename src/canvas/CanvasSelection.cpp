#include "CanvasItem.h"
#include <QtConcurrent>
namespace scalar {
std::vector<CanvasObject> CanvasItem::selectedObjects() const{
    if(!editPreview_.empty())return editPreview_;
    std::vector<CanvasObject> result;if(controller_&&controller_->page())for(const auto& id:selection_.ids())if(auto o=findObject(*controller_->page(),id))result.push_back(*o);return result;
}
Bounds CanvasItem::selectedBounds() const{
    const auto selected=selectedObjects();if(selected.empty())return {};
    auto b=bounds(selected[0]);for(const auto& o:selected){const auto r=bounds(o);b.left=std::min(b.left,r.left);b.right=std::max(b.right,r.right);b.top=std::min(b.top,r.top);b.bottom=std::max(b.bottom,r.bottom);}return b;
}
QRectF CanvasItem::selectionRect() const{
    Bounds b;if(state_==State::Marquee||state_==State::CreatingText)b={std::min(dragStart_.x,lastPan_.x),std::min(dragStart_.y,lastPan_.y),std::max(dragStart_.x,lastPan_.x),std::max(dragStart_.y,lastPan_.y)};else if(selectedCount())b=selectedBounds();else return {};
    const auto a=view_.worldToScreen({b.left,b.top}),c=view_.worldToScreen({b.right,b.bottom});return {a.x,a.y,c.x-a.x,c.y-a.y};
}
QVariantList CanvasItem::selectionHandles() const{
    QVariantList result;if(!selectedCount()||state_==State::Marquee)return result;
    const auto add=[&](Point p,const QString& type,int index){const auto screen=view_.worldToScreen(p);result.append(QVariantMap{{"x",screen.x},{"y",screen.y},{"type",type},{"index",index}});};
    const auto selected=selectedObjects();const auto b=selectedBounds();
    if(selected.size()==1)if(const auto* s=std::get_if<ShapeObject>(&selected[0])){
        if(s->kind==ShapeKind::Line||s->kind==ShapeKind::Triangle||s->kind==ShapeKind::Polygon){for(std::size_t i=0;i<s->vertices.size();++i)add(s->vertices[i],"vertex",int(i));}
        if(s->kind==ShapeKind::Circle||s->kind==ShapeKind::Ellipse){add(s->center,"center",0);add(rotatePoint(s->center+Point{s->radiusX,0},s->center,s->rotation),"radiusX",0);if(s->kind==ShapeKind::Ellipse)add(rotatePoint(s->center+Point{0,s->radiusY},s->center,s->rotation),"radiusY",0);}
    }
    add({b.right,b.bottom},"resize",0);add({b.center().x,b.top-28/(view_.zoom*view_.pixelsPerMm)},"rotate",0);return result;
}
QString CanvasItem::selectionName() const{
    const auto selected=selectedObjects();if(selected.size()!=1)return QString::number(selected.size())+" objetos";if(const auto* s=std::get_if<ShapeObject>(&selected[0]))return QString::fromStdString(shapeName(s->kind));if(std::holds_alternative<TextObject>(selected[0]))return "Texto";return std::holds_alternative<ImageObject>(selected[0])?"Imagem":"Traço";
}
double CanvasItem::selectedWidth() const{
    const auto selected=selectedObjects();if(selected.empty())return 0.85;
    return std::visit([](const auto& s){if constexpr(std::is_same_v<std::decay_t<decltype(s)>,ImageObject>)return 0.85;else return s.style.maxWidthMm;},selected.front());
}
double CanvasItem::selectedFill() const{const auto selected=selectedObjects();if(selected.size()==1)if(const auto* s=std::get_if<ShapeObject>(&selected[0]))return s->fillOpacity;return 0;}
int CanvasItem::selectedPattern() const{
    int result=-1;for(const auto& object:selectedObjects()){
        const int pattern=std::visit([](const auto& s){if constexpr(std::is_same_v<std::decay_t<decltype(s)>,StrokeObject>||std::is_same_v<std::decay_t<decltype(s)>,ShapeObject>)return int(s.style.pattern);else return -1;},object);
        if(pattern<0||(result>=0&&result!=pattern))return -1;result=pattern;
    }return result;
}
void CanvasItem::setSelectedPattern(int pattern){
    if(!controller_||pattern<0||pattern>2)return;
    std::vector<ObjectChange> changes;for(auto object:selectedObjects()){
        auto before=object;bool changed=false;
        std::visit([&](auto& s){if constexpr(std::is_same_v<std::decay_t<decltype(s)>,StrokeObject>||std::is_same_v<std::decay_t<decltype(s)>,ShapeObject>){if(int(s.style.pattern)!=pattern){s.style.pattern=LinePattern(pattern);++s.properties.revision;changed=true;}}},object);
        if(changed)changes.push_back({before,object});
    }controller_->changeObjects(std::move(changes),CommandKind::ChangeStyle);selectionUpdated();
}
QColor CanvasItem::selectedFillColor() const{
    const auto selected=selectedObjects();if(selected.size()==1)if(const auto* s=std::get_if<ShapeObject>(&selected[0]))return QColor::fromRgba((s->fillColor()>>8)|0xff000000);
    return {};
}
QColor CanvasItem::selectedBorderColor() const{
    const auto selected=selectedObjects();if(selected.size()==1)return std::visit([](const auto& s)->QColor{
        if constexpr(std::is_same_v<std::decay_t<decltype(s)>,ImageObject>)return {};
        else return QColor::fromRgba((s.style.rgba>>8)|0xff000000);
    },selected.front());return {};
}
QString CanvasItem::interactionHint() const{
    if(state_==State::DrawingCompass)return compassComplete_?"Circunferência completa · solte para confirmar":"Compasso · gire a ponta para desenhar um arco";
    if(drawing()&&guidedEdge_)return "Snap da régua · traço alinhado";
    if(state_==State::ShapePreview&&previewShape_)return QString::fromStdString(shapeName(previewShape_->kind))+" reconhecido"+(previewShape_->style.pattern==LinePattern::Dashed?" · tracejado":" · Shift para tracejado")+" · solte para confirmar";
    if(state_==State::CreatingShape)return "Arraste para definir a forma";
    return drawing()?"Escrevendo · segure para reconhecer uma forma":"";
}
void CanvasItem::selectionUpdated(){emit selectionChanged();update();}
void CanvasItem::beginSelection(Point p,Qt::KeyboardModifiers modifiers){
    if(!controller_||!controller_->page())return;
    dragStart_=p;lastPan_=p;const double tolerance=12/(view_.pixelsPerMm*view_.zoom);handleIndex_=-1;
    const auto handles=selectionHandles();for(int i=0;i<handles.size();++i){const auto h=handles[i].toMap();const auto screen=view_.worldToScreen(p);
        if(length(screen-Point{h["x"].toDouble(),h["y"].toDouble()})<12){handleIndex_=i;const auto type=h["type"].toString();state_=type=="resize"?State::Resizing:type=="rotate"?State::Rotating:State::EditingHandle;break;}}
    if(handleIndex_<0){std::optional<CanvasObject> hit;const auto all=objects(*controller_->page());for(auto i=all.rbegin();i!=all.rend();++i)if(hitTest(*i,p,tolerance)){hit=*i;break;}
        if(hit){const auto id=objectId(*hit);if(modifiers&Qt::ShiftModifier){selection_.select(id,true);state_=State::Idle;selectionUpdated();return;}if(!selection_.contains(id))selection_.select(id);state_=State::Moving;}
        else {marqueeAdditive_=bool(modifiers&Qt::ShiftModifier);if(!marqueeAdditive_)selection_.clear();state_=State::Marquee;lastPan_=p;selectionUpdated();return;}}
    editBefore_=selectedObjects();editPreview_=editBefore_;editBounds_=selectedBounds();selectionUpdated();
}
void CanvasItem::updateSelection(Point p){
    if(state_==State::Marquee){lastPan_=p;selectionUpdated();return;}
    lastPan_=p;editPreview_=editBefore_;const auto center=editBounds_.center();
    for(auto& o:editPreview_){
        if(state_==State::Moving)o=transformed(o,center,p-dragStart_);
        else if(state_==State::Resizing){const auto sx=std::clamp((p.x-editBounds_.left)/std::max(0.1,editBounds_.width()),0.05,20.),sy=std::clamp((p.y-editBounds_.top)/std::max(0.1,editBounds_.height()),0.05,20.);const auto scale=std::min(sx,sy);o=transformed(o,{editBounds_.left,editBounds_.top},{},scale,scale);}
        else if(state_==State::Rotating){const auto a=dragStart_-center,b=p-center;o=transformed(o,center,{},1,1,std::atan2(b.y,b.x)-std::atan2(a.y,a.x));}
        else if(state_==State::EditingHandle){auto* shape=std::get_if<ShapeObject>(&o);if(!shape)continue;
            const auto handles=selectionHandles();if(handleIndex_<0||handleIndex_>=handles.size())continue;const auto handle=handles[handleIndex_].toMap();const auto type=handle["type"].toString();
            if(type=="vertex")shape->vertices[handle["index"].toInt()]=p;
            else if(type=="center")shape->center=p;
            else {const auto local=rotatePoint(p,shape->center,-shape->rotation)-shape->center;if(type=="radiusX")shape->radiusX=std::clamp(std::abs(local.x),0.2,5000.);else shape->radiusY=std::clamp(std::abs(local.y),0.2,5000.);if(shape->kind==ShapeKind::Circle)shape->radiusY=shape->radiusX;}
            ++shape->properties.revision;
        }
    }selectionUpdated();
}
void CanvasItem::commitSelection(){
    std::vector<ObjectChange> changes;
    if(length(lastPan_-dragStart_)>0.01){for(std::size_t i=0;i<editBefore_.size();++i)changes.push_back({editBefore_[i],editPreview_[i]});}
    state_=State::Idle;editBefore_.clear();editPreview_.clear();if(controller_)controller_->changeObjects(std::move(changes),CommandKind::TransformObject);selectionUpdated();
}
QString CanvasItem::selectedTextId() const {const auto selected=selectedObjects();if(selected.size()==1&&std::holds_alternative<TextObject>(selected[0]))return QString::fromStdString(objectId(selected[0]));return {};}
void CanvasItem::editSelectedText(){const auto id=selectedTextId();if(!id.isEmpty())emit textRequested({},id);}
void CanvasItem::deleteSelection(){
    if(selectedGuide_=="compass"){setCompassVisible(false);selectedGuide_.clear();emit geometryToolsChanged();return;}
    if(selectedGuide_=="ruler"){setRulerVisible(false);selectedGuide_.clear();emit geometryToolsChanged();return;}
    if(!controller_)return;std::vector<ObjectChange> changes;for(auto o:selectedObjects())changes.push_back({o,{}});selection_.clear();controller_->changeObjects(std::move(changes),CommandKind::DeleteObject);selectionUpdated();
}
void CanvasItem::duplicateSelection(){
    if(!controller_)return;std::vector<ObjectChange> changes;auto selected=selectedObjects();selection_.clear();auto z=controller_->nextZIndex();
    for(auto o:selected){o=transformed(o,{},{5,5});const auto id=newId();if(std::holds_alternative<ImageObject>(o)||std::holds_alternative<TextObject>(o))controller_->aliasImage(objectId(o),id);std::visit([&](auto& s){s.id=id;s.properties.zIndex=z++;},o);changes.push_back({{},o});selection_.select(id,true);}
    controller_->changeObjects(std::move(changes),CommandKind::AddObject);selectionUpdated();
}
void CanvasItem::moveSelectionLayer(bool forward){
    if(!controller_||!controller_->page()||state_!=State::Idle||!selectedCount())return;
    const auto before=objects(*controller_->page());auto ordered=before;bool moved=false;
    // Move each selected block by one neighbor, preserving its internal order.
    if(forward){
        for(std::size_t i=ordered.size();i>1;--i)
            if(selection_.contains(objectId(ordered[i-2]))&&!selection_.contains(objectId(ordered[i-1]))){std::swap(ordered[i-2],ordered[i-1]);moved=true;}
    }else{
        for(std::size_t i=1;i<ordered.size();++i)
            if(selection_.contains(objectId(ordered[i]))&&!selection_.contains(objectId(ordered[i-1]))){std::swap(ordered[i],ordered[i-1]);moved=true;}
    }
    if(!moved)return;
    std::vector<ObjectChange> changes;
    for(std::size_t i=0;i<ordered.size();++i){
        auto& object=ordered[i];auto& p=properties(object);
        if(p.zIndex==static_cast<int>(i))continue;
        const auto original=findObject(*controller_->page(),objectId(object));
        p.zIndex=static_cast<int>(i);++p.revision;changes.push_back({original,object});
    }
    controller_->changeObjects(std::move(changes),CommandKind::ChangeLayer);selectionUpdated();
}
void CanvasItem::recognizeSelection(){
    const auto selected=selectedObjects();if(!controller_||selected.size()!=1||recognitionWatcher_.isRunning())return;
    if(const auto* s=std::get_if<StrokeObject>(&selected[0])){
        current_=*s;requestEpoch_=++inputEpoch_;state_=State::Idle;
        const auto snapshot=*s;const auto page=controller_->page()?*controller_->page():Page{};
        recognitionWatcher_.setFuture(QtConcurrent::run([snapshot,page]{return recognizeShape(snapshot,page);}));
    }
}
void CanvasItem::setSelectedFill(double opacity){
    if(!controller_)return;std::vector<ObjectChange> changes;for(auto o:selectedObjects()){auto before=o;if(auto* s=std::get_if<ShapeObject>(&o)){s->fillOpacity=std::clamp(opacity,0.,1.);++s->properties.revision;changes.push_back({before,o});}}
    controller_->changeObjects(std::move(changes),CommandKind::ChangeStyle);selectionUpdated();
}
void CanvasItem::setSelectedFillColor(const QColor& color){
    if(!controller_||!color.isValid())return;std::vector<ObjectChange> changes;
    for(auto object:selectedObjects())if(auto* shape=std::get_if<ShapeObject>(&object);shape&&shape->kind!=ShapeKind::Line){
        auto before=object;shape->fillRgba=(std::uint32_t(color.red())<<24)|(std::uint32_t(color.green())<<16)|(std::uint32_t(color.blue())<<8)|255;
        ++shape->properties.revision;changes.push_back({before,object});
    }
    controller_->changeObjects(std::move(changes),CommandKind::ChangeStyle);selectionUpdated();
}
void CanvasItem::setSelectedColor(const QColor& color){
    if(!controller_||!color.isValid())return;std::vector<ObjectChange> changes;for(auto o:selectedObjects()){if(std::holds_alternative<ImageObject>(o))continue;auto before=o;std::visit([&](auto& s){if constexpr(!std::is_same_v<std::decay_t<decltype(s)>,ImageObject>){s.style.rgba=(std::uint32_t(color.red())<<24)|(std::uint32_t(color.green())<<16)|(std::uint32_t(color.blue())<<8)|255;++s.properties.revision;}},o);changes.push_back({before,o});}
    controller_->changeObjects(std::move(changes),CommandKind::ChangeStyle);selectionUpdated();
}
void CanvasItem::setSelectedWidth(double width){
    if(!controller_)return;std::vector<ObjectChange> changes;for(auto o:selectedObjects()){if(std::holds_alternative<ImageObject>(o))continue;auto before=o;std::visit([&](auto& s){if constexpr(!std::is_same_v<std::decay_t<decltype(s)>,ImageObject>){s.style.maxWidthMm=std::clamp(width,0.2,5.);s.style.minWidthMm=std::min(s.style.minWidthMm,s.style.maxWidthMm);++s.properties.revision;}},o);changes.push_back({before,o});}
    controller_->changeObjects(std::move(changes),CommandKind::ChangeStyle);selectionUpdated();
}
}
