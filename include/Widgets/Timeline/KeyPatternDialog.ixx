module;

#include <QDialog>
#include <QString>
#include <QVector>
#include <functional>
#include <wobjectdefs.h>

export module Artifact.Widgets.Timeline.KeyPatternDialog;

import Animation.KeyframePatternGenerator;

export namespace Artifact {

class KeyPatternDialog : public QDialog {
  W_OBJECT(KeyPatternDialog)
public:
  explicit KeyPatternDialog(
      QWidget *parent = nullptr,
      std::function<void(const ArtifactCore::KeyframePatternRequest &)> applyCallback = {},
      const ArtifactCore::KeyframePatternRequest &initialRequest = {});
  ~KeyPatternDialog() override;

  void setRequest(const ArtifactCore::KeyframePatternRequest &request);
  ArtifactCore::KeyframePatternRequest request() const;

private:
  class Impl;
  Impl *impl_ = nullptr;
  void refreshPreview();
  void applyCurrentRequest();
};

} // namespace Artifact
