module;

#include <QJsonArray>
#include <QPointF>

#include <memory>
#include <vector>

module Artifact.Widgets.LayerEditor.ShapeCommands;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Undo.UndoManager;

namespace Artifact {
namespace {
bool sameCustomPathVertices(
    const std::vector<Artifact::CustomPathVertex>& lhs,
    const std::vector<Artifact::CustomPathVertex>& rhs) {
  if (lhs.size() != rhs.size()) {
   return false;
  }
  for (size_t index = 0; index < lhs.size(); ++index) {
   const auto& left = lhs[index];
   const auto& right = rhs[index];
   if (left.pos != right.pos || left.inTangent != right.inTangent ||
       left.outTangent != right.outTangent || left.smooth != right.smooth) {
    return false;
   }
  }
  return true;
}

class ShapeEditCommand final : public Artifact::UndoCommand {public:
 ShapeEditCommand(Artifact::ArtifactAbstractLayerPtr layer,
                  std::vector<QPointF> beforePoints,
                  std::vector<QPointF> afterPoints,
                  bool beforeClosed, bool afterClosed)
     : layer_(layer),
       beforePoints_(std::move(beforePoints)),
       afterPoints_(std::move(afterPoints)),
       beforeClosed_(beforeClosed),
       afterClosed_(afterClosed) {}

  void undo() override {
   lastOperationSucceeded_ = applySnapshot(beforePoints_, beforeClosed_);
  }

 void redo() override {
   lastOperationSucceeded_ = applySnapshot(afterPoints_, afterClosed_);
  }

  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

