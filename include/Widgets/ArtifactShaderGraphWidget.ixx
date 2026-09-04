module;
#include <utility>
#include <wobjectimpl.h>
#include <QWidget>
export module Artifact.Widgets.ShaderGraphWidget;

export namespace Artifact {
    class ArtifactShaderGraphWidget final : public QWidget {
        W_OBJECT(ArtifactShaderGraphWidget)
    private:
        class Impl;
        Impl* impl_;
    public:
        explicit ArtifactShaderGraphWidget(QWidget* parent = nullptr);
        ~ArtifactShaderGraphWidget() override;

        QSize sizeHint() const override;
    };
}
