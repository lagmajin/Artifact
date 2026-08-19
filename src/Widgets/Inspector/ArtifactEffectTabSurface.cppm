module;
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QSettings>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QToolTip>

module Artifact.Widgets.Inspector.EffectTabSurface;

import Artifact.Effect.Ofx.Host;

namespace {
constexpr auto kEffectRackVisibleSetting =
    "Inspector/Effects/EffectRackVisible";

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
    setFixedHeight(30);
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
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());
    painter.setPen(pal.color(QPalette::Text));
    painter.drawText(QRect(12, 0, width() - 24, height()),
                     Qt::AlignVCenter | Qt::AlignLeft, text_);
    painter.setPen(pal.color(QPalette::PlaceholderText));
    painter.drawText(QRect(12, 0, width() - 24, height()),
                     Qt::AlignVCenter | Qt::AlignRight,
                     QStringLiteral("Rescan"));
  }

 private:
  void refreshText() {
    const auto& plugins = Artifact::Ofx::ArtifactOfxHost::instance().getLoadedPlugins();
    text_ = QStringLiteral("OFX Host  •  %1 plug-in%2")
                .arg(static_cast<qsizetype>(plugins.size()))
                .arg(plugins.size() == 1 ? QString() : QStringLiteral("s"));
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

class EffectRackDisclosure final : public QWidget {
 public:
  explicit EffectRackDisclosure(QWidget* rack, QWidget* parent = nullptr)
      : QWidget(parent), rack_(rack) {
    setFixedHeight(28);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Show or hide the effect rack."));
    setAccessibleName(QStringLiteral("Effect Rack visibility"));

    setRackVisible(true, false);
  }

 protected:
  void showEvent(QShowEvent* event) override {
    QWidget::showEvent(event);
    if (settingsLoaded_) {
      return;
    }
    settingsLoaded_ = true;
    const bool visible =
        QSettings().value(QString::fromLatin1(kEffectRackVisibleSetting), true)
            .toBool();
    setRackVisible(visible, false);
  }

  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QPalette pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::AlternateBase));
    painter.setPen(pal.color(QPalette::Mid));
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());

    painter.setPen(Qt::NoPen);
    painter.setBrush(pal.color(QPalette::Text));
    if (rackVisible_) {
      const QPoint arrow[] = {QPoint(9, 10), QPoint(21, 10), QPoint(15, 17)};
      painter.drawPolygon(arrow, 3);
    } else {
      const QPoint arrow[] = {QPoint(11, 7), QPoint(19, 14), QPoint(11, 21)};
      painter.drawPolygon(arrow, 3);
    }

    painter.setPen(pal.color(QPalette::Text));
    painter.drawText(QRect(30, 0, qMax(0, width() - 92), height()),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("Effect Rack"));

    painter.setPen(pal.color(QPalette::PlaceholderText));
    painter.drawText(QRect(qMax(0, width() - 64), 0, 54, height()),
                     Qt::AlignVCenter | Qt::AlignRight,
                     rackVisible_ ? QStringLiteral("Hide")
                                  : QStringLiteral("Show"));
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      setRackVisible(!rackVisible_, true);
      event->accept();
      return;
    }
    QWidget::mousePressEvent(event);
  }

 private:
  void setRackVisible(bool visible, bool persist) {
    rackVisible_ = visible;
    if (rack_) {
      rack_->setVisible(visible);
    }
    setAccessibleDescription(visible ? QStringLiteral("Effect Rack shown")
                                     : QStringLiteral("Effect Rack hidden"));
    if (persist) {
      QSettings().setValue(QString::fromLatin1(kEffectRackVisibleSetting),
                           visible);
    }
    update();
  }

  QWidget* rack_ = nullptr;
  bool rackVisible_ = true;
  bool settingsLoaded_ = false;
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
  canvasLayout->addWidget(new OfxHostStatusWidget(canvas));
  if (stackPanel) {
    canvasLayout->addWidget(new EffectRackDisclosure(stackPanel, canvas));
  }
  if (stackPanel) canvasLayout->addWidget(stackPanel);
  if (detailPanel) canvasLayout->addWidget(detailPanel, 1);
  layout->addWidget(canvas, 1);
}
}