 QString label() const override {
  return QStringLiteral("Edit Polygon");
 }

private:
  bool applySnapshot(const std::vector<QPointF>& points, bool closed) {
   auto layer = layer_.lock();
   if (!layer) {
    return false;
  }
  auto shape = ArtifactCore::dynamicPointerCast<Artifact::ArtifactShapeLayer>(
      ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
   if (!shape) {
    return false;
  }
  if (points.size() >= 3) {
   shape->setCustomPolygonPoints(points, closed);
  } else {
   shape->clearCustomPolygonPoints();
  }
   if (auto* mgr = Artifact::UndoManager::instance()) {
    mgr->notifyAnythingChanged();
   }
   const auto expected = points.size() >= 3 ? points : std::vector<QPointF>{};
   return shape->customPolygonPoints() == expected &&
          (expected.empty() || shape->customPolygonClosed() == closed);
 }

 Artifact::ArtifactAbstractLayerWeak layer_;
  std::vector<QPointF> beforePoints_;
  std::vector<QPointF> afterPoints_;
  bool beforeClosed_ = true;
  bool afterClosed_ = true;
  bool lastOperationSucceeded_ = true;
};

class CornerRadiusEditCommand final : public Artifact::UndoCommand {
public:
 CornerRadiusEditCommand(Artifact::ArtifactAbstractLayerPtr layer, float before, float after)
     : layer_(std::move(layer)), before_(before), after_(after) {}
  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
 QString label() const override { return QStringLiteral("Edit Corner Radius"); }
private:
  bool apply(float r) {
   auto layer = layer_.lock();
   if (!layer) return false;
   auto shape = ArtifactCore::dynamicPointerCast<Artifact::ArtifactShapeLayer>(
           ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
   if (!shape) return false;
   shape->setCornerRadius(r);
   if (auto* mgr = Artifact::UndoManager::instance()) mgr->notifyAnythingChanged();
   return std::abs(shape->cornerRadius() - r) <= 0.0001f;
 }
  Artifact::ArtifactAbstractLayerWeak layer_;
  float before_, after_;
  bool lastOperationSucceeded_ = true;
};

class StarInnerRadiusEditCommand final : public Artifact::UndoCommand {
public:
 StarInnerRadiusEditCommand(Artifact::ArtifactAbstractLayerPtr layer, float before, float after)
     : layer_(std::move(layer)), before_(before), after_(after) {}
  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
 QString label() const override { return QStringLiteral("Edit Star Inner Radius"); }
private:
  bool apply(float r) {
   auto layer = layer_.lock();
   if (!layer) return false;
   auto shape = ArtifactCore::dynamicPointerCast<Artifact::ArtifactShapeLayer>(
           ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
   if (!shape) return false;
   shape->setStarInnerRadius(r);
   if (auto* mgr = Artifact::UndoManager::instance()) mgr->notifyAnythingChanged();
   return std::abs(shape->starInnerRadius() - r) <= 0.0001f;
 }
  Artifact::ArtifactAbstractLayerWeak layer_;
  float before_, after_;
  bool lastOperationSucceeded_ = true;
};

class PathVertexEditCommand final : public Artifact::UndoCommand {
public:
 PathVertexEditCommand(Artifact::ArtifactAbstractLayerPtr layer,
                       std::vector<Artifact::CustomPathVertex> before,
                       std::vector<Artifact::CustomPathVertex> after,
                       bool beforeClosed, bool afterClosed)
     : layer_(std::move(layer)),
       before_(std::move(before)), after_(std::move(after)),
       beforeClosed_(beforeClosed), afterClosed_(afterClosed) {}
  void undo() override { lastOperationSucceeded_ = apply(before_, beforeClosed_); }
  void redo() override { lastOperationSucceeded_ = apply(after_, afterClosed_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
 QString label() const override { return QStringLiteral("Edit Path Vertices"); }
private:
  bool apply(const std::vector<Artifact::CustomPathVertex>& verts, bool closed) {
   auto layer = layer_.lock();
   if (!layer) return false;
   auto shape = ArtifactCore::dynamicPointerCast<Artifact::ArtifactShapeLayer>(
       ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
   if (!shape) return false;
  if (verts.size() >= 3)
   shape->setCustomPathVertices(verts, closed);
  else
   shape->clearCustomPath();
   if (auto* mgr = Artifact::UndoManager::instance()) mgr->notifyAnythingChanged();
   if (verts.size() < 3) {
    return !shape->hasCustomPath();
   }
   return sameCustomPathVertices(shape->customPathVertices(), verts) &&
          shape->customPathClosed() == closed;
 }
 Artifact::ArtifactAbstractLayerWeak layer_;
 std::vector<Artifact::CustomPathVertex> before_, after_;
  bool beforeClosed_, afterClosed_;
  bool lastOperationSucceeded_ = true;
};

class ShapeConversionCommand final : public Artifact::UndoCommand {
public:
 ShapeConversionCommand(Artifact::ArtifactAbstractLayerPtr layer,
                        std::vector<QPointF> beforePolygon,
                        bool beforePolygonClosed,
                        std::vector<Artifact::CustomPathVertex> beforePath,
                        bool beforePathClosed,
                        std::vector<QPointF> afterPolygon,
                        bool afterPolygonClosed,
                        std::vector<Artifact::CustomPathVertex> afterPath,
                        bool afterPathClosed)
     : layer_(std::move(layer)),
       beforePolygon_(std::move(beforePolygon)),
       beforePolygonClosed_(beforePolygonClosed),
       beforePath_(std::move(beforePath)),
       beforePathClosed_(beforePathClosed),
       afterPolygon_(std::move(afterPolygon)),
       afterPolygonClosed_(afterPolygonClosed),
       afterPath_(std::move(afterPath)),
       afterPathClosed_(afterPathClosed) {}

  void undo() override {
   lastOperationSucceeded_ = apply(beforePolygon_, beforePolygonClosed_,
                                    beforePath_, beforePathClosed_);
  }
  void redo() override {
   lastOperationSucceeded_ = apply(afterPolygon_, afterPolygonClosed_,
                                    afterPath_, afterPathClosed_);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
 QString label() const override { return QStringLiteral("Convert Shape Structure"); }

private:
  bool apply(const std::vector<QPointF>& polygon, bool polygonClosed,
             const std::vector<Artifact::CustomPathVertex>& path, bool pathClosed) {
   auto layer = layer_.lock();
   if (!layer) {
    return false;
  }
  auto shape = ArtifactCore::dynamicPointerCast<Artifact::ArtifactShapeLayer>(
      ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
   if (!shape) {
    return false;
  }
  if (path.size() >= 3) {
   shape->setCustomPathVertices(path, pathClosed);
  } else {
   shape->clearCustomPath();
  }
  if (polygon.size() >= 3) {
   shape->setCustomPolygonPoints(polygon, polygonClosed);
  } else {
   shape->clearCustomPolygonPoints();
  }
   if (auto* mgr = Artifact::UndoManager::instance()) {
    mgr->notifyAnythingChanged();
   }
   const auto expectedPolygon = polygon.size() >= 3
       ? polygon : std::vector<QPointF>{};
   const auto expectedPath = path.size() >= 3
       ? path : std::vector<Artifact::CustomPathVertex>{};
   return shape->customPolygonPoints() == expectedPolygon &&
          sameCustomPathVertices(shape->customPathVertices(), expectedPath) &&
          (expectedPolygon.empty() || shape->customPolygonClosed() == polygonClosed) &&
          (expectedPath.empty() || shape->customPathClosed() == pathClosed);
 }

 Artifact::ArtifactAbstractLayerWeak layer_;
 std::vector<QPointF> beforePolygon_;
 bool beforePolygonClosed_;
 std::vector<Artifact::CustomPathVertex> beforePath_;
 bool beforePathClosed_;
 std::vector<QPointF> afterPolygon_;
 bool afterPolygonClosed_;
  std::vector<Artifact::CustomPathVertex> afterPath_;
  bool afterPathClosed_;
  bool lastOperationSucceeded_ = true;
};

class ShapeOperatorStackCommand final : public Artifact::UndoCommand {
public:
 ShapeOperatorStackCommand(Artifact::ArtifactAbstractLayerPtr layer,
                           QJsonArray before, QJsonArray after)
     : layer_(std::move(layer)), before_(std::move(before)), after_(std::move(after)) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
 QString label() const override { return QStringLiteral("Edit Shape Operator Stack"); }

private:
  bool apply(const QJsonArray& operators) {
   auto layer = layer_.lock();
   auto shape = ArtifactCore::dynamicPointerCast<Artifact::ArtifactShapeLayer>(
       ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
   if (!shape) return false;
   shape->restoreOperatorsFromJson(operators);
   if (auto* mgr = Artifact::UndoManager::instance()) mgr->notifyAnythingChanged();
   return shape->toJson().value(QStringLiteral("shapeOperators")).toArray() == operators;
 }

 Artifact::ArtifactAbstractLayerWeak layer_;
  QJsonArray before_, after_;
  bool lastOperationSucceeded_ = true;
};


} // namespace

std::unique_ptr<UndoCommand> makeShapeEditCommand(
    const ArtifactAbstractLayerPtr& layer,
    std::vector<QPointF> beforePoints,
    std::vector<QPointF> afterPoints,
    bool beforeClosed, bool afterClosed) {
  return std::make_unique<ShapeEditCommand>(
      layer, std::move(beforePoints), std::move(afterPoints),
      beforeClosed, afterClosed);
}

std::unique_ptr<UndoCommand> makeCornerRadiusEditCommand(
    const ArtifactAbstractLayerPtr& layer, float before, float after) {
  return std::make_unique<CornerRadiusEditCommand>(layer, before, after);
}

std::unique_ptr<UndoCommand> makeStarInnerRadiusEditCommand(
    const ArtifactAbstractLayerPtr& layer, float before, float after) {
  return std::make_unique<StarInnerRadiusEditCommand>(layer, before, after);
}

std::unique_ptr<UndoCommand> makePathVertexEditCommand(
    const ArtifactAbstractLayerPtr& layer,
    std::vector<CustomPathVertex> before,
    std::vector<CustomPathVertex> after,
    bool beforeClosed, bool afterClosed) {
  return std::make_unique<PathVertexEditCommand>(
      layer, std::move(before), std::move(after), beforeClosed, afterClosed);
}

std::unique_ptr<UndoCommand> makeShapeConversionCommand(
    const ArtifactAbstractLayerPtr& layer,
    std::vector<QPointF> beforePolygon, bool beforePolygonClosed,
    std::vector<CustomPathVertex> beforePath, bool beforePathClosed,
    std::vector<QPointF> afterPolygon, bool afterPolygonClosed,
    std::vector<CustomPathVertex> afterPath, bool afterPathClosed) {
  return std::make_unique<ShapeConversionCommand>(
      layer, std::move(beforePolygon), beforePolygonClosed,
      std::move(beforePath), beforePathClosed, std::move(afterPolygon),
      afterPolygonClosed, std::move(afterPath), afterPathClosed);
}

std::unique_ptr<UndoCommand> makeShapeOperatorStackCommand(
    const ArtifactAbstractLayerPtr& layer,
    QJsonArray before, QJsonArray after) {
  return std::make_unique<ShapeOperatorStackCommand>(
      layer, std::move(before), std::move(after));
}

} // namespace Artifact
