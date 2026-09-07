module;

#include <QString>
#include <QWidget>

export module Artifact.Widgets.SplashScreen;

export namespace Artifact {

// Startup-only presentation surface. AppMain deliberately does not construct
// it yet; the future startup coordinator owns its lifetime and progress steps.
class ArtifactSplashScreen final : public QWidget
{
public:
  explicit ArtifactSplashScreen(QWidget* parent = nullptr);
  ~ArtifactSplashScreen() override;

  void setProgress(int percent);
  [[nodiscard]] int progress() const;
  void setStatusText(const QString& text);

private:
  class Impl;
  Impl* impl_ = nullptr;
};

} // namespace Artifact
