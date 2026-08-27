#include <algorithm>
#include <iterator>
#include <utility>

module Artifact.Composition.Nodes;

namespace Artifact {

bool CompositionNode::isValid() const {
  return !id.trimmed().isEmpty();
}

QJsonObject CompositionNode::toJson() const {
  QJsonObject obj;
  obj[QStringLiteral("id")] = id;
  obj[QStringLiteral("parentId")] = parentId;
  obj[QStringLiteral("kind")] = static_cast<int>(kind);
  obj[QStringLiteral("order")] = order;
  obj[QStringLiteral("properties")] = properties;
  return obj;
}

CompositionNode CompositionNode::fromJson(const QJsonObject& obj) {
  CompositionNode node;
  node.id = obj.value(QStringLiteral("id")).toString();
  node.parentId = obj.value(QStringLiteral("parentId")).toString();
  const int rawKind = obj.value(QStringLiteral("kind")).toInt(0);
  node.kind = rawKind >= 0 && rawKind <= 2
      ? static_cast<CompositionNodeKind>(rawKind)
      : CompositionNodeKind::Layer;
  node.order = obj.value(QStringLiteral("order")).toInt(0);
  node.properties = obj.value(QStringLiteral("properties")).toObject();
  return node;
}

ContainerNode::ContainerNode(CompositionNode node)
    : node_(std::move(node)) {
  if (node_.kind == CompositionNodeKind::Layer) {
    node_.kind = CompositionNodeKind::Container;
  }
}

const CompositionNode& ContainerNode::node() const { return node_; }

void ContainerNode::setParentId(const QString& parentId) {
  node_.parentId = parentId;
}

const std::vector<QString>& ContainerNode::children() const { return children_; }

bool ContainerNode::addChild(const QString& childId) {
  const QString normalized = childId.trimmed();
  if (normalized.isEmpty() || normalized == node_.id || containsChild(normalized)) {
    return false;
  }
  children_.push_back(normalized);
  return true;
}

bool ContainerNode::removeChild(const QString& childId) {
  const auto it = std::find(children_.begin(), children_.end(), childId);
  if (it == children_.end()) return false;
  children_.erase(it);
  return true;
}

bool ContainerNode::moveChild(const QString& childId, int targetIndex) {
  const auto it = std::find(children_.begin(), children_.end(), childId);
  if (it == children_.end() || targetIndex < 0 ||
      targetIndex >= static_cast<int>(children_.size())) {
    return false;
  }
  const int currentIndex = static_cast<int>(std::distance(children_.begin(), it));
  if (currentIndex == targetIndex) return true;
  const QString value = *it;
  children_.erase(it);
  children_.insert(children_.begin() + targetIndex, value);
  return true;
}

bool ContainerNode::containsChild(const QString& childId) const {
  return std::find(children_.begin(), children_.end(), childId) != children_.end();
}

QJsonObject ContainerNode::toJson() const {
  QJsonObject obj = node_.toJson();
  QJsonArray children;
  for (const auto& child : children_) children.append(child);
  obj[QStringLiteral("children")] = children;
  return obj;
}

ContainerNode ContainerNode::fromJson(const QJsonObject& obj) {
  ContainerNode result(CompositionNode::fromJson(obj));
  for (const auto value : obj.value(QStringLiteral("children")).toArray()) {
    if (value.isString()) result.addChild(value.toString());
  }
  return result;
}

GroupContainerNode::GroupContainerNode(CompositionNode node)
    : ContainerNode([&node] {
        node.kind = CompositionNodeKind::GroupContainer;
        return node;
      }()) {}

GroupContainerOutputMode GroupContainerNode::outputMode() const { return outputMode_; }

void GroupContainerNode::setOutputMode(GroupContainerOutputMode mode) {
  outputMode_ = static_cast<GroupContainerOutputMode>(
      std::clamp(static_cast<int>(mode), 0, 2));
}

const QString& GroupContainerNode::activeChildId() const { return activeChildId_; }

void GroupContainerNode::setActiveChildId(const QString& id) {
  activeChildId_ = id.trimmed();
}

bool GroupContainerNode::enabled() const { return enabled_; }

void GroupContainerNode::setEnabled(bool enabled) { enabled_ = enabled; }

double GroupContainerNode::opacity() const { return opacity_; }

void GroupContainerNode::setOpacity(double opacity) {
  opacity_ = std::clamp(opacity, 0.0, 1.0);
}

const QString& GroupContainerNode::blendMode() const { return blendMode_; }

void GroupContainerNode::setBlendMode(const QString& blendMode) {
  const QString normalized = blendMode.trimmed().toLower();
  blendMode_ = normalized.isEmpty() ? QStringLiteral("normal") : normalized;
}

QJsonObject GroupContainerNode::toJson() const {
  QJsonObject obj = ContainerNode::toJson();
  obj[QStringLiteral("outputMode")] = static_cast<int>(outputMode_);
  obj[QStringLiteral("activeChildId")] = activeChildId_;
  obj[QStringLiteral("enabled")] = enabled_;
  obj[QStringLiteral("opacity")] = opacity_;
  obj[QStringLiteral("blendMode")] = blendMode_;
  return obj;
}

GroupContainerNode GroupContainerNode::fromJson(const QJsonObject& obj) {
  GroupContainerNode result(CompositionNode::fromJson(obj));
  result.setOutputMode(static_cast<GroupContainerOutputMode>(
      obj.value(QStringLiteral("outputMode")).toInt(0)));
  result.setActiveChildId(obj.value(QStringLiteral("activeChildId")).toString());
  result.setEnabled(obj.value(QStringLiteral("enabled")).toBool(true));
  result.setOpacity(obj.value(QStringLiteral("opacity")).toDouble(1.0));
  result.setBlendMode(obj.value(QStringLiteral("blendMode")).toString());
  for (const auto value : obj.value(QStringLiteral("children")).toArray()) {
    if (value.isString()) result.addChild(value.toString());
  }
  return result;
}

bool CompositionNodeStore::contains(const QString& id) const {
  return node(id) != nullptr;
}

const CompositionNode* CompositionNodeStore::node(const QString& id) const {
  const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                               [&id](const CompositionNode& value) { return value.id == id; });
  return it == nodes_.end() ? nullptr : &*it;
}

