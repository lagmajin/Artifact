module;
#include <utility>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QColor>
#include <QString>
#include <QWidget>
#include <wobjectimpl.h>
#include <QPainterPath>

module Artifact.Widgets.LayerLockIndicator;

import std;

namespace Artifact {

class LayerLockIndicator::Impl {
public:
    bool locked_ = false;
    QString userName_;
    QString userColor_ = QStringLiteral("#FF9800");  // デフォルトはオレンジ
};

LayerLockIndicator::LayerLockIndicator(QWidget* parent)
    : QWidget(parent)
    , impl_(new Impl())
{
    setFixedWidth(20);
    setMinimumHeight(16);
    setToolTip(QStringLiteral(""));
}

LayerLockIndicator::~LayerLockIndicator() {
    delete impl_;
}

void LayerLockIndicator::setLocked(bool locked, const QString& userName, const QString& userColor) {
    impl_->locked_ = locked;
    impl_->userName_ = userName;
    if (!userColor.isEmpty()) {
        impl_->userColor_ = userColor;
    }

    if (locked && !userName.isEmpty()) {
        setToolTip(QStringLiteral("Locked by %1").arg(userName));
    } else {
        setToolTip(QStringLiteral(""));
    }

    update();
}

bool LayerLockIndicator::isLocked() const {
    return impl_->locked_;
}

QString LayerLockIndicator::lockingUserName() const {
    return impl_->userName_;
}

QString LayerLockIndicator::lockingUserColor() const {
    return impl_->userColor_;
}

QSize LayerLockIndicator::sizeHint() const {
    return QSize(20, 16);
}

void LayerLockIndicator::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int cx = w / 2;
    const int cy = h / 2;

    if (impl_->locked_) {
        // ロック中表示: 南京錠アイコン
        const QColor lockColor(impl_->userColor_);

        // 錠前本体
        QRectF bodyRect(cx - 4, cy - 1, 8, 6);
        painter.setPen(QPen(lockColor.darker(120), 1.0));
        painter.setBrush(lockColor);
        painter.drawRoundedRect(bodyRect, 1.5, 1.5);

        // シャクル（鍵の輪っか部分）
        QPainterPath shackle;
        shackle.moveTo(cx - 3, cy - 1);
        shackle.lineTo(cx - 3, cy - 4);
        shackle.arcTo(QRectF(cx - 4, cy - 6, 8, 5), 180, -180);
        shackle.lineTo(cx + 3, cy - 1);

        painter.setPen(QPen(lockColor.darker(120), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(shackle);

        // キーホール
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 100));
        painter.drawEllipse(QPointF(cx, cy + 1.5), 1.0, 1.0);
    } else {
        // 非ロック表示: 開いた南京錠（オプション、通常は非表示）
        // 何もしない
    }
}

} // namespace Artifact
