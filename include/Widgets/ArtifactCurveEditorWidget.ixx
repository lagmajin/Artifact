module;
#include <QWidget>

export module Widget.CurveEditor;



export namespace ArtifactCore {

 class ArtifactCurveEditorWidget:public QWidget{
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactCurveEditorWidget(QWidget*parent=nullptr);
  ~ArtifactCurveEditorWidget();
  void setViewRange(float xMin, float xMax, float yMin, float yMax);
 };










};


