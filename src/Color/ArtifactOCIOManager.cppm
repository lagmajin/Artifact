module;

#include <utility>
#include <algorithm>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <wobjectimpl.h>

module Artifact.Color.OCIOManager;

import Color.OCIOConfig;
import Color.ScienceManager;
import Color.ColorSpace;
import Image.ImageF32x4_RGBA;

namespace Artifact {

class ArtifactOCIOManager::Impl {
public:
    ArtifactCore::OCIOConfig config_;
    QString activePresetName_;
    QString workingSpace_;
    QString display_;
    QString view_;
    QString looks_;

    static ArtifactCore::ColorSpace mapOCIOColorSpaceToEnum(const QString& csName)
    {
        const QString lower = csName.toLower();
        if (lower == QLatin1String("srgb") || lower == QLatin1String("sRGB"))
            return ArtifactCore::ColorSpace::sRGB;
        if (lower == QLatin1String("rec709") || lower == QLatin1String("rec.709"))
            return ArtifactCore::ColorSpace::Rec709;
        if (lower == QLatin1String("rec2020") || lower == QLatin1String("rec.2020"))
            return ArtifactCore::ColorSpace::Rec2020;
        if (lower == QLatin1String("p3") || lower == QLatin1String("dci-p3"))
            return ArtifactCore::ColorSpace::P3;
        if (lower == QLatin1String("acescg") || lower == QLatin1String("ap1"))
            return ArtifactCore::ColorSpace::ACES_AP1;
        if (lower == QLatin1String("aces2065") || lower == QLatin1String("ap0"))
            return ArtifactCore::ColorSpace::ACES_AP0;
        return ArtifactCore::ColorSpace::Linear;
    }

ArtifactOCIOManager* ArtifactOCIOManager::instance()
{
    static ArtifactOCIOManager s_instance;
    return &s_instance;
}

ArtifactOCIOManager::ArtifactOCIOManager()
    : impl_(new Impl())
{
}

ArtifactOCIOManager::~ArtifactOCIOManager()
{
    delete impl_;
}

bool ArtifactOCIOManager::setActivePreset(const QString& presetName)
{
    const QString lower = presetName.toLower();
    if (lower == QLatin1String("aces")) {
        impl_->config_ = ArtifactCore::OCIOConfig::createACESConfig();
    } else if (lower == QLatin1String("srgb")) {
        impl_->config_ = ArtifactCore::OCIOConfig::createSRGBConfig();
    } else if (lower == QLatin1String("rec.709") || lower == QLatin1String("rec709")) {
        impl_->config_ = ArtifactCore::OCIOConfig::createRec709Config();
    } else if (lower == QLatin1String("rec.2020") || lower == QLatin1String("rec2020")) {
        impl_->config_ = ArtifactCore::OCIOConfig::createRec2020Config();
    } else {
        // Fallback to ACES
        impl_->config_ = ArtifactCore::OCIOConfig::createACESConfig();
        impl_->activePresetName_ = QStringLiteral("ACES");
        configChanged();
        return false;
    }
    impl_->activePresetName_ = presetName;

    // Sync working space from config
    impl_->workingSpace_ = impl_->config_.workingSpace();
    if (impl_->workingSpace_.isEmpty()) {
        impl_->workingSpace_ = QStringLiteral("ACEScg");
    }
    impl_->config_.setWorkingSpace(impl_->workingSpace_);

    // Sync display/view from config
    const QStringList displays = impl_->config_.displays();
    if (!displays.isEmpty()) {
        impl_->display_ = impl_->config_.activeDisplay();
        if (impl_->display_.isEmpty()) {
            impl_->display_ = displays.first();
        }
        const QStringList views = impl_->config_.viewsForDisplay(impl_->display_);
        if (!views.isEmpty()) {
            impl_->view_ = impl_->config_.activeView();
            if (impl_->view_.isEmpty()) {
                impl_->view_ = views.first();
bool ArtifactOCIOManager::loadConfigFile(const QString& path)
{
    if (!impl_->config_.loadFromFile(path)) {
        return false;
    }
    impl_->activePresetName_ = QStringLiteral("Custom");
    impl_->workingSpace_ = impl_->config_.workingSpace();
    impl_->display_ = impl_->config_.activeDisplay();
    impl_->view_ = impl_->config_.activeView();
    impl_->looks_ = impl_->config_.activeLooks();
    configChanged();
    return true;
}

bool ArtifactOCIOManager::loadConfig(const ArtifactCore::OCIOConfig& config)
{
    impl_->config_ = config;
    impl_->activePresetName_ = QStringLiteral("Custom");
    impl_->workingSpace_ = impl_->config_.workingSpace();
    impl_->display_ = impl_->config_.activeDisplay();
    impl_->view_ = impl_->config_.activeView();
    impl_->looks_ = impl_->config_.activeLooks();
    configChanged();
    return true;
}

void ArtifactOCIOManager::clearConfig()
{
    impl_->config_ = ArtifactCore::OCIOConfig();
    impl_->activePresetName_.clear();
    impl_->workingSpace_.clear();
    impl_->display_.clear();
    impl_->view_.clear();
    impl_->looks_.clear();
    configChanged();
}

const ArtifactCore::OCIOConfig* ArtifactOCIOManager::activeConfig() const
{
    return impl_->config_.isValid() ? &impl_->config_ : nullptr;
}

QString ArtifactOCIOManager::activePresetName() const
{
    return impl_->activePresetName_;
}

bool ArtifactOCIOManager::hasActiveConfig() const
{
    return impl_->config_.isValid();
}

QStringList ArtifactOCIOManager::availablePresets() const
{
    return { QStringLiteral("ACES"),
             QStringLiteral("sRGB"),
             QStringLiteral("Rec.709"),
             QStringLiteral("Rec.2020") };
}

QStringList ArtifactOCIOManager::availableWorkingSpaces() const
{
    if (!impl_->config_.isValid())
        return {};
    const auto spaces = impl_->config_.colorSpaces();
    QStringList names;
    names.reserve(spaces.size());
    for (const auto& cs : spaces) {
        names.append(cs.name);
    }
    return names;
}

QStringList ArtifactOCIOManager::availableDisplays() const
{
    if (!impl_->config_.isValid())
        return {};
    return impl_->config_.displays();
}

QStringList ArtifactOCIOManager::availableViews(const QString& display) const
{
    if (!impl_->config_.isValid())
        return {};
    return impl_->config_.viewsForDisplay(display);
}

QString ArtifactOCIOManager::workingSpace() const
{
    return impl_->workingSpace_;
}

void ArtifactOCIOManager::setWorkingSpace(const QString& cs)
{
    if (impl_->workingSpace_ == cs)
        return;
    impl_->workingSpace_ = cs;
    impl_->config_.setWorkingSpace(cs);
    workingSpaceChanged(cs);
}

QString ArtifactOCIOManager::display() const
{
    return impl_->display_;
}

void ArtifactOCIOManager::setDisplay(const QString& display)
{
    if (impl_->display_ == display)
        return;
    impl_->display_ = display;
    impl_->config_.setActiveDisplay(display);
    // Reset view to first available for this display
    const QStringList views = impl_->config_.viewsForDisplay(display);
    if (!views.isEmpty() && !views.contains(impl_->view_)) {
        impl_->view_ = views.first();
        impl_->config_.setActiveView(impl_->view_);
    }
    displayViewChanged(impl_->display_, impl_->view_);
}

QString ArtifactOCIOManager::view() const
{
    return impl_->view_;
}

void ArtifactOCIOManager::setView(const QString& view)
{
    if (impl_->view_ == view)
        return;
    impl_->view_ = view;
    impl_->config_.setActiveView(view);
    displayViewChanged(impl_->display_, impl_->view_);
}

QString ArtifactOCIOManager::looks() const
{
    return impl_->looks_;
}

void ArtifactOCIOManager::setLooks(const QString& looks)
{
    if (impl_->looks_ == looks)
        return;
    impl_->looks_ = looks;
    impl_->config_.setActiveLooks(looks);
}

void ArtifactOCIOManager::syncToColorScienceManager(ArtifactColorScienceManager* mgr) const
{
    if (!mgr || !impl_->config_.isValid())
        return;

    auto settings = mgr->getSettings();

    // Map OCIO working space to ColorSpace enum
    if (!impl_->workingSpace_.isEmpty()) {
        settings.workingSpace = Impl::mapOCIOColorSpaceToEnum(impl_->workingSpace_);
    }

    // Set output space from display/view transform
    if (const QString displayName = impl_->display_; !displayName.isEmpty()) {
        if (displayName.contains(QLatin1String("sRGB"), Qt::CaseInsensitive) ||
            displayName.contains(QLatin1String("srgb"), Qt::CaseInsensitive)) {
            settings.outputSpace = ArtifactCore::ColorSpace::sRGB;
        } else if (displayName.contains(QLatin1String("Rec.709"), Qt::CaseInsensitive) ||
                   displayName.contains(QLatin1String("rec709"), Qt::CaseInsensitive)) {
            settings.outputSpace = ArtifactCore::ColorSpace::Rec709;
        } else if (displayName.contains(QLatin1String("Rec.2020"), Qt::CaseInsensitive) ||
                   displayName.contains(QLatin1String("rec2020"), Qt::CaseInsensitive)) {
            settings.outputSpace = ArtifactCore::ColorSpace::Rec2020;
        } else if (displayName.contains(QLatin1String("P3"), Qt::CaseInsensitive) ||
                   displayName.contains(QLatin1String("dci"), Qt::CaseInsensitive)) {
            settings.outputSpace = ArtifactCore::ColorSpace::P3;
        }
    }

void ArtifactOCIOManager::applyViewTransformToImage(ArtifactCore::ImageF32x4_RGBA& image) const
{
    if (!impl_->config_.isValid() || !image.data()) return;

    const auto workingCS = Impl::mapOCIOColorSpaceToEnum(impl_->workingSpace_);
    const auto displayCS = Impl::mapOCIOColorSpaceToEnum(impl_->display_);

    if (workingCS == displayCS) return;

    const auto matrix = ArtifactCore::ColorSpaceConverter::getConversionMatrix(workingCS, displayCS);

    const int w = image.width();
    const int h = image.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r = image.getPixel(x, y, 0);
            float g = image.getPixel(x, y, 1);
            float b = image.getPixel(x, y, 2);
            float a = image.getPixel(x, y, 3);

            float outR = matrix[0] * r + matrix[1] * g + matrix[2] * b + matrix[3] * a;
            float outG = matrix[4] * r + matrix[5] * g + matrix[6] * b + matrix[7] * a;
            float outB = matrix[8] * r + matrix[9] * g + matrix[10] * b + matrix[11] * a;

            image.setPixel(x, y, 0, std::clamp(outR, 0.0f, 1.0f));
            image.setPixel(x, y, 1, std::clamp(outG, 0.0f, 1.0f));
            image.setPixel(x, y, 2, std::clamp(outB, 0.0f, 1.0f));
        }
    }
}

QJsonObject ArtifactOCIOManager::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("activePresetName")] = impl_->activePresetName_;
    obj[QStringLiteral("workingSpace")] = impl_->workingSpace_;
    obj[QStringLiteral("display")] = impl_->display_;
    obj[QStringLiteral("view")] = impl_->view_;
    obj[QStringLiteral("looks")] = impl_->looks_;
    obj[QStringLiteral("config")] = impl_->config_.toJson();
    return obj;
}

bool ArtifactOCIOManager::fromJson(const QJsonObject& obj)
{
    impl_->activePresetName_ = obj.value(QStringLiteral("activePresetName")).toString();
    impl_->workingSpace_ = obj.value(QStringLiteral("workingSpace")).toString();
    impl_->display_ = obj.value(QStringLiteral("display")).toString();
    impl_->view_ = obj.value(QStringLiteral("view")).toString();
    impl_->looks_ = obj.value(QStringLiteral("looks")).toString();

    const QJsonObject configJson = obj.value(QStringLiteral("config")).toObject();
    if (!configJson.isEmpty()) {
        impl_->config_.loadFromJson(configJson);
    }

    configChanged();
    return true;
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactOCIOManager)
