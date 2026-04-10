module;

#include <QWidget>

export module Artifact.Widgets.Dialog.FloatColorPickerHooks;

export namespace Artifact {

void installSliderJumpBehavior(QWidget* pickerRoot);
void installFloatColorPickerSliderJump(QWidget* pickerRoot);

}
