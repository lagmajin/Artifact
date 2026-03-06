module;
#include <wobjectimpl.h>
module Artifact.Tool.Manager;

namespace Artifact {

 class ArtifactToolManager::Impl {
 public:
  ToolType activeTool_ = ToolType::Selection;
 };

 W_OBJECT_IMPL(ArtifactToolManager)

 ArtifactToolManager::ArtifactToolManager(QObject* parent) 
  : QObject(parent), impl_(new Impl()) 
 {
 }

 ArtifactToolManager::~ArtifactToolManager() {
  delete impl_;
 }

 void ArtifactToolManager::setActiveTool(ToolType type) {
  if (impl_->activeTool_ == type) return;
  impl_->activeTool_ = type;
  toolChanged(type);
 }

 ToolType ArtifactToolManager::activeTool() const {
  return impl_->activeTool_;
 }

 QString ArtifactToolManager::toolName(ToolType type) const {
  switch (type) {
   case ToolType::Selection: return "SelectionTool";
   case ToolType::Hand: return "HandTool";
   case ToolType::Zoom: return "ZoomTool";
   case ToolType::Rotation: return "RotationTool";
   case ToolType::AnchorPoint: return "AnchorPointTool";
   case ToolType::Pen: return "PenTool";
   case ToolType::Text: return "TextTool";
   case ToolType::Shape: return "ShapeTool";
   default: return "Unknown";
  }
 }

}