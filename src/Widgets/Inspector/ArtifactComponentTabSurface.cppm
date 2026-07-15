module;
#include <QFont>
#include <QPainter>
#include <QVBoxLayout>

module Artifact.Widgets.Inspector.ComponentTabSurface;

namespace Artifact {
namespace {
class ComponentTabHeading final : public QWidget {
 public:
  using QWidget::QWidget;

  QSize sizeHint() const override { return QSize(0, 30); }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QPalette pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::Window));
    QFont headingFont = font();
    headingFont.setPointSize(14);
    headingFont.setWeight(QFont::DemiBold);
    painter.setFont(headingFont);
    painter.setPen(pal.color(QPalette::WindowText));
    painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Components"));
    painter.setPen(pal.color(QPalette::Mid));
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());
  }
};
}  // namespace

ArtifactComponentTabSurface::ArtifactComponentTabSurface(QWidget* componentPanel,
                                                         QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);
  layout->addWidget(new ComponentTabHeading(this));
  if (componentPanel) layout->addWidget(componentPanel, 1);
}
}
