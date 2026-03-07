module;
#include <QMenu>
module Menu.Time;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>


//#include "../../../include/Widgets/Menu/ArtifactTimeMenu.hpp"


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
  setSeparatorsCollapsible(true);
  setMinimumWidth(160);
 }
 ArtifactTimeMenu::~ArtifactTimeMenu()
 {

 }



};