module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QColor>
#include <QFont>
#include <QPalette>
module Artifact.Widgets.QuickToolBox;

import Artifact.Widgets.QuickToolBox;
import Artifact.Core.Theme;

namespace Artifact {

W_OBJECT_IMPL(ArtifactQuickToolBox)

class ArtifactQuickToolBox::Impl {
public:
    QTabWidget* tabWidget = nullptr;
};

ArtifactQuickToolBox::ArtifactQuickToolBox(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Theme setup
    setAutoFillBackground(true);
    QPalette widgetPalette = palette();
    widgetPalette.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
    widgetPalette.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    setPalette(widgetPalette);

    // Tab widget
    impl_->tabWidget = new QTabWidget(this);

    // TODO: 各タブに便利ツールを追加
    // impl_->tabWidget->addTab(createAnchorPointTool(), "Anchor");
    // impl_->tabWidget->addTab(createAlignmentTool(), "Align");
    // impl_->tabWidget->addTab(createTransformTool(), "Transform");
    // impl_->tabWidget->addTab(createCalculatorTool(), "Calc");

    root->addWidget(impl_->tabWidget);
}

ArtifactQuickToolBox::~ArtifactQuickToolBox()
{
    delete impl_;
}

} // namespace Artifact
