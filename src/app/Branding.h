#pragma once

#include <QIcon>
#include <QImage>
#include <QQuickPaintedItem>

namespace scalar {

// Both the window icon and the Home logo use the same transparent, rounded image.
QImage roundedApplicationImage(int size);
QIcon applicationIcon();

class ApplicationLogo : public QQuickPaintedItem {
    Q_OBJECT
public:
    explicit ApplicationLogo(QQuickItem* parent = nullptr);
    void paint(QPainter* painter) override;
};

} // namespace scalar
