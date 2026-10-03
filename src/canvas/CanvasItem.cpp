#include "CanvasItem.h"
#include "rendering/StrokeMesh.h"
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>
#include <QSGTransformNode>
#include <QSGClipNode>
#include <QSGSimpleRectNode>
#include <QMatrix4x4>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QSet>
#include <QLineF>
#include <QNativeGestureEvent>
#include <unordered_map>
namespace scalar {
namespace {
QColor fromRgba(std::uint32_t c){return QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);}
struct SceneRoot : QSGTransformNode {
    QSGSimpleRectNode* paper=new QSGSimpleRectNode;
    QSGClipNode* clip=new QSGClipNode;
    std::unordered_map<std::string,QSGGeometryNode*> strokes;
    QSGGeometryNode* live=nullptr;
    SceneRoot(){appendChildNode(paper);appendChildNode(clip);clip->setIsRectangular(true);}
};
QSGGeometryNode* meshNode(const StrokeObject& s,QSGGeometryNode* node=nullptr) {
    if(!node){
        node=new QSGGeometryNode;
        auto* geometry=new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(geometry);node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGFlatColorMaterial);node->setFlag(QSGNode::OwnsMaterial);
    }
    const auto vertices=strokeMesh(s);auto* g=node->geometry();g->allocate(int(vertices.size()));
    auto* data=g->vertexDataAsPoint2D();for(std::size_t i=0;i<vertices.size();++i)data[i].set(float(vertices[i].x),float(vertices[i].y));
    static_cast<QSGFlatColorMaterial*>(node->material())->setColor(fromRgba(s.style.rgba));
    node->markDirty(QSGNode::DirtyGeometry|QSGNode::DirtyMaterial);return node;
}
}
CanvasItem::CanvasItem(QQuickItem* parent):QQuickItem(parent){
    setFlag(ItemHasContents,true);setFlag(ItemIsFocusScope,true);setAcceptedMouseButtons(Qt::LeftButton|Qt::MiddleButton);setAcceptTouchEvents(true);setClip(true);
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
    if(c)connect(c,&AppController::documentChanged,this,[this]{update();});emit controllerChanged();update();
}
void CanvasItem::setTool(const QString& t){if(t!="pen"&&t!="hand")return;cancelStroke();tool_=t;emit toolChanged();}
void CanvasItem::setPenColor(const QColor& c){if(!c.isValid())return;color_=c;style_.rgba=(std::uint32_t(c.red())<<24)|(std::uint32_t(c.green())<<16)|(std::uint32_t(c.blue())<<8)|255;emit penChanged();}
void CanvasItem::setPenWidth(double w){style_.maxWidthMm=std::clamp(w,0.2,5.0);style_.minWidthMm=std::min(0.15,style_.maxWidthMm);emit penChanged();}
void CanvasItem::setPressureGamma(double g){style_.gamma=std::clamp(g,0.3,3.0);emit penChanged();}
void CanvasItem::viewUpdated(){update();emit viewChanged();}
void CanvasItem::fitPage(){
    if(!controller_||!controller_->active()||width()<1||height()<1)return;
    cancelStroke();const double margin=40;
    view_.zoom=std::clamp(std::min((width()-margin*2)/(controller_->pageWidth()*view_.pixelsPerMm),(height()-margin*2)/(controller_->pageHeight()*view_.pixelsPerMm)),0.05,8.0);
    view_.pan={(width()-controller_->pageWidth()*view_.pixelsPerMm*view_.zoom)/2,(height()-controller_->pageHeight()*view_.pixelsPerMm*view_.zoom)/2};viewUpdated();
}
void CanvasItem::zoomBy(double f){if(drawing())return;view_.zoomAt({width()/2,height()/2},f);viewUpdated();}
void CanvasItem::begin(PointerSample s){
    if(!controller_||!controller_->active())return;
    if(s.position.x<0||s.position.y<0||s.position.x>controller_->pageWidth()||s.position.y>controller_->pageHeight())return;
    input_.reset();current_={newId(),style_,{input_.filter(s)}};state_=State::Drawing;emit drawingChanged();update();
}
void CanvasItem::append(PointerSample s){
    if(!drawing())return;s=input_.filter(s);
    if(current_.samples.empty()||length(s.position-current_.samples.back().position)>0.005)current_.samples.push_back(s);
    update();
}
void CanvasItem::commit(){if(!drawing())return;state_=State::Idle;if(controller_)controller_->addStroke(std::move(current_));current_={};emit drawingChanged();update();}
void CanvasItem::cancelStroke(){const bool was=drawing();state_=State::Idle;current_={};tabletActive_=false;touchDistance_=0;if(was)emit drawingChanged();update();}
bool CanvasItem::tablet(QTabletEvent* e,QPointF local){
    const bool inside=QRectF(0,0,width(),height()).contains(local)&&!toolbarExclusion_.contains(local)&&!optionsExclusion_.contains(local);
    if(e->type()==QEvent::TabletPress){
        if(!inside||!isVisible()||!isEnabled())return false;
        forceActiveFocus();tabletActive_=true;
        if(tool_=="hand"||space_){state_=State::Panning;lastPan_={local.x(),local.y()};}
        else begin(InputManager::tablet(*e,world(local)));
    } else if(!tabletActive_)return false;
    else if(e->type()==QEvent::TabletMove){
        if(state_==State::Panning){const Point p{local.x(),local.y()};view_.pan=view_.pan+p-lastPan_;lastPan_=p;viewUpdated();}
        else append(InputManager::tablet(*e,world(local)));
    } else if(e->type()==QEvent::TabletRelease){
        // Release pressure often drops to zero; retain the final nonzero sample.
        if(drawing()){auto sample=InputManager::tablet(*e,world(local));sample.pressure=current_.samples.back().pressure;append(sample);commit();}
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
    else begin(InputManager::mouse(*e,world(e->position())));e->accept();
}
void CanvasItem::mouseMoveEvent(QMouseEvent* e){
    if(tabletActive_){e->accept();return;}
    if(state_==State::Panning){const Point p{e->position().x(),e->position().y()};view_.pan=view_.pan+p-lastPan_;lastPan_=p;viewUpdated();}
    else append(InputManager::mouse(*e,world(e->position())));e->accept();
}
void CanvasItem::mouseReleaseEvent(QMouseEvent* e){if(tabletActive_){e->accept();return;}if(drawing()){append(InputManager::mouse(*e,world(e->position())));commit();}state_=State::Idle;e->accept();}
void CanvasItem::mouseUngrabEvent(){if(!tabletActive_)cancelStroke();}
void CanvasItem::wheelEvent(QWheelEvent* e){
    if(!drawing()){
        if(!e->pixelDelta().isNull()&&!(e->modifiers()&Qt::ControlModifier))view_.pan=view_.pan+Point{double(e->pixelDelta().x()),double(e->pixelDelta().y())};
        else {const double delta=e->angleDelta().y()?e->angleDelta().y():e->pixelDelta().y();view_.zoomAt({e->position().x(),e->position().y()},std::exp(delta/600.));}viewUpdated();
    }e->accept();
}
void CanvasItem::touchEvent(QTouchEvent* e){
    if(tabletActive_||drawing()){e->accept();return;}
    if(e->type()==QEvent::TouchCancel||e->type()==QEvent::TouchEnd){state_=State::Idle;touchDistance_=0;e->accept();return;}
    const auto points=e->points();if(points.isEmpty())return;
    QPointF center=points[0].position();double distance=0;
    if(points.size()>1){center=(center+points[1].position())/2;distance=QLineF(points[0].position(),points[1].position()).length();}
    if(state_==State::TouchGesture){view_.pan=view_.pan+Point{center.x()-touchCenter_.x(),center.y()-touchCenter_.y()};if(distance>0&&touchDistance_>0)view_.zoomAt({center.x(),center.y()},distance/touchDistance_);viewUpdated();}
    state_=State::TouchGesture;touchCenter_=center;touchDistance_=distance;e->accept();
}
void CanvasItem::keyPressEvent(QKeyEvent* e){if(e->key()==Qt::Key_Space){space_=true;e->accept();}else if(e->key()==Qt::Key_Escape){cancelStroke();e->accept();}else QQuickItem::keyPressEvent(e);}
void CanvasItem::keyReleaseEvent(QKeyEvent* e){if(e->key()==Qt::Key_Space){space_=false;e->accept();}else QQuickItem::keyReleaseEvent(e);}
void CanvasItem::geometryChange(const QRectF& a,const QRectF& b){QQuickItem::geometryChange(a,b);if(b.isEmpty()&&!a.isEmpty())fitPage();}
QSGNode* CanvasItem::updatePaintNode(QSGNode* old,UpdatePaintNodeData*){
    auto* root=static_cast<SceneRoot*>(old);if(!root)root=new SceneRoot;
    QMatrix4x4 matrix;matrix.translate(float(view_.pan.x),float(view_.pan.y));matrix.scale(float(view_.pixelsPerMm*view_.zoom));root->setMatrix(matrix);
    const Page* page=controller_?controller_->page():nullptr;
    root->paper->setRect(0,0,page?page->size.widthMm:0,page?page->size.heightMm:0);root->paper->setColor(page?fromRgba(page->background):QColor(Qt::transparent));
    root->clip->setClipRect(root->paper->rect());
    QSet<QString> present;
    if(page)for(const auto& stroke:page->strokes){present.insert(QString::fromStdString(stroke.id));if(!root->strokes.contains(stroke.id)){auto* node=meshNode(stroke);root->clip->appendChildNode(node);root->strokes.emplace(stroke.id,node);}}
    for(auto it=root->strokes.begin();it!=root->strokes.end();){if(!present.contains(QString::fromStdString(it->first))){root->clip->removeChildNode(it->second);delete it->second;it=root->strokes.erase(it);}else ++it;}
    if(root->live){root->clip->removeChildNode(root->live);}
    if(drawing()){root->live=meshNode(current_,root->live);root->clip->appendChildNode(root->live);}else{delete root->live;root->live=nullptr;}
    return root;
}
}