bool CompositionNodeStore::addNode(const CompositionNode& nodeValue) {
  CompositionNode normalized = nodeValue;
  normalized.id = normalized.id.trimmed();
  normalized.parentId = normalized.parentId.trimmed();
  if (!normalized.isValid() || contains(normalized.id) ||
      (!normalized.parentId.isEmpty() && !contains(normalized.parentId)) ||
      normalized.parentId == normalized.id) {
    return false;
  }
  nodes_.push_back(std::move(normalized));
  return true;
}

bool CompositionNodeStore::wouldCreateCycle(const QString& id, const QString& parentId) const {
  QString current = parentId;
  while (!current.isEmpty()) {
    if (current == id) return true;
    const auto* currentNode = node(current);
    if (!currentNode) return false;
    current = currentNode->parentId;
  }
  return false;
}

bool CompositionNodeStore::setParent(const QString& id, const QString& parentId) {
  auto it = std::find_if(nodes_.begin(), nodes_.end(),
                         [&id](const CompositionNode& value) { return value.id == id; });
  const QString normalizedParent = parentId.trimmed();
  if (it == nodes_.end() || (normalizedParent != id && !normalizedParent.isEmpty() &&
                             !contains(normalizedParent)) ||
      normalizedParent == id || wouldCreateCycle(id, normalizedParent)) {
    return false;
  }
  it->parentId = normalizedParent;
  return true;
}

bool CompositionNodeStore::setOrder(const QString& id, int order) {
  auto it = std::find_if(nodes_.begin(), nodes_.end(),
                         [&id](const CompositionNode& value) { return value.id == id; });
  if (it == nodes_.end() || order < 0) return false;
  it->order = order;
  return true;
}

bool CompositionNodeStore::setProperties(const QString& id, const QJsonObject& properties) {
  auto it = std::find_if(nodes_.begin(), nodes_.end(),
                         [&id](const CompositionNode& value) { return value.id == id; });
  if (it == nodes_.end()) return false;
  for (auto property = properties.constBegin(); property != properties.constEnd(); ++property) {
    it->properties.insert(property.key(), property.value());
  }
  return true;
}

bool CompositionNodeStore::getGroupContainer(const QString& id,
                                             GroupContainerNode& out) const {
  const auto* stored = node(id);
  if (!stored || stored->kind != CompositionNodeKind::GroupContainer) return false;

  GroupContainerNode result(*stored);
  for (const auto& childId : childrenOf(id)) result.addChild(childId);
  const auto& properties = stored->properties;
  result.setOutputMode(static_cast<GroupContainerOutputMode>(
      properties.value(QStringLiteral("outputMode")).toInt(0)));
  result.setActiveChildId(
      properties.value(QStringLiteral("activeChildId")).toString());
  result.setEnabled(properties.value(QStringLiteral("enabled")).toBool(true));
  result.setOpacity(properties.value(QStringLiteral("opacity")).toDouble(1.0));
  result.setBlendMode(properties.value(QStringLiteral("blendMode")).toString());
  out = std::move(result);
  return true;
}

bool CompositionNodeStore::removeNode(const QString& id) {
  const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                               [&id](const CompositionNode& value) { return value.id == id; });
  if (it == nodes_.end()) return false;
  for (auto& value : nodes_) {
    if (value.parentId == id) value.parentId.clear();
  }
  nodes_.erase(it);
  return true;
}

std::vector<QString> CompositionNodeStore::childrenOf(const QString& parentId) const {
  std::vector<QString> result;
  for (const auto& value : nodes_) {
    if (value.parentId == parentId) result.push_back(value.id);
  }
  std::sort(result.begin(), result.end(), [this](const QString& left, const QString& right) {
    const auto* leftNode = node(left);
    const auto* rightNode = node(right);
    return leftNode && rightNode ? leftNode->order < rightNode->order : left < right;
  });
  return result;
}

const std::vector<CompositionNode>& CompositionNodeStore::nodes() const { return nodes_; }

QJsonArray CompositionNodeStore::toJson() const {
  QJsonArray result;
  for (const auto& value : nodes_) result.append(value.toJson());
  return result;
}

CompositionNodeStore CompositionNodeStore::fromJson(const QJsonArray& array) {
  CompositionNodeStore result;
  for (const auto value : array) {
    if (value.isObject()) result.addNode(CompositionNode::fromJson(value.toObject()));
  }
  return result;
}

} // namespace Artifact
