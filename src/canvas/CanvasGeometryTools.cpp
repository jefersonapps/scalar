#include "CanvasItem.h"
#include <cmath>
namespace scalar {
QVariantMap CanvasItem::rulerGeometry() const {
    const auto origin=view_.worldToScreen(ruler_.origin);const double scale=view_.pixelsPerMm*view_.zoom;
    return {{"x",origin.x},{"y",origin.y},{"angle",rulerAngle()},{"length",ruler_.lengthMm*scale},{"width",RulerGeometry::widthMm*scale},{"scale",scale},{"mm",ruler_.lengthMm}};
}
QVariantMap CanvasItem::compassGeometry() const {
    const auto center=view_.worldToScreen(compass_.center),pencil=view_.worldToScreen(compass_.pencil()),hinge=view_.worldToScreen(compass_.hinge()),opening=view_.worldToScreen(compass_.openingHandle());
    return {{"cx",center.x},{"cy",center.y},{"px",pencil.x},{"py",pencil.y},{"hx",hinge.x},{"hy",hinge.y},{"ox",opening.x},{"oy",opening.y},{"radius",compass_.radiusMm*view_.pixelsPerMm*view_.zoom},{"mm",compass_.radiusMm}};
}
void CanvasItem::centerGeometryTools(){
    cancelStroke();const auto center=world({width()/2,height()/2});ruler_.origin=center-Point{ruler_.lengthMm/2,RulerGeometry::widthMm/2};ruler_.angle=0;compass_.center=center;emit geometryToolsChanged();
}
void CanvasItem::setRulerVisible(bool value){if(value==rulerVisible_)return;cancelStroke();rulerVisible_=value;if(value){const auto center=world({width()/2,height()/2});ruler_.origin=center-Point{ruler_.lengthMm/2,RulerGeometry::widthMm/2};}emit geometryToolsChanged();}
void CanvasItem::setCompassVisible(bool value){if(value==compassVisible_)return;cancelStroke();compassVisible_=value;if(value)compass_.center=world({width()/2,height()/2});emit geometryToolsChanged();}
void CanvasItem::setRulerSnap(bool value){cancelStroke();rulerSnap_=value;emit geometryToolsChanged();}
void CanvasItem::setRulerSnapDistance(double value){if(!std::isfinite(value))return;cancelStroke();rulerSnapDistance_=std::clamp(value,.5,10.);emit geometryToolsChanged();}
void CanvasItem::setRulerLength(double value){if(!std::isfinite(value))return;cancelStroke();ruler_.lengthMm=std::clamp(value,20.,500.);emit geometryToolsChanged();}
void CanvasItem::setRulerAngle(double value){if(!std::isfinite(value))return;cancelStroke();const auto center=ruler_.world({ruler_.lengthMm/2,RulerGeometry::widthMm/2});ruler_.angle=std::remainder(value*std::numbers::pi/180,2*std::numbers::pi);ruler_.origin=center-rotatePoint({ruler_.lengthMm/2,RulerGeometry::widthMm/2},{},ruler_.angle);emit geometryToolsChanged();}
void CanvasItem::setCompassRadius(double value){if(!std::isfinite(value))return;cancelStroke();compass_.radiusMm=std::clamp(value,1.,500.);emit geometryToolsChanged();}
void CanvasItem::resetGeometryTools(){cancelStroke();selectedGuide_.clear();rulerVisible_=compassVisible_=false;if(tool_=="ruler"||tool_=="compass"){tool_="pen";emit toolChanged();}emit geometryToolsChanged();}
bool CanvasItem::geometryGesture() const {return state_==State::MovingRuler||state_==State::RotatingRuler||state_==State::ResizingRuler||state_==State::MovingCompass||state_==State::ResizingCompass||state_==State::DrawingCompass;}
bool CanvasItem::beginGeometryPointer(PointerSample s){
    const double tolerance=12/(view_.pixelsPerMm*view_.zoom);const auto p=s.position;
    if(compassVisible_&&(tool_=="compass"||tool_=="select")){
        compassBefore_=compass_;dragStart_=p;
        const double pencil=length(p-compass_.pencil()),center=length(p-compass_.center),opening=length(p-compass_.openingHandle()),hinge=length(p-compass_.hinge());
        const double nearest=std::min({pencil,center,opening,hinge});
        if(pencil==nearest&&nearest<tolerance){
            if(s.device==DeviceType::Touch){compassBefore_.reset();return false;}
            else {
            selection_.clear();selectionUpdated();state_=State::DrawingCompass;input_.reset();current_={newId(),style_,{input_.filter(s)}};current_.samples[0].position=compass_.pencil();compassSweep_.begin(compass_.angle);compassComplete_=false;emit drawingChanged();update();
            }
        }else if((center==nearest||hinge==nearest)&&nearest<tolerance)state_=State::MovingCompass;
        else if(opening==nearest&&nearest<tolerance)state_=State::ResizingCompass;
        else if(distanceToSegment(p,compass_.hinge(),compass_.pencil())<tolerance)state_=State::ResizingCompass;
        else if(distanceToSegment(p,compass_.center,compass_.hinge())<tolerance)state_=State::MovingCompass;
        else {compassBefore_.reset();}
        if(compassBefore_){selectedGuide_="compass";selection_.clear();selectionUpdated();emit geometryToolsChanged();return true;}
    }
    if(rulerVisible_&&(tool_=="ruler"||tool_=="pen"||tool_=="select")){
        const auto q=ruler_.local(p);const Point center{ruler_.lengthMm/2,RulerGeometry::widthMm/2};
        const double left=length(q-Point{0,RulerGeometry::widthMm/2}),right=length(q-Point{ruler_.lengthMm,RulerGeometry::widthMm/2}),middle=length(q-center);
        const bool rotate=left<tolerance&&left<std::min(right,middle),resize=right<tolerance&&right<std::min(left,middle);
        const bool body=q.x>=0&&q.x<=ruler_.lengthMm&&q.y>0&&q.y<RulerGeometry::widthMm;
        if(rotate||resize||body||middle<tolerance){selectedGuide_="ruler";selection_.clear();selectionUpdated();emit geometryToolsChanged();rulerBefore_=ruler_;dragStart_=p;state_=rotate?State::RotatingRuler:resize?State::ResizingRuler:State::MovingRuler;return true;}
        if(tool_=="ruler"&&!ruler_.nearEdge(p,rulerSnapDistance_)){if(s.device==DeviceType::Touch)return false;ruler_.origin=p-center;emit geometryToolsChanged();return true;}
    }
    return false;
}
bool CanvasItem::moveGeometryPointer(PointerSample s){
    if(!geometryGesture())return false;
    const auto p=s.position;
    if(state_==State::MovingRuler&&rulerBefore_)ruler_.origin=rulerBefore_->origin+p-dragStart_;
    else if(state_==State::RotatingRuler&&rulerBefore_){const auto center=rulerBefore_->world({rulerBefore_->lengthMm/2,RulerGeometry::widthMm/2});const auto d=p-center,a=dragStart_-center;ruler_.angle=rulerBefore_->angle+std::atan2(d.y,d.x)-std::atan2(a.y,a.x);ruler_.origin=center-rotatePoint({ruler_.lengthMm/2,RulerGeometry::widthMm/2},{},ruler_.angle);}
    else if(state_==State::ResizingRuler&&rulerBefore_){const auto q=rulerBefore_->local(p);ruler_.lengthMm=std::clamp(q.x,20.,500.);}
    else if(state_==State::MovingCompass&&compassBefore_)compass_.center=compassBefore_->center+p-dragStart_;
    else if(state_==State::ResizingCompass&&compassBefore_){
        // Preserve the initial grab offset: the pencil follows the drag without jumping.
        const auto tip=compassBefore_->pencil()+p-dragStart_,d=tip-compass_.center;
        compass_.radiusMm=std::clamp(length(d),1.,500.);
        if(length(d)>.1)compass_.angle=std::atan2(d.y,d.x);
    }
    else if(state_==State::DrawingCompass){
        if(length(p-compass_.center)<.1)return true;
        const double angle=std::atan2(p.y-compass_.center.y,p.x-compass_.center.x),previous=compassSweep_.angle(),delta=compassSweep_.advance(angle);
        compass_.angle=compassSweep_.angle();
        if(!compassComplete_){const int steps=std::max(1,int(std::ceil(std::abs(delta)/(std::numbers::pi/90))));s=input_.filter(s);
            for(int i=1;i<=steps;++i){auto sample=s;const double a=previous+delta*i/steps;sample.position=compass_.center+Point{std::cos(a),std::sin(a)}*compass_.radiusMm;current_.samples.push_back(sample);}
            if(compassSweep_.complete()){compassComplete_=true;previewShape_=compassCircle();emit drawingChanged();}
        }
        update();
    }
    emit geometryToolsChanged();return true;
}
ShapeObject CanvasItem::compassCircle() const {ShapeObject out;out.id=current_.id.empty()?newId():current_.id;out.kind=ShapeKind::Circle;out.center=compass_.center;out.radiusX=out.radiusY=compass_.radiusMm;out.style=style_;out.fillOpacity=0;return out;}
void CanvasItem::finishGeometryPointer(){
    const bool wasDrawing=state_==State::DrawingCompass;
    if(wasDrawing&&controller_){if(compassComplete_)controller_->addShape(compassCircle());else if(current_.samples.size()>1)controller_->addStroke(current_);}
    rulerBefore_.reset();compassBefore_.reset();state_=State::Idle;current_={};previewShape_.reset();if(wasDrawing)emit drawingChanged();emit geometryToolsChanged();update();
}
void CanvasItem::drawCompassCircle(){if(!controller_||!controller_->active()||!compassVisible_)return;cancelStroke();controller_->addShape(compassCircle());}
bool CanvasItem::constructFromSelection(const QString& kind){
    if(!controller_||!controller_->page())return false;const auto selected=selectedObjects();if(selected.size()!=1||!std::holds_alternative<ShapeObject>(selected[0]))return false;
    const auto& source=std::get<ShapeObject>(selected[0]);std::optional<ShapeObject> result;
    if(kind=="circumcircle")result=circumcircle(source);
    else if(kind=="incircle")result=incircle(source);
    else if(kind=="parallel"||kind=="perpendicular"||kind=="bisector")result=constructLine(source,kind=="parallel"?LineConstruction::Parallel:kind=="perpendicular"?LineConstruction::Perpendicular:LineConstruction::Bisector);
    if(!result)return false;cancelStroke();controller_->addShape(*result);selection_.clear();selection_.select(result->id);selectionUpdated();return true;
}
}
