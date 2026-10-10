#include "CanvasItem.h"
#include "tools/StrokeEraser.h"
#include "rendering/StrokeMesh.h"
#include <QtConcurrent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QLineF>
#include <QNativeGestureEvent>
#include <QHoverEvent>
#include <QCursor>
#include <QGuiApplication>
#include <functional>
namespace scalar {
namespace {
Qt::CursorShape selectionResizeCursor(double degrees){
    const int direction=(int(std::round(std::remainder(degrees,180.)/45))+5)%4;
    constexpr Qt::CursorShape cursors[]={Qt::SizeHorCursor,Qt::SizeFDiagCursor,Qt::SizeVerCursor,Qt::SizeBDiagCursor};
    return cursors[direction];
}
template<class Object> void appendErasedRegion(Object& object,Point from,Point to,double radius,bool restore){
    if(!object.erasedRegions.empty()){
        auto& previous=object.erasedRegions.back();
        if(previous.restore==restore&&previous.radius==radius&&length(previous.to-from)<1e-8&&distanceToSegment(previous.to,previous.from,to)<1e-6){
            if(previous.to!=to){previous.to=to;++object.properties.revision;}
            return;
        }
    }
    object.erasedRegions.push_back({from,to,radius,restore});++object.properties.revision;
}
class TabletProximityFilter final : public QObject {
public:
    TabletProximityFilter(QObject* parent,std::function<void()> leave):QObject(parent),leave_(std::move(leave)){}
protected:
    bool eventFilter(QObject*,QEvent* event) override {
        if(event->type()==QEvent::TabletLeaveProximity)leave_();
        return false;
    }
private:
    std::function<void()> leave_;
};
}
CanvasItem::CanvasItem(QQuickItem* parent):QQuickItem(parent){
    if(auto* app=QGuiApplication::instance())app->installEventFilter(new TabletProximityFilter(this,[this]{setTabletEraser(false);}));
    setFlag(ItemHasContents,true);setFlag(ItemIsFocusScope,true);setAcceptedMouseButtons(Qt::LeftButton|Qt::MiddleButton|Qt::RightButton);setAcceptTouchEvents(true);setAcceptHoverEvents(true);setClip(true);
    setCursor(QCursor(Qt::CrossCursor));
    holdTimer_.setSingleShot(true);connect(&holdTimer_,&QTimer::timeout,this,&CanvasItem::beginRecognition);
    viewRefinementTimer_.setSingleShot(true);viewRefinementTimer_.setInterval(150);
    connect(&viewRefinementTimer_,&QTimer::timeout,this,[this]{
        if(controller_){const auto scale=view_.pixelsPerMm*view_.zoom*(window()?window()->devicePixelRatio():1);controller_->refreshTextTextures(scale);controller_->refreshPdf(scale);}
        update();
    });
    connect(&recognitionWatcher_,&QFutureWatcher<RecognitionResult>::finished,this,[this]{
        if(requestEpoch_!=inputEpoch_||!controller_)return;auto result=recognitionWatcher_.result();if(!result.shape)return;
        if(state_==State::Drawing||state_==State::PossibleHold){previewShape_=*result.shape;if(recognitionDashed_||shiftHeld_)previewShape_->style.pattern=LinePattern::Dashed;state_=State::ShapePreview;emit drawingChanged();if(previewShape_->kind==ShapeKind::Line)updateRecognizedLine(lastPan_);else selectionUpdated();}
        else if(state_==State::Idle&&controller_->page()){
            if(auto before=findObject(*controller_->page(),current_.id);before&&std::holds_alternative<StrokeObject>(*before)&&properties(*before).revision==current_.properties.revision){controller_->changeObjects({{before,CanvasObject(*result.shape)}},CommandKind::ConvertStrokeToShape);selectionUpdated();}
        }
    });
    connect(this,&QQuickItem::enabledChanged,this,[this]{if(!isEnabled()){space_=false;cancelStroke();setTemporaryHand(false);setTabletEraser(false);}});
    connect(this,&QQuickItem::visibleChanged,this,[this]{if(!isVisible()){space_=false;cancelStroke();setTemporaryHand(false);setTabletEraser(false);}});
    connect(this,&QQuickItem::windowChanged,this,[this](QQuickWindow* w){
        disconnect(handFocusConnection_);
        if(filteredWindow_)filteredWindow_->removeEventFilter(this);
        filteredWindow_=w;if(w)w->installEventFilter(this);
        if(w)handFocusConnection_=connect(w,&QQuickWindow::activeFocusItemChanged,this,[this]{if(textInputFocused())setTemporaryHand(false);});
    });
}
CanvasItem::~CanvasItem(){if(filteredWindow_)filteredWindow_->removeEventFilter(this);}
void CanvasItem::setController(AppController* c){
    if(controller_==c)return;if(controller_)disconnect(controller_,nullptr,this,nullptr);controller_=c;viewPageId_.clear();
    if(c)connect(c,&AppController::documentChanged,this,[this]{
        // An external edit/undo invalidates the gesture's spatial snapshot.
        // commitErase switches to Idle before publishing its own changes.
        if(state_==State::Erasing)cancelStroke();
        if(controller_->page()){selection_.prune(*controller_->page());const auto ids=selection_.ids();for(const auto& id:ids)if(auto object=findObject(*controller_->page(),id);object&&!hasVisibleInk(*object))selection_.select(id,true);}else selection_.clear();selectionUpdated();update();
    });
    if(c)connect(c,&AppController::pageChanged,this,[this]{cancelStroke();resetGeometryTools();selection_.clear();selectionUpdated();restorePageView();});
    if(c&&c->pageColor().lightness()<128)setPenColor(QColor("#f4f4f5"));emit controllerChanged();update();
}
void CanvasItem::setTool(const QString& t){
    if(t!="ruler"&&t!="compass"&&t!="text"&&t!="eraser"&&t!="pen"&&t!="marker"&&t!="hand"&&t!="select"&&t!="line"&&t!="circle"&&t!="ellipse"&&t!="triangle"&&t!="rectangle")return;
    if(t==tool_)return;
    // Selecting the current tool leaves contact uninterrupted. Switching away
    // from ink finishes it with its original tool, pressure and smoothing.
    if(drawing()){if(!current_.samples.empty())endPointer(current_.samples.back());else commit();}
    else if(state_==State::Erasing)commitErase();
    else if(geometryGesture())finishGeometryPointer();
    handToolBefore_.clear();tabletToolBefore_.clear();cancelStroke();selectedGuide_.clear();tool_=t;updateCursor();if(t=="ruler"||t=="compass"){selection_.clear();selectionUpdated();}if(t=="ruler")setRulerVisible(true);if(t=="compass")setCompassVisible(true);emit geometryToolsChanged();emit toolChanged();
}
void CanvasItem::setPenColor(const QColor& c){if(!c.isValid())return;color_=c;style_.rgba=(std::uint32_t(c.red())<<24)|(std::uint32_t(c.green())<<16)|(std::uint32_t(c.blue())<<8)|255;emit penChanged();}
void CanvasItem::setPenWidth(double w){style_.maxWidthMm=std::clamp(w,0.2,5.0);style_.minWidthMm=std::min(0.15,style_.maxWidthMm);emit penChanged();}
void CanvasItem::setPressureGamma(double g){style_.gamma=std::clamp(g,0.3,3.0);emit penChanged();}
QString CanvasItem::shapeLineStyle() const {return manualShapePattern_==LinePattern::Dashed?"dashed":manualShapePattern_==LinePattern::Dotted?"dotted":"solid";}
void CanvasItem::setShapeLineStyle(const QString& value){
    if(value!="solid"&&value!="dashed"&&value!="dotted")return;
    manualShapePattern_=value=="dashed"?LinePattern::Dashed:value=="dotted"?LinePattern::Dotted:LinePattern::Solid;
    emit penChanged();
}
QString CanvasItem::penLineStyle() const {return style_.pattern==LinePattern::Dashed?"dashed":style_.pattern==LinePattern::Dotted?"dotted":"solid";}
void CanvasItem::setPenLineStyle(const QString& value){
    if(value!="solid"&&value!="dashed"&&value!="dotted")return;
    style_.pattern=value=="dashed"?LinePattern::Dashed:value=="dotted"?LinePattern::Dotted:LinePattern::Solid;emit penChanged();
}
void CanvasItem::viewUpdated(bool interactive){
    if(controller_&&!viewPageId_.isEmpty())controller_->rememberPageView(viewPageId_,{view_.zoom,view_.screenToWorld({width()/2,height()/2})});
    if(interactive)viewRefinementTimer_.start();
    else {viewRefinementTimer_.stop();if(controller_){const auto scale=view_.pixelsPerMm*view_.zoom*(window()?window()->devicePixelRatio():1);controller_->refreshTextTextures(scale);controller_->refreshPdf(scale);}}
    update();emit viewChanged();emit selectionChanged();emit geometryToolsChanged();
}
void CanvasItem::restorePageView(){
    if(!controller_||!controller_->page()){viewPageId_.clear();return;}
    if(width()<1||height()<1)return;
    const auto id=QString::fromStdString(controller_->page()->id);
    if(id==viewPageId_)return;
    const bool first=viewPageId_.isEmpty();viewPageId_=id;
    if(const auto saved=controller_->sessionPageView(id)){
        view_.zoom=saved->zoom;view_.pan={width()/2-saved->center.x*view_.pixelsPerMm*view_.zoom,height()/2-saved->center.y*view_.pixelsPerMm*view_.zoom};
        viewUpdated(false);
    }else if(first)fitPage();
    else {
        const auto area=pageRenderBounds(*controller_->page());const double scale=view_.pixelsPerMm*view_.zoom;
        view_.pan={std::max(40.,(width()-area.width()*scale)/2)-area.left*scale,40-area.top*scale};
        viewUpdated(false);
    }
}
void CanvasItem::fitPage(){
    if(!controller_||!controller_->active()||width()<1||height()<1)return;
    viewPageId_=QString::fromStdString(controller_->page()->id);
    cancelStroke();const double margin=40;
    const auto area=pageRenderBounds(*controller_->page());
    view_.zoom=std::clamp(std::min((width()-margin*2)/(area.width()*view_.pixelsPerMm),(height()-margin*2)/(area.height()*view_.pixelsPerMm)),0.05,8.0);
    const auto center=area.center();view_.pan={width()/2-center.x*view_.pixelsPerMm*view_.zoom,height()/2-center.y*view_.pixelsPerMm*view_.zoom};viewUpdated(false);
}
void CanvasItem::zoomBy(double f){if(drawing()||state_==State::Erasing)return;view_.zoomAt({width()/2,height()/2},f);viewUpdated();}
bool CanvasItem::setZoom(double value){
    if(!std::isfinite(value)||value<.05||value>8||drawing()||state_==State::Erasing)return false;
    view_.zoomAt({width()/2,height()/2},value/view_.zoom);viewUpdated();return true;
}
void CanvasItem::begin(PointerSample s){
    if(!controller_||!controller_->active())return;
    if(!controller_->pageInfinite()&&(s.position.x<0||s.position.y<0||s.position.x>controller_->pageWidth()||s.position.y>controller_->pageHeight()))return;
    selection_.clear();selectionUpdated();++inputEpoch_;holdTimer_.stop();previewShape_.reset();
    guidedEdge_.reset();if(rulerVisible_&&rulerSnap_&&(tool_=="pen"||tool_=="ruler")){guidedEdge_=ruler_.nearEdge(s.position,rulerSnapDistance_);if(guidedEdge_)s.position=ruler_.project(s.position,*guidedEdge_);}
    auto ink=style_;if(tool_=="marker"){ink.rgba=(ink.rgba&0xffffff00)|std::uint32_t(std::lround(markerOpacity_*255));ink.minWidthMm=ink.maxWidthMm=markerWidth_;ink.pattern=LinePattern::Solid;}
    smoothFreehand_=!rulerVisible_&&(tool_=="pen"||tool_=="marker");
    recognitionDashed_=false;input_.reset();current_={newId(),ink,{input_.filter(s,smoothFreehand_)}};holdAnchor_=lastPan_=s.position;
    freehandSamples_=current_.samples;
    liveCurveStableSamples_=current_.samples.size();
    current_.marker=tool_=="marker";
    state_=State::Drawing;if(!guidedEdge_&&tool_=="pen"&&controller_->recognitionEnabled())holdTimer_.start(controller_->holdDelay());emit drawingChanged();update();
}
void CanvasItem::append(PointerSample s){
    if(!drawing())return;
    lastPan_=s.position;
    if(state_==State::ShapePreview){updateRecognizedLine(s.position);return;}
    if(rulerVisible_&&rulerSnap_&&(tool_=="pen"||tool_=="ruler"))guidedEdge_=ruler_.nearEdge(s.position,rulerSnapDistance_);
    else guidedEdge_.reset();
    if(guidedEdge_)s.position=ruler_.project(s.position,*guidedEdge_);
    // Stop at the physical edge, including fast moves that cross the entire ruler.
    if(rulerVisible_&&(tool_=="pen"||tool_=="ruler")&&!current_.samples.empty()){
        if(const auto entry=ruler_.entryPoint(current_.samples.back().position,s.position)){
            s.position=*entry;s=input_.filter(s);
            if(length(s.position-current_.samples.back().position)>0.005)current_.samples.push_back(s);
            commit();return;
        }
    }
    const auto rawPosition=s.position;
    s=input_.filter(s,smoothFreehand_);
    auto& samples=smoothFreehand_?freehandSamples_:current_.samples;
    const bool added=samples.empty()||length(s.position-samples.back().position)>0.005;
    if(added)samples.push_back(s);
    if(smoothFreehand_&&added){
        if(!current_.marker){
            // Finalize one segment once its five-point smoothing window is complete.
            // A new input point never rebuilds the already smoothed prefix.
            current_.samples.resize(liveCurveStableSamples_);
            if(samples.size()>=5){appendSmoothStrokeSegment(samples,samples.size()-5,current_.samples);liveCurveStableSamples_=current_.samples.size();}
            for(std::size_t segment=samples.size()>4?samples.size()-4:0;segment+1<samples.size();++segment)
                appendSmoothStrokeSegment(samples,segment,current_.samples);
        }else current_.samples.push_back(samples.back());
    }
    if(!guidedEdge_&&tool_=="pen"&&controller_&&controller_->recognitionEnabled()&&!current_.samples.empty()){
        if(length(rawPosition-holdAnchor_)>0.6){holdAnchor_=rawPosition;holdTimer_.start(controller_->holdDelay());state_=State::PossibleHold;++inputEpoch_;}
        else if(!holdTimer_.isActive()&&state_!=State::PossibleHold){holdTimer_.start(controller_->holdDelay());state_=State::PossibleHold;}
    }
    update();
}
void CanvasItem::updateRecognizedLine(Point end){
    if(!previewShape_||previewShape_->kind!=ShapeKind::Line||previewShape_->vertices.size()!=2)return;
    lastPan_=end;
    if(angleSnap_){const auto start=previewShape_->vertices[0],delta=end-start;const double step=std::numbers::pi/12,angle=std::round(std::atan2(delta.y,delta.x)/step)*step;end=start+Point{std::cos(angle),std::sin(angle)}*length(delta);}
    previewShape_->vertices[1]=end;selectionUpdated();
}
void CanvasItem::beginRecognition(){
    if(!controller_||!controller_->recognitionEnabled()||(state_!=State::Drawing&&state_!=State::PossibleHold)||current_.samples.empty())return;
    if(recognitionWatcher_.isRunning()){holdTimer_.start(100);return;}
    recognitionDashed_=shiftHeld_;requestEpoch_=inputEpoch_;const auto snapshot=current_;
    const auto page=controller_->page()?*controller_->page():Page{};
    recognitionWatcher_.setFuture(QtConcurrent::run([snapshot,page]{
        if(auto angle=recognizeRightAngle(page,snapshot))return RecognitionResult{angle,.95};
        return recognizeShape(snapshot,page);
    }));
}
void CanvasItem::commit(){
    if(!drawing())return;holdTimer_.stop();++inputEpoch_;
    if(controller_){if(previewShape_&&state_==State::ShapePreview)controller_->addRecognizedStroke(current_,*previewShape_);else if(previewShape_&&state_==State::CreatingShape)controller_->addShape(*previewShape_);else controller_->addStroke(current_);}
    state_=State::Idle;current_={};previewShape_.reset();guidedEdge_.reset();emit drawingChanged();selectionUpdated();
}
void CanvasItem::cancelStroke(){
    eraserCursorVisible_=false;
    eraseIndex_.clear();restoreIndex_.clear();
    if(rulerBefore_)ruler_=*rulerBefore_;if(compassBefore_)compass_=*compassBefore_;rulerBefore_.reset();compassBefore_.reset();guidedEdge_.reset();
    const bool was=drawing();holdTimer_.stop();++inputEpoch_;state_=State::Idle;current_={};previewShape_.reset();editBefore_.clear();editPreview_.clear();eraseBefore_.clear();erasePreview_.clear();eraseShapesBefore_.clear();eraseShapesPreview_.clear();eraseFillsBefore_.clear();eraseFillsPreview_.clear();recoverableInk_.clear();tabletActive_=false;touchDistance_=0;
    updateCursor();
    textResizeVisual_.reset();if(was)emit drawingChanged();emit geometryToolsChanged();selectionUpdated();update();
}
ShapeObject CanvasItem::directShape(Point a,Point b) const{
    ShapeObject s;s.id=current_.id;s.style=style_;s.style.pattern=manualShapePattern_;
    const auto center=(a+b)*0.5;const double w=std::max(0.2,std::abs(b.x-a.x)),h=std::max(0.2,std::abs(b.y-a.y));
    if(tool_=="line"){if(angleSnap_){const auto delta=b-a;const double angle=std::round(std::atan2(delta.y,delta.x)/(std::numbers::pi/12))*(std::numbers::pi/12);b=a+Point{std::cos(angle),std::sin(angle)}*length(delta);}s.kind=ShapeKind::Line;s.vertices={a,b};s.fillOpacity=0;}
    else if(tool_=="circle"){s.kind=ShapeKind::Circle;s.center=center;s.radiusX=s.radiusY=std::max(0.2,length(b-a)/2);}
    else if(tool_=="ellipse"){s.kind=ShapeKind::Ellipse;s.center=center;s.radiusX=w/2;s.radiusY=h/2;}
    else if(tool_=="triangle"){s.kind=ShapeKind::Triangle;s.vertices={{center.x,std::min(a.y,b.y)},{std::max(a.x,b.x),std::max(a.y,b.y)},{std::min(a.x,b.x),std::max(a.y,b.y)}};}
    else {s.kind=ShapeKind::Rectangle;s.vertices={{std::min(a.x,b.x),std::min(a.y,b.y)},{std::max(a.x,b.x),std::min(a.y,b.y)},{std::max(a.x,b.x),std::max(a.y,b.y)},{std::min(a.x,b.x),std::max(a.y,b.y)}};}
    return s;
}
void CanvasItem::startPointer(PointerSample s,Qt::KeyboardModifiers modifiers){
    angleSnap_=modifiers.testFlag(Qt::ControlModifier);
    shiftHeld_=modifiers.testFlag(Qt::ShiftModifier);
    if(tool_=="marker"&&modifiers.testFlag(Qt::ControlModifier)){if(controller_)controller_->fillRegion(s.position,color_,markerOpacity_);return;}
    if(beginGeometryPointer(s))return;
    selectedGuide_.clear();emit geometryToolsChanged();
    if(tool_=="text"){
        if(!controller_||!controller_->page())return;
        if(!controller_->pageInfinite()&&(s.position.x<0||s.position.y<0||s.position.x>controller_->pageWidth()||s.position.y>controller_->pageHeight()))return;
        for(const auto& text:controller_->page()->texts)if(text.properties.visible&&!text.properties.locked&&hitTest(CanvasObject(text),s.position,0)){
            emit textRequested({},QString::fromStdString(text.id));return;
        }
        selection_.clear();dragStart_=lastPan_=s.position;state_=State::CreatingText;selectionUpdated();return;
    }
    if(tool_=="eraser"){
        if(!controller_||!controller_->page())return;cancelStroke();selection_.clear();state_=State::Erasing;eraserCtrl_=modifiers.testFlag(Qt::ControlModifier);shiftHeld_=modifiers.testFlag(Qt::ShiftModifier);recoverableInk_=controller_->page()->erasedInk;lastPan_=s.position;eraseBefore_.clear();erasePreview_.clear();eraseShapesBefore_.clear();eraseShapesPreview_.clear();eraseFillsBefore_.clear();eraseFillsPreview_.clear();
        for(const auto& object:objectViews(*controller_->page()))eraseIndex_.insert(object);
        for(const auto& ink:recoverableInk_)restoreIndex_.insert(&ink);
        eraserWorldPosition_=s.position;eraserCursorVisible_=true;eraseAt(s.position);return;
    }
    if(tool_=="select"){beginSelection(s.position,modifiers);return;}
    begin(s);if(state_==State::Idle)return;
    if(tool_!="pen"&&tool_!="marker"&&tool_!="ruler"&&tool_!="compass"){dragStart_=lastPan_=s.position;state_=State::CreatingShape;previewShape_=directShape(dragStart_,dragStart_);selectionUpdated();}
}
void CanvasItem::movePointer(PointerSample s){
    if(state_==State::CreatingText){lastPan_=s.position;selectionUpdated();return;}
    if(moveGeometryPointer(s))return;
    if(state_==State::Erasing){eraseAt(s.position);return;}
    if(state_==State::CreatingShape){lastPan_=s.position;previewShape_=directShape(dragStart_,s.position);selectionUpdated();return;}
    if(state_==State::Moving||state_==State::Resizing||state_==State::Rotating||state_==State::EditingHandle||state_==State::Marquee){updateSelection(s.position);return;}
    append(s);
}
void CanvasItem::endPointer(PointerSample s){
    if(state_==State::CreatingText){
        state_=State::Idle;selectionUpdated();
        const auto w=std::abs(s.position.x-dragStart_.x),h=std::abs(s.position.y-dragStart_.y);
        emit textBoxRequested({std::min(s.position.x,dragStart_.x),std::min(s.position.y,dragStart_.y),w<2?60:w,h<2?18:h});return;
    }
    if(geometryGesture()){moveGeometryPointer(s);finishGeometryPointer();}
    else if(state_==State::Erasing){eraseAt(s.position);commitErase();}
    else if(drawing()){
        movePointer(s);
        if(smoothFreehand_&&!previewShape_&&!freehandSamples_.empty()){
            // Preserve the pen-up endpoint instead of leaving the filter's lag.
            freehandSamples_.back().position=s.position;current_.samples=smoothStrokeSamples(freehandSamples_);
        }
        commit();
    }
    else if(state_==State::Marquee){updateSelection(s.position);selection_.marquee(*controller_->page(),{std::min(dragStart_.x,s.position.x),std::min(dragStart_.y,s.position.y),std::max(dragStart_.x,s.position.x),std::max(dragStart_.y,s.position.y)},marqueeAdditive_);const auto ids=selection_.ids();for(const auto& id:ids)if(auto object=findObject(*controller_->page(),id);object&&!hasVisibleInk(*object))selection_.select(id,true);state_=State::Idle;selectionUpdated();}
    else if(state_==State::Moving||state_==State::Resizing||state_==State::Rotating||state_==State::EditingHandle){updateSelection(s.position);commitSelection();}
}
void CanvasItem::eraseAt(Point point){
    if(!controller_||!controller_->page())return;
    const auto nearby=eraseIndex_.query(lastPan_,point,eraserRadius_);
    const auto eraseVisible=[&](const StrokeObject& stroke){
        std::vector<std::array<Point,4>> shields;
        for(const auto& image:controller_->page()->images)
            if(!image.inkFill&&image.properties.visible&&image.properties.zIndex>stroke.properties.zIndex&&image.corners.size()==4){
                std::array<Point,4> corners;std::copy(image.corners.begin(),image.corners.end(),corners.begin());shields.push_back(corners);
            }
        auto cut=cutStroke(stroke,lastPan_,point,eraserRadius_,shields);
        if(!cut.removed.empty()){
            eraseIndex_.erase(stroke.id);for(const auto& fragment:cut.visible)eraseIndex_.insert(&fragment);
            for(const auto& fragment:cut.removed)restoreIndex_.insert(&fragment);
            recoverableInk_.insert(recoverableInk_.end(),std::make_move_iterator(cut.removed.begin()),std::make_move_iterator(cut.removed.end()));
        }
        return std::move(cut.visible);
    };
    if(shiftHeld_){
        const auto restorable=restoreIndex_.query(lastPan_,point,eraserRadius_);
        std::vector<StrokeObject> remaining;
        for(auto& ink:recoverableInk_){
            if(ink.properties.locked||!ink.properties.visible||!restorable.contains(ink.id)){remaining.push_back(std::move(ink));continue;}
            auto cut=cutStroke(ink,lastPan_,point,eraserRadius_);
            restoreIndex_.erase(ink.id);for(const auto& fragment:cut.visible)restoreIndex_.insert(&fragment);for(const auto& fragment:cut.removed)eraseIndex_.insert(&fragment);
            erasePreview_.insert(erasePreview_.end(),std::make_move_iterator(cut.removed.begin()),std::make_move_iterator(cut.removed.end()));
            remaining.insert(remaining.end(),std::make_move_iterator(cut.visible.begin()),std::make_move_iterator(cut.visible.end()));
        }recoverableInk_=std::move(remaining);
    }else {
    std::vector<StrokeObject> candidates=std::move(erasePreview_);
    erasePreview_.clear();
    for(auto& stroke:candidates){
        if(stroke.marker||!nearby.contains(stroke.id)){erasePreview_.push_back(std::move(stroke));continue;}
        auto fragments=eraseVisible(stroke);erasePreview_.insert(erasePreview_.end(),std::make_move_iterator(fragments.begin()),std::make_move_iterator(fragments.end()));
    }
    // Process newly touched originals after existing fragments: never clip the
    // fragments a second time with the same swept segment.
    for(const auto& s:controller_->page()->strokes){if(s.marker||s.properties.locked||!s.properties.visible||!nearby.contains(s.id))continue;
        if(std::none_of(eraseBefore_.begin(),eraseBefore_.end(),[&](const auto& b){return b.id==s.id;})){
            auto fragments=eraseVisible(s);
            if(fragments.size()!=1||fragments[0].id!=s.id){eraseBefore_.push_back(s);erasePreview_.insert(erasePreview_.end(),std::make_move_iterator(fragments.begin()),std::make_move_iterator(fragments.end()));}
        }
    }
    }
    // Keep translucent markers whole. Splitting them adds overlapping round
    // caps, which darken the ink and cannot erase its interior precisely.
    for(const auto& original:controller_->page()->strokes){
        if(!original.marker||original.properties.locked||!original.properties.visible||!nearby.contains(original.id))continue;
        if(!strokeTouchesEraser(original,lastPan_,point,eraserRadius_))continue;
        auto it=std::find_if(erasePreview_.begin(),erasePreview_.end(),[&](const auto& stroke){return stroke.id==original.id;});
        if(shiftHeld_&&(it==erasePreview_.end()?original.erasedRegions.empty():it->erasedRegions.empty()))continue;
        if(it==erasePreview_.end()){eraseBefore_.push_back(original);erasePreview_.push_back(original);it=std::prev(erasePreview_.end());}
        appendErasedRegion(*it,lastPan_,point,eraserRadius_,shiftHeld_);
    }
    // Bucket fills are marker ink stored as an image, and are erased without Ctrl.
    for(const auto& original:controller_->page()->images){
        if(!original.inkFill||original.properties.locked||!original.properties.visible||original.corners.size()!=4||!nearby.contains(original.id))continue;
        ShapeObject footprint;footprint.kind=ShapeKind::Polygon;footprint.vertices=original.corners;footprint.style.minWidthMm=footprint.style.maxWidthMm=0;
        if(!shapeTouchesEraser(footprint,lastPan_,point,eraserRadius_))continue;
        auto it=std::find_if(eraseFillsPreview_.begin(),eraseFillsPreview_.end(),[&](const auto& fill){return fill.id==original.id;});
        if(shiftHeld_&&(it==eraseFillsPreview_.end()?original.erasedRegions.empty():it->erasedRegions.empty()))continue;
        if(it==eraseFillsPreview_.end()){eraseFillsBefore_.push_back(original);eraseFillsPreview_.push_back(original);it=std::prev(eraseFillsPreview_.end());}
        appendErasedRegion(*it,lastPan_,point,eraserRadius_,shiftHeld_);
    }
    if(eraserShapes_||eraserCtrl_||shiftHeld_){
        for(const auto& original:controller_->page()->shapes){
            if(original.properties.locked||!original.properties.visible||!nearby.contains(original.id))continue;
            auto it=std::find_if(eraseShapesPreview_.begin(),eraseShapesPreview_.end(),[&](const auto& shape){return shape.id==original.id;});
            if(!shapeTouchesEraser(original,lastPan_,point,eraserRadius_))continue;
            if(shiftHeld_&&(it==eraseShapesPreview_.end()?original.erasedRegions.empty():it->erasedRegions.empty()))continue;
            if(it==eraseShapesPreview_.end()){eraseShapesBefore_.push_back(original);eraseShapesPreview_.push_back(original);it=std::prev(eraseShapesPreview_.end());}
            appendErasedRegion(*it,lastPan_,point,eraserRadius_,shiftHeld_);
        }
    }
    lastPan_=point;eraserWorldPosition_=point;selectionUpdated();
}
void CanvasItem::commitErase(){
    eraserCursorVisible_=false;
    eraseIndex_.clear();restoreIndex_.clear();
    std::vector<ObjectChange> changes;for(const auto& s:eraseBefore_)changes.push_back({CanvasObject(s),{}});for(const auto& s:erasePreview_)changes.push_back({{},CanvasObject(s)});
    for(std::size_t i=0;i<eraseShapesBefore_.size();++i)changes.push_back({CanvasObject(eraseShapesBefore_[i]),CanvasObject(eraseShapesPreview_[i])});
    for(std::size_t i=0;i<eraseFillsBefore_.size();++i)changes.push_back({CanvasObject(eraseFillsBefore_[i]),CanvasObject(eraseFillsPreview_[i])});
    state_=State::Idle;eraseBefore_.clear();erasePreview_.clear();eraseShapesBefore_.clear();eraseShapesPreview_.clear();eraseFillsBefore_.clear();eraseFillsPreview_.clear();if(controller_)controller_->eraseObjects(std::move(changes),std::move(recoverableInk_));recoverableInk_.clear();selectionUpdated();
}
void CanvasItem::setTabletEraser(bool erasing){
    if(erasing==!tabletToolBefore_.isEmpty())return;
    const auto previous=erasing?tool_:tabletToolBefore_;
    // A driver can change pointer type while the tip is still touching the tablet.
    if(state_==State::Erasing)commitErase();else if(drawing())commit();else cancelStroke();
    setTool(erasing?QStringLiteral("eraser"):previous);
    tabletToolBefore_=erasing?previous:QString{};
}
bool CanvasItem::tablet(QTabletEvent* e,QPointF local){
    if(!isVisible()||!isEnabled())return false;
    const bool inside=QRectF(0,0,width(),height()).contains(local)&&!toolbarExclusion_.contains(local)&&!optionsExclusion_.contains(local);
    const bool eraserPointer=e->pointerType()==QPointingDevice::PointerType::Eraser;
    const bool changedMode=handToolBefore_.isEmpty()&&(eraserPointer!=!tabletToolBefore_.isEmpty());
    const bool wasContact=tabletActive_;
    if(handToolBefore_.isEmpty()&&(!eraserPointer||inside))setTabletEraser(eraserPointer);
    const bool continuingContact=e->type()==QEvent::TabletMove||(e->type()==QEvent::TabletRelease&&e->buttons().testFlag(Qt::LeftButton));
    if(changedMode&&wasContact&&continuingContact&&e->pressure()>0&&inside){
        startPointer(InputManager::tablet(*e,world(local)),e->modifiers());tabletActive_=true;
    }
    angleSnap_=e->modifiers().testFlag(Qt::ControlModifier);eraserCtrl_=angleSnap_;if(tool_=="eraser")shiftHeld_=e->modifiers().testFlag(Qt::ShiftModifier);
    if(e->type()==QEvent::TabletPress){
        if(!inside||!isVisible()||!isEnabled())return false;
        // Side-button presses in hover must not draw or erase until tip contact.
        if(e->pressure()<=0){e->accept();return true;}
        forceActiveFocus();
        if(tabletActive_){movePointer(InputManager::tablet(*e,world(local)));e->accept();return true;}
        if(tool_=="hand"||space_){state_=State::Panning;lastPan_={local.x(),local.y()};updateCursor();}
        else startPointer(InputManager::tablet(*e,world(local)),e->modifiers());
        tabletActive_=true;
    } else if(!tabletActive_){
        // Some drivers report zero pressure at press, or coalesce the press into
        // a move. Recover tip contact without treating hover buttons as ink.
        if(e->type()==QEvent::TabletMove&&inside&&e->pressure()>0&&e->buttons().testFlag(Qt::LeftButton)){
            forceActiveFocus();
            if(tool_=="hand"||space_){state_=State::Panning;lastPan_={local.x(),local.y()};updateCursor();}
            else startPointer(InputManager::tablet(*e,world(local)),e->modifiers());
            tabletActive_=true;
        }else{if(tool_=="eraser"){eraserCursorVisible_=inside;if(inside)eraserWorldPosition_=world(local);emit selectionChanged();}return false;}
    }
    else if(e->type()==QEvent::TabletMove){
        if(state_==State::Panning){const Point p{local.x(),local.y()};view_.pan=view_.pan+p-lastPan_;lastPan_=p;viewUpdated();}
        else movePointer(InputManager::tablet(*e,world(local)));
    } else if(e->type()==QEvent::TabletRelease){
        if(e->pressure()>0&&e->buttons().testFlag(Qt::LeftButton)){
            movePointer(InputManager::tablet(*e,world(local)));e->accept();return true;
        }
        // Release pressure often drops to zero; retain the final nonzero sample.
        eraserCursorVisible_=false;emit selectionChanged();
        auto sample=InputManager::tablet(*e,world(local));if(!current_.samples.empty())sample.pressure=current_.samples.back().pressure;endPointer(sample);
        state_=State::Idle;tabletActive_=false;updateCursor();
    }
    e->accept();return true;
}
bool CanvasItem::eventFilter(QObject* object,QEvent* e){
    if(object==filteredWindow_){
        if(e->type()==QEvent::KeyPress||e->type()==QEvent::KeyRelease){
            auto* key=static_cast<QKeyEvent*>(e);
            if(key->key()==Qt::Key_H){
                if(e->type()==QEvent::KeyRelease&&!handToolBefore_.isEmpty()){
                    if(!key->isAutoRepeat())setTemporaryHand(false);key->accept();return true;
                }
                if(e->type()==QEvent::KeyPress&&isVisible()&&isEnabled()&&!textInputFocused()&&!(key->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))){
                    if(!key->isAutoRepeat())setTemporaryHand(true);key->accept();return true;
                }
            }
        }
        // Capture the trigger state before Qt Quick dismisses an outside popup.
        if(e->type()==QEvent::MouseButtonPress){const auto* mouse=static_cast<QMouseEvent*>(e);if(mouse->button()==Qt::LeftButton)emit windowPointerPressed(mouse->position());}
        if(e->type()==QEvent::MouseButtonRelease){const auto* mouse=static_cast<QMouseEvent*>(e);if(mouse->button()==Qt::LeftButton)emit windowPointerReleased();}
        if(e->type()==QEvent::TouchBegin){const auto* touch=static_cast<QTouchEvent*>(e);if(!touch->points().isEmpty())emit windowPointerPressed(touch->points().front().position());}
        if(e->type()==QEvent::TouchEnd||e->type()==QEvent::TouchCancel)emit windowPointerReleased();
        if(e->type()==QEvent::Wheel&&isVisible()&&wheelZoomEnabled_&&controller_&&controller_->active()&&!drawing()&&state_!=State::Erasing){
            auto* wheel=static_cast<QWheelEvent*>(e);const auto local=mapFromScene(wheel->position());
            if(wheel->modifiers().testFlag(Qt::ControlModifier)&&QRectF(0,0,width(),height()).contains(local)){
                zoomWheelAt(local,*wheel);wheel->accept();return true;
            }
        }
        if(e->type()==QEvent::TabletPress||e->type()==QEvent::TabletMove||e->type()==QEvent::TabletRelease){auto* t=static_cast<QTabletEvent*>(e);return tablet(t,mapFromScene(t->position()));}
        if(e->type()==QEvent::WindowDeactivate){space_=false;cancelStroke();setTemporaryHand(false);setTabletEraser(false);}
        if(e->type()==QEvent::NativeGesture && isVisible() && isEnabled() && !drawing()) {
            auto* gesture=static_cast<QNativeGestureEvent*>(e);
            const auto local=mapFromScene(gesture->position());
            if(gesture->gestureType()==Qt::ZoomNativeGesture && QRectF(0,0,width(),height()).contains(local)) {
                view_.zoomAt({local.x(),local.y()},std::exp(gesture->value()));viewUpdated();e->accept();return true;
            }
        }
    }
    return QQuickItem::eventFilter(object,e);
}
bool CanvasItem::event(QEvent* e){if(e->type()==QEvent::TabletPress||e->type()==QEvent::TabletMove||e->type()==QEvent::TabletRelease)return tablet(static_cast<QTabletEvent*>(e),static_cast<QTabletEvent*>(e)->position());return QQuickItem::event(e);}
void CanvasItem::mousePressEvent(QMouseEvent* e){
    if(e->button()==Qt::RightButton){
        if(tool_=="select"&&state_==State::Idle&&controller_&&controller_->page()){
            const auto point=world(e->position());
            if(!selectedCount()||!selectedBounds().contains(point)){
                const auto all=objects(*controller_->page());for(auto i=all.rbegin();i!=all.rend();++i)
                    if(hitTest(*i,point,12/(view_.pixelsPerMm*view_.zoom))&&hasVisibleInk(*i,point)){selection_.select(objectId(*i));selectionUpdated();break;}
            }
            if(selectedCount())emit selectionContextRequested(e->position());
        }e->accept();return;
    }
    if(tabletActive_||e->source()!=Qt::MouseEventNotSynthesized){e->accept();return;}
    forceActiveFocus();if(space_||tool_=="hand"||e->button()==Qt::MiddleButton){state_=State::Panning;lastPan_={e->position().x(),e->position().y()};updateCursor();}
    else startPointer(InputManager::mouse(*e,world(e->position())),e->modifiers());e->accept();
}
void CanvasItem::mouseMoveEvent(QMouseEvent* e){
    angleSnap_=e->modifiers().testFlag(Qt::ControlModifier);eraserCtrl_=angleSnap_;if(tool_=="eraser")shiftHeld_=e->modifiers().testFlag(Qt::ShiftModifier);
    if(tabletActive_){e->accept();return;}
    if(state_==State::Panning){const Point p{e->position().x(),e->position().y()};view_.pan=view_.pan+p-lastPan_;lastPan_=p;viewUpdated();}
    else movePointer(InputManager::mouse(*e,world(e->position())));e->accept();
}
void CanvasItem::mouseDoubleClickEvent(QMouseEvent* e){
    if(controller_&&controller_->page()&&(tool_=="select"||tool_=="text")){
        const auto point=world(e->position());
        for(auto i=controller_->page()->texts.rbegin();i!=controller_->page()->texts.rend();++i)if(i->properties.visible&&!i->properties.locked&&hitTest(CanvasObject(*i),point,0)){
            cancelStroke();emit textRequested({},QString::fromStdString(i->id));e->accept();return;
        }
    }
    // Qt replaces the second press in a rapid sequence with a double-click.
    // It must still begin ink, otherwise its moves and release have no stroke.
    mousePressEvent(e);
}
void CanvasItem::mouseReleaseEvent(QMouseEvent* e){if(e->button()==Qt::RightButton){e->accept();return;}angleSnap_=e->modifiers().testFlag(Qt::ControlModifier);eraserCtrl_=angleSnap_;if(tool_=="eraser")shiftHeld_=e->modifiers().testFlag(Qt::ShiftModifier);if(tabletActive_){e->accept();return;}eraserCursorVisible_=false;emit selectionChanged();endPointer(InputManager::mouse(*e,world(e->position())));state_=State::Idle;updateCursor();e->accept();}
void CanvasItem::hoverMoveEvent(QHoverEvent* e){
    if(tool_=="select"&&state_==State::Idle){
        bool resizing=false;for(const auto& value:selectionHandles()){const auto handle=value.toMap();
            if(QLineF(e->position(),QPointF(handle["x"].toDouble(),handle["y"].toDouble())).length()<12){resizing=handle["type"].toString()=="resize";break;}}
        const auto local=rotatePoint(world(e->position())-selectionFrame_.origin,{},-selectionFrame_.angle);
        if(resizing)setCursor(QCursor(selectionResizeCursor(selectionRotation())));
        else if(selectedCount()&&Bounds{0,0,selectionFrame_.width,selectionFrame_.height}.contains(local))setCursor(QCursor(Qt::SizeAllCursor));
        else updateCursor();
    }
    if(tool_=="eraser"&&state_==State::Idle){eraserCursorVisible_=contains(e->position());if(eraserCursorVisible_)eraserWorldPosition_=world(e->position());emit selectionChanged();}
    QQuickItem::hoverMoveEvent(e);
}
void CanvasItem::hoverLeaveEvent(QHoverEvent* e){eraserCursorVisible_=false;updateCursor();emit selectionChanged();QQuickItem::hoverLeaveEvent(e);}
void CanvasItem::mouseUngrabEvent(){
    if(tabletActive_)return;
    // A lost mouse grab must not discard ink already shown to the user.
    if(drawing()&&!current_.samples.empty())endPointer(current_.samples.back());
    else cancelStroke();
}
void CanvasItem::zoomWheelAt(QPointF point,const QWheelEvent& event){
    const double steps=event.angleDelta().y()?event.angleDelta().y()/120.:event.pixelDelta().y()/40.;
    view_.zoomAt({point.x(),point.y()},std::pow(1.05,steps));viewUpdated();
}
void CanvasItem::wheelEvent(QWheelEvent* e){
    if(!drawing()&&state_!=State::Erasing){
        if(e->modifiers().testFlag(Qt::ControlModifier)){
            if(wheelZoomEnabled_)zoomWheelAt(e->position(),*e);
        }else {
            // Positive wheel deltas move content down, as in a document viewer.
            const double pixels=!e->pixelDelta().isNull()?double(e->pixelDelta().y()):e->angleDelta().y()/120.*60.;
            if(pixels!=0){view_.pan.y+=pixels;viewUpdated();}
        }
    }e->accept();
}
void CanvasItem::touchEvent(QTouchEvent* e){
    if(tabletActive_){e->accept();return;}
    if(geometryGesture()&&state_!=State::DrawingCompass){
        if(e->type()==QEvent::TouchCancel)cancelStroke();
        else if(e->type()==QEvent::TouchEnd)finishGeometryPointer();
        else if(!e->points().isEmpty())moveGeometryPointer({world(e->points()[0].position()),1,0,0,0,0,0,DeviceType::Touch});
        e->accept();return;
    }
    if(tabletActive_||drawing()||state_==State::Erasing){e->accept();return;}
    if(e->type()==QEvent::TouchCancel||e->type()==QEvent::TouchEnd){state_=State::Idle;touchDistance_=0;updateCursor();e->accept();return;}
    const auto points=e->points();if(points.isEmpty())return;
    if(e->type()==QEvent::TouchBegin&&points.size()==1&&beginGeometryPointer({world(points[0].position()),1,0,0,0,0,0,DeviceType::Touch})){e->accept();return;}
    QPointF center=points[0].position();double distance=0;
    if(points.size()>1){center=(center+points[1].position())/2;distance=QLineF(points[0].position(),points[1].position()).length();}
    if(state_==State::TouchGesture){view_.pan=view_.pan+Point{center.x()-touchCenter_.x(),center.y()-touchCenter_.y()};if(distance>0&&touchDistance_>0)view_.zoomAt({center.x(),center.y()},distance/touchDistance_);viewUpdated();}
    state_=State::TouchGesture;touchCenter_=center;touchDistance_=distance;updateCursor();e->accept();
}
void CanvasItem::updateCursor(){
    const auto shape=state_==State::Panning||state_==State::TouchGesture?Qt::ClosedHandCursor:
        tool_=="hand"||space_?Qt::OpenHandCursor:
        state_==State::Resizing?selectionResizeCursor(editFrame_.angle*180/std::numbers::pi):
        state_==State::Moving?Qt::SizeAllCursor:
        tool_=="pen"||tool_=="marker"?Qt::CrossCursor:Qt::ArrowCursor;
    setCursor(QCursor(shape));
}
bool CanvasItem::textInputFocused() const {
    const auto* focus=window()?window()->activeFocusItem():nullptr;
    return focus&&(focus->inherits("QQuickTextInput")||focus->inherits("QQuickTextEdit"));
}
void CanvasItem::setTemporaryHand(bool enabled){
    if(enabled==!handToolBefore_.isEmpty())return;
    if(enabled){
        setTabletEraser(false);const auto previous=tool_;setTool("hand");handToolBefore_=previous;
    }else {const auto previous=handToolBefore_;setTool(previous);}
}
void CanvasItem::keyPressEvent(QKeyEvent* e){if(e->key()==Qt::Key_Shift){shiftHeld_=true;
    if(state_==State::ShapePreview&&previewShape_){previewShape_->style.pattern=LinePattern::Dashed;recognitionDashed_=true;emit drawingChanged();update();}e->accept();}
    else if(e->key()==Qt::Key_H&&!textInputFocused()&&!(e->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))){if(!e->isAutoRepeat())setTemporaryHand(true);e->accept();}else if(e->key()==Qt::Key_Control){eraserCtrl_=angleSnap_=true;if(state_==State::Rotating||state_==State::EditingHandle)updateSelection(lastPan_);else if(state_==State::CreatingShape){previewShape_=directShape(dragStart_,lastPan_);selectionUpdated();}else if(state_==State::ShapePreview)updateRecognizedLine(lastPan_);e->accept();}else if(e->key()==Qt::Key_Space){space_=true;updateCursor();e->accept();}else if(e->key()==Qt::Key_Escape){cancelStroke();e->accept();}else QQuickItem::keyPressEvent(e);}
