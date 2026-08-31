module;
#include <QString>

export module Artifact.Render.CompositionRenderPassPlan;

export namespace Artifact {

enum class FrameRenderPassKind {
  Setup,
  Base,
  Surface,
  Mask,
  Composite,
  Post,
  Overlay,
  Flush,
  Present,
};

struct FrameRenderPass {
  FrameRenderPassKind kind = FrameRenderPassKind::Setup;
  QString name;
  QString note;
};

}
