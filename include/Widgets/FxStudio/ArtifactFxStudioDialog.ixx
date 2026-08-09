module;
#include <QDialog>
export module Artifact.Widgets.FxStudio.Dialog;
export namespace Artifact { class ArtifactFxStudioDialog final : public QDialog { public: explicit ArtifactFxStudioDialog(QWidget* parent = nullptr); ~ArtifactFxStudioDialog() override; private: class Impl; Impl* impl_ = nullptr; }; }
