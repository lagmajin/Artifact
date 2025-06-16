module;
#include <wobjectdefs.h>
#include <memory>
#include <QObject>
#include <QWidget>
export module ArtifactCompositionWidget;



export namespace Artifact {

 class ArtifactCompositionWidgetPrivate;

 class ArtifactCompositionWidget:public QWidget
 {
 private:

 public:
  explicit ArtifactCompositionWidget(QWidget* parent = nullptr);
  ~ArtifactCompositionWidget();
	 
 };







}