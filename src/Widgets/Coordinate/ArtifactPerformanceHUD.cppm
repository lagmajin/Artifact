module;
#include <wobjectimpl.h>
#include <QObject>
#include <QTimer>
#include <QString>
#include <QDebug>

#include <chrono>

module Artifact.Widgets.PerformanceHUD;

import std;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Artifact {
using namespace ArtifactCore;

namespace {
#if defined(_WIN32)
quint64 processWorkingSetMB() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024ull * 1024ull);
    }
    return 0;
}
#else
quint64 processWorkingSetMB() { return 0; }
#endif
} // anon namespace

class ArtifactPerformanceHUD::Impl {
public:
    explicit Impl(ArtifactPerformanceHUD* parent)
        : parent_(parent)
    {
        timer_.setInterval(std::chrono::milliseconds(500));
        QObject::connect(&timer_, &QTimer::timeout, parent_, [this]() {
            updateHUD();
        });
    }

    void setController(CompositionRenderController* ctrl)
    {
        controller_ = ctrl;
    }

    void setEnabled(bool enabled)
    {
        if (enabled_ == enabled) return;
        enabled_ = enabled;
        if (enabled_) {
            timer_.start();
        } else {
            timer_.stop();
            if (controller_) {
                controller_->clearInfoOverlayText();
            }
        }
    }

    bool isEnabled() const { return enabled_; }

private:
    void updateHUD()
    {
        if (!controller_ || !enabled_) return;

        // FPS from frame time
        const double avgMs = controller_->averageFrameTimeMs();
        double fps = 0.0;
        if (avgMs > 0.0) {
            fps = 1000.0 / avgMs;
        }

        // Layer count
        int layerCount = 0;
        if (auto comp = controller_->composition()) {
            layerCount = static_cast<int>(comp->allLayer().size());
        }

        // Memory
        const quint64 memMB = processWorkingSetMB();

        // Zoom
        float zoomPct = 100.0f;
        if (auto* renderer = controller_->renderer()) {
            zoomPct = renderer->getZoom() * 100.0f;
        }

        // Format: "FPS: 59.9 | Layers: 12 | Mem: 845 MB | Zoom: 87%"
        QString detail = QStringLiteral("FPS: %1 | Layers: %2 | Mem: %3 MB | Zoom: %4%")
            .arg(fps, 0, 'f', 1)
            .arg(layerCount)
            .arg(memMB)
            .arg(static_cast<int>(zoomPct));

        controller_->setInfoOverlayText(QString(), detail);
    }

    ArtifactPerformanceHUD* parent_;
    QTimer timer_;
    CompositionRenderController* controller_ = nullptr;
    bool enabled_ = false;
};

W_OBJECT_IMPL(ArtifactPerformanceHUD)

ArtifactPerformanceHUD::ArtifactPerformanceHUD(QObject* parent)
    : QObject(parent), impl_(new Impl(this)) {}

ArtifactPerformanceHUD::~ArtifactPerformanceHUD()
{
    delete impl_;
}

void ArtifactPerformanceHUD::setController(CompositionRenderController* ctrl)
{
    impl_->setController(ctrl);
}

void ArtifactPerformanceHUD::setEnabled(bool enabled)
{
    impl_->setEnabled(enabled);
}

bool ArtifactPerformanceHUD::isEnabled() const
{
    return impl_->isEnabled();
}

} // namespace Artifact
