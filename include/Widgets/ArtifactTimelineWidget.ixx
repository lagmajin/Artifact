module;

#include <wobjectcpp.h>

#include <QtWidgets/QtWidgets>

#include <wobjectdefs.h>

export module ArtifactTimelineWidget;


export namespace Artifact {
 
 struct ArtifactTimelineWidgetPrivate;
 
 class ArtifactTimelineWidget :public QWidget{
  W_OBJECT(ArtifactTimelineWidget)
 private:
 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
 public:
  explicit ArtifactTimelineWidget(QWidget *parent=nullptr);
  ~ArtifactTimelineWidget();
  void update();
 signals:
 public slots:
 };









};