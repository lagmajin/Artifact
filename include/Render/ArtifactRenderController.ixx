module;
#include <cstdint>
#include <memory>
#include <vector>
#include <QColor>
#include <QRectF>
#include <QString>

export module Artifact.Render.Controller;

namespace ArtifactCore {

export enum class RenderCommandType : std::uint8_t {
  Rectangle,
  RectangleOutline,
  Sprite,
};

export struct RenderCommand {
  RenderCommandType type = RenderCommandType::Rectangle;
  QRectF bounds;
  QColor color = QColor(255, 255, 255, 255);
  QString resourceId;
  float opacity = 1.0f;
};

export class ArtifactRenderController {
public:
  ArtifactRenderController();
  ~ArtifactRenderController();

  ArtifactRenderController(const ArtifactRenderController&) = delete;
  ArtifactRenderController& operator=(const ArtifactRenderController&) = delete;
  ArtifactRenderController(ArtifactRenderController&&) noexcept;
  ArtifactRenderController& operator=(ArtifactRenderController&&) noexcept;

  void clear();
  std::size_t commandCount() const;
  bool empty() const;
  std::vector<RenderCommand> takeCommands();

  bool drawRect(const QRectF& bounds, const QColor& color,
               float opacity = 1.0f);
  bool drawRectOutline(const QRectF& bounds, const QColor& color,
                       float opacity = 1.0f);
  bool drawSprite(const QRectF& bounds, const QString& resourceId,
                  float opacity = 1.0f);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

export class ArtifactLayerRenderContext {
public:
  ArtifactLayerRenderContext() = default;
  explicit ArtifactLayerRenderContext(ArtifactRenderController* controller)
      : controller_(controller) {}

  ArtifactRenderController* controller() const { return controller_; }
  void setController(ArtifactRenderController* controller) { controller_ = controller; }

private:
  ArtifactRenderController* controller_ = nullptr;
};

export class ArtifactCompositionRenderContext {
public:
  ArtifactCompositionRenderContext() = default;
  explicit ArtifactCompositionRenderContext(ArtifactRenderController* controller)
      : controller_(controller) {}

  ArtifactRenderController* controller() const { return controller_; }
  void setController(ArtifactRenderController* controller) { controller_ = controller; }

private:
  ArtifactRenderController* controller_ = nullptr;
};

} // namespace ArtifactCore
