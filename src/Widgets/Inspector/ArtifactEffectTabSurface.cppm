module;
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QVBoxLayout>
#include <QToolTip>
#include <cmath>

module Artifact.Widgets.Inspector.EffectTabSurface;

import Artifact.Effect.Ofx.Host;

namespace {

class EffectTabCanvas final : public QWidget {
 public:
  using QWidget::QWidget;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    const QPalette pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::Window));
  }
};

class OfxHostStatusWidget final : public QWidget {
 public:
  explicit OfxHostStatusWidget(QWidget* parent = nullptr) : QWidget(parent) {
    setFixedHeight(48);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Click to rescan OFX plug-ins"));
    setAccessibleName(QStringLiteral("OFX plug-in status"));
    refreshText();
  }

 protected:
  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      Artifact::Ofx::ArtifactOfxHost::instance().rescan();
      refreshText();
      QToolTip::showText(mapToGlobal(QPoint(8, height())),
                         QStringLiteral("OFX plug-ins rescanned"), this);
      event->accept();
      return;
    }
    QWidget::mousePressEvent(event);
  }

  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    const QPalette pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::AlternateBase));
    painter.setPen(pal.color(QPalette::Mid));
    painter.drawLine(rect().topLeft(), rect().topRight());
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());
    painter.setPen(pal.color(QPalette::Text));
    painter.drawText(QRect(16, 0, width() - 150, height()),
                     Qt::AlignVCenter | Qt::AlignLeft, text_);
    painter.setPen(pal.color(QPalette::PlaceholderText));
    painter.drawText(QRect(16, 0, width() - 54, height()),
                     Qt::AlignVCenter | Qt::AlignRight,
                     QStringLiteral("Rescan"));

    const int centerX = width() - 24;
    const int centerY = height() / 2;
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(pal.color(QPalette::Text), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPoint(centerX, centerY), 7, 7);
    painter.drawEllipse(QPoint(centerX, centerY), 2, 2);
    for (int i = 0; i < 8; ++i) {
      const double angle = i * 3.14159265358979323846 / 4.0;
      painter.drawLine(QPointF(centerX + std::cos(angle) * 8.0,
                               centerY + std::sin(angle) * 8.0),
                       QPointF(centerX + std::cos(angle) * 11.0,
                               centerY + std::sin(angle) * 11.0));
    }
  }

 private:
  void refreshText() {
    const auto& plugins = Artifact::Ofx::ArtifactOfxHost::instance().getLoadedPlugins();
    text_ = QStringLiteral("Plug-ins   %1 loaded%2")
                .arg(static_cast<qsizetype>(plugins.size()))
                .arg(QString());
    QStringList details;
    details.reserve(static_cast<qsizetype>(plugins.size()));
    for (const auto& plugin : plugins) {
      QString detail = plugin.identifier.toQString().trimmed();
      if (detail.isEmpty()) {
        detail = plugin.pluginPath.toQString().trimmed();
      }
      if (!plugin.version.toQString().trimmed().isEmpty()) {
        detail += QStringLiteral(" ") + plugin.version.toQString().trimmed();
      }
      if (!detail.isEmpty()) {
        details.append(detail);
      }
    }
    setToolTip(details.isEmpty()
                   ? QStringLiteral("No OFX plug-ins loaded. Click to rescan.")
                   : QStringLiteral("Loaded OFX plug-ins:\n%1\n\nClick to rescan.")
                         .arg(details.join(QStringLiteral("\n"))));
    update();
  }

  QString text_;
};

}  // namespace

namespace Artifact {
ArtifactEffectTabSurface::ArtifactEffectTabSurface(QWidget* stackPanel,
                                                   QWidget* detailPanel,
                                                   QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  auto* canvas = new EffectTabCanvas(this);
  auto* canvasLayout = new QVBoxLayout(canvas);
  canvasLayout->setContentsMargins(0, 0, 0, 0);
  canvasLayout->setSpacing(1);
  if (stackPanel) canvasLayout->addWidget(stackPanel);
  // Borrowed by the selected rack row; initially parked outside the layout.
  if (detailPanel) {
    detailPanel->setParent(canvas);
    detailPanel->hide();
  }
  canvasLayout->addStretch(1);
  canvasLayout->addWidget(new OfxHostStatusWidget(canvas));
  layout->addWidget(canvas, 1);
}
}
