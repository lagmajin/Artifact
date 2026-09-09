module;
#include <cmath>
#include <QDialog>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMouseEvent>
#include <QShowEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <algorithm>
#include <wobjectimpl.h>
module Artifact.Widgets.ResolutionRemapDialog;

import Geometry.ResolutionRemap;

namespace Artifact {

using namespace ArtifactCore;

class AspectPreviewWidget : public QWidget {
public:
    QSize oldSize_;
    QSize newSize_;
    QColor oldColor_ = QColor(100, 140, 255, 160);
    QColor newColor_ = QColor(255, 160, 80, 120);

    AspectPreviewWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setMinimumHeight(100);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (oldSize_.isEmpty() || newSize_.isEmpty()) return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int w = width() - 20;
        const int h = height() - 20;
        const int cx = width() / 2;
        const int cy = height() / 2;

        const auto fitAspect = [w, h](const QSize& size) {
            const double aspect = static_cast<double>(size.width()) / size.height();
            const double availableAspect = static_cast<double>(w) / h;
            const int fittedWidth = availableAspect > aspect
                ? static_cast<int>(h * aspect) : w;
            const int fittedHeight = availableAspect > aspect
                ? h : static_cast<int>(w / aspect);
            return QSize(std::max(fittedWidth, 20), std::max(fittedHeight, 20));
        };
        const QSize oldPreviewSize = fitAspect(oldSize_);
        const QSize newPreviewSize = fitAspect(newSize_);
        const int oldW = oldPreviewSize.width();
        const int oldH = oldPreviewSize.height();
        const int newW = newPreviewSize.width();
        const int newH = newPreviewSize.height();

        // Draw old aspect (blue, filled)
        QRect oldRect(cx - oldW / 2, cy - oldH / 2, oldW, oldH);
        p.setPen(QPen(oldColor_.darker(130), 2));
        p.setBrush(oldColor_);
        p.drawRoundedRect(oldRect, 4, 4);
        p.setPen(oldColor_.darker(180));
        p.drawText(oldRect, Qt::AlignCenter,
            QStringLiteral("%1×%2").arg(oldSize_.width()).arg(oldSize_.height()));

        // Draw new aspect (orange, outline)
        QRect newRect(cx - newW / 2, cy - newH / 2, newW, newH);
        p.setPen(QPen(newColor_, 2, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(newRect, 4, 4);

        // Labels
        p.setPen(oldColor_.darker(200));
        p.drawText(QRectF(10, 5, 100, 20), Qt::AlignLeft, QStringLiteral("Old"));
        p.setPen(newColor_.lighter(120));
        p.drawText(QRectF(width() - 110, 5, 100, 20), Qt::AlignRight, QStringLiteral("New"));
    }
};

class ArtifactResolutionRemapDialog::Impl {
public:
    QSize oldSize_;
    QSize newSize_;
    RemapImpact impact_;
    QListWidget* policyList_ = nullptr;
    AspectPreviewWidget* preview_ = nullptr;
    bool remapRequested_ = false;

    QString policyLabel(RemapPolicy p) const {
        switch (p) {
        case RemapPolicy::CenterLocked:   return QStringLiteral("Center Locked — scale from center");
        case RemapPolicy::TopLeftLocked:  return QStringLiteral("Top-Left Locked — scale from origin");
        case RemapPolicy::StretchToFit:   return QStringLiteral("Stretch To Fit — fill new aspect");
        case RemapPolicy::FitWithPadding: return QStringLiteral("Fit With Padding — letterbox");
        case RemapPolicy::FitWithCrop:    return QStringLiteral("Fit With Crop — crop excess");
        }
        return {};
    }
};

ArtifactResolutionRemapDialog::ArtifactResolutionRemapDialog(
    const QSize& oldSize,
    const QSize& newSize,
    const RemapImpact& impact,
    QWidget* parent)
    : QDialog(parent)
    , impl_(new Impl)
{
    impl_->oldSize_ = oldSize;
    impl_->newSize_ = newSize;
    impl_->impact_ = impact;

    setWindowTitle(QStringLiteral("Resolution Change — Remap Wizard"));
    setAccessibleName(QStringLiteral("Resolution remap"));
    setAccessibleDescription(QStringLiteral("Choose how masks, keyframes, and anchors adapt to the new resolution"));
    setMinimumSize(820, 600);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 18);
    mainLayout->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("Resolution Change — Remap Wizard"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(14);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    auto* headerLabel = new QLabel(QStringLiteral(
        "%1 × %2  (%5)    →    %3 × %4  (%6)")
        .arg(oldSize.width()).arg(oldSize.height())
        .arg(newSize.width()).arg(newSize.height())
        .arg(impact.oldAspectRatio, 0, 'f', 3)
        .arg(impact.newAspectRatio, 0, 'f', 3), this);
    QFont summaryFont = headerLabel->font();
    summaryFont.setBold(true);
    summaryFont.setPointSize(11);
    headerLabel->setFont(summaryFont);
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);

