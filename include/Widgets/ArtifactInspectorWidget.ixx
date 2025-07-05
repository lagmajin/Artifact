module;

#include <wobjectdefs.h>

#include <QtWidgets/QScrollArea>

#include <Audio/AudioDllImport.hpp>

export module ArtifactInspectorWidget;

namespace Artifact {

 class ArtifactInspectorWidgetPrivate;

 class ArtifactInspectorWidget :public QScrollArea{
 W_OBJECT(ArtifactInspectorWidget)
 private:

 protected:
  void update();
 public:
  explicit ArtifactInspectorWidget(QWidget* parent = nullptr);
  ~ArtifactInspectorWidget();
  void clear();
 signals:
 
 public slots:
  void triggerUpdate();
 };








};