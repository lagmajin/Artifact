module;
#include <utility>
#include <memory>
#include <wobjectdefs.h>
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
