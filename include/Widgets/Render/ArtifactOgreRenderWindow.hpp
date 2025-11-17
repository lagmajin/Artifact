module;
#include <QtWidgets/QApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QWindow>
#include <ogre/Ogre.h>
#include <wobjectdefs.h>
export module Render.Ogre;





export namespace Artifact {

 class QTOgreWindow : public QWindow, public Ogre::FrameListener
 {
  W_OBJECT(QTOgreWindow)

 public:
  explicit QTOgreWindow(QWindow* parent = nullptr);
  ~QTOgreWindow();

  /*
  We declare these methods virtual to allow for further inheritance.
  */
  virtual void render(QPainter* painter);
  virtual void render();
  virtual void initialize();
  virtual void createScene();
 public slots:

  virtual void renderLater();
  virtual void renderNow();

  /*
  We use an event filter to be able to capture keyboard/mouse events. More on this later.
  */
  virtual bool eventFilter(QObject* target, QEvent* event);

 signals:
  /*
  Event for clicking on an entity.
  */
  void entitySelected(Ogre::Entity* entity);

 protected:
  Ogre::Root* mRoot=nullptr;
  Ogre::RenderWindow* mRenderWindow=nullptr;
  virtual void keyPressEvent(QKeyEvent* ev);
  virtual void keyReleaseEvent(QKeyEvent* ev);
  virtual void mouseMoveEvent(QMouseEvent* e);
  virtual void wheelEvent(QWheelEvent* e);
  virtual void mousePressEvent(QMouseEvent* e);
  virtual void mouseReleaseEvent(QMouseEvent* e);
  virtual void exposeEvent(QExposeEvent* event);
  virtual bool event(QEvent* event);

  /*
  FrameListener method
  */
  virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt);

  /*
  Write log messages to Ogre log
  */
  void log(Ogre::String msg);
  void log(QString msg);
 };








};