module;
#include <utility>
#include <wobjectimpl.h>
#include <QWidget>
export module Artifact.Widgets.CompositionGraphWidget;

export namespace Artifact {
    class ArtifactCompositionGraphWidget final : public QWidget {
        W_OBJECT(ArtifactCompositionGraphWidget)
    private:
        class Impl;
        Impl* impl_;
    public:
        explicit ArtifactCompositionGraphWidget(QWidget* parent = nullptr);
        ~ArtifactCompositionGraphWidget() override;

        QSize sizeHint() const override;
    };
}
