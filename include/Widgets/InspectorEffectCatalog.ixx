module;

#include <QString>

#include <vector>

export module Artifact.Widgets.InspectorEffectCatalog;

import Artifact.Effect.Abstract;

export namespace Artifact {

struct EffectCatalogEntry {
  EffectPipelineStage stage = EffectPipelineStage::Rasterizer;
  QString effectId;
  QString displayName;
  QString category;
  QString description;
  QString keywords;
};

QString stageDisplayName(EffectPipelineStage stage);
std::vector<EffectCatalogEntry> buildEffectCatalogEntries();
bool effectCatalogEntryMatches(const EffectCatalogEntry& entry,
                               const QString& query);

}
