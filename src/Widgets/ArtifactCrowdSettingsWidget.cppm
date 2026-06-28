module;
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <wobjectimpl.h>

module Artifact.Widgets.CrowdSettings;

W_OBJECT_IMPL(CrowdSettingsWidget)

namespace Artifact {

CrowdSettingsWidget::CrowdSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void CrowdSettingsWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    auto* group = new QGroupBox(QStringLiteral("Crowd / Boids"));
    auto* layout = new QFormLayout(group);
    layout->setSpacing(4);

    enabledCheck_ = new QCheckBox(QStringLiteral("Enable crowd simulation"));
    layout->addRow(QString(), enabledCheck_);

    densitySpin_ = createSpin(0.0, 100.0, 0.1, 2);
    layout->addRow(QStringLiteral("Density:"), densitySpin_);

    cohesionSpin_ = createSpin(0.0, 10.0, 0.1, 2);
    layout->addRow(QStringLiteral("Cohesion:"), cohesionSpin_);

    separationSpin_ = createSpin(0.0, 10.0, 0.1, 2);
    layout->addRow(QStringLiteral("Separation:"), separationSpin_);

    alignmentSpin_ = createSpin(0.0, 10.0, 0.1, 2);
    layout->addRow(QStringLiteral("Alignment:"), alignmentSpin_);

    maxSpeedSpin_ = createSpin(0.1, 100.0, 0.5, 1);
    layout->addRow(QStringLiteral("Max Speed:"), maxSpeedSpin_);

    jitterSpin_ = createSpin(0.0, 10.0, 0.01, 3);
    layout->addRow(QStringLiteral("Jitter:"), jitterSpin_);

    mainLayout->addWidget(group);
    mainLayout->addStretch();

    connect(enabledCheck_, &QCheckBox::toggled, this, &CrowdSettingsWidget::emitChanged);
    connect(densitySpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CrowdSettingsWidget::emitChanged);
    connect(cohesionSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CrowdSettingsWidget::emitChanged);
    connect(separationSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CrowdSettingsWidget::emitChanged);
    connect(alignmentSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CrowdSettingsWidget::emitChanged);
    connect(maxSpeedSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CrowdSettingsWidget::emitChanged);
    connect(jitterSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CrowdSettingsWidget::emitChanged);
}

QDoubleSpinBox* CrowdSettingsWidget::createSpin(double min, double max, double step, int decimals)
{
    auto* spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    return spin;
}

void CrowdSettingsWidget::setSettings(const ArtifactCore::CrowdSettings& s)
{
    enabledCheck_->setChecked(s.enabled);
    densitySpin_->setValue(s.density);
    cohesionSpin_->setValue(s.cohesion);
    separationSpin_->setValue(s.separation);
    alignmentSpin_->setValue(s.alignment);
    maxSpeedSpin_->setValue(s.maxSpeed);
    jitterSpin_->setValue(s.jitter);
}

ArtifactCore::CrowdSettings CrowdSettingsWidget::settings() const
{
    ArtifactCore::CrowdSettings s;
    s.enabled = enabledCheck_->isChecked();
    s.density = static_cast<float>(densitySpin_->value());
    s.cohesion = static_cast<float>(cohesionSpin_->value());
    s.separation = static_cast<float>(separationSpin_->value());
    s.alignment = static_cast<float>(alignmentSpin_->value());
    s.maxSpeed = static_cast<float>(maxSpeedSpin_->value());
    s.jitter = static_cast<float>(jitterSpin_->value());
    return s;
}

void CrowdSettingsWidget::emitChanged()
{
    Q_EMIT settingsChanged();
}

} // namespace Artifact
