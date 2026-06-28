module;
#include <QWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <wobjectdefs.h>

export module Artifact.Widgets.CrowdSettings;

import Scene.SimulationSettings;

export class CrowdSettingsWidget : public QWidget {
    W_OBJECT(CrowdSettingsWidget)
public:
    explicit CrowdSettingsWidget(QWidget* parent = nullptr);

    void setSettings(const ArtifactCore::CrowdSettings& s);
    ArtifactCore::CrowdSettings settings() const;

public:
    void settingsChanged()
    W_SIGNAL(settingsChanged)

private:
    void setupUi();
    QDoubleSpinBox* createSpin(double min, double max, double step, int decimals);
    void emitChanged();

    QCheckBox* enabledCheck_ = nullptr;
    QDoubleSpinBox* densitySpin_ = nullptr;
    QDoubleSpinBox* cohesionSpin_ = nullptr;
    QDoubleSpinBox* separationSpin_ = nullptr;
    QDoubleSpinBox* alignmentSpin_ = nullptr;
    QDoubleSpinBox* maxSpeedSpin_ = nullptr;
    QDoubleSpinBox* jitterSpin_ = nullptr;
};
