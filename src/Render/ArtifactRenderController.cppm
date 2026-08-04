module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

module Artifact.Render.Controller;

namespace ArtifactCore {

class ArtifactRenderController::Impl {
public:
  std::vector<RenderCommand> commands;
};

namespace {
float clampedOpacity(const float opacity) {
  return std::clamp(std::isfinite(opacity) ? opacity : 0.0f, 0.0f, 1.0f);
}

bool validBounds(const QRectF& bounds) {
  return bounds.isValid() && std::isfinite(bounds.left()) &&
         std::isfinite(bounds.top()) && std::isfinite(bounds.width()) &&
         std::isfinite(bounds.height());
}
}

ArtifactRenderController::ArtifactRenderController()
    : impl_(std::make_unique<Impl>()) {}

ArtifactRenderController::~ArtifactRenderController() = default;

ArtifactRenderController::ArtifactRenderController(
    ArtifactRenderController&& other) noexcept = default;

ArtifactRenderController& ArtifactRenderController::operator=(
    ArtifactRenderController&& other) noexcept = default;

void ArtifactRenderController::clear() {
  if (impl_) impl_->commands.clear();
}

std::size_t ArtifactRenderController::commandCount() const {
  return impl_ ? impl_->commands.size() : 0;
}

bool ArtifactRenderController::empty() const { return commandCount() == 0; }

std::vector<RenderCommand> ArtifactRenderController::takeCommands() {
  if (!impl_) return {};
  std::vector<RenderCommand> result = std::move(impl_->commands);
  impl_->commands.clear();
  return result;
}

bool ArtifactRenderController::drawRect(const QRectF& bounds,
                                        const QColor& color,
                                        const float opacity) {
  if (!impl_ || !validBounds(bounds) || !color.isValid()) return false;
  impl_->commands.push_back({RenderCommandType::Rectangle, bounds, color, {},
                             clampedOpacity(opacity)});
  return true;
}

bool ArtifactRenderController::drawRectOutline(const QRectF& bounds,
                                               const QColor& color,
                                               const float opacity) {
  if (!impl_ || !validBounds(bounds) || !color.isValid()) return false;
  impl_->commands.push_back({RenderCommandType::RectangleOutline, bounds,
                             color, {}, clampedOpacity(opacity)});
  return true;
}

bool ArtifactRenderController::drawSprite(const QRectF& bounds,
                                          const QString& resourceId,
                                          const float opacity) {
  if (!impl_ || !validBounds(bounds) || resourceId.trimmed().isEmpty())
    return false;
  impl_->commands.push_back({RenderCommandType::Sprite, bounds,
                             QColor(255, 255, 255, 255), resourceId,
                             clampedOpacity(opacity)});
  return true;
}

} // namespace ArtifactCore
