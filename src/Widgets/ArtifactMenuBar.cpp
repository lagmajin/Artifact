

module Menu.MenuBar;

import Menu;





namespace Artifact {

 class ArtifactMenuBar::Impl {
 private:

 public:
  QMenuBar* menuBar = nullptr;
  QMenu* fileMenu=nullptr;
  QMenu* compositionMenu=nullptr;
  QMenu* layerMenu = nullptr;
  QMenu* viewMenu = nullptr;
  Impl(QMenuBar*menu);
  ~Impl();
  //void setUpUI(QMenu* menu);
 };

 ArtifactMenuBar::Impl::Impl(QMenuBar* menu)
 {

 }

 ArtifactMenuBar::Impl::~Impl()
 {

 }

 ArtifactMenuBar::ArtifactMenuBar(QWidget* parent) :QMenuBar(parent),impl(new Impl(this))
 {
  impl->fileMenu = new ArtifactFileMenu(this);


  impl->compositionMenu = new ArtifactCompositionMenu(this);
  impl->layerMenu = new ArtifactLayerMenu(this);
  impl->viewMenu = new ArtifactViewMenu(this);


  addMenu(impl->fileMenu);
  addMenu(impl->compositionMenu);
  addMenu(impl->viewMenu);


 }

 ArtifactMenuBar::~ArtifactMenuBar()
 {
  delete impl;
 }


};