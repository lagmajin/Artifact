module;
#include <QMenu>
module Menu:Time;

//#include "../../../include/Widgets/Menu/ArtifactTimeMenu.hpp"








namespace Artifact {

 class KeyframeAssistantMenu : public QMenu {
  //Q_OBJECT
 public:
  explicit KeyframeAssistantMenu(QWidget* parent = nullptr)
   : QMenu("�L�[�t���[���⏕", parent)
  {
   addAction("�C�[�W�[�C�[�Y");
   addAction("�C�[�W�[�C�[�Y�C��");
   addAction("�C�[�W�[�C�[�Y�A�E�g");
  }
 };

 ArtifactTimeMenu::ArtifactTimeMenu(QWidget* parent /*= nullptr*/):QMenu(parent)
 {
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
 }

 ArtifactTimeMenu::~ArtifactTimeMenu()
 {

 }

};