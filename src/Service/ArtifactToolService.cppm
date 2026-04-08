module;
#include <utility>
#include <wobjectimpl.h>
#include <QObject>
#include <QDebug>

module Artifact.Tool.Service;

import Tool;
import Artifact.Tool.Manager;

namespace Artifact {

W_OBJECT_IMPL(ArtifactToolService)

class ArtifactToolService::Impl {
public:
 EditMode editMode_ = EditMode::View;
 DisplayMode displayMode_ = DisplayMode::Color;
 ArtifactToolManager* toolManager_ = nullptr;
};

ArtifactToolService::ArtifactToolService(QObject* parent)
 : QObject(parent), impl_(new Impl())
{
}

ArtifactToolService::~ArtifactToolService()
{
 delete impl_;
}

void ArtifactToolService::setActiveTool(ToolType type)
{
 if (impl_->toolManager_) {
  impl_->toolManager_->setActiveTool(type);
 }
 toolChanged(type);
}

ToolType ArtifactToolService::activeTool() const
{
 if (impl_->toolManager_) {
  return impl_->toolManager_->activeTool();
 }
 return ToolType::Selection;
}

QString ArtifactToolService::activeToolName() const
{
 if (impl_->toolManager_) {
  return impl_->toolManager_->toolName(activeTool());
 }
 return "SelectionTool";
}

void ArtifactToolService::setEditMode(EditMode mode)
{
 if (impl_->editMode_ == mode) return;
 impl_->editMode_ = mode;

 // EditMode drives default tool type
 switch (mode) {
  case EditMode::View:
   setActiveTool(ToolType::Hand);
   break;
  case EditMode::Transform:
   setActiveTool(ToolType::Selection);
   break;
  case EditMode::Mask:
   setActiveTool(ToolType::Pen);
   break;
  case EditMode::Paint:
   setActiveTool(ToolType::Shape);
   break;
 }

 editModeChanged(mode);
}

EditMode ArtifactToolService::editMode() const
{
 return impl_->editMode_;
}

void ArtifactToolService::setDisplayMode(DisplayMode mode)
{
 if (impl_->displayMode_ == mode) return;
 impl_->displayMode_ = mode;
 displayModeChanged(mode);
}

DisplayMode ArtifactToolService::displayMode() const
{
 return impl_->displayMode_;
}

bool ArtifactToolService::isViewOnly() const
{
 return impl_->editMode_ == EditMode::View;
}

bool ArtifactToolService::isPaintMode() const
{
 return impl_->editMode_ == EditMode::Paint;
}

bool ArtifactToolService::isAlphaView() const
{
 return impl_->displayMode_ == DisplayMode::Alpha;
}

void ArtifactToolService::setToolManager(ArtifactToolManager* manager)
{
 if (impl_->toolManager_ == manager) return;

 if (impl_->toolManager_) {
  disconnect(impl_->toolManager_, &ArtifactToolManager::toolChanged,
             this, &ArtifactToolService::toolChanged);
 }

 impl_->toolManager_ = manager;

 if (impl_->toolManager_) {
  connect(impl_->toolManager_, &ArtifactToolManager::toolChanged,
          this, &ArtifactToolService::toolChanged);
 }
}

ArtifactToolManager* ArtifactToolService::toolManager() const
{
 return impl_->toolManager_;
}

}
