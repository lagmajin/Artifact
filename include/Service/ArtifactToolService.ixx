module;
#include <utility>

#include <wobjectdefs.h>
#include <QObject>
#include <QString>
#include <QDebug>

export module Artifact.Tool.Service;


import Tool;
import Artifact.Tool.Manager;

export namespace Artifact {

 class ArtifactToolService : public QObject {
  W_OBJECT(ArtifactToolService)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactToolService(QObject* parent = nullptr);
  ~ArtifactToolService();

  // ToolType delegation
  void setActiveTool(ToolType type);
  ToolType activeTool() const;
  QString activeToolName() const;

  // EditMode
  void setEditMode(EditMode mode);
  EditMode editMode() const;

  // DisplayMode
  void setDisplayMode(DisplayMode mode);
  DisplayMode displayMode() const;

  // Composite queries
  bool isViewOnly() const;
  bool isPaintMode() const;
  bool isAlphaView() const;

  // Bind a ToolManager instance for tool-type coordination
  void setToolManager(ArtifactToolManager* manager);
  ArtifactToolManager* toolManager() const;

  void editModeChanged(EditMode mode) W_SIGNAL(editModeChanged, mode);
  void displayModeChanged(DisplayMode mode) W_SIGNAL(displayModeChanged, mode);
  void toolChanged(ToolType type) W_SIGNAL(toolChanged, type);
 };

}