void CanvasItem::keyReleaseEvent(QKeyEvent* e){if(e->key()==Qt::Key_Shift){shiftHeld_=false;e->accept();}else if(e->key()==Qt::Key_H&&!handToolBefore_.isEmpty()){if(!e->isAutoRepeat())setTemporaryHand(false);e->accept();}else if(e->key()==Qt::Key_Control){eraserCtrl_=angleSnap_=false;if(state_==State::Rotating||state_==State::EditingHandle)updateSelection(lastPan_);else if(state_==State::CreatingShape){previewShape_=directShape(dragStart_,lastPan_);selectionUpdated();}else if(state_==State::ShapePreview)updateRecognizedLine(lastPan_);e->accept();}else if(e->key()==Qt::Key_Space){space_=false;updateCursor();e->accept();}else QQuickItem::keyReleaseEvent(e);}
void CanvasItem::geometryChange(const QRectF& current,const QRectF& previous){
    QQuickItem::geometryChange(current,previous);
    if(current.size()==previous.size()||current.isEmpty())return;
    if(previous.isEmpty()){restorePageView();return;}
    // Keep the same world point at the viewport center without changing zoom.
    if(state_!=State::Idle)cancelStroke();
    view_.pan=view_.pan+Point{(current.width()-previous.width())/2,
                             (current.height()-previous.height())/2};
    viewUpdated();
}
}
