#include "CanvasItem.h"
#include "tools/StrokeEraser.h"
#include <QtConcurrent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QLineF>
#include <QNativeGestureEvent>
#include <QHoverEvent>
namespace scalar {
CanvasItem::CanvasItem(QQuickItem* parent):QQuickItem(parent){
    setFlag(ItemHasContents,true);setFlag(ItemIsFocusScope,true);setAcceptedMouseButtons(Qt::LeftButton|Qt::MiddleButton);setAcceptTouchEvents(true);setAcceptHoverEvents(true);setClip(true);
    holdTimer_.setSingleShot(true);connect(&holdTimer_,&QTimer::timeout,this,&CanvasItem::beginRecognition);
    connect(&recognitionWatcher_,&QFutureWatcher<RecognitionResult>::finished,this,[this]{
        if(requestEpoch_!=inputEpoch_||!controller_)return;const auto result=recognitionWatcher_.result();if(!result.shape)return;
        if(state_==State::Drawing||state_==State::PossibleHold){previewShape_=*result.shape;if(recognitionDashed_||shiftHeld_)previewShape_->style.pattern=LinePattern::Dashed;state_=State::ShapePreview;emit drawingChanged();update();}
        else if(state_==State::Idle&&controller_->page()){
            if(auto before=findObject(*controller_->page(),current_.id);before&&std::holds_alternative<StrokeObject>(*before)&&properties(*before).revision==current_.properties.revision){controller_->changeObjects({{before,CanvasObject(*result.shape)}},CommandKind::ConvertStrokeToShape);selectionUpdated();}
        }
    });
    connect(this,&QQuickItem::enabledChanged,this,[this]{if(!isEnabled()){space_=false;cancelStroke();}});
    connect(this,&QQuickItem::visibleChanged,this,[this]{if(!isVisible()){space_=false;cancelStroke();}});
    connect(this,&QQuickItem::windowChanged,this,[this](QQuickWindow* w){
        if(filteredWindow_)filteredWindow_->removeEventFilter(this);
        filteredWindow_=w;if(w)w->installEventFilter(this);
    });
}
CanvasItem::~CanvasItem(){if(filteredWindow_)filteredWindow_->removeEventFilter(this);}
void CanvasItem::setController(AppController* c){
    if(controller_==c)return;if(controller_)disconnect(controller_,nullptr,this,nullptr);controller_=c;
    if(c)connect(c,&AppController::documentChanged,this,[this]{if(controller_->page())selection_.prune(*controller_->page());else selection_.clear();selectionUpdated();update();});if(c&&c->pageColor().lightness()<128)setPenColor(QColor("#f4f4f5"));emit controllerChanged();update();
}
void CanvasItem::setTool(const QString& t){if(t!="text"&&t!="eraser"&&t!="pen"&&t!="hand"&&t!="select"&&t!="line"&&t!="circle"&&t!="ellipse"&&t!="triangle"&&t!="rectangle")return;cancelStroke();tool_=t;emit toolChanged();}
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
void CanvasItem::viewUpdated(){if(controller_)controller_->refreshTextTextures(view_.pixelsPerMm*view_.zoom*(window()?window()->devicePixelRatio():1));update();emit viewChanged();emit selectionChanged();}
void CanvasItem::fitPage(){
    if(!controller_||!controller_->active()||width()<1||height()<1)return;
    cancelStroke();const double margin=40;
    view_.zoom=std::clamp(std::min((width()-margin*2)/(controller_->pageWidth()*view_.pixelsPerMm),(height()-margin*2)/(controller_->pageHeight()*view_.pixelsPerMm)),0.05,8.0);
    view_.pan={(width()-controller_->pageWidth()*view_.pixelsPerMm*view_.zoom)/2,(height()-controller_->pageHeight()*view_.pixelsPerMm*view_.zoom)/2};viewUpdated();
}
void CanvasItem::zoomBy(double f){if(drawing()||state_==State::Erasing)return;view_.zoomAt({width()/2,height()/2},f);viewUpdated();}
void CanvasItem::begin(PointerSample s){
    if(!controller_||!controller_->active())return;
    if(s.position.x<0||s.position.y<0||s.position.x>controller_->pageWidth()||s.position.y>controller_->pageHeight())return;
    selection_.clear();selectionUpdated();++inputEpoch_;holdTimer_.stop();previewShape_.reset();
    recognitionDashed_=false;input_.reset();current_={newId(),style_,{input_.filter(s)}};holdAnchor_=s.position;
    state_=State::Drawing;if(tool_=="pen"&&controller_->recognitionEnabled())holdTimer_.start(controller_->holdDelay());emit drawingChanged();update();
}
void CanvasItem::append(PointerSample s){
    if(!drawing())return;
    if(state_==State::ShapePreview){if(previewShape_&&previewShape_->kind==ShapeKind::Line){previewShape_->vertices[1]=s.position;update();}return;}
    s=input_.filter(s);
    if(current_.samples.empty()||length(s.position-current_.samples.back().position)>0.005)current_.samples.push_back(s);
    if(controller_&&controller_->recognitionEnabled()&&!current_.samples.empty()){
        if(length(s.position-holdAnchor_)>0.6){holdAnchor_=s.position;holdTimer_.start(controller_->holdDelay());state_=State::PossibleHold;++inputEpoch_;}
        else if(!holdTimer_.isActive()&&state_!=State::PossibleHold){holdTimer_.start(controller_->holdDelay());state_=State::PossibleHold;}
    }
    update();
}
void CanvasItem::beginRecognition(){
    if(!controller_||!controller_->recognitionEnabled()||(state_!=State::Drawing&&state_!=State::PossibleHold)||current_.samples.empty())return;
    if(recognitionWatcher_.isRunning()){holdTimer_.start(100);return;}
    recognitionDashed_=shiftHeld_;requestEpoch_=inputEpoch_;const auto snapshot=current_;
    recognitionWatcher_.setFuture(QtConcurrent::run([snapshot]{return recognizeShape(snapshot);}));
}
void CanvasItem::commit(){
    if(!drawing())return;holdTimer_.stop();++inputEpoch_;
    if(controller_){if(previewShape_&&state_==State::ShapePreview)controller_->addRecognizedStroke(current_,*previewShape_);else if(previewShape_&&state_==State::CreatingShape)controller_->addShape(*previewShape_);else controller_->addStroke(current_);}
    state_=State::Idle;current_={};previewShape_.reset();emit drawingChanged();update();
}
void CanvasItem::cancelStroke(){
    const bool was=drawing();holdTimer_.stop();++inputEpoch_;state_=State::Idle;current_={};previewShape_.reset();editBefore_.clear();editPreview_.clear();eraseBefore_.clear();erasePreview_.clear();tabletActive_=false;touchDistance_=0;
    if(was)emit drawingChanged();selectionUpdated();update();
}
ShapeObject CanvasItem::directShape(Point a,Point b) const{
    ShapeObject s;s.id=current_.id;s.style=style_;s.style.pattern=manualShapePattern_;
    const auto center=(a+b)*0.5;const double w=std::max(0.2,std::abs(b.x-a.x)),h=std::max(0.2,std::abs(b.y-a.y));
    if(tool_=="line"){s.kind=ShapeKind::Line;s.vertices={a,b};s.fillOpacity=0;}
    else if(tool_=="circle"){s.kind=ShapeKind::Circle;s.center=center;s.radiusX=s.radiusY=std::max(0.2,length(b-a)/2);}
    else if(tool_=="ellipse"){s.kind=ShapeKind::Ellipse;s.center=center;s.radiusX=w/2;s.radiusY=h/2;}
    else if(tool_=="triangle"){s.kind=ShapeKind::Triangle;s.vertices={{center.x,std::min(a.y,b.y)},{std::max(a.x,b.x),std::max(a.y,b.y)},{std::min(a.x,b.x),std::max(a.y,b.y)}};}
    else {s.kind=ShapeKind::Rectangle;s.vertices={{std::min(a.x,b.x),std::min(a.y,b.y)},{std::max(a.x,b.x),std::min(a.y,b.y)},{std::max(a.x,b.x),std::max(a.y,b.y)},{std::min(a.x,b.x),std::max(a.y,b.y)}};}
    return s;
}
void CanvasItem::startPointer(PointerSample s,Qt::KeyboardModifiers modifiers){
    shiftHeld_=modifiers.testFlag(Qt::ShiftModifier);
    if(tool_=="text"){emit textRequested(QPointF(s.position.x,s.position.y),{});return;}
    if(tool_=="eraser"){
        if(!controller_||!controller_->page())return;cancelStroke();selection_.clear();state_=State::Erasing;lastPan_=s.position;eraseBefore_.clear();erasePreview_.clear();eraseAt(s.position);return;
    }
    if(tool_=="select"){beginSelection(s.position,modifiers);return;}
    begin(s);if(state_==State::Idle)return;
    if(tool_!="pen"){dragStart_=s.position;state_=State::CreatingShape;previewShape_=directShape(dragStart_,dragStart_);}
}
void CanvasItem::movePointer(PointerSample s){
    if(state_==State::Erasing){eraseAt(s.position);return;}
    if(state_==State::CreatingShape){previewShape_=directShape(dragStart_,s.position);update();return;}
    if(state_==State::Moving||state_==State::Resizing||state_==State::Rotating||state_==State::EditingHandle||state_==State::Marquee){updateSelection(s.position);return;}
    append(s);
}
void CanvasItem::endPointer(PointerSample s){
    if(state_==State::Erasing){eraseAt(s.position);commitErase();}
    else if(drawing()){movePointer(s);commit();}
    else if(state_==State::Marquee){updateSelection(s.position);selection_.marquee(*controller_->page(),{std::min(dragStart_.x,s.position.x),std::min(dragStart_.y,s.position.y),std::max(dragStart_.x,s.position.x),std::max(dragStart_.y,s.position.y)},marqueeAdditive_);state_=State::Idle;selectionUpdated();}
    else if(state_==State::Moving||state_==State::Resizing||state_==State::Rotating||state_==State::EditingHandle){updateSelection(s.position);commitSelection();}
}
void CanvasItem::eraseAt(Point point){
    if(!controller_||!controller_->page())return;
    const auto eraseVisible=[&](const StrokeObject& stroke){
        std::vector<std::array<Point,4>> shields;
        for(const auto& image:controller_->page()->images)
            if(image.properties.visible&&image.properties.zIndex>stroke.properties.zIndex&&image.corners.size()==4){
                std::array<Point,4> corners;std::copy(image.corners.begin(),image.corners.end(),corners.begin());shields.push_back(corners);
            }
        return eraseStroke(stroke,lastPan_,point,eraserRadius_,shields);
    };
    std::vector<StrokeObject> candidates=erasePreview_;
    for(const auto& s:controller_->page()->strokes){if(s.properties.locked||!s.properties.visible)continue;
        if(std::none_of(eraseBefore_.begin(),eraseBefore_.end(),[&](const auto& b){return b.id==s.id;})){
            const auto fragments=eraseVisible(s);
            if(fragments.size()!=1||fragments[0].id!=s.id){eraseBefore_.push_back(s);candidates.insert(candidates.end(),fragments.begin(),fragments.end());}
        }
    }
    erasePreview_.clear();for(const auto& stroke:candidates){auto fragments=eraseVisible(stroke);erasePreview_.insert(erasePreview_.end(),fragments.begin(),fragments.end());}
    lastPan_=point;selectionUpdated();
}
void CanvasItem::commitErase(){
    std::vector<ObjectChange> changes;for(const auto& s:eraseBefore_)changes.push_back({CanvasObject(s),{}});for(const auto& s:erasePreview_)changes.push_back({{},CanvasObject(s)});
    state_=State::Idle;eraseBefore_.clear();erasePreview_.clear();if(controller_)controller_->changeObjects(std::move(changes),CommandKind::DeleteObject);selectionUpdated();
}
bool CanvasItem::tablet(QTabletEvent* e,QPointF local){
    const bool inside=QRectF(0,0,width(),height()).contains(local)&&!toolbarExclusion_.contains(local)&&!optionsExclusion_.contains(local);
    if(e->type()==QEvent::TabletPress){
        if(!inside||!isVisible()||!isEnabled())return false;
        forceActiveFocus();
        if(tool_=="hand"||space_){state_=State::Panning;lastPan_={local.x(),local.y()};}
        else startPointer(InputManager::tablet(*e,world(local)),e->modifiers());
        tabletActive_=true;
    } else if(!tabletActive_){if(tool_=="eraser"&&inside){lastPan_=world(local);emit selectionChanged();}return false;}
    else if(e->type()==QEvent::TabletMove){
        if(state_==State::Panning){const Point p{local.x(),local.y()};view_.pan=view_.pan+p-lastPan_;lastPan_=p;viewUpdated();}
        else movePointer(InputManager::tablet(*e,world(local)));
    } else if(e->type()==QEvent::TabletRelease){
        // Release pressure often drops to zero; retain the final nonzero sample.
        auto sample=InputManager::tablet(*e,world(local));if(!current_.samples.empty())sample.pressure=current_.samples.back().pressure;endPointer(sample);
        state_=State::Idle;tabletActive_=false;
    }
    e->accept();return true;
}
bool CanvasItem::eventFilter(QObject* object,QEvent* e){
    if(object==filteredWindow_){
        if(e->type()==QEvent::TabletPress||e->type()==QEvent::TabletMove||e->type()==QEvent::TabletRelease){auto* t=static_cast<QTabletEvent*>(e);return tablet(t,mapFromScene(t->position()));}
        if(e->type()==QEvent::WindowDeactivate){space_=false;cancelStroke();}
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
    if(tabletActive_||e->source()!=Qt::MouseEventNotSynthesized){e->accept();return;}
    forceActiveFocus();if(space_||tool_=="hand"||e->button()==Qt::MiddleButton){state_=State::Panning;lastPan_={e->position().x(),e->position().y()};}
    else startPointer(InputManager::mouse(*e,world(e->position())),e->modifiers());e->accept();
}
void CanvasItem::mouseMoveEvent(QMouseEvent* e){
    if(tabletActive_){e->accept();return;}
    if(state_==State::Panning){const Point p{e->position().x(),e->position().y()};view_.pan=view_.pan+p-lastPan_;lastPan_=p;viewUpdated();}
    else movePointer(InputManager::mouse(*e,world(e->position())));e->accept();
}
void CanvasItem::mouseReleaseEvent(QMouseEvent* e){if(tabletActive_){e->accept();return;}endPointer(InputManager::mouse(*e,world(e->position())));state_=State::Idle;e->accept();}
void CanvasItem::hoverMoveEvent(QHoverEvent* e){if(tool_=="eraser"&&state_==State::Idle){lastPan_=world(e->position());emit selectionChanged();}QQuickItem::hoverMoveEvent(e);}
void CanvasItem::mouseUngrabEvent(){if(!tabletActive_)cancelStroke();}
void CanvasItem::wheelEvent(QWheelEvent* e){
    if(!drawing()&&state_!=State::Erasing){
        if(!e->pixelDelta().isNull()&&!(e->modifiers()&Qt::ControlModifier))view_.pan=view_.pan+Point{double(e->pixelDelta().x()),double(e->pixelDelta().y())};
        else {const double delta=e->angleDelta().y()?e->angleDelta().y():e->pixelDelta().y();view_.zoomAt({e->position().x(),e->position().y()},std::exp(delta/600.));}viewUpdated();
    }e->accept();
}
void CanvasItem::touchEvent(QTouchEvent* e){
    if(tabletActive_||drawing()||state_==State::Erasing){e->accept();return;}
    if(e->type()==QEvent::TouchCancel||e->type()==QEvent::TouchEnd){state_=State::Idle;touchDistance_=0;e->accept();return;}
    const auto points=e->points();if(points.isEmpty())return;
    QPointF center=points[0].position();double distance=0;
    if(points.size()>1){center=(center+points[1].position())/2;distance=QLineF(points[0].position(),points[1].position()).length();}
    if(state_==State::TouchGesture){view_.pan=view_.pan+Point{center.x()-touchCenter_.x(),center.y()-touchCenter_.y()};if(distance>0&&touchDistance_>0)view_.zoomAt({center.x(),center.y()},distance/touchDistance_);viewUpdated();}
    state_=State::TouchGesture;touchCenter_=center;touchDistance_=distance;e->accept();
}
void CanvasItem::keyPressEvent(QKeyEvent* e){if(e->key()==Qt::Key_Shift){shiftHeld_=true;
    if(state_==State::ShapePreview&&previewShape_){previewShape_->style.pattern=LinePattern::Dashed;recognitionDashed_=true;emit drawingChanged();update();}e->accept();}
    else if(e->key()==Qt::Key_Space){space_=true;e->accept();}else if(e->key()==Qt::Key_Escape){cancelStroke();e->accept();}else QQuickItem::keyPressEvent(e);}
void CanvasItem::keyReleaseEvent(QKeyEvent* e){if(e->key()==Qt::Key_Shift){shiftHeld_=false;e->accept();}else if(e->key()==Qt::Key_Space){space_=false;e->accept();}else QQuickItem::keyReleaseEvent(e);}
void CanvasItem::geometryChange(const QRectF& current,const QRectF& previous){
    QQuickItem::geometryChange(current,previous);
    if(current.size()==previous.size()||current.isEmpty())return;
    if(previous.isEmpty()){fitPage();return;}
    // Keep the same world point at the viewport center without changing zoom.
    if(state_!=State::Idle)cancelStroke();
    view_.pan=view_.pan+Point{(current.width()-previous.width())/2,
                             (current.height()-previous.height())/2};
    viewUpdated();
}
}
