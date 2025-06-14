module;
#include <QMenu>
module Menu:Time;

//#include "../../../include/Widgets/Menu/ArtifactTimeMenu.hpp"








namespace Artifact {

 class KeyframeAssistantMenu : public QMenu {
  //Q_OBJECT
 public:
  explicit KeyframeAssistantMenu(QWidget* parent = nullptr)
   : QMenu("キーフレーム補助", parent)
  {
   addAction("イージーイーズ");
   addAction("イージーイーズイン");
   addAction("イージーイーズアウト");
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