#include "Branding.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <algorithm>
#include <cmath>

namespace scalar {

QImage roundedApplicationImage(int size) {
    static const QImage source(QStringLiteral(":/icon.png"));
    if (source.isNull() || size <= 0) return {};

    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF bounds(0, 0, size, size);
    QPainterPath outline;
    outline.addRoundedRect(bounds, size * 0.20, size * 0.20);
    painter.setClipPath(outline);
    painter.drawImage(bounds, source);
    return image;
}

QIcon applicationIcon() {
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256, 512})
        icon.addPixmap(QPixmap::fromImage(roundedApplicationImage(size)));
    return icon;
}

ApplicationLogo::ApplicationLogo(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    setOpaquePainting(false);
}

void ApplicationLogo::paint(QPainter* painter) {
    const qreal side = std::min(width(), height());
    // Render enough source detail for HiDPI screens as well as the normal UI size.
    const auto image = roundedApplicationImage(std::max(128, int(std::ceil(side * 4))));
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(QRectF((width() - side) / 2, (height() - side) / 2, side, side), image);
}

} // namespace scalar
