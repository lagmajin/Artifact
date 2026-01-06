module;
#include <QMenu>
module Menu.Time;

//#include "../../../include/Widgets/Menu/ArtifactTimeMenu.hpp"

import std;
import Artifact.Service.Project;






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

 class ArtifactTimeMenu::Impl {

 public:
  void handleProjectOpened();
  void handleCompositionOpened();
  void handleCompositionClosed();
  void handleProjectClosed();
 };

 void ArtifactTimeMenu::Impl::handleProjectOpened()
 {

 }

 void ArtifactTimeMenu::Impl::handleProjectClosed()
 {

 }

 ArtifactTimeMenu::ArtifactTimeMenu(QWidget* parent /*= nullptr*/):QMenu(parent)
 {
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
  setSeparatorsCollapsible(true);
  setMinimumWidth(160);
 }
 ArtifactTimeMenu::~ArtifactTimeMenu()
 {

 }



};