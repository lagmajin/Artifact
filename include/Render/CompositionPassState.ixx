module;
#include <QString>

export module Artifact.Render.CompositionPassState;

import Color.Float;

export namespace Artifact {

struct GpuBasePassState {
  float origZoom = 1.0f;
  ArtifactCore::FloatColor origClearColor{};
  float origPanX = 0.0f;
  float origPanY = 0.0f;
  float origViewW = 0.0f;
  float origViewH = 0.0f;
};

struct GpuLayerBlendResult {
  bool blended = false;
  bool directFallbackUsed = false;
  bool convertedLayerToFloat = false;
};

struct PresentStageResult {
  double presentedGpuFrameMs = 0.0;
  QString presentedStatus;
  QString presentedVideoDebug;
};

}
