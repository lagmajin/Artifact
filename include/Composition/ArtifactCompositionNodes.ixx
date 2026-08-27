module;
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <vector>

export module Artifact.Composition.Nodes;

export namespace Artifact {

enum class CompositionNodeKind {
  Layer = 0,
  Container = 1,
  GroupContainer = 2
};

struct CompositionNode {
  QString id;
  QString parentId;
  CompositionNodeKind kind = CompositionNodeKind::Layer;
  int order = 0;
  QJsonObject properties;

  bool isValid() const;
  QJsonObject toJson() const;
  static CompositionNode fromJson(const QJsonObject& obj);
};

class ContainerNode {
public:
  explicit ContainerNode(CompositionNode node = {});

  const CompositionNode& node() const;
  void setParentId(const QString& parentId);
  const std::vector<QString>& children() const;
  bool addChild(const QString& childId);
  bool removeChild(const QString& childId);
  bool moveChild(const QString& childId, int targetIndex);
  bool containsChild(const QString& childId) const;
  QJsonObject toJson() const;
  static ContainerNode fromJson(const QJsonObject& obj);

private:
  CompositionNode node_;
  std::vector<QString> children_;
};

enum class GroupContainerOutputMode { All = 0, Single = 1, Share = 2 };

class GroupContainerNode final : public ContainerNode {
public:
  explicit GroupContainerNode(CompositionNode node = {});
  GroupContainerOutputMode outputMode() const;
  void setOutputMode(GroupContainerOutputMode mode);
  const QString& activeChildId() const;
  void setActiveChildId(const QString& id);
  bool enabled() const;
  void setEnabled(bool enabled);
  double opacity() const;
  void setOpacity(double opacity);
  const QString& blendMode() const;
  void setBlendMode(const QString& blendMode);
  QJsonObject toJson() const;
  static GroupContainerNode fromJson(const QJsonObject& obj);

private:
  GroupContainerOutputMode outputMode_ = GroupContainerOutputMode::All;
  QString activeChildId_;
  bool enabled_ = true;
  double opacity_ = 1.0;
  QString blendMode_ = QStringLiteral("normal");
};

class CompositionNodeStore {
public:
  bool addNode(const CompositionNode& node);
  bool removeNode(const QString& id);
  bool contains(const QString& id) const;
  const CompositionNode* node(const QString& id) const;
  bool setParent(const QString& id, const QString& parentId);
  bool setOrder(const QString& id, int order);
  bool setProperties(const QString& id, const QJsonObject& properties);
  bool getGroupContainer(const QString& id, GroupContainerNode& out) const;
  std::vector<QString> childrenOf(const QString& parentId) const;
  const std::vector<CompositionNode>& nodes() const;
  QJsonArray toJson() const;
  static CompositionNodeStore fromJson(const QJsonArray& array);

private:
  bool wouldCreateCycle(const QString& id, const QString& parentId) const;
  std::vector<CompositionNode> nodes_;
};

} // namespace Artifact
