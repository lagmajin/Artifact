module;
#include <utility>
#include <wobjectdefs.h>
#include <QObject>
#include <QString>

export module Artifact.Tool.Manager;

import std;

export namespace Artifact {

 enum class ToolType {
  Selection,
  Hand,
  Zoom,
  Rotation,
  AnchorPoint,
  Pen,
  Text,
  Shape,
  Rectangle,
  Ellipse,
  Move,
  Scale,
  // Advanced Trimming Tools
  Ripple,
  Rolling,
  Slip,
  Slide
 };

 class ArtifactToolManager : public QObject {
   W_OBJECT(ArtifactToolManager)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactToolManager(QObject* parent = nullptr);
  ~ArtifactToolManager();

  void setActiveTool(ToolType type);
  ToolType activeTool() const;
  QString toolName(ToolType type) const;
 };

}

W_REGISTER_ARGTYPE(Artifact::ToolType)
