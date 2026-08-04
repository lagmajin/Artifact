module;
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QColor>
#include <cmath>


export module Artifact.AI.RenderAutomation;

import std;
import Core.AI.Describable;
import Artifact.Service.Project;

export namespace Artifact {

class RenderAutomation : public ArtifactCore::IDescribable {
public:
  static void ensureRegistered() {
    static const bool registered = []() {
      ArtifactCore::DescriptionRegistry::instance().registerDescribable(
          QStringLiteral("RenderAutomation"),
          []() -> const ArtifactCore::IDescribable * {
            return &RenderAutomation::instance();
          });
      return true;
    }();
    (void)registered;
  }

  static RenderAutomation &instance() {
    static RenderAutomation automation;
    return automation;
  }

  QString className() const override {
    return QStringLiteral("RenderAutomation");
  }

  ArtifactCore::LocalizedText briefDescription() const override {
    return ArtifactCore::IDescribable::loc(
        "Provides 3D viewport and camera control operations.",
        "Provides 3D viewport and camera control operations.", {});
  }

  ArtifactCore::LocalizedText detailedDescription() const override {
    return ArtifactCore::IDescribable::loc(
        "This tool enables AI to control 3D viewport settings, camera "
        "positioning, "
        "render modes, and visualization options for 3D scenes.",
        "This tool enables AI to control 3D viewport settings, camera "
        "positioning, "
        "render modes, and visualization options for 3D scenes.",
        {});
  }

  QList<ArtifactCore::MethodDescription> methodDescriptions() const override {
    using ArtifactCore::IDescribable;
    return {
        {"setCameraPosition",
         IDescribable::loc("Set the 3D camera position.",
                           "Set the 3D camera position.", {}),
         "bool",
         {QStringLiteral("double"), QStringLiteral("double"),
          QStringLiteral("double")},
         {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z")}},
        {"setCameraRotation",
         IDescribable::loc("Set the 3D camera rotation.",
                           "Set the 3D camera rotation.", {}),
         "bool",
         {QStringLiteral("double"), QStringLiteral("double"),
          QStringLiteral("double")},
         {QStringLiteral("rx"), QStringLiteral("ry"), QStringLiteral("rz")}},
        {"setRenderMode",
         IDescribable::loc("Set the render mode for 3D layers.",
                           "Set the render mode for 3D layers.", {}),
         "bool",
         {QStringLiteral("QString")},
         {QStringLiteral("mode")}},
        {"toggleWireframe",
         IDescribable::loc("Toggle wireframe rendering.",
                           "Toggle wireframe rendering.", {}),
         "bool"},
        {"setViewportBackground",
         IDescribable::loc("Set viewport background color.",
                           "Set viewport background color.", {}),
         "bool",
         {QStringLiteral("QString")},
         {QStringLiteral("color")}},
        {"focusOnLayer",
         IDescribable::loc("Focus camera on a specific layer.",
                           "Focus camera on a specific layer.", {}),
         "bool",
         {QStringLiteral("QString")},
         {QStringLiteral("layerId")}},
    };
  }

  QVariant invokeMethod(QStringView methodName,
                        const QVariantList &args) override {
    if (methodName == "setCameraPosition") {
      return setCameraPosition(args);
    } else if (methodName == "setCameraRotation") {
      return setCameraRotation(args);
    } else if (methodName == "setRenderMode") {
      return setRenderMode(args);
    } else if (methodName == "toggleWireframe") {
      return toggleWireframe(args);
    } else if (methodName == "setViewportBackground") {
      return setViewportBackground(args);
    } else if (methodName == "focusOnLayer") {
      return focusOnLayer(args);
    }
    return QVariant();
  }

  double cameraX() const { return cameraX_; }
  double cameraY() const { return cameraY_; }
  double cameraZ() const { return cameraZ_; }
  double rotationX() const { return rotationX_; }
  double rotationY() const { return rotationY_; }
  double rotationZ() const { return rotationZ_; }
  QString renderMode() const { return renderMode_; }
  bool wireframeEnabled() const { return wireframe_; }
  QColor viewportBackground() const { return viewportBackground_; }
  QString focusedLayerId() const { return focusedLayerId_; }

private:
  QVariant setCameraPosition(const QVariantList &args) {
    if (args.size() < 3)
      return false;
    double x = args[0].toDouble();
    double y = args[1].toDouble();
    double z = args[2].toDouble();

    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
      return false;
    cameraX_ = x;
    cameraY_ = y;
    cameraZ_ = z;
    return true;
  }

  QVariant setCameraRotation(const QVariantList &args) {
    if (args.size() < 3)
      return false;
    double rx = args[0].toDouble();
    double ry = args[1].toDouble();
    double rz = args[2].toDouble();

    if (!std::isfinite(rx) || !std::isfinite(ry) || !std::isfinite(rz))
      return false;
    rotationX_ = rx;
    rotationY_ = ry;
    rotationZ_ = rz;
    return true;
  }

  QVariant setRenderMode(const QVariantList &args) {
    if (args.isEmpty())
      return false;
    const QString mode = args[0].toString().trimmed().toLower();
    if (mode != QStringLiteral("solid") &&
        mode != QStringLiteral("wireframe") &&
        mode != QStringLiteral("material") &&
        mode != QStringLiteral("shaded"))
      return false;
    renderMode_ = mode;
    wireframe_ = mode == QStringLiteral("wireframe");
    return true;
  }

  QVariant toggleWireframe(const QVariantList &args) {
    Q_UNUSED(args)
    wireframe_ = !wireframe_;
    if (wireframe_)
      renderMode_ = QStringLiteral("wireframe");
    else if (renderMode_ == QStringLiteral("wireframe"))
      renderMode_ = QStringLiteral("solid");
    return true;
  }

  QVariant setViewportBackground(const QVariantList &args) {
    if (args.isEmpty())
      return false;
    const QColor color(args[0].toString().trimmed());
    if (!color.isValid())
      return false;
    viewportBackground_ = color;
    return true;
  }

  QVariant focusOnLayer(const QVariantList &args) {
    if (args.isEmpty())
      return false;
    const QString layerId = args[0].toString().trimmed();
    if (layerId.isEmpty())
      return false;
    auto *service = ArtifactProjectService::instance();
    if (!service) return false;
    const auto composition = service->currentComposition().lock();
    if (!composition || !composition->layerById(ArtifactCore::LayerID(layerId))) {
      return false;
    }
    focusedLayerId_ = layerId;
    return true;
  }

  double cameraX_ = 0.0;
  double cameraY_ = 0.0;
  double cameraZ_ = 10.0;
  double rotationX_ = 0.0;
  double rotationY_ = 0.0;
  double rotationZ_ = 0.0;
  QString renderMode_ = QStringLiteral("solid");
  bool wireframe_ = false;
  QColor viewportBackground_ = QColor(32, 32, 32);
  QString focusedLayerId_;
};

} // namespace Artifact
