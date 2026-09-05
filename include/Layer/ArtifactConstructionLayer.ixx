module;
#include <utility>
#include <cstddef>
#include <QString>
#include <QJsonObject>
#include <QVariant>
#include <QPointF>
#include <vector>

export module Artifact.Layer.Construction;

import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;
import Core.ArtifactArray;

export namespace Artifact {

enum class ConstructionItemType { Line, Circle, Annotation };

struct ConstructionItem {
  QString id;
  ConstructionItemType type = ConstructionItemType::Line;
  QPointF start;
  QPointF end;
  QPointF center;
  double radius = 24.0;
  QString text;
  bool enabled = true;
  double opacity = 1.0;
  QJsonObject toJson() const;
  static ConstructionItem fromJson(const QJsonObject& obj);
};

class ArtifactConstructionLayer : public ArtifactAbstract2DLayer {
private:
  class Impl;
  Impl* impl_;

public:
  ArtifactConstructionLayer();
  ~ArtifactConstructionLayer();

  void draw(ArtifactIRenderer* renderer) override;
  bool isNullLayer() const override;
  bool hasVideo() const override;
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  bool isConstructionLayer() const override;
  bool shouldIncludeInFinalRender() const override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
  GuideSet constructionGuideSet() const;
  ArtifactCore::Array<QPointF> constructionSnapPoints() const;
  void setConstructionGuideSet(const GuideSet& guideSet);
  void addConstructionGuide(const GuideDefinition& guide);
  void clearConstructionGuides();
  const std::vector<ConstructionItem>& constructionItems() const;
  bool setConstructionItem(size_t index, const ConstructionItem& item);
  void setConstructionItems(const std::vector<ConstructionItem>& items);
  void addConstructionItem(const ConstructionItem& item);
  void clearConstructionItems();
};

} // namespace Artifact
