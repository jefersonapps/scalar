#include "CanvasItem.h"
#include <cmath>
namespace scalar {
void CanvasItem::setGeometryMagnet(bool value){
    if(value==geometryMagnet_)return;
    cancelStroke();geometryMagnet_=value;compassMagnet_.reset();rulerMagnet_.reset();emit geometryToolsChanged();
}
void CanvasItem::prepareGeometryMagnet(){
    if(!controller_||!controller_->page())return;
    if(magneticPage_!=controller_->page()||magneticRevision_!=controller_->documentRevision()){
        magneticGeometry_.reset(*controller_->page());magneticPage_=controller_->page();magneticRevision_=controller_->documentRevision();compassMagnet_.reset();rulerMagnet_.reset();
    }
}
QVariantList CanvasItem::magneticAnchors() const {
    QVariantList out;if(!geometryMagnet_||!controller_||magneticRevision_!=controller_->documentRevision()||magneticPage_!=controller_->page())return out;
    const auto append=[&](Point p){const auto q=view_.worldToScreen(p);out.push_back(QVariantMap{{"x",q.x},{"y",q.y}});};
    if(state_==State::MovingCompass&&compassMagnet_)append(*compassMagnet_);
    if(state_==State::Idle&&rulerVisible_&&selectedGuide_=="ruler"&&rulerMagnet_){append(rulerMagnet_->first);if(!rulerMagnet_->single)append(rulerMagnet_->second);}
    if(state_==State::RotatingRuler&&rulerRotationPivot_)append(*rulerRotationPivot_);
    return out;
}
double CanvasItem::rulerAngle() const {
    return std::remainder(ruler_.angle*180/std::numbers::pi,360.);
}
QVariantMap CanvasItem::rulerGeometry() const {
    const auto origin=view_.worldToScreen(ruler_.origin);const double scale=view_.pixelsPerMm*view_.zoom;
    // Screen coordinates grow downward; present the angle in the opposite direction.
    const double displayAngle=std::fmod(360.-rulerAngle(),360.);
    return {{"x",origin.x},{"y",origin.y},{"angle",rulerAngle()},{"displayAngle",displayAngle},{"length",ruler_.lengthMm*scale},{"width",RulerGeometry::widthMm*scale},{"scale",scale},{"mm",ruler_.lengthMm},{"flipped",rulerFlipped_},{"rotationInset",std::min(10.,ruler_.lengthMm*.2)*scale}};
}
Point CanvasItem::compassDrawHandle() const {
    const auto tip=compass_.pencil(),direction=tip-compass_.hinge();
    return tip+direction*(34./(view_.pixelsPerMm*view_.zoom*length(direction)));
}
QVariantMap CanvasItem::compassGeometry() const {
    const auto center=view_.worldToScreen(compass_.center),pencil=view_.worldToScreen(compass_.pencil()),hinge=view_.worldToScreen(compass_.hinge()),opening=view_.worldToScreen(compass_.openingHandle());
    const auto draw=view_.worldToScreen(compassDrawHandle());
    return {{"cx",center.x},{"cy",center.y},{"px",pencil.x},{"py",pencil.y},{"dx",draw.x},{"dy",draw.y},{"hx",hinge.x},{"hy",hinge.y},{"ox",opening.x},{"oy",opening.y},{"radius",compass_.radiusMm*view_.pixelsPerMm*view_.zoom},{"mm",compass_.radiusMm},{"visualScale",view_.zoom}};
}
void CanvasItem::flipRuler(){cancelStroke();rulerMagnet_.reset();rulerFlipped_=!rulerFlipped_;selectedGuide_="ruler";selection_.clear();selectionUpdated();emit geometryToolsChanged();update();}
void CanvasItem::centerGeometryTools(){
    cancelStroke();rulerMagnet_.reset();const auto center=world({width()/2,height()/2});ruler_.origin=center-Point{ruler_.lengthMm/2,RulerGeometry::widthMm/2};ruler_.angle=0;compass_.center=center;emit geometryToolsChanged();
}
void CanvasItem::setRulerVisible(bool value){if(value==rulerVisible_)return;cancelStroke();rulerMagnet_.reset();rulerVisible_=value;if(value){const auto center=world({width()/2,height()/2});ruler_.origin=center-Point{ruler_.lengthMm/2,RulerGeometry::widthMm/2};}emit geometryToolsChanged();}
void CanvasItem::setCompassVisible(bool value){if(value==compassVisible_)return;cancelStroke();compassVisible_=value;if(value)compass_.center=world({width()/2,height()/2});emit geometryToolsChanged();}
void CanvasItem::setRulerSnap(bool value){cancelStroke();rulerSnap_=value;emit geometryToolsChanged();}
void CanvasItem::setRulerSnapDistance(double value){if(!std::isfinite(value))return;cancelStroke();rulerSnapDistance_=std::clamp(value,.5,10.);emit geometryToolsChanged();}
void CanvasItem::setRulerLength(double value){if(!std::isfinite(value))return;cancelStroke();rulerMagnet_.reset();ruler_.lengthMm=std::clamp(value,20.,500.);emit geometryToolsChanged();}
void CanvasItem::setRulerAngle(double value){if(!std::isfinite(value))return;cancelStroke();rulerMagnet_.reset();const auto center=ruler_.world({ruler_.lengthMm/2,RulerGeometry::widthMm/2});ruler_.angle=std::remainder(value*std::numbers::pi/180,2*std::numbers::pi);ruler_.origin=center-rotatePoint({ruler_.lengthMm/2,RulerGeometry::widthMm/2},{},ruler_.angle);emit geometryToolsChanged();}
void CanvasItem::setCompassRadius(double value){if(!std::isfinite(value))return;cancelStroke();compass_.radiusMm=std::clamp(value,1.,500.);emit geometryToolsChanged();}
void CanvasItem::resetGeometryTools(){cancelStroke();rulerMagnet_.reset();selectedGuide_.clear();rulerFlipped_=false;rulerVisible_=compassVisible_=false;if(tool_=="ruler"||tool_=="compass"){tool_="pen";emit toolChanged();}emit geometryToolsChanged();}
bool CanvasItem::geometryGesture() const {return state_==State::FlippingRuler||state_==State::MovingRuler||state_==State::RotatingRuler||state_==State::ResizingRuler||state_==State::MovingCompass||state_==State::ResizingCompass||state_==State::DrawingCompass;}
bool CanvasItem::beginGeometryPointer(PointerSample s){
    if(!std::isfinite(s.position.x)||!std::isfinite(s.position.y))return false;
    const double tolerance=12/(view_.pixelsPerMm*view_.zoom);const auto p=s.position;
    if(compassVisible_&&(tool_=="compass"||tool_=="ruler"||tool_=="select")){
        const double pencil=std::min(length(p-compass_.pencil()),length(p-compassDrawHandle())),center=length(p-compass_.center),opening=length(p-compass_.openingHandle()),hinge=length(p-compass_.hinge());
        const double nearest=std::min({pencil,center,opening,hinge});
        // Match the rotated, zoom-scaled 14 x 22 px grip above the joint.
        const auto grip=rotatePoint(p-compass_.hinge(),{},-compass_.angle)*view_.pixelsPerMm;
        const bool topGrip=std::abs(grip.x)<=7&&grip.y>=-32&&grip.y<=-10;
        State next=State::Idle;
        if(topGrip)next=State::MovingCompass;
        else if(pencil==nearest&&nearest<tolerance){
            if(s.device==DeviceType::Touch)return false;
            next=State::DrawingCompass;
        }else if((center==nearest||hinge==nearest)&&nearest<tolerance)next=State::MovingCompass;
        else if(opening==nearest&&nearest<tolerance)next=State::ResizingCompass;
        else if(distanceToSegment(p,compass_.hinge(),compass_.pencil())<tolerance)next=State::ResizingCompass;
        else if(distanceToSegment(p,compass_.center,compass_.hinge())<tolerance)next=State::MovingCompass;
        if(next!=State::Idle){
            // Switch before capturing the gesture: setTool clears gesture state.
            if(tool_=="ruler")setTool("compass");
            compassMagnet_.reset();
            compassBefore_=compass_;dragStart_=lastPan_=p;state_=next;
            selectedGuide_="compass";selection_.clear();selectionUpdated();
            if(state_==State::DrawingCompass){
                // Compass samples are exact arc points, independent of the
                // preceding freehand stroke's smoothing window.
                smoothFreehand_=false;freehandSamples_.clear();
                liveCurveStableSamples_=liveCurveReleasePrefix_=1;liveCurveReleaseSegment_=0;
                input_.reset();current_={newId(),style_,{input_.filter(s)}};current_.samples[0].position=compass_.pencil();
                compassSweep_.begin(compass_.angle);compassPointerAngle_=std::atan2(p.y-compass_.center.y,p.x-compass_.center.x);compassComplete_=false;
                emit drawingChanged();update();
            }
            emit geometryToolsChanged();return true;
        }
    }
    if(rulerVisible_&&(tool_=="ruler"||tool_=="compass"||tool_=="pen"||tool_=="select")){
        const auto q=ruler_.local(p);const Point center{ruler_.lengthMm/2,RulerGeometry::widthMm/2};
        const double inset=std::min(10.,ruler_.lengthMm*.2);
        const double left=length(q-Point{inset,center.y});
        const double right=length(q-Point{ruler_.lengthMm-inset,center.y});
        const double end=length(q-Point{ruler_.lengthMm,center.y});
        const double middle=length(q-center);
        const double nearest=std::min({left,right,end,middle});
        if(middle==nearest&&middle<tolerance){
            if(tool_=="compass")setTool("ruler");
            flipRuler();state_=State::FlippingRuler;return true;
        }
        const bool rotate=std::min(left,right)==nearest&&nearest<tolerance;
        const bool resize=end==nearest&&nearest<tolerance&&!rotate;
        // A pen starting near an edge draws along it; the controls still move the ruler.
        const bool nearDrawingEdge=tool_=="pen"&&rulerSnap_&&ruler_.nearEdge(p,rulerSnapDistance_).has_value();
        const bool body=!nearDrawingEdge&&q.x>=0&&q.x<=ruler_.lengthMm&&q.y>0&&q.y<RulerGeometry::widthMm;
        if(rotate||resize||body||middle<tolerance){if(tool_=="compass")setTool("ruler");selectedGuide_="ruler";selection_.clear();selectionUpdated();emit geometryToolsChanged();rulerRotationPivot_.reset();
            if(rotate&&geometryMagnet_&&rulerMagnet_){rulerRotationPivot_=rulerMagnet_->rotationAnchor(p);}
            rulerMagnet_.reset();rulerBefore_=ruler_;dragStart_=lastPan_=p;state_=rotate?State::RotatingRuler:resize?State::ResizingRuler:State::MovingRuler;return true;}
    }
    return false;
}
bool CanvasItem::moveGeometryPointer(PointerSample s){
    if(!geometryGesture())return false;
    const auto p=s.position;
    if(!std::isfinite(p.x)||!std::isfinite(p.y))return true;
    if(state_==State::MovingRuler&&rulerBefore_){ruler_=*rulerBefore_;ruler_.origin=rulerBefore_->origin+p-dragStart_;}
    else if(state_==State::RotatingRuler&&rulerBefore_){
        lastPan_=p;
        const auto center=rulerRotationPivot_.value_or(rulerBefore_->world({rulerBefore_->lengthMm/2,RulerGeometry::widthMm/2}));
        const auto d=p-center,a=dragStart_-center;
        ruler_.angle=rulerBefore_->angle+std::atan2(d.y,d.x)-std::atan2(a.y,a.x);
        if(angleSnap_){const double step=std::numbers::pi/12;ruler_.angle=std::round(ruler_.angle/step)*step;}
        ruler_.origin=rotatePoint(rulerBefore_->origin,center,ruler_.angle-rulerBefore_->angle);
    }
    else if(state_==State::ResizingRuler&&rulerBefore_){const auto q=rulerBefore_->local(p);ruler_=*rulerBefore_;ruler_.lengthMm=std::clamp(q.x,20.,500.);}
    else if(state_==State::MovingCompass&&compassBefore_)compass_.center=compassBefore_->center+p-dragStart_;
    else if(state_==State::ResizingCompass&&compassBefore_){
        lastPan_=p;
        // Preserve the initial grab offset: the pencil follows the drag without jumping.
        const auto tip=compassBefore_->pencil()+p-dragStart_,d=tip-compass_.center;
        compass_.radiusMm=std::clamp(length(d),1.,500.);
        if(length(d)>.1)compass_.angle=std::atan2(d.y,d.x);
        if(angleSnap_)compass_.radiusMm=std::round(compass_.radiusMm);
    }
    else if(state_==State::DrawingCompass){
        if(!compassBefore_)return true;
        lastPan_=p;
        // Angles are unreliable near the pivot. Reanchor on leaving this zone
        // instead of bridging across it with an unintended half-circle.
        const double deadZone=std::max(.1,std::min(compass_.radiusMm*.25,8./(view_.pixelsPerMm*view_.zoom)));
        if(length(p-compass_.center)<deadZone){compassPointerAngle_.reset();return true;}
        const double pointerAngle=std::atan2(p.y-compass_.center.y,p.x-compass_.center.x);
        if(!compassPointerAngle_){compassPointerAngle_=pointerAngle;return true;}
        const double step=std::remainder(pointerAngle-*compassPointerAngle_,2*std::numbers::pi);
        compassPointerAngle_=pointerAngle;
        // A single event spanning more than a quarter turn is ambiguous.
        if(std::abs(step)>std::numbers::pi/2+1e-9)return true;
        const double previous=compass_.angle;
        compassSweep_.advance(compassSweep_.angle()+step);
        compass_.angle=compassSweep_.angle();
        const double delta=compass_.angle-previous;
        if(!compassComplete_){const int steps=std::max(1,int(std::ceil(std::abs(delta)/(std::numbers::pi/90))));s=input_.filter(s);
            for(int i=1;i<=steps;++i){auto sample=s;const double a=previous+delta*i/steps;sample.position=compass_.center+Point{std::cos(a),std::sin(a)}*compass_.radiusMm;current_.samples.push_back(sample);}
            if(compassSweep_.complete()){compassComplete_=true;previewShape_=compassCircle();emit drawingChanged();}
        }
        update();
    }
    if(geometryMagnet_&&controller_&&controller_->page()&&state_==State::MovingCompass){
        prepareGeometryMagnet();const double tolerance=14./(view_.pixelsPerMm*view_.zoom);
        if(state_==State::MovingCompass){
            const auto raw=compass_.center;
            if(!compassMagnet_||length(raw-*compassMagnet_)>tolerance*1.5)compassMagnet_=magneticGeometry_.nearest(raw,tolerance);
            if(compassMagnet_)compass_.center=*compassMagnet_;
        }
    }
    emit geometryToolsChanged();return true;
}
ShapeObject CanvasItem::compassCircle() const {ShapeObject out;out.id=current_.id.empty()?newId():current_.id;out.kind=ShapeKind::Circle;out.center=compass_.center;out.radiusX=out.radiusY=compass_.radiusMm;out.style=style_;out.fillOpacity=0;return out;}
void CanvasItem::commitCompassObject(CanvasObject object){
    if(!controller_||!controller_->active())return;
    auto z=controller_->nextZIndex();properties(object).zIndex=z++;
    std::vector<ObjectChange> changes;changes.push_back({{},std::move(object)});
    if(controller_->sessionCompassMarkCenter()){
        StrokeObject dot;dot.id=newId();dot.style=style_;
        dot.style.minWidthMm=dot.style.maxWidthMm=std::max(.8,style_.maxWidthMm);
        dot.style.pattern=LinePattern::Solid;dot.samples.push_back({compass_.center,1});dot.properties.zIndex=z;
        changes.push_back({{},CanvasObject(std::move(dot))});
    }
    controller_->changeObjects(std::move(changes),CommandKind::AddObject);
}
void CanvasItem::finishGeometryPointer(){
    // Keep placement and rotation entirely under the pointer until release.
    if(geometryMagnet_&&controller_&&controller_->page()&&(state_==State::MovingRuler||state_==State::RotatingRuler||state_==State::ResizingRuler)){
        prepareGeometryMagnet();rulerMagnet_.reset();
        magneticGeometry_.alignRuler(ruler_,14./(view_.pixelsPerMm*view_.zoom),rulerMagnet_,rulerFlipped_?1:0,state_==State::RotatingRuler?rulerRotationPivot_:std::optional<Point>{});
    }
    const bool wasDrawing=state_==State::DrawingCompass;
    if(wasDrawing&&controller_){if(compassComplete_)commitCompassObject(compassCircle());else if(current_.samples.size()>1)commitCompassObject(current_);}
    rulerBefore_.reset();rulerRotationPivot_.reset();compassBefore_.reset();compassPointerAngle_.reset();state_=State::Idle;current_={};previewShape_.reset();if(wasDrawing)emit drawingChanged();emit geometryToolsChanged();update();
}
void CanvasItem::drawCompassCircle(){if(!controller_||!controller_->active()||!compassVisible_)return;cancelStroke();commitCompassObject(compassCircle());}
bool CanvasItem::constructFromSelection(const QString& kind){
    if(!controller_||!controller_->page())return false;const auto selected=selectedObjects();if(selected.size()!=1||!std::holds_alternative<ShapeObject>(selected[0]))return false;
    const auto& source=std::get<ShapeObject>(selected[0]);std::optional<ShapeObject> result;
    if(kind=="circumcircle")result=circumcircle(source);
    else if(kind=="incircle")result=incircle(source);
    else if(kind=="parallel"||kind=="perpendicular"||kind=="bisector")result=constructLine(source,kind=="parallel"?LineConstruction::Parallel:kind=="perpendicular"?LineConstruction::Perpendicular:LineConstruction::Bisector);
    if(!result)return false;cancelStroke();controller_->addShape(*result);selection_.clear();selection_.select(result->id);selectionUpdated();return true;
}
}
