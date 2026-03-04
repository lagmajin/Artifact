module;
#include <wobjectimpl.h>
#include <QWidget>
export module Artifact.Widgets.ExpressionCopilotWidget;

export namespace Artifact {
    class ArtifactExpressionCopilotWidget final : public QWidget {
        W_OBJECT(ArtifactExpressionCopilotWidget)
    private:
        class Impl;
        Impl* impl_;
    public:
        explicit ArtifactExpressionCopilotWidget(QWidget* parent = nullptr);
        ~ArtifactExpressionCopilotWidget() override;

        QSize sizeHint() const override;
    };
}