    auto* comparisonRow = new QHBoxLayout();
    comparisonRow->setSpacing(14);
    auto* previewPanel = new QGroupBox(QStringLiteral("OLD VS NEW FRAME"), this);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    impl_->preview_ = new AspectPreviewWidget(this);
    impl_->preview_->oldSize_ = oldSize;
    impl_->preview_->newSize_ = newSize;
    previewLayout->addWidget(impl_->preview_);
    comparisonRow->addWidget(previewPanel, 3);

    auto* impactGroup = new QGroupBox(QStringLiteral("IMPACT"), this);
        auto* impactLayout = new QVBoxLayout(impactGroup);

        QStringList details;
        if (impact.hasMaskPaths) {
            details << QStringLiteral("• Mask paths will be remapped (%1 vertices)")
                       .arg(impact.maskVertexCount);
        }
        if (impact.keyframeCount > 0) {
            details << QStringLiteral("• %1 keyframe tracks will be repositioned")
                       .arg(impact.keyframeCount);
        }
        if (impact.hasAnchorPoints) {
            details << QStringLiteral("• Anchor points will be recalculated");
        }
        if (details.isEmpty()) {
            details << QStringLiteral("• No coordinate-dependent data detected");
        }

        auto* impactLabel = new QLabel(details.join(QStringLiteral("<br>")));
        impactLabel->setWordWrap(true);
        impactLayout->addWidget(impactLabel);
        comparisonRow->addWidget(impactGroup, 2);
    mainLayout->addLayout(comparisonRow, 1);

    // Policy selector
    auto* policyGroup = new QGroupBox(QStringLiteral("Remap Policy"));
    auto* policyLayout = new QVBoxLayout(policyGroup);

    impl_->policyList_ = new QListWidget(policyGroup);
    for (int i = 0; i <= static_cast<int>(RemapPolicy::FitWithCrop); ++i) {
        const auto policy = static_cast<RemapPolicy>(i);
        impl_->policyList_->addItem(impl_->policyLabel(policy));
    }
    impl_->policyList_->setCurrentRow(static_cast<int>(RemapPolicy::CenterLocked));
    impl_->policyList_->setAlternatingRowColors(true);
    impl_->policyList_->setMinimumHeight(150);
    impl_->policyList_->setAccessibleName(QStringLiteral("Remap policy"));
    impl_->policyList_->setAccessibleDescription(QStringLiteral("Choose how coordinate-dependent data adapts to the new resolution"));
    policyLayout->addWidget(impl_->policyList_);
    mainLayout->addWidget(policyGroup);

    // Warning for aspect ratio change
    if (std::abs(impact.oldAspectRatio - impact.newAspectRatio) > 0.01) {
        auto* warnLabel = new QLabel(QStringLiteral(
            "⚠  Aspect ratio changed. Masks and keyframes may shift; review before applying."), this);
        QPalette warningPalette = warnLabel->palette();
        warningPalette.setColor(QPalette::WindowText, QColor(225, 151, 63));
        warnLabel->setPalette(warningPalette);
        warnLabel->setWordWrap(true);
        mainLayout->addWidget(warnLabel);
    }

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    auto* skipButton = new QPushButton(QStringLiteral("Skip — Keep as-is"));
    auto* applyButton = new QPushButton(QStringLiteral("Apply Remap"));
    skipButton->setAccessibleName(QStringLiteral("Skip remap"));
    skipButton->setAccessibleDescription(QStringLiteral("Keep existing coordinates without remapping"));
    applyButton->setAccessibleName(QStringLiteral("Apply remap"));
    applyButton->setAccessibleDescription(QStringLiteral("Apply the selected resolution remap policy"));
    applyButton->setMinimumSize(130, 36);
    skipButton->setMinimumHeight(36);
    QPalette applyPalette = applyButton->palette();
    applyPalette.setColor(QPalette::Button, QColor(43, 111, 232));
    applyPalette.setColor(QPalette::ButtonText, Qt::white);
    applyButton->setPalette(applyPalette);
    applyButton->setAutoFillBackground(true);

    buttonLayout->addWidget(skipButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(applyButton);
    mainLayout->addLayout(buttonLayout);

    connect(skipButton, &QPushButton::clicked, this, [this]() {
        impl_->remapRequested_ = false;
        reject();
    });
    connect(applyButton, &QPushButton::clicked, this, [this]() {
        impl_->remapRequested_ = true;
        accept();
    });
}

ArtifactResolutionRemapDialog::~ArtifactResolutionRemapDialog() {
    delete impl_;
}

RemapPolicy ArtifactResolutionRemapDialog::selectedPolicy() const {
    const int idx = impl_->policyList_->currentRow();
    return static_cast<RemapPolicy>(idx);
}

bool ArtifactResolutionRemapDialog::remapRequested() const {
    return impl_->remapRequested_;
}

void ArtifactResolutionRemapDialog::mousePressEvent(QMouseEvent* e) {
    QDialog::mousePressEvent(e);
}

void ArtifactResolutionRemapDialog::mouseReleaseEvent(QMouseEvent* e) {
    QDialog::mouseReleaseEvent(e);
}

void ArtifactResolutionRemapDialog::mouseMoveEvent(QMouseEvent* e) {
    QDialog::mouseMoveEvent(e);
}

void ArtifactResolutionRemapDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
}

W_OBJECT_IMPL(ArtifactResolutionRemapDialog)

}
