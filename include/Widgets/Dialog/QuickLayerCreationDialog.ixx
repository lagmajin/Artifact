module;
#include <memory>
#include <QDialog>
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.QuickLayerCreationDialog;

import Artifact.Layer.InitParams;
import Artifact.Menu.Layer;
import Artifact.Animation.LayerEffectEnvelope;

export namespace Artifact {

enum class QuickLayerMaskShape {
  None,
  Rectangle,
  Ellipse
};

enum class QuickLayerEnvelopeTiming {
  Simultaneous,
  OpacityLead,
  EffectLead
};

struct QuickLayerCreationOptions {
  ArtifactSolidLayerInitParams solidParams;
  QuickLayerMaskShape maskShape = QuickLayerMaskShape::None;
  float maskFeather = 0.0f;
  bool entryEnvelope = false;
  bool exitEnvelope = false;
  QuickLayerEnvelopeTiming envelopeTiming = QuickLayerEnvelopeTiming::Simultaneous;
  LayerEnvelopeCurve envelopeCurve = LayerEnvelopeCurve::Linear;
  int envelopeFrames = 8;
  LayerEffectEnvelope envelope;
  LayerCreationPlacementMode placementMode = LayerCreationPlacementMode::AfterSelected;
};

class QuickLayerCreationDialog final : public QDialog {
  W_OBJECT(QuickLayerCreationDialog)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit QuickLayerCreationDialog(QWidget* parent = nullptr);
  ~QuickLayerCreationDialog();
  QuickLayerCreationOptions submittedOptions() const;
};

}
