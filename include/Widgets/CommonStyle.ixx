module;
#include <utility>
#include <QProxyStyle>
#include <QPainter>
#include <QPalette>
#include <QSize>
#include <QStyleOption>
#include <QWidget>
#include <QEvent>
#include <QResizeEvent>

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
  QSize sizeFromContents(ContentsType type, const QStyleOption* option,
                         const QSize& contentsSize,
                         const QWidget* widget = nullptr) const override;
  void drawControl(ControlElement element, const QStyleOption* option,
                   QPainter* painter, const QWidget* widget = nullptr) const override;
  void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget = nullptr) const override;
  void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                          QPainter* painter, const QWidget* widget = nullptr) const override;
};

class StudioSectionStack final : public QWidget {
public:
  explicit StudioSectionStack(QWidget* parent = nullptr);
  ~StudioSectionStack() override;

  void appendWidget(QWidget* widget, bool expands = false);
  void removeWidget(QWidget* widget);
  void setWidgetExpands(QWidget* widget, bool expands);
  void setSpacing(int spacing);
  int spacing() const;
  int count() const;
  QWidget* widgetAt(int index) const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  class Impl;
  Impl* impl_ = nullptr;
  void updateChildGeometry();
};

}
