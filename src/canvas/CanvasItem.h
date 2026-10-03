#pragma once
#include "app/AppController.h"
#include "input/InputManager.h"
#include "selection/SelectionModel.h"
#include "recognition/ShapeRecognizer.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
namespace scalar {
class CanvasItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(AppController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY penChanged)
    Q_PROPERTY(double penWidth READ penWidth WRITE setPenWidth NOTIFY penChanged)
    Q_PROPERTY(double pressureGamma READ pressureGamma WRITE setPressureGamma NOTIFY penChanged)
    Q_PROPERTY(QString penLineStyle READ penLineStyle WRITE setPenLineStyle NOTIFY penChanged)
    Q_PROPERTY(double dashLength READ dashLength WRITE setDashLength NOTIFY penChanged)
    Q_PROPERTY(double gapLength READ gapLength WRITE setGapLength NOTIFY penChanged)
    Q_PROPERTY(double dotSpacing READ dotSpacing WRITE setDotSpacing NOTIFY penChanged)
    Q_PROPERTY(QString shapeLineStyle READ shapeLineStyle WRITE setShapeLineStyle NOTIFY penChanged)
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(bool drawing READ drawing NOTIFY drawingChanged)
    Q_PROPERTY(double eraserRadius READ eraserRadius WRITE setEraserRadius NOTIFY penChanged)
    Q_PROPERTY(QPointF eraserPosition READ eraserPosition NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectionHandles READ selectionHandles NOTIFY selectionChanged)
    Q_PROPERTY(QRectF selectionRect READ selectionRect NOTIFY selectionChanged)
    Q_PROPERTY(QString selectionName READ selectionName NOTIFY selectionChanged)
    Q_PROPERTY(double selectedWidth READ selectedWidth NOTIFY selectionChanged)
    Q_PROPERTY(QColor selectedFillColor READ selectedFillColor NOTIFY selectionChanged)
    Q_PROPERTY(QColor selectedBorderColor READ selectedBorderColor NOTIFY selectionChanged)
    Q_PROPERTY(double selectedFill READ selectedFill NOTIFY selectionChanged)
    Q_PROPERTY(QString interactionHint READ interactionHint NOTIFY drawingChanged)
    Q_PROPERTY(QRectF toolbarExclusion MEMBER toolbarExclusion_)
    Q_PROPERTY(QRectF optionsExclusion MEMBER optionsExclusion_)
public:
    explicit CanvasItem(QQuickItem* parent=nullptr);
    ~CanvasItem() override;
    AppController* controller() const {return controller_;}
    void setController(AppController* controller);
    QString tool() const {return tool_;}
    void setTool(const QString& tool);
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
    bool drawing() const {return state_==State::Drawing||state_==State::PossibleHold||state_==State::ShapePreview||state_==State::CreatingShape;}
    double eraserRadius() const {return eraserRadius_;}
    void setEraserRadius(double radius){eraserRadius_=std::clamp(radius,0.5,20.);emit penChanged();}
    QPointF eraserPosition() const {const auto p=view_.worldToScreen(lastPan_);return {p.x,p.y};}
    int selectedCount() const {return int(selection_.ids().size());}
    QVariantList selectionHandles() const;
    QRectF selectionRect() const;
    QString selectionName() const;
    double selectedFill() const;
    QColor selectedFillColor() const;
    QColor selectedBorderColor() const;
    double selectedWidth() const;
    QString interactionHint() const;
    Q_INVOKABLE void editSelectedText();
    Q_INVOKABLE QString selectedTextId() const;
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void duplicateSelection();
    Q_INVOKABLE void recognizeSelection();
    Q_INVOKABLE void setSelectedFill(double opacity);
    Q_INVOKABLE void setSelectedFillColor(const QColor& color);
    Q_INVOKABLE void setSelectedColor(const QColor& color);
    Q_INVOKABLE void setSelectedWidth(double width);
    Q_INVOKABLE void fitPage();
    Q_INVOKABLE QPointF viewportCenter() const {const auto p=view_.screenToWorld({width()/2,height()/2});return {p.x,p.y};}
    Q_INVOKABLE void importImage(const QUrl& url){if(controller_)controller_->importImage(url,viewportCenter());}
    Q_INVOKABLE void pasteImage(){if(controller_)controller_->pasteImage(viewportCenter());}
    Q_INVOKABLE void zoomBy(double factor);
    Q_INVOKABLE void cancelStroke();
signals:
    void controllerChanged();
    void toolChanged();
    void penChanged();
    void viewChanged();
    void drawingChanged();
    void selectionChanged();
    void textRequested(QPointF position,QString id);
protected:
    QSGNode* updatePaintNode(QSGNode*,UpdatePaintNodeData*) override;
    bool event(QEvent*) override;
    bool eventFilter(QObject*,QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseUngrabEvent() override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void wheelEvent(QWheelEvent*) override;
    void touchEvent(QTouchEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void geometryChange(const QRectF&,const QRectF&) override;
private:
    enum class State { Idle,Erasing,Drawing,PossibleHold,ShapePreview,CreatingShape,Panning,TouchGesture,Marquee,Moving,Resizing,Rotating,EditingHandle };
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
    bool tablet(QTabletEvent* event,QPointF local);
    void viewUpdated();
    QPointer<AppController> controller_;
    QPointer<QQuickWindow> filteredWindow_;
    InputManager input_;
    ViewTransform view_;
    StrokeObject current_;
    std::optional<ShapeObject> previewShape_;
    QTimer holdTimer_;
    QFutureWatcher<RecognitionResult> recognitionWatcher_;
    std::uint64_t inputEpoch_=0,requestEpoch_=0;
    Point holdAnchor_;
    SelectionModel selection_;
    std::vector<CanvasObject> editBefore_,editPreview_;
    Point dragStart_;
    Bounds editBounds_;
    int handleIndex_=-1;
    bool marqueeAdditive_=false;
    double eraserRadius_=3;
    std::vector<StrokeObject> eraseBefore_,erasePreview_;
    PenStyle style_;
    QColor color_{"#263345"};
    QString tool_="pen";
    LinePattern manualShapePattern_=LinePattern::Solid;
    bool shiftHeld_=false,recognitionDashed_=false;
    State state_=State::Idle;
    Point lastPan_;
    double touchDistance_=0;
    QPointF touchCenter_;
    bool space_=false,tabletActive_=false;
    QRectF toolbarExclusion_,optionsExclusion_;
};
}
