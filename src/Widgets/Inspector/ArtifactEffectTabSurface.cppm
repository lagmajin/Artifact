module;
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>

module Artifact.Widgets.Inspector.EffectTabSurface;

namespace {
constexpr auto kEffectRackVisibleSetting =
    "Inspector/Effects/EffectRackVisible";

class EffectRackDisclosure final : public QWidget {
 public:
  explicit EffectRackDisclosure(QWidget* rack, QWidget* parent = nullptr)
      : QWidget(parent), rack_(rack) {
    setFixedHeight(28);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Show or hide the effect rack."));
    setAccessibleName(QStringLiteral("Effect Rack visibility"));

    const bool visible =
        QSettings().value(QString::fromLatin1(kEffectRackVisibleSetting), true)
            .toBool();
    setRackVisible(visible, false);
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QPalette pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::AlternateBase));
    painter.setPen(pal.color(QPalette::Mid));
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());

    const QRect arrowRect(8, 6, 16, 16);
    QStyleOption arrowOption;
    arrowOption.initFrom(this);
    arrowOption.rect = arrowRect;
    style()->drawPrimitive(rackVisible_ ? QStyle::PE_IndicatorArrowDown
                                       : QStyle::PE_IndicatorArrowRight,
                           &arrowOption, &painter, this);

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
};
}  // namespace

namespace Artifact {
ArtifactEffectTabSurface::ArtifactEffectTabSurface(QWidget* stackPanel,
                                                   QWidget* detailPanel,
                                                   QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(1);
  if (stackPanel) {
    layout->addWidget(new EffectRackDisclosure(stackPanel, this));
  }
  if (stackPanel) layout->addWidget(stackPanel);
  if (detailPanel) layout->addWidget(detailPanel, 1);
}
}
