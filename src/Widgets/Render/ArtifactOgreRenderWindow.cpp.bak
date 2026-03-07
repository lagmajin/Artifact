module;
#include <wobjectimpl.h>

#include "QOpenGLContext"
#include <QWindow>
#include <ogre/OgreCommon.h>
#include <ogre/OgreStringConverter.h>
module Render.Ogre;





namespace Artifact {

 W_OBJECT_IMPL(QTOgreWindow)


 QTOgreWindow::QTOgreWindow(QWindow* parent)
  : QWindow(parent)
 {
  //setAnimating(true);
  installEventFilter(this);
  //m_ogreBackground = Ogre::ColourValue(0.0f, 0.5f, 1.0f);

  setSurfaceType(SurfaceType::VulkanSurface);
 }

 /*
 Upon destruction of the QWindow object we destroy the Ogre3D scene.
 */
 QTOgreWindow::~QTOgreWindow()
 {
  if (mRenderWindow) {
   mRenderWindow->destroy();
  }
  if (mRoot) {
   delete mRoot;
  }
 }

 /*
 In case any drawing surface backing stores (QRasterWindow or QOpenGLWindow) of Qt are supplied to this
 class in any way we inform Qt that they will be unused.
 */
 void QTOgreWindow::render(QPainter* painter)
 {
  Q_UNUSED(painter);
 }

 /*
 Our initialization function. Called by our renderNow() function once when the window is first exposed.
 */
 void QTOgreWindow::initialize()
 {
  auto root = new Ogre::Root("", "");
  

  Ogre::NameValuePairList options;
  options["externalWindowHandle"] = Ogre::StringConverter::toString((uintptr_t)this->winId());
  mRenderWindow = mRoot->initialise(true, "Vulkan + Qt");

 
  auto context = new QOpenGLContext;
  context->setFormat(QSurfaceFormat::defaultFormat());
  context->create();

  //mRoot->createRenderWindow("options",this->width(), this->height(),false,options);
 }

 void QTOgreWindow::createScene()
 {
  
 }
 void QTOgreWindow::render()
 {
 
 }

 void QTOgreWindow::renderLater()
 {
  
 }

 bool QTOgreWindow::event(QEvent* event)
 {


  return false;
 }


 void QTOgreWindow::exposeEvent(QExposeEvent* event)
 {
  Q_UNUSED(event);


 }

 void QTOgreWindow::renderNow()
 {
 

 }

 /*
 Our event filter; handles the resizing of the QWindow. When the size of the QWindow changes note the
 call to the Ogre3D window and camera. This keeps the Ogre3D scene looking correct.
 */
 bool QTOgreWindow::eventFilter(QObject* target, QEvent* event)
 {


  return false;
 }

 /*
 How we handle keyboard and mouse events.
 */
 void QTOgreWindow::keyPressEvent(QKeyEvent* ev)
 {

 }

 void QTOgreWindow::keyReleaseEvent(QKeyEvent* ev)
 {

 }

 void QTOgreWindow::mouseMoveEvent(QMouseEvent* e)
 {


 }

 void QTOgreWindow::wheelEvent(QWheelEvent* e)
 {

 }

 void QTOgreWindow::mousePressEvent(QMouseEvent* e)
 {
 
 }

 void QTOgreWindow::mouseReleaseEvent(QMouseEvent* e)
 {

 }

 bool QTOgreWindow::frameRenderingQueued(const Ogre::FrameEvent& evt)
 {

  return true;
 }

 void QTOgreWindow::log(Ogre::String msg)
 {
  if (Ogre::LogManager::getSingletonPtr() != NULL) Ogre::LogManager::getSingletonPtr()->logMessage(msg);
 }

 void QTOgreWindow::log(QString msg)
 {
  log(Ogre::String(msg.toStdString().c_str()));
 }




};