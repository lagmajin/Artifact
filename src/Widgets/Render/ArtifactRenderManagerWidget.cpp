
#include "../../../include/Widgets/Render/ArtifactRenderManagerWidget.hpp"






namespace Artifact {



 ArtifactRenderManagerWidget::ArtifactRenderManagerWidget(QWidget* parent /*= nullptr*/):QWidget(parent)
 {
  //wi::renderer::SetShaderPath("shaders");

  //wi::renderer::SetShaderSourcePath(wi::helper::GetCurrentPath()+"/App/shaders/");
  
  //app = new wi::Application();
  

  //app->SetWindow((HWND)winId());

  //app->Initialize();

  //wi::initializer::InitializeComponentsImmediate();

  //wi::RenderPath3D myGame; // Declare a game screen component, aka "RenderPath" (you could also override its Update(), Render() etc. functions). 
  //app.ActivatePath(&myGame);


  initialized = true;
 }

 ArtifactRenderManagerWidget::~ArtifactRenderManagerWidget()
 {

 }

 bool ArtifactRenderManagerWidget::event(QEvent* e)
 {
  //if (initialized)
  //{
   //app->Run();
  //}

  return QWidget::event(e);
 }

 void ArtifactRenderManagerWidget::clear()
 {

 }

};