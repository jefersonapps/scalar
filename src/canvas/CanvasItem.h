#pragma once
#include "app/AppController.h"
#include "input/InputManager.h"
#include "selection/SelectionModel.h"
#include "recognition/ShapeRecognizer.h"
#include "tools/GeometryTools.h"
#include "tools/EraserIndex.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include "rendering/ShapeRasterCache.h"
#include "rendering/TextRenderer.h"
namespace scalar {
class CanvasItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(AppController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(QString editingTextId READ editingTextId WRITE setEditingTextId NOTIFY selectionChanged)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY penChanged)
    Q_PROPERTY(double penWidth READ penWidth WRITE setPenWidth NOTIFY penChanged)
    Q_PROPERTY(double markerWidth READ markerWidth WRITE setMarkerWidth NOTIFY penChanged)
    Q_PROPERTY(double markerOpacity READ markerOpacity WRITE setMarkerOpacity NOTIFY penChanged)
    Q_PROPERTY(double pressureGamma READ pressureGamma WRITE setPressureGamma NOTIFY penChanged)
    Q_PROPERTY(QString penLineStyle READ penLineStyle WRITE setPenLineStyle NOTIFY penChanged)
    Q_PROPERTY(double dashLength READ dashLength WRITE setDashLength NOTIFY penChanged)
    Q_PROPERTY(double gapLength READ gapLength WRITE setGapLength NOTIFY penChanged)
    Q_PROPERTY(double dotSpacing READ dotSpacing WRITE setDotSpacing NOTIFY penChanged)
    Q_PROPERTY(QString shapeLineStyle READ shapeLineStyle WRITE setShapeLineStyle NOTIFY penChanged)
    Q_PROPERTY(bool wheelZoomEnabled MEMBER wheelZoomEnabled_)
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(bool drawing READ drawing NOTIFY drawingChanged)
    Q_PROPERTY(double eraserRadius READ eraserRadius WRITE setEraserRadius NOTIFY penChanged)
    Q_PROPERTY(bool eraserShapes READ eraserShapes WRITE setEraserShapes NOTIFY penChanged)
    Q_PROPERTY(QPointF eraserPosition READ eraserPosition NOTIFY selectionChanged)
    Q_PROPERTY(bool eraserVisible READ eraserVisible NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap segmentGuide READ segmentGuide NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedShowAngle READ selectedShowAngle WRITE setSelectedShowAngle NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList sectorAngles READ sectorAngles NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedGuide READ selectedGuide NOTIFY geometryToolsChanged)
    Q_PROPERTY(QVariantList selectionHandles READ selectionHandles NOTIFY selectionChanged)
    Q_PROPERTY(QRectF selectionRect READ selectionRect NOTIFY selectionChanged)
    Q_PROPERTY(QRectF selectionFrame READ selectionFrame NOTIFY selectionChanged)
    Q_PROPERTY(double selectionRotation READ selectionRotation NOTIFY selectionChanged)
    Q_PROPERTY(QString selectionName READ selectionName NOTIFY selectionChanged)
    Q_PROPERTY(double selectedWidth READ selectedWidth NOTIFY selectionChanged)
    Q_PROPERTY(int selectedPattern READ selectedPattern NOTIFY selectionChanged)
    Q_PROPERTY(double selectedDashLength READ selectedDashLength WRITE setSelectedDashLength NOTIFY selectionChanged)
    Q_PROPERTY(double selectedGapLength READ selectedGapLength WRITE setSelectedGapLength NOTIFY selectionChanged)
    Q_PROPERTY(double selectedDotSpacing READ selectedDotSpacing WRITE setSelectedDotSpacing NOTIFY selectionChanged)
    Q_PROPERTY(QColor selectedFillColor READ selectedFillColor NOTIFY selectionChanged)
    Q_PROPERTY(QColor selectedBorderColor READ selectedBorderColor NOTIFY selectionChanged)
    Q_PROPERTY(double selectedFill READ selectedFill NOTIFY selectionChanged)
    Q_PROPERTY(QString interactionHint READ interactionHint NOTIFY drawingChanged)
    Q_PROPERTY(QRectF toolbarExclusion MEMBER toolbarExclusion_)
    Q_PROPERTY(QRectF optionsExclusion MEMBER optionsExclusion_)
    Q_PROPERTY(QVariantMap rulerGeometry READ rulerGeometry NOTIFY geometryToolsChanged)
    Q_PROPERTY(QVariantMap compassGeometry READ compassGeometry NOTIFY geometryToolsChanged)
    Q_PROPERTY(bool rulerVisible READ rulerVisible WRITE setRulerVisible NOTIFY geometryToolsChanged)
    Q_PROPERTY(bool compassVisible READ compassVisible WRITE setCompassVisible NOTIFY geometryToolsChanged)
    Q_PROPERTY(bool rulerSnap READ rulerSnap WRITE setRulerSnap NOTIFY geometryToolsChanged)
    Q_PROPERTY(double rulerSnapDistance READ rulerSnapDistance WRITE setRulerSnapDistance NOTIFY geometryToolsChanged)
    Q_PROPERTY(double rulerLength READ rulerLength WRITE setRulerLength NOTIFY geometryToolsChanged)
    Q_PROPERTY(double rulerAngle READ rulerAngle WRITE setRulerAngle NOTIFY geometryToolsChanged)
    Q_PROPERTY(double compassRadius READ compassRadius WRITE setCompassRadius NOTIFY geometryToolsChanged)
public:
    explicit CanvasItem(QQuickItem* parent=nullptr);
    ~CanvasItem() override;
    AppController* controller() const {return controller_;}
    void setController(AppController* controller);
    QString tool() const {return tool_;}
    QString editingTextId() const {return editingTextId_;}
    void setEditingTextId(const QString& id){editingTextId_=id;selectionUpdated();}
    Q_INVOKABLE QPointF screenPoint(QPointF p) const {const auto s=view_.worldToScreen({p.x(),p.y()});return {s.x,s.y};}
    Q_INVOKABLE void selectText(const QString& id){selection_.select(id.toStdString());selectionUpdated();}
    void setTool(const QString& tool);
    double markerWidth() const {return markerWidth_;}
    double markerOpacity() const {return markerOpacity_;}
    void setMarkerOpacity(double value){if(std::isfinite(value)){markerOpacity_=std::clamp(value,.01,1.);emit penChanged();}}
    void setMarkerWidth(double value){if(std::isfinite(value)){markerWidth_=std::clamp(value,1.,20.);emit penChanged();}}
    QColor penColor() const {return color_;}
    void setPenColor(const QColor& color);
    double penWidth() const {return style_.maxWidthMm;}
    void setPenWidth(double width);
    double pressureGamma() const {return style_.gamma;}
    void setPressureGamma(double gamma);
    QString penLineStyle() const;
    void setPenLineStyle(const QString& value);
    double dashLength() const {return style_.dashLengthMm;}
    double gapLength() const {return style_.gapLengthMm;}
    double dotSpacing() const {return style_.dotSpacingMm;}
    void setDashLength(double value){style_.dashLengthMm=std::clamp(value,0.1,100.);emit penChanged();}
    void setGapLength(double value){style_.gapLengthMm=std::clamp(value,0.1,100.);emit penChanged();}
    void setDotSpacing(double value){style_.dotSpacingMm=std::clamp(value,0.1,100.);emit penChanged();}
    QString shapeLineStyle() const;
    void setShapeLineStyle(const QString& style);
    double zoom() const {return view_.zoom;}
    bool drawing() const {return state_==State::Drawing||state_==State::PossibleHold||state_==State::ShapePreview||state_==State::CreatingShape||state_==State::DrawingCompass;}
    double eraserRadius() const {return eraserRadius_;}
    bool eraserShapes() const {return eraserShapes_;}
    void setEraserShapes(bool value){eraserShapes_=value;emit penChanged();}
    void setEraserRadius(double radius){
        if(!std::isfinite(radius))return;
        const double bounded=std::clamp(radius,0.5,12.);
        if(eraserRadius_==bounded)return;
        eraserRadius_=bounded;emit penChanged();update();
    }
    Q_INVOKABLE void adjustEraserSize(bool increase){setEraserRadius(eraserRadius_*(increase?1.05:1./1.05));}
    QPointF eraserPosition() const {const auto p=view_.worldToScreen(eraserWorldPosition_);return {p.x,p.y};}
    bool eraserVisible() const {return tool_=="eraser"&&eraserCursorVisible_;}
    QVariantMap segmentGuide() const;
    int selectedCount() const {return int(selection_.ids().size());}
    bool selectedShowAngle() const;
    void setSelectedShowAngle(bool visible);
    QVariantList sectorAngles() const;
    QString selectedGuide() const {return selectedGuide_;}
    QVariantList selectionHandles() const;
    QRectF selectionRect() const;
    QRectF selectionFrame() const;
    double selectionRotation() const;
    QString selectionName() const;
    double selectedFill() const;
    QColor selectedFillColor() const;
    QColor selectedBorderColor() const;
    double selectedWidth() const;
    int selectedPattern() const;
    double selectedPatternSpacing(const QString& field) const;
    void setSelectedPatternSpacing(const QString& field,double value);
    double selectedDashLength() const{return selectedPatternSpacing("dashLength");}
    double selectedGapLength() const{return selectedPatternSpacing("gapLength");}
    double selectedDotSpacing() const{return selectedPatternSpacing("dotSpacing");}
    void setSelectedDashLength(double v){setSelectedPatternSpacing("dashLength",v);}
    void setSelectedGapLength(double v){setSelectedPatternSpacing("gapLength",v);}
    void setSelectedDotSpacing(double v){setSelectedPatternSpacing("dotSpacing",v);}
    Q_INVOKABLE void setSelectedPattern(int pattern);
    QString interactionHint() const;
    Q_INVOKABLE void editSelectedText();
    Q_INVOKABLE QString selectedTextId() const;
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void duplicateSelection();
    Q_INVOKABLE void exportSelection(const QUrl& url,bool svg=false){if(controller_)controller_->exportSelection(selectedObjects(),url,svg);}
    Q_INVOKABLE void moveSelectionLayer(bool forward);
    Q_INVOKABLE void recognizeSelection();
    Q_INVOKABLE void setSelectedFill(double opacity);
    Q_INVOKABLE void setSelectedFillColor(const QColor& color);
    Q_INVOKABLE void setSelectedColor(const QColor& color);
    Q_INVOKABLE void setSelectedWidth(double width);
    Q_INVOKABLE void fitPage();
    Q_INVOKABLE QPointF viewportCenter() const {const auto p=view_.screenToWorld({width()/2,height()/2});return {p.x,p.y};}
    Q_INVOKABLE void importImage(const QUrl& url){if(controller_)controller_->importImage(url,viewportCenter());}
    Q_INVOKABLE void importImages(const QVariantList& urls){if(controller_)controller_->importImages(urls,viewportCenter());}
    Q_INVOKABLE void pasteImage(){if(controller_)controller_->pasteImage(viewportCenter());}
    Q_INVOKABLE void zoomBy(double factor);
    Q_INVOKABLE void restorePageView();
    Q_INVOKABLE bool setZoom(double value);
    Q_INVOKABLE void cancelStroke();
    QVariantMap rulerGeometry() const;
    QVariantMap compassGeometry() const;
    bool rulerVisible() const {return rulerVisible_;}
    bool compassVisible() const {return compassVisible_;}
    bool rulerSnap() const {return rulerSnap_;}
    double rulerSnapDistance() const {return rulerSnapDistance_;}
    double rulerLength() const {return ruler_.lengthMm;}
    double rulerAngle() const {return ruler_.angle*180/std::numbers::pi;}
    double compassRadius() const {return compass_.radiusMm;}
    void setRulerVisible(bool value);
    void setCompassVisible(bool value);
    void setRulerSnap(bool value);
    void setRulerSnapDistance(double value);
    void setRulerLength(double value);
    void setRulerAngle(double value);
    void setCompassRadius(double value);
    Q_INVOKABLE void centerGeometryTools();
    Q_INVOKABLE void drawCompassCircle();
    Q_INVOKABLE bool constructFromSelection(const QString& kind);
signals:
    void windowPointerPressed(QPointF position);
    void windowPointerReleased();
    void controllerChanged();
    void toolChanged();
    void penChanged();
    void viewChanged();
    void drawingChanged();
    void selectionChanged();
    void selectionContextRequested(QPointF position);
    void textRequested(QPointF position,QString id);
    void textBoxRequested(QRectF box);
    void geometryToolsChanged();
protected:
    QSGNode* updatePaintNode(QSGNode*,UpdatePaintNodeData*) override;
    bool event(QEvent*) override;
    bool eventFilter(QObject*,QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseUngrabEvent() override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void wheelEvent(QWheelEvent*) override;
    void touchEvent(QTouchEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void geometryChange(const QRectF&,const QRectF&) override;
private:
    bool hasVisibleInk(const CanvasObject& object,std::optional<Point> near={}) const;
    mutable std::map<std::string,ShapeRasterCache> visibilityCaches_;
    bool wheelZoomEnabled_=true;
    void zoomWheelAt(QPointF point,const QWheelEvent& event);
    double markerWidth_=4;
    double markerOpacity_=.10;
    enum class State { Idle,Erasing,Drawing,PossibleHold,ShapePreview,CreatingShape,CreatingText,Panning,TouchGesture,Marquee,Moving,Resizing,Rotating,EditingHandle,MovingRuler,RotatingRuler,ResizingRuler,MovingCompass,ResizingCompass,DrawingCompass };
    bool beginGeometryPointer(PointerSample sample);
    bool moveGeometryPointer(PointerSample sample);
    bool geometryGesture() const;
    void finishGeometryPointer();
    void resetGeometryTools();
    ShapeObject compassCircle() const;
    Point world(QPointF p) const {return view_.screenToWorld({p.x(),p.y()});}
    void begin(PointerSample sample);
    void append(PointerSample sample);
    void commit();
    void startPointer(PointerSample sample,Qt::KeyboardModifiers modifiers);
    void movePointer(PointerSample sample);
    void endPointer(PointerSample sample);
    void beginRecognition();
    void eraseAt(Point point);
    void commitErase();
    void beginSelection(Point point,Qt::KeyboardModifiers modifiers);
    void updateSelection(Point point);
    void commitSelection();
    Bounds selectedBounds() const;
    std::vector<CanvasObject> selectedObjects() const;
    void selectionUpdated();
    ShapeObject directShape(Point start,Point end) const;
    void updateRecognizedLine(Point end);
    bool tablet(QTabletEvent* event,QPointF local);
    void setTabletEraser(bool erasing);
    void setTemporaryHand(bool enabled);
    void updateCursor();
    bool textInputFocused() const;
    void viewUpdated(bool interactive=true);
    QPointer<AppController> controller_;
    QPointer<QQuickWindow> filteredWindow_;
    QMetaObject::Connection handFocusConnection_;
    InputManager input_;
    ViewTransform view_;
    QString viewPageId_;
    std::vector<PointerSample> freehandSamples_;
    std::size_t liveCurveStableSamples_=1;
    bool smoothFreehand_=false;
    StrokeObject current_;
    std::optional<ShapeObject> previewShape_;
    QTimer holdTimer_;
    QTimer viewRefinementTimer_;
    QFutureWatcher<RecognitionResult> recognitionWatcher_;
    std::uint64_t inputEpoch_=0,requestEpoch_=0;
    Point holdAnchor_;
    SelectionModel selection_;
    std::vector<CanvasObject> editBefore_,editPreview_;
    Point dragStart_;
    Bounds editBounds_;
    struct SelectionFrame {Point origin;double width=0,height=0,angle=0;};
    SelectionFrame selectionFrame_,editFrame_;
    std::vector<std::string> frameIds_;
    double frameAngleOffset_=0;
    QSizeF editTextSize_;
    std::optional<TextVisual> textResizeVisual_;
    int handleIndex_=-1;
    bool marqueeAdditive_=false;
    double eraserRadius_=3;
    Point eraserWorldPosition_;
    bool eraserCursorVisible_=false,angleSnap_=false;
    bool eraserShapes_=false,eraserCtrl_=false;
    std::vector<StrokeObject> eraseBefore_,erasePreview_;
    EraserIndex eraseIndex_,restoreIndex_;
    std::vector<StrokeObject> recoverableInk_;
    std::vector<ShapeObject> eraseShapesBefore_,eraseShapesPreview_;
    std::vector<ImageObject> eraseFillsBefore_,eraseFillsPreview_;
    PenStyle style_;
    QColor color_{"#263345"};
    QString tool_="pen";
    QString editingTextId_;
    LinePattern manualShapePattern_=LinePattern::Solid;
    bool shiftHeld_=false,recognitionDashed_=false;
    State state_=State::Idle;
    Point lastPan_;
    double touchDistance_=0;
    QPointF touchCenter_;
    bool space_=false,tabletActive_=false;
    QString tabletToolBefore_;
    QString handToolBefore_;
    QRectF toolbarExclusion_,optionsExclusion_;
    RulerGeometry ruler_;
    CompassGeometry compass_;
    CompassSweep compassSweep_;
    std::optional<RulerGeometry> rulerBefore_;
    std::optional<CompassGeometry> compassBefore_;
    std::optional<int> guidedEdge_;
    QString selectedGuide_;
    bool rulerVisible_=false,compassVisible_=false,rulerSnap_=true,compassComplete_=false;
    double rulerSnapDistance_=3;
};
}
