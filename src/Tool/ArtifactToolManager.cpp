module;
#include <wobjectimpl.h>
module Artifact.Tool.Manager;

import Artifact.Event.Types;
import Event.Bus;

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
  ArtifactCore::globalEventBus().publish<ToolChangedEvent>(ToolChangedEvent{type});
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
   case ToolType::Rectangle: return "RectangleTool";
   case ToolType::Ellipse: return "EllipseTool";
   case ToolType::Move: return "MoveTool";
   case ToolType::Scale: return "ScaleTool";
   case ToolType::Ripple: return "RippleTool";
   case ToolType::Rolling: return "RollingTool";
   case ToolType::Slip: return "SlipTool";
   case ToolType::Slide: return "SlideTool";
   default: return "Unknown";
  }
 }

}
