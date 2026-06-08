module;
#include <QDialog>
#include <QLabel>
#include <QComboBox>
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
#include <QString>
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

        // Compute bounding rect for old aspect
        const double oldAspect = static_cast<double>(oldSize_.width()) / oldSize_.height();
        int oldW, oldH;
        if (oldAspect > 1.0) {
            oldW = w;
            oldH = static_cast<int>(w / oldAspect);
        } else {
            oldH = h;
            oldW = static_cast<int>(h * oldAspect);
        }
        oldW = std::max(oldW, 20);
        oldH = std::max(oldH, 20);

        // Compute bounding rect for new aspect
        const double newAspect = static_cast<double>(newSize_.width()) / newSize_.height();
        int newW, newH;
        if (newAspect > 1.0) {
            newW = w;
            newH = static_cast<int>(w / newAspect);
        } else {
            newH = h;
            newW = static_cast<int>(h * newAspect);
        }
        newW = std::max(newW, 20);
        newH = std::max(newH, 20);

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
    QComboBox* policyCombo_ = nullptr;
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
    setMinimumWidth(420);

    auto* mainLayout = new QVBoxLayout(this);

    // Header: size change summary
    auto* headerLabel = new QLabel(QStringLiteral(
        "<b>Resolution Change</b><br>"
        "%1x%2 → %3x%4  (aspect: %5 → %6)")
        .arg(oldSize.width()).arg(oldSize.height())
        .arg(newSize.width()).arg(newSize.height())
        .arg(impact.oldAspectRatio, 0, 'f', 3)
        .arg(impact.newAspectRatio, 0, 'f', 3));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);

    // Aspect ratio preview
    impl_->preview_ = new AspectPreviewWidget(this);
    impl_->preview_->oldSize_ = oldSize;
    impl_->preview_->newSize_ = newSize;
    mainLayout->addWidget(impl_->preview_);

    // Impact summary
    if (impact.hasImpact()) {
        auto* impactGroup = new QGroupBox(QStringLiteral("Impact"));
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
        mainLayout->addWidget(impactGroup);
    }

    // Policy selector
    auto* policyGroup = new QGroupBox(QStringLiteral("Remap Policy"));
    auto* policyLayout = new QVBoxLayout(policyGroup);

    impl_->policyCombo_ = new QComboBox();
    for (int i = 0; i <= static_cast<int>(RemapPolicy::FitWithCrop); ++i) {
        const auto policy = static_cast<RemapPolicy>(i);
        impl_->policyCombo_->addItem(impl_->policyLabel(policy));
    }
    impl_->policyCombo_->setCurrentIndex(static_cast<int>(RemapPolicy::CenterLocked));
    policyLayout->addWidget(impl_->policyCombo_);
    mainLayout->addWidget(policyGroup);

    // Warning for aspect ratio change
    if (std::abs(impact.oldAspectRatio - impact.newAspectRatio) > 0.01) {
        auto* warnLabel = new QLabel(QStringLiteral(
            "<span style='color:orange;'><b>⚠ Aspect ratio changed.</b> "
            "Masks and keyframes may shift. Review the preview before applying.</span>"));
        warnLabel->setWordWrap(true);
        mainLayout->addWidget(warnLabel);
    }

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    auto* skipButton = new QPushButton(QStringLiteral("Skip — Keep as-is"));
    auto* applyButton = new QPushButton(QStringLiteral("Apply Remap"));

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
    const int idx = impl_->policyCombo_->currentIndex();
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
