module;

#include <QString>
#include <vector>

class QDialog;
class QWidget;

export module Artifact.Widgets.InspectorEffectPicker;

import Artifact.Widgets.InspectorEffectCatalog;
import Artifact.Effect.Abstract;

export namespace Artifact {

QDialog* createInspectorEffectPickerDialog(
    const std::vector<EffectCatalogEntry>& entries, EffectPipelineStage stage,
    const QString& targetLabel, QWidget* parent = nullptr);
QString inspectorEffectPickerSelectedEffectId(QDialog* dialog);

} // namespace Artifact
