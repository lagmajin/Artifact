module;
#include <QDialog>
#include <wobjectimpl.h>
#include <QComboBox>

module Artifact.Dialog.EditComposition;

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



import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;


namespace Artifact
{
 using namespace ArtifactCore;
 using namespace ArtifactWidgets;

 class ArtifactEditCompositionSettingPage::Impl
 {
 public:
  QComboBox* resolutionCombobox_ = nullptr;
  EditableLabel* compositionNameEdit_ = nullptr;
  DragSpinBox* widthSpinBox = nullptr;
  DragSpinBox* heightSpinBox = nullptr;
 };
	
 ArtifactEditCompositionSettingPage::ArtifactEditCompositionSettingPage(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl)
 {
  impl_->widthSpinBox = new DragSpinBox();
  impl_->heightSpinBox = new DragSpinBox();

  auto line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
 }

 ArtifactEditCompositionSettingPage::~ArtifactEditCompositionSettingPage()
 {
  delete impl_;
 }
	
 class  ArtifactEditCompositionDialog::Impl
 {
 public:
  Impl();
  ~Impl();
 };

 ArtifactEditCompositionDialog::Impl::Impl()
 {

 }

	W_OBJECT_IMPL(ArtifactEditCompositionDialog)
	
 ArtifactEditCompositionDialog::ArtifactEditCompositionDialog(QWidget* parent /*= nullptr*/):QDialog(parent)
 {

 }

 ArtifactEditCompositionDialog::~ArtifactEditCompositionDialog()
 {

 }

 

};

