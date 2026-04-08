module;
#include <utility>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QDebug>
#include <wobjectimpl.h>

module Artifact.Widgets.Test.ScrollPoC;

namespace Artifact {

W_OBJECT_IMPL(ArtifactScrollPoCWidget)

ArtifactScrollPoCWidget::ArtifactScrollPoCWidget(QWidget* parent)
    : QWidget(parent)
{
    // スクロールバーを手動で作成
    verticalScrollBar_ = new QScrollBar(Qt::Vertical, this);
    verticalScrollBar_->setRange(0, contentHeight_);
    verticalScrollBar_->setPageStep(100);
    
    // スクロールしたら再描画
    connect(verticalScrollBar_, &QScrollBar::valueChanged, this, [this](int){
        update();
    });

    // 暗めの背景
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

ArtifactScrollPoCWidget::~ArtifactScrollPoCWidget()
{
}

void ArtifactScrollPoCWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    // スクロールバーを右端に配置（幅 12px）
    int sbWidth = 12;
    verticalScrollBar_->setGeometry(width() - sbWidth, 0, sbWidth, height());
    
    // 可視領域に応じて範囲を更新
    int maxScroll = std::max(0, contentHeight_ - height());
    verticalScrollBar_->setRange(0, maxScroll);
    verticalScrollBar_->setPageStep(height());
}

void ArtifactScrollPoCWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    
    // 1. 背景を全域塗りつぶし（これがリサイズと同期する）
    painter.fillRect(rect(), QColor(30, 30, 30));
    
    // 2. スクロールオフセットを適用してコンテンツを描画
    int offset = verticalScrollBar_->value();
    painter.translate(0, -offset);
    
    // ダミーコンテンツ（100pxごとの目盛り）
    painter.setPen(QColor(80, 80, 80));
    for (int y = 0; y <= contentHeight_; y += 100) {
        painter.drawLine(0, y, width(), y);
        painter.drawText(20, y + 20, QString("Row at %1 px").arg(y));
    }
    
    // 現在のスクロール位置を強調
    painter.setPen(Qt::yellow);
    painter.drawText(50, offset + 50, QString("CURRENT SCROLL: %1").arg(offset));
}

void ArtifactScrollPoCWidget::wheelEvent(QWheelEvent* event)
{
    // ホイールイベントをスクロールバーに転送
    int delta = event->angleDelta().y();
    verticalScrollBar_->setValue(verticalScrollBar_->value() - delta);
    event->accept();
}

} // namespace Artifact
