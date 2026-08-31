module;

#include <QString>
#include <QVariant>
#include <QVector2D>

#include <utility>
#include <vector>

export module Artifact.Widgets.CompositionRigUndoCommands;

import Artifact.Layer.Abstract;
import Artifact.Layers.Abstract._2D;
import ArtifactCore.Rig2D;
import Undo.UndoManager;

export namespace Artifact {

class RigBoneTransformUndoCommand final : public UndoCommand {
public:
  RigBoneTransformUndoCommand(ArtifactAbstractLayerPtr layer,
                              ArtifactCore::Id boneId,
                              ArtifactCore::BoneTransform before,
                              ArtifactCore::BoneTransform after)
      : layer_(layer), boneId_(std::move(boneId)), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Rig Bone Rotation"); }

private:
  bool apply(const ArtifactCore::BoneTransform &transform) {
    auto layer = layer_.lock();
    auto *rigLayer = layer
        ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
        : nullptr;
    if (!rigLayer || !rigLayer->setRigBoneLocalTransform(boneId_, transform)) {
      return false;
    }
    if (rigLayer->rig2D().rootBone()) {
      rigLayer->rig2D().rootBone()->updateHierarchy();
    }
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  ArtifactCore::Id boneId_;
  ArtifactCore::BoneTransform before_;
  ArtifactCore::BoneTransform after_;
  bool lastOperationSucceeded_ = true;
};

class RigControlValueUndoCommand final : public UndoCommand {
public:
  RigControlValueUndoCommand(ArtifactAbstractLayerPtr layer,
                             ArtifactCore::Id controlId,
                             QVariant before, QVariant after)
      : layer_(layer), controlId_(std::move(controlId)),
        before_(std::move(before)), after_(std::move(after)) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Rig Control Value"); }

private:
  bool apply(const QVariant &value) {
    auto layer = layer_.lock();
    auto *rigLayer = layer
        ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
        : nullptr;
    if (!rigLayer) return false;
    if (auto *control = rigLayer->rig2D().findControl(controlId_)) {
      control->setValue(value);
      if (control->value() != value) return false;
      rigLayer->setDirty(LayerDirtyFlag::Transform);
      rigLayer->changed();
      if (auto *manager = UndoManager::instance()) {
        manager->notifyAnythingChanged();
      }
      return true;
    }
    return false;
  }

  ArtifactAbstractLayerWeak layer_;
  ArtifactCore::Id controlId_;
  QVariant before_;
  QVariant after_;
  bool lastOperationSucceeded_ = true;
};

class RigSkinWeightsUndoCommand final : public UndoCommand {
public:
  RigSkinWeightsUndoCommand(ArtifactAbstractLayerPtr layer,
                            std::vector<ArtifactCore::SkinVertex> before,
                            std::vector<ArtifactCore::SkinVertex> after)
      : layer_(layer), before_(std::move(before)), after_(std::move(after)) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Rig Weight Paint"); }

private:
  bool apply(const std::vector<ArtifactCore::SkinVertex> &vertices) {
    auto layer = layer_.lock();
    auto *rigLayer = layer
        ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
        : nullptr;
    if (!rigLayer || !rigLayer->rig2D().skinMesh()) return false;
    rigLayer->rig2D().skinMesh()->setVertices(vertices);
    if (rigLayer->rig2D().skinMesh()->vertices().size() != vertices.size()) {
      return false;
    }
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  std::vector<ArtifactCore::SkinVertex> before_;
  std::vector<ArtifactCore::SkinVertex> after_;
  bool lastOperationSucceeded_ = true;
};

class RigPoseUndoCommand final : public UndoCommand {
public:
  RigPoseUndoCommand(ArtifactAbstractLayerPtr layer,
                     ArtifactCore::PoseSnapshot before,
                     ArtifactCore::PoseSnapshot after)
      : layer_(layer), before_(std::move(before)), after_(std::move(after)) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Rig Pose"); }

private:
  bool apply(const ArtifactCore::PoseSnapshot &pose) {
    auto layer = layer_.lock();
    auto *rigLayer = layer
        ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
        : nullptr;
    if (!rigLayer) return false;
    ArtifactCore::applyPose(rigLayer->rig2D(), pose);
    if (rigLayer->rig2D().rootBone()) {
      rigLayer->rig2D().rootBone()->updateHierarchy();
    }
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  ArtifactCore::PoseSnapshot before_;
  ArtifactCore::PoseSnapshot after_;
  bool lastOperationSucceeded_ = true;
};

QVariantMap rigPoseToVariantMap(const ArtifactCore::PoseSnapshot &pose) {
  QVariantMap result;
  result.insert(QStringLiteral("name"), pose.name);
  QVariantMap bones;
  for (const auto &[id, transform] : pose.boneTransforms) {
    QVariantList values;
    values << transform.position.x() << transform.position.y()
           << transform.rotation << transform.scale.x() << transform.scale.y();
    bones.insert(id.toString(), values);
  }
  result.insert(QStringLiteral("bones"), bones);
  QVariantMap controls;
  for (const auto &[id, value] : pose.controlValues) {
    if (value.canConvert<QVector2D>()) {
      const QVector2D point = value.value<QVector2D>();
      QVariantMap pointData;
      pointData.insert(QStringLiteral("type"), QStringLiteral("point"));
      pointData.insert(QStringLiteral("x"), point.x());
      pointData.insert(QStringLiteral("y"), point.y());
      controls.insert(id.toString(), pointData);
    } else {
      QVariantMap scalarData;
      scalarData.insert(QStringLiteral("type"), QStringLiteral("scalar"));
      scalarData.insert(QStringLiteral("value"), value);
      controls.insert(id.toString(), scalarData);
    }
  }
  result.insert(QStringLiteral("controls"), controls);
  return result;
}

ArtifactCore::PoseSnapshot rigPoseFromVariantMap(const QVariantMap &data) {
  ArtifactCore::PoseSnapshot pose;
  pose.name = data.value(QStringLiteral("name")).toString();
  const QVariantMap bones = data.value(QStringLiteral("bones")).toMap();
  for (auto it = bones.cbegin(); it != bones.cend(); ++it) {
    const QVariantList values = it.value().toList();
    if (values.size() < 5) continue;
    ArtifactCore::BoneTransform transform;
    transform.position = QVector2D(values[0].toFloat(), values[1].toFloat());
    transform.rotation = values[2].toFloat();
    transform.scale = QVector2D(values[3].toFloat(), values[4].toFloat());
    pose.boneTransforms[ArtifactCore::Id(it.key())] = transform;
  }
  const QVariantMap controls = data.value(QStringLiteral("controls")).toMap();
  for (auto it = controls.cbegin(); it != controls.cend(); ++it) {
    const QVariantMap controlData = it.value().toMap();
    if (controlData.value(QStringLiteral("type")).toString() ==
        QStringLiteral("point")) {
      pose.controlValues[ArtifactCore::Id(it.key())] = QVariant::fromValue(
          QVector2D(controlData.value(QStringLiteral("x")).toFloat(),
                    controlData.value(QStringLiteral("y")).toFloat()));
    } else {
      pose.controlValues[ArtifactCore::Id(it.key())] =
          controlData.value(QStringLiteral("value"));
    }
  }
  return pose;
}

} // namespace Artifact
