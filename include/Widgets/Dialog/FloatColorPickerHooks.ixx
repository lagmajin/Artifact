module;

#include <QWidget>

export module Artifact.Widgets.Dialog.FloatColorPickerHooks;

export namespace Artifact {

enum class ColorSelectionPurpose { Creation, Edit };

// Configure after setInitialColor and before exec; callers retain commit/rollback ownership.
void configureFloatColorPicker(QWidget* pickerRoot, ColorSelectionPurpose purpose);

void installSliderJumpBehavior(QWidget* pickerRoot);
void installFloatColorPickerSliderJump(QWidget* pickerRoot);

}
