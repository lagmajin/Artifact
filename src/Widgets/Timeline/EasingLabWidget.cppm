module;

#include <QDialogButtonBox>
#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <functional>
#include <QComboBox>
#include <QSignalBlocker>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <wobjectimpl.h>

module Artifact.Widgets.Timeline.EasingLab;

import Animation.EasingCurveUtil;

namespace Artifact {

namespace {
static constexpr int kTileWidth = 220;
static constexpr int kTileHeight = 160;

static QString titleForCandidate(const ArtifactCore::EasingCandidate& candidate)
{
    return candidate.name.isEmpty() ? ArtifactCore::easingTypeToString(candidate.type)
                                    : candidate.name;
}

static QString interpolationLabelForCandidate(const ArtifactCore::EasingCandidate& candidate)
{
    switch (ArtifactCore::easingTypeToInterpolation(candidate.type)) {
    case ArtifactCore::InterpolationType::Constant:
        return QStringLiteral("maps to hold");
    case ArtifactCore::InterpolationType::Linear:
        return QStringLiteral("maps to linear");
    case ArtifactCore::InterpolationType::EaseIn:
        return QStringLiteral("maps to ease-in");
    case ArtifactCore::InterpolationType::EaseOut:
        return QStringLiteral("maps to ease-out");
    case ArtifactCore::InterpolationType::EaseInOut:
        return QStringLiteral("maps to ease-in-out");
    case ArtifactCore::InterpolationType::BackOut:
        return QStringLiteral("maps to back-out");
    case ArtifactCore::InterpolationType::Exponential:
        return QStringLiteral("maps to expo");
    default:
        return QStringLiteral("maps to interpolation");
    }
}
} // namespace

W_OBJECT_IMPL(EasingPreviewWidget)
W_OBJECT_IMPL(EasingLabDialog)

class EasingPreviewWidget::Impl {
public:
    ArtifactCore::EasingCandidate candidate;
    float previewProgress = 0.0f;
};

EasingPreviewWidget::EasingPreviewWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setMinimumSize(kTileWidth, kTileHeight);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

EasingPreviewWidget::~EasingPreviewWidget()
{
    delete impl_;
}

void EasingPreviewWidget::setCandidate(const ArtifactCore::EasingCandidate& candidate)
{
    impl_->candidate = candidate;
    update();
}

void EasingPreviewWidget::setPreviewProgress(float progress)
{
    impl_->previewProgress = ArtifactCore::clampUnit(progress);
    update();
}

ArtifactCore::EasingCandidate EasingPreviewWidget::candidate() const
{
    return impl_->candidate;
}

float EasingPreviewWidget::previewProgress() const
{
    return impl_->previewProgress;
}

QSize EasingPreviewWidget::sizeHint() const
{
    return {kTileWidth, kTileHeight};
}

void EasingPreviewWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);
    painter.fillRect(r, QColor(22, 24, 28));
    painter.setPen(QPen(QColor(64, 72, 84), 1.0));
    painter.drawRoundedRect(r, 10.0, 10.0);

    const QRectF chartRect = r.adjusted(12, 26, -12, -38);
    painter.setPen(QPen(QColor(58, 62, 72), 1.0));
    painter.drawRect(chartRect);

    painter.drawText(QRectF(r.left() + 12, r.top() + 6, r.width() - 24, 18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     titleForCandidate(impl_->candidate));
    painter.drawText(QRectF(r.left() + 12, r.top() + 20, r.width() - 24, 16),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     interpolationLabelForCandidate(impl_->candidate));

    const float eased = ArtifactCore::evaluateEasing(impl_->candidate.type, impl_->previewProgress);

    QPainterPath curve;
    const int samples = 48;
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float y = ArtifactCore::evaluateEasing(impl_->candidate.type, t);
        const qreal xPos = chartRect.left() + chartRect.width() * t;
        const qreal yPos = chartRect.bottom() - chartRect.height() * y;
        if (i == 0) {
            curve.moveTo(xPos, yPos);
        } else {
            curve.lineTo(xPos, yPos);
        }
    }

    painter.setPen(QPen(QColor(122, 184, 255), 2.0));
    painter.drawPath(curve);

    painter.setPen(QPen(QColor(90, 96, 110), 1.0, Qt::DashLine));
    painter.drawLine(chartRect.bottomLeft(), chartRect.topRight());

    const qreal xPos = chartRect.left() + chartRect.width() * impl_->previewProgress;
    const qreal yPos = chartRect.bottom() - chartRect.height() * eased;
    painter.setBrush(QColor(255, 220, 120));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(xPos, yPos), 5.5, 5.5);

    painter.setPen(QColor(180, 188, 200));
    painter.drawText(QRectF(r.left() + 12, r.bottom() - 24, r.width() - 24, 16),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("t=%1  value=%2")
                         .arg(QString::number(impl_->previewProgress, 'f', 2),
                              QString::number(eased, 'f', 3)));
}

