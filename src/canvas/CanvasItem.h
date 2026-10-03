#pragma once
#include "app/AppController.h"
#include "input/InputManager.h"
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
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(bool drawing READ drawing NOTIFY drawingChanged)
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
    double zoom() const {return view_.zoom;}
    bool drawing() const {return state_==State::Drawing;}
    Q_INVOKABLE void fitPage();
    Q_INVOKABLE void zoomBy(double factor);
    Q_INVOKABLE void cancelStroke();
signals:
    void controllerChanged();
    void toolChanged();
    void penChanged();
    void viewChanged();
    void drawingChanged();
protected:
    QSGNode* updatePaintNode(QSGNode*,UpdatePaintNodeData*) override;
    bool event(QEvent*) override;
    bool eventFilter(QObject*,QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseUngrabEvent() override;
    void wheelEvent(QWheelEvent*) override;
    void touchEvent(QTouchEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void geometryChange(const QRectF&,const QRectF&) override;
private:
    enum class State { Idle,Drawing,Panning,TouchGesture };
    Point world(QPointF p) const {return view_.screenToWorld({p.x(),p.y()});}
    void begin(PointerSample sample);
    void append(PointerSample sample);
    void commit();
    bool tablet(QTabletEvent* event,QPointF local);
    void viewUpdated();
    QPointer<AppController> controller_;
    QPointer<QQuickWindow> filteredWindow_;
    InputManager input_;
    ViewTransform view_;
    StrokeObject current_;
    PenStyle style_;
    QColor color_{"#263345"};
    QString tool_="pen";
    State state_=State::Idle;
    Point lastPan_;
    double touchDistance_=0;
    QPointF touchCenter_;
    bool space_=false,tabletActive_=false;
    QRectF toolbarExclusion_,optionsExclusion_;
};
}
