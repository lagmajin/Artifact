module;

#include <wobjectcpp.h>

#include <QtWidgets/QtWidgets>

export module ArtifactTimelineWidget;


export namespace Artifact {
 
 struct ArtifactTimelineWidgetPrivate;
 
 class ArtifactTimelineWidget :public QWidget{
 private:
 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
 public:
  ArtifactTimelineWidget();
  ~ArtifactTimelineWidget();
 signals:
 public slots:
 };









};