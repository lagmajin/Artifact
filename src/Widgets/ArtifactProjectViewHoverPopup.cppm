module;

#include <QGuiApplication>
#include <QLabel>
#include <QPalette>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <algorithm>

export module Artifact.Widgets.ProjectViewHoverPopup;

import Artifact.Widgets.ProjectManagerWidget;
import Widgets.Utils.CSS;

export namespace Artifact {

// --- Hover Popup ---
class HoverThumbnailPopupWidget::Impl {
 public:
  Impl() : thumbnailLabel(nullptr) {}
  QLabel* thumbnailLabel;
  QVector<QLabel*> infoLabels;
  QVBoxLayout* layout;
};

HoverThumbnailPopupWidget::HoverThumbnailPopupWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setAutoFillBackground(false);

  impl_->layout = new QVBoxLayout(this);
  impl_->layout->setContentsMargins(10, 10, 10, 10);
  impl_->layout->setSpacing(6);

  impl_->thumbnailLabel = new QLabel(this);
  impl_->thumbnailLabel->setFixedSize(200, 112);
  impl_->thumbnailLabel->setScaledContents(true);
  {
    QPalette pal = impl_->thumbnailLabel->palette();
    pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    impl_->thumbnailLabel->setAutoFillBackground(true);
    impl_->thumbnailLabel->setPalette(pal);
  }
  impl_->layout->addWidget(impl_->thumbnailLabel, 0, Qt::AlignCenter);

  for (int i = 0; i < 3; ++i) {
    QLabel* l = new QLabel(this);
    {
      QPalette pal = l->palette();
      pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(115));
      l->setPalette(pal);
    }
    impl_->infoLabels.append(l);
    impl_->layout->addWidget(l);
  }
}

void HoverThumbnailPopupWidget::paintEvent(QPaintEvent* event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor(30, 30, 30, 240));
  painter.setPen(QPen(QColor(ArtifactCore::currentDCCTheme().borderColor)));
  painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
  QWidget::paintEvent(event);
}

HoverThumbnailPopupWidget::~HoverThumbnailPopupWidget() { delete impl_; }
void HoverThumbnailPopupWidget::setThumbnail(const QPixmap& px) { if(impl_->thumbnailLabel) impl_->thumbnailLabel->setPixmap(px); }
void HoverThumbnailPopupWidget::setLabels(const QStringList& ls) {
  for (int i = 0; i < impl_->infoLabels.size(); ++i) {
    impl_->infoLabels[i]->setText(i < ls.size() ? ls[i] : QString());
  }
}
void HoverThumbnailPopupWidget::setLabel(int idx, const QString& t) { if(idx>=0 && idx<impl_->infoLabels.size()) impl_->infoLabels[idx]->setText(t); }
void HoverThumbnailPopupWidget::showAt(const QPoint& p) {
  QPoint pos = p;
  const QSize popupSize = sizeHint().expandedTo(QSize(240, 140));
  if (QScreen* screen = QGuiApplication::screenAt(p)) {
    const QRect avail = screen->availableGeometry();
    if (pos.x() + popupSize.width() > avail.right() - 8) {
      pos.setX(avail.right() - popupSize.width() - 8);
    }
    if (pos.y() + popupSize.height() > avail.bottom() - 8) {
      pos.setY(avail.bottom() - popupSize.height() - 8);
    }
    pos.setX(std::max(avail.left() + 8, pos.x()));
    pos.setY(std::max(avail.top() + 8, pos.y()));
  }
  move(pos);
  show();
  raise();
  QTimer::singleShot(4500, this, &QWidget::hide);
}

} // namespace Artifact
