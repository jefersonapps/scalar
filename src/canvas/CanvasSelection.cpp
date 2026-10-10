#include "CanvasItem.h"
#include <QtConcurrent>
namespace scalar {
bool CanvasItem::hasVisibleInk(const CanvasObject& object,std::optional<Point> near) const{
    if(!properties(object).visible)return false;
    if(const auto* text=std::get_if<TextObject>(&object))return (text->style.rgba&255)!=0;
    if(const auto* stroke=std::get_if<StrokeObject>(&object)){
        if(stroke->samples.empty()||(stroke->style.rgba&255)==0)return false;
        // Split pen fragments already contain only surviving samples. Keep
        // ordinary solid ink selection independent of its mesh size.
        if(stroke->erasedRegions.empty()&&!stroke->eraseMask&&stroke->style.pattern==LinePattern::Solid)return true;
    }
    const auto box=bounds(object);const double tolerance=12/(view_.pixelsPerMm*view_.zoom);
    QRectF area(box.left,box.top,box.width(),box.height());area=area.adjusted(-1,-1,1,1);
    double scale=std::clamp(256/std::max({area.width(),area.height(),1.}),.25,4.);
    ShapeRasterCache local;ShapeRasterCache* cache=&local;
    if(near){area=QRectF(near->x-tolerance,near->y-tolerance,tolerance*2,tolerance*2);scale=std::clamp(view_.pixelsPerMm*view_.zoom,1.,64.);}
    else {if(visibilityCaches_.size()>32)visibilityCaches_.clear();cache=&visibilityCaches_[objectId(object)];
        // Bound raster work to the viewport for unusually large objects.
        const auto a=world({0,0}),b=world({width(),height()});area=area.intersected(QRectF(a.x,a.y,b.x-a.x,b.y-a.y));if(area.isEmpty())return false;}
    std::visit([&](const auto& item){using T=std::decay_t<decltype(item)>;if constexpr(std::is_same_v<T,ImageObject>)cache->update(item,controller_->image(item.id),scale,area);else if constexpr(!std::is_same_v<T,TextObject>)cache->update(item,scale,area);},object);
    for(const auto& [key,tile]:cache->tiles()){
        const auto& image=tile.image;const double pixelScale=(image.width()-2)/tile.worldRect.width();
        for(int y=1;y+1<image.height();++y){const auto* row=reinterpret_cast<const QRgb*>(image.constScanLine(y));for(int x=1;x+1<image.width();++x){
            if(!qAlpha(row[x]))continue;const Point point{tile.worldRect.left()+(x-.5)/pixelScale,tile.worldRect.top()+(y-.5)/pixelScale};
            if(!area.contains(QPointF(point.x,point.y)))continue;
            if(!near||length(point-*near)<=tolerance+.75/pixelScale)return true;
        }}
    }return false;
}
bool CanvasItem::selectedShowAngle() const{
    const auto selected=selectedObjects();if(selected.size()!=1)return false;
    const auto* shape=std::get_if<ShapeObject>(&selected.front());return shape&&shape->kind==ShapeKind::CircularSector&&shape->showAngle;
}
void CanvasItem::setSelectedShowAngle(bool visible){
    if(!controller_)return;const auto selected=selectedObjects();if(selected.size()!=1)return;
    auto after=selected.front();auto* shape=std::get_if<ShapeObject>(&after);
    if(!shape||shape->kind!=ShapeKind::CircularSector||shape->showAngle==visible)return;
    shape->showAngle=visible;++shape->properties.revision;controller_->changeObjects({{selected.front(),after}},CommandKind::ChangeStyle);selectionUpdated();
}
QVariantList CanvasItem::sectorAngles() const{
    QVariantList result;if(!controller_||!controller_->page())return result;
    for(const auto& stored:controller_->page()->shapes){
        const ShapeObject* shape=&stored;for(const auto& object:editPreview_)if(const auto* preview=std::get_if<ShapeObject>(&object);preview&&preview->id==stored.id){shape=preview;break;}
        for(const auto& preview:eraseShapesPreview_)if(preview.id==stored.id){shape=&preview;break;}
        if(shape->kind!=ShapeKind::CircularSector||!shape->showAngle||!shape->properties.visible||shape->vertices.size()<4)continue;
        if(!hasVisibleInk(CanvasObject(*shape)))continue;
        double sweep=0;for(std::size_t i=2;i<shape->vertices.size();++i){const auto a=shape->vertices[i-1]-shape->center,b=shape->vertices[i]-shape->center;sweep+=std::atan2(a.x*b.y-a.y*b.x,a.x*b.x+a.y*b.y);}
        const auto first=shape->vertices[1]-shape->center;const double middle=std::atan2(first.y,first.x)+sweep/2;
        const auto point=view_.worldToScreen(shape->center+Point{std::cos(middle),std::sin(middle)}*(shape->radiusX+4));
        result.append(QVariantMap{{"x",point.x},{"y",point.y},{"angle",std::clamp(std::abs(sweep)*180/std::numbers::pi,0.,360.)}});
    }return result;
}
namespace {
double PenStyle::* spacingField(const QString& field){if(field=="dashLength")return &PenStyle::dashLengthMm;if(field=="gapLength")return &PenStyle::gapLengthMm;if(field=="dotSpacing")return &PenStyle::dotSpacingMm;return nullptr;}
}
double CanvasItem::selectedPatternSpacing(const QString& field) const{
    const auto member=spacingField(field);if(!member)return 0;const auto selected=selectedObjects();if(selected.empty())return PenStyle{}.*member;
    return std::visit([&](const auto& object){if constexpr(std::is_same_v<std::decay_t<decltype(object)>,StrokeObject>||std::is_same_v<std::decay_t<decltype(object)>,ShapeObject>)return object.style.*member;else return PenStyle{}.*member;},selected.front());
}
void CanvasItem::setSelectedPatternSpacing(const QString& field,double value){
    const auto member=spacingField(field);if(!controller_||!member||!std::isfinite(value))return;value=std::clamp(value,.1,100.);
    std::vector<ObjectChange> changes;for(auto object:selectedObjects()){auto before=object;bool changed=false;std::visit([&](auto& item){if constexpr(std::is_same_v<std::decay_t<decltype(item)>,StrokeObject>||std::is_same_v<std::decay_t<decltype(item)>,ShapeObject>){if(item.style.*member!=value){item.style.*member=value;++item.properties.revision;changed=true;}}},object);if(changed)changes.push_back({before,object});}
    if(!changes.empty())controller_->changeObjects(std::move(changes),CommandKind::ChangeStyle);selectionUpdated();
}
QVariantMap CanvasItem::segmentGuide() const{
    if(state_!=State::Moving&&state_!=State::Rotating&&state_!=State::EditingHandle&&state_!=State::Resizing&&state_!=State::CreatingShape&&state_!=State::ShapePreview)return {};
    const auto selected=(state_==State::CreatingShape||state_==State::ShapePreview)&&previewShape_?std::vector<CanvasObject>{*previewShape_}:selectedObjects();
    if(selected.size()!=1)return {};const auto* line=std::get_if<ShapeObject>(&selected.front());if(!line||line->kind!=ShapeKind::Line||line->vertices.size()!=2)return {};
    const auto delta=line->vertices[1]-line->vertices[0];if(length(delta)<1e-8)return {};double degrees=std::atan2(-delta.y,delta.x)*180/std::numbers::pi;if(degrees<0)degrees+=360;if(degrees>=360-1e-8)degrees=0;
    const auto origin=view_.worldToScreen(line->vertices[0]);return {{"x",origin.x},{"y",origin.y},{"angle",degrees},{"snapped",angleSnap_&&(state_==State::Rotating||state_==State::EditingHandle||state_==State::CreatingShape)}};
}
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
    if(state_==State::ShapePreview&&previewShape_){
        const auto kind=previewShape_->kind;
        const bool feminine=kind==ShapeKind::Line||kind==ShapeKind::Ellipse;
        return QString::fromStdString(shapeName(kind))+(feminine?" reconhecida":" reconhecido")
            +(previewShape_->style.pattern==LinePattern::Dashed?" · tracejado":" · Shift: tracejar")
            +(kind==ShapeKind::Line?" · Ctrl: 15°":"");
    }
    if(state_==State::CreatingShape)return "Arraste para definir a forma";
    return drawing()?"Escrevendo · segure para reconhecer uma forma":"";
}
void CanvasItem::selectionUpdated(){emit selectionChanged();update();}
void CanvasItem::beginSelection(Point p,Qt::KeyboardModifiers modifiers){
    angleSnap_=modifiers.testFlag(Qt::ControlModifier);
    if(!controller_||!controller_->page())return;
    dragStart_=p;lastPan_=p;const double tolerance=12/(view_.pixelsPerMm*view_.zoom);handleIndex_=-1;
    const auto handles=selectionHandles();for(int i=0;i<handles.size();++i){const auto h=handles[i].toMap();const auto screen=view_.worldToScreen(p);
        if(length(screen-Point{h["x"].toDouble(),h["y"].toDouble()})<12){handleIndex_=i;const auto type=h["type"].toString();state_=type=="resize"?State::Resizing:type=="rotate"?State::Rotating:State::EditingHandle;break;}}
    if(handleIndex_<0&&selectedCount()>0&&!(modifiers&Qt::ShiftModifier)&&selectedBounds().contains(p))state_=State::Moving;
    else if(handleIndex_<0){std::optional<CanvasObject> hit;const auto all=objects(*controller_->page());for(auto i=all.rbegin();i!=all.rend();++i)if(hitTest(*i,p,tolerance)&&hasVisibleInk(*i,p)){hit=*i;break;}
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
        else if(state_==State::Rotating){const auto a=dragStart_-center,b=p-center;double rotation=std::atan2(b.y,b.x)-std::atan2(a.y,a.x);
            if(angleSnap_)if(const auto* line=std::get_if<ShapeObject>(&o);line&&line->kind==ShapeKind::Line&&line->vertices.size()==2){const auto delta=line->vertices[1]-line->vertices[0];const double original=std::atan2(delta.y,delta.x),step=std::numbers::pi/12;rotation=std::round((original+rotation)/step)*step-original;}
            o=transformed(o,center,{},1,1,rotation);}
        else if(state_==State::EditingHandle){auto* shape=std::get_if<ShapeObject>(&o);if(!shape)continue;
            const auto handles=selectionHandles();if(handleIndex_<0||handleIndex_>=handles.size())continue;const auto handle=handles[handleIndex_].toMap();const auto type=handle["type"].toString();
            if(type=="vertex"){auto point=p;const int index=handle["index"].toInt();if(angleSnap_&&shape->kind==ShapeKind::Line&&shape->vertices.size()==2){const auto anchor=shape->vertices[1-index],delta=p-anchor;const double step=std::numbers::pi/12,angle=std::round(std::atan2(delta.y,delta.x)/step)*step;point=anchor+Point{std::cos(angle),std::sin(angle)}*length(delta);}shape->vertices[index]=point;}
            else if(type=="center")shape->center=p;
            else {const auto local=rotatePoint(p,shape->center,-shape->rotation)-shape->center;if(type=="radiusX")shape->radiusX=std::clamp(std::abs(local.x),0.2,5000.);else shape->radiusY=std::clamp(std::abs(local.y),0.2,5000.);if(shape->kind==ShapeKind::Circle)shape->radiusY=shape->radiusX;}
            ++shape->properties.revision;
        }
    }selectionUpdated();
}
void CanvasItem::commitSelection(){
    std::vector<ObjectChange> changes;
    for(std::size_t i=0;i<editBefore_.size();++i){
        bool changed=length(lastPan_-dragStart_)>0.01;
        // Ctrl can snap a segment even when the pointer has not moved.
        if(const auto* before=std::get_if<ShapeObject>(&editBefore_[i]);before&&before->kind==ShapeKind::Line)
            if(const auto* after=std::get_if<ShapeObject>(&editPreview_[i]))changed|=before->vertices!=after->vertices;
        if(changed)changes.push_back({editBefore_[i],editPreview_[i]});
    }
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
