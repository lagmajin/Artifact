module;
#include <utility>
#include <QProxyStyle>
#include <QPalette>
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
};

}
