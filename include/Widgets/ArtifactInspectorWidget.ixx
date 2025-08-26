module;

#include <wobjectdefs.h>

#include <QtWidgets/QScrollArea>

#include <Audio/AudioDllImport.hpp>

export module Widgets.Inspector;

export namespace Artifact {

 //class ArtifactInspectorWidgetPrivate;

 class ArtifactInspectorWidget :public QScrollArea{
 W_OBJECT(ArtifactInspectorWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void update();

  void contextMenuEvent(QContextMenuEvent*) override;

 public:
  explicit ArtifactInspectorWidget(QWidget* parent = nullptr);
  ~ArtifactInspectorWidget();
  void clear();
 signals:
 
 public slots:
  void triggerUpdate();
 };








};