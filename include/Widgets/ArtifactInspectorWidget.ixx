module;
#include <utility>
#include <QFocusEvent>
#include <QKeyEvent>

#include <Audio/AudioDllImport.hpp>

#include <wobjectdefs.h>
#include <QtWidgets/QScrollArea>
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

  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void contextMenuEvent(QContextMenuEvent*) override;

 public:
  explicit ArtifactInspectorWidget(QWidget* parent = nullptr);
  ~ArtifactInspectorWidget();

  QSize sizeHint() const override;

  void clear();
 //signals:
 
  public /*slots*/:
   void triggerUpdate();
  };








};