class EasingLabDialog::Impl {
public:
    QVector<ArtifactCore::EasingCandidate> candidates;
    QVector<EasingPreviewWidget*> previews;
    QSlider* scrubSlider = nullptr;
    QComboBox* candidateCombo = nullptr;
    QPushButton* applyButton = nullptr;
    QLabel* countLabel = nullptr;
    QLabel* progressLabel = nullptr;
    QWidget* scrollContent = nullptr;
    QGridLayout* grid = nullptr;
    std::function<void(ArtifactCore::InterpolationType)> applyCallback;
};

void EasingLabDialog::clearPreviewGrid()
{
    if (!impl_ || !impl_->grid) {
        return;
    }
    while (auto* item = impl_->grid->takeAt(0)) {
        if (auto* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    impl_->previews.clear();
}

void EasingLabDialog::rebuildPreviewGrid()
{
    if (!impl_ || !impl_->scrollContent || !impl_->grid) {
        return;
    }
    clearPreviewGrid();
    impl_->previews.reserve(impl_->candidates.size());

    const int columns = 2;
    for (int i = 0; i < impl_->candidates.size(); ++i) {
        auto* preview = new EasingPreviewWidget(impl_->scrollContent);
        preview->setCandidate(impl_->candidates[i]);
        preview->setPreviewProgress(impl_->scrubSlider
                                        ? static_cast<float>(impl_->scrubSlider->value()) / 1000.0f
                                        : 0.55f);
        impl_->previews.push_back(preview);

        const int row = i / columns;
        const int col = i % columns;
        impl_->grid->addWidget(preview, row, col);
    }
}

EasingLabDialog::EasingLabDialog(QWidget* parent,
                                 std::function<void(ArtifactCore::InterpolationType)> applyCallback)
    : QDialog(parent), impl_(new Impl())
{
    setWindowTitle(QStringLiteral("Easing Lab"));
    resize(920, 620);
    impl_->applyCallback = std::move(applyCallback);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto* titleRow = new QHBoxLayout();
    auto* title = new QLabel(QStringLiteral("Easing comparison"));
    titleRow->addWidget(title);
    titleRow->addStretch();
    impl_->candidateCombo = new QComboBox(this);
    titleRow->addWidget(impl_->candidateCombo);
    impl_->applyButton = new QPushButton(QStringLiteral("Apply"), this);
    impl_->applyButton->setEnabled(static_cast<bool>(impl_->applyCallback));
    titleRow->addWidget(impl_->applyButton);
    impl_->countLabel = new QLabel(this);
    impl_->progressLabel = new QLabel(this);
    titleRow->addWidget(impl_->countLabel);
    titleRow->addWidget(impl_->progressLabel);
    root->addLayout(titleRow);

    impl_->scrubSlider = new QSlider(Qt::Horizontal, this);
    impl_->scrubSlider->setRange(0, 1000);
    impl_->scrubSlider->setValue(550);
    root->addWidget(impl_->scrubSlider);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    root->addWidget(scrollArea, 1);

    impl_->scrollContent = new QWidget(scrollArea);
    impl_->grid = new QGridLayout(impl_->scrollContent);
    impl_->grid->setContentsMargins(0, 0, 0, 0);
    impl_->grid->setHorizontalSpacing(10);
    impl_->grid->setVerticalSpacing(10);
    scrollArea->setWidget(impl_->scrollContent);

    const auto defaultCandidates = ArtifactCore::defaultEasingCandidates();
    impl_->candidates = QVector<ArtifactCore::EasingCandidate>(defaultCandidates.begin(),
                                                               defaultCandidates.end());
    if (impl_->candidateCombo) {
        impl_->candidateCombo->clear();
        for (const auto& candidate : impl_->candidates) {
            impl_->candidateCombo->addItem(titleForCandidate(candidate));
        }
        impl_->candidateCombo->setCurrentIndex(0);
    }
    rebuildPreviewGrid();
    if (impl_->countLabel) {
        impl_->countLabel->setText(
            QStringLiteral("%1 candidate(s)").arg(impl_->candidates.size()));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
    root->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(impl_->applyButton, &QPushButton::clicked, this, [this]() {
        if (!impl_ || !impl_->candidateCombo || !impl_->applyCallback) {
            return;
        }
        const int index = impl_->candidateCombo->currentIndex();
        if (index < 0 || index >= impl_->candidates.size()) {
            return;
        }
        impl_->applyCallback(ArtifactCore::easingTypeToInterpolation(impl_->candidates[index].type));
    });

    QObject::connect(impl_->scrubSlider, &QSlider::valueChanged, this, [this](int value) {
        const float progress = static_cast<float>(value) / 1000.0f;
        if (impl_->progressLabel) {
            impl_->progressLabel->setText(
                QStringLiteral("t=%1").arg(QString::number(progress, 'f', 2)));
        }
        for (auto* preview : impl_->previews) {
            preview->setPreviewProgress(progress);
        }
    });

    const float initialProgress = static_cast<float>(impl_->scrubSlider->value()) / 1000.0f;
    if (impl_->progressLabel) {
        impl_->progressLabel->setText(
            QStringLiteral("t=%1").arg(QString::number(initialProgress, 'f', 2)));
    }
    for (auto* preview : impl_->previews) {
        preview->setPreviewProgress(initialProgress);
    }
}

EasingLabDialog::~EasingLabDialog()
{
    delete impl_;
}

void EasingLabDialog::setCandidates(const QVector<ArtifactCore::EasingCandidate>& candidates)
{
    impl_->candidates = candidates;
    if (impl_->candidateCombo) {
        const QSignalBlocker blocker(impl_->candidateCombo);
        impl_->candidateCombo->clear();
        for (const auto& candidate : impl_->candidates) {
            impl_->candidateCombo->addItem(titleForCandidate(candidate));
        }
        impl_->candidateCombo->setCurrentIndex(0);
    }
    rebuildPreviewGrid();
    if (impl_->countLabel) {
        impl_->countLabel->setText(
            QStringLiteral("%1 candidate(s)").arg(impl_->candidates.size()));
    }
}

QVector<ArtifactCore::EasingCandidate> EasingLabDialog::candidates() const
{
    return impl_->candidates;
}

void EasingLabDialog::setPreviewProgress(float progress)
{
    progress = ArtifactCore::clampUnit(progress);
    if (impl_->scrubSlider) {
        const int sliderValue = static_cast<int>(progress * 1000.0f);
        impl_->scrubSlider->setValue(sliderValue);
    }
    for (auto* preview : impl_->previews) {
        preview->setPreviewProgress(progress);
    }
}

float EasingLabDialog::previewProgress() const
{
    if (!impl_->scrubSlider) {
        return 0.0f;
    }
    return static_cast<float>(impl_->scrubSlider->value()) / 1000.0f;
}

} // namespace Artifact
