module;

#include <cmath>
#include <limits>

module Artifact.Widgets.LayerEditor.ViewWheelController;

namespace Artifact {

LayerEditorViewWheelResult LayerEditorViewWheelController::handle(
    const LayerEditorViewWheelInput& input) const
{
 if (input.shiftModifier) {
  const int rawHorizontalDelta = input.angleDelta.x() != 0
      ? input.angleDelta.x()
      : input.pixelDelta.x();
  const int rawVerticalDelta = input.angleDelta.y() != 0
      ? input.angleDelta.y()
      : input.pixelDelta.y();
  const float horizontalDelta = rawHorizontalDelta != 0
      ? (input.angleDelta.x() != 0
          ? static_cast<float>(rawHorizontalDelta)
          : static_cast<float>(rawHorizontalDelta) * 2.5f)
      : (input.angleDelta.y() != 0
          ? static_cast<float>(rawVerticalDelta)
          : static_cast<float>(rawVerticalDelta) * 2.5f);
  return {LayerEditorViewWheelAction::PanHorizontal, horizontalDelta};
 }

 // Match Composition View's trackpad normalization. Some platforms do not
 // provide the traditional 120-unit angle delta at all.
 const float steps = input.angleDelta.y() != 0
     ? static_cast<float>(input.angleDelta.y()) / 120.0f
     : static_cast<float>(input.pixelDelta.y()) / 48.0f;
 if (std::abs(steps) <= std::numeric_limits<float>::epsilon()) return {};
 return {LayerEditorViewWheelAction::Zoom, std::pow(1.1f, steps)};
}

}
