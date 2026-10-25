module;

#include <functional>
#include <memory>
#include <QStringList>

module Artifact.Project.Manager;

import Artifact.Diagnostics.AppValidationRules;
import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Core.Diagnostics.DiagnosticEngine;
import Core.Diagnostics.ProjectDiagnostic;

namespace Artifact {

void appendArtifactProjectValidationDiagnostics(
    const ArtifactProjectPtr& projectPtr,
    QStringList& warnings,
    QStringList& errors)
{
    if (!projectPtr) {
        return;
    }

    static ArtifactCore::DiagnosticEngine engine;
    static const bool initialized = [] {
        engine.ruleRegistry().clearRules();
        engine.ruleRegistry().registerRule(
            std::make_unique<ArtifactMissingFileRule>());
        engine.ruleRegistry().registerRule(
            std::make_unique<ArtifactPerformanceRule>());
        engine.ruleRegistry().registerRule(
            std::make_unique<ArtifactMatteReferenceRule>());
        return true;
    }();
    (void)initialized;

    const auto items = projectPtr->projectItems();
    for (auto* root : items) {
        std::function<void(ProjectItem*)> walk = [&](ProjectItem* item) {
            if (!item) {
                return;
            }
            if (item->type() == eProjectItemType::Composition) {
                auto* compItem = static_cast<CompositionItem*>(item);
                const auto result = projectPtr->findComposition(
                    compItem->compositionId);
                if (result.success) {
                    if (auto composition = result.ptr.lock()) {
                        const auto validation = engine.validateAll(
                            composition.get());
                        for (const auto& diagnostic :
                             validation.getDiagnostics()) {
                            if (diagnostic.isError() &&
                                diagnostic.getCategory() ==
                                    ArtifactCore::DiagnosticCategory::File) {
                                warnings.append(diagnostic.getMessage());
                            } else if (diagnostic.isError()) {
                                errors.append(diagnostic.getMessage());
                            } else if (diagnostic.isWarning()) {
                                warnings.append(diagnostic.getMessage());
                            }
                        }
                    }
                }
            }
            for (auto* child : item->children) {
                walk(child);
            }
        };
        walk(root);
    }
}

} // namespace Artifact
