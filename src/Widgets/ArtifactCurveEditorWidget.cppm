module;
#include <QWidget>
#include <wobjectimpl.h>
module Widget.CurveEditor;



namespace ArtifactCore {

 class ArtifactCurveEditorWidget::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactCurveEditorWidget::Impl::Impl()
 {

 }

 ArtifactCurveEditorWidget::Impl::~Impl()
 {

 }

 W_OBJECT_IMPL(ArtifactCurveEditorWidget)

 ArtifactCurveEditorWidget::ArtifactCurveEditorWidget(QWidget* parent/*=nullptr*/):QWidget(parent)
 {

 }

 ArtifactCurveEditorWidget::~ArtifactCurveEditorWidget()
 {

 }

 void ArtifactCurveEditorWidget::setViewRange(float xMin, float xMax, float yMin, float yMax)
 {

 }



};