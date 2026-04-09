module;
#include <utility>
#include <QProxyStyle>
#include <QPainter>
#include <QPalette>
#include <QStyleOption>
#include <QWidget>

export module Widgets.CommonStyle;

export namespace Artifact {

class ArtifactCommonStyle : public QProxyStyle {
public:
  explicit ArtifactCommonStyle(QStyle* baseStyle = nullptr);
  ~ArtifactCommonStyle() override;

  void polish(QWidget* widget) override;
  void polish(QPalette& palette) override;
  int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                  const QWidget* widget = nullptr) const override;
  void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget = nullptr) const override;
  void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                          QPainter* painter, const QWidget* widget = nullptr) const override;
};

}
