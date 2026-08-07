module;

#include <utility>
#include <algorithm>
#include <cmath>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <wobjectimpl.h>
#include <OpenColorIO/OpenColorIO.h>
#include <OpenColorIO/OpenColorTransforms.h>
#include <OpenColorIO/OpenColorTypes.h>

module Artifact.Color.OCIOManager;

import Color.OCIOConfig;
import Color.ScienceManager;
import Color.ColorSpace;
import Color.GamutConversion;
import Color.TransferFunction;
import Core.Parallel;
import Image.ImageF32x4_RGBA;

namespace OCIO = OCIO_NAMESPACE;

namespace Artifact {

class ArtifactOCIOManager::Impl {
public:
    ArtifactCore::OCIOConfig config_;
    QString activePresetName_;
    QString workingSpace_;
    QString display_;
    QString view_;
    QString looks_;
    OCIO::ConstConfigRcPtr ocioConfig_;
    float viewerExposure_ = 0.0f;
    float viewerGamma_ = 1.0f;

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

    static ArtifactCore::Gamut mapColorSpaceToGamut(ArtifactCore::ColorSpace space)
    {
        switch (space) {
        case ArtifactCore::ColorSpace::sRGB:
            return ArtifactCore::Gamut::sRGB;
        case ArtifactCore::ColorSpace::Rec709:
            return ArtifactCore::Gamut::Rec709;
        case ArtifactCore::ColorSpace::Rec2020:
            return ArtifactCore::Gamut::Rec2020;
        case ArtifactCore::ColorSpace::P3:
            return ArtifactCore::Gamut::DCI_P3;
        case ArtifactCore::ColorSpace::ACES_AP0:
            return ArtifactCore::Gamut::ACES_AP0;
        case ArtifactCore::ColorSpace::ACES_AP1:
            return ArtifactCore::Gamut::ACES_AP1;
        case ArtifactCore::ColorSpace::Linear:
        default:
            // ColorSpace::Linear uses the sRGB/Rec.709 primaries in the
            // legacy converter; preserve that contract in the Gamut path.
            return ArtifactCore::Gamut::sRGB;
        }
    }
};

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
    impl_->ocioConfig_.reset();
    const QString normalizedPresetName = presetName.trimmed().left(256);
    const QString lower = normalizedPresetName.toLower();
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
    impl_->activePresetName_ = normalizedPresetName;

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
            }
        }
    }
    configChanged();
    return true;
}
bool ArtifactOCIOManager::loadConfigFile(const QString& path)
{
    const QString trimmedPath = path.trimmed().left(32768);
    if (trimmedPath.isEmpty()) {
        return false;
    }
    const QString normalizedPath = QFileInfo(trimmedPath).absoluteFilePath();
    if (!QFileInfo::exists(normalizedPath) ||
        !impl_->config_.loadFromFile(normalizedPath)) {
        return false;
    }
    try {
        impl_->ocioConfig_ = OCIO::Config::CreateFromFile(
            normalizedPath.toUtf8().constData());
    } catch (const OCIO::Exception&) {
        impl_->ocioConfig_.reset();
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
    impl_->ocioConfig_.reset();
    if (!config.configFilePath().isEmpty()) {
        try {
            impl_->ocioConfig_ = OCIO::Config::CreateFromFile(
                config.configFilePath().toUtf8().constData());
        } catch (const OCIO::Exception&) {
            impl_->ocioConfig_.reset();
        }
    }
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
    impl_->ocioConfig_.reset();
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
    const QString normalized = cs.trimmed().left(4096);
    const auto colorSpaces = impl_->config_.colorSpaces();
    const bool isKnownColorSpace = std::any_of(
        colorSpaces.cbegin(), colorSpaces.cend(),
        [&normalized](const ArtifactCore::OCIOColorSpace& colorSpace) {
            return colorSpace.name == normalized;
        });
    if (impl_->config_.isValid() && !isKnownColorSpace) {
        return;
    }
    if (impl_->workingSpace_ == normalized)
        return;
    impl_->workingSpace_ = normalized;
    impl_->config_.setWorkingSpace(normalized);
    workingSpaceChanged(normalized);
    configChanged();
}

QString ArtifactOCIOManager::display() const
{
    return impl_->display_;
}

void ArtifactOCIOManager::setDisplay(const QString& display)
{
    const QString normalized = display.trimmed().left(4096);
    if (impl_->config_.isValid() &&
        !impl_->config_.displays().contains(normalized)) {
        return;
    }
    if (impl_->display_ == normalized)
        return;
    impl_->display_ = normalized;
    impl_->config_.setActiveDisplay(normalized);
    // Reset view to first available for this display
    const QStringList views = impl_->config_.viewsForDisplay(normalized);
    if (!views.isEmpty() && !views.contains(impl_->view_)) {
        impl_->view_ = views.first();
        impl_->config_.setActiveView(impl_->view_);
    }
    displayViewChanged(impl_->display_, impl_->view_);
    configChanged();
}

QString ArtifactOCIOManager::view() const
{
    return impl_->view_;
}

void ArtifactOCIOManager::setView(const QString& view)
{
    const QString normalized = view.trimmed().left(4096);
    if (impl_->config_.isValid() &&
        !impl_->config_.viewsForDisplay(impl_->display_).contains(normalized)) {
        return;
    }
    if (impl_->view_ == normalized)
        return;
    impl_->view_ = normalized;
    impl_->config_.setActiveView(normalized);
    displayViewChanged(impl_->display_, impl_->view_);
    configChanged();
}

QString ArtifactOCIOManager::looks() const
{
    return impl_->looks_;
}

void ArtifactOCIOManager::setLooks(const QString& looks)
{
    const QString normalized = looks.trimmed().left(4096);
    if (impl_->looks_ == normalized)
        return;
    impl_->looks_ = normalized;
    impl_->config_.setActiveLooks(normalized);
    configChanged();
}

float ArtifactOCIOManager::viewerExposure() const
{
    return impl_->viewerExposure_;
}

void ArtifactOCIOManager::setViewerExposure(const float ev)
{
    const float clamped = std::isfinite(ev) ? std::clamp(ev, -10.0f, 10.0f) : 0.0f;
    if (qFuzzyCompare(impl_->viewerExposure_, clamped))
        return;
    impl_->viewerExposure_ = clamped;
    configChanged();
}

float ArtifactOCIOManager::viewerGamma() const
{
    return impl_->viewerGamma_;
}

void ArtifactOCIOManager::setViewerGamma(const float gamma)
{
    const float clamped = std::isfinite(gamma) ? std::clamp(gamma, 0.1f, 4.0f) : 1.0f;
    if (qFuzzyCompare(impl_->viewerGamma_, clamped))
        return;
    impl_->viewerGamma_ = clamped;
    configChanged();
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
        QString resolvedColorSpace;
        for (const auto& displayView : impl_->config_.displayViews()) {
            if (displayView.display == impl_->display_ &&
                displayView.view == impl_->view_ &&
                !displayView.colorspace.isEmpty()) {
                resolvedColorSpace = displayView.colorspace;
                break;
            }
        }
        const QString outputName = resolvedColorSpace.isEmpty()
            ? displayName
            : resolvedColorSpace;
        if (outputName.compare(QStringLiteral("sRGB"), Qt::CaseInsensitive) == 0 ||
            outputName.compare(QStringLiteral("sRGB - Display"), Qt::CaseInsensitive) == 0) {
            settings.outputSpace = ArtifactCore::ColorSpace::sRGB;
        } else if (outputName.compare(QStringLiteral("Rec.709"), Qt::CaseInsensitive) == 0 ||
                   outputName.compare(QStringLiteral("Rec709"), Qt::CaseInsensitive) == 0) {
            settings.outputSpace = ArtifactCore::ColorSpace::Rec709;
        } else if (outputName.compare(QStringLiteral("Rec.2020"), Qt::CaseInsensitive) == 0 ||
                   outputName.compare(QStringLiteral("Rec2020"), Qt::CaseInsensitive) == 0) {
            settings.outputSpace = ArtifactCore::ColorSpace::Rec2020;
        } else if (outputName.compare(QStringLiteral("P3"), Qt::CaseInsensitive) == 0 ||
                   outputName.compare(QStringLiteral("DCI-P3"), Qt::CaseInsensitive) == 0) {
            settings.outputSpace = ArtifactCore::ColorSpace::P3;
        }
    }
    mgr->setSettings(settings);
}

void ArtifactOCIOManager::applyViewTransformToImage(ArtifactCore::ImageF32x4_RGBA& image) const
{
    if (!impl_->config_.isValid() || !image.rgba32fData()) return;

    if (impl_->ocioConfig_ && !impl_->workingSpace_.isEmpty() &&
        !impl_->display_.isEmpty() && !impl_->view_.isEmpty()) {
        try {
            OCIO::ConstProcessorRcPtr processor;
            if (impl_->looks_.trimmed().isEmpty()) {
                processor = impl_->ocioConfig_->getProcessor(
                    impl_->workingSpace_.toUtf8().constData(),
                    impl_->display_.toUtf8().constData(),
                    impl_->view_.toUtf8().constData(),
                    OCIO::TRANSFORM_DIR_FORWARD);
            } else {
                const auto group = OCIO::GroupTransform::Create();
                const auto look = OCIO::LookTransform::Create();
                look->setSrc(impl_->workingSpace_.toUtf8().constData());
                look->setDst(impl_->workingSpace_.toUtf8().constData());
                look->setLooks(impl_->looks_.toUtf8().constData());
                group->appendTransform(look);
                const auto displayView = OCIO::DisplayViewTransform::Create();
                displayView->setSrc(impl_->workingSpace_.toUtf8().constData());
                displayView->setDisplay(impl_->display_.toUtf8().constData());
                displayView->setView(impl_->view_.toUtf8().constData());
                group->appendTransform(displayView);
                processor = impl_->ocioConfig_->getProcessor(group);
            }
            const auto cpuProcessor = processor->getDefaultCPUProcessor();
            OCIO::PackedImageDesc pixels(image.rgba32fData(), image.width(),
                                          image.height(), 4);
            cpuProcessor->apply(pixels);
            const float exposureScale = std::pow(2.0f, impl_->viewerExposure_);
            const float viewerGamma = impl_->viewerGamma_;
            if (std::abs(impl_->viewerExposure_) > 1.0e-6f ||
                std::abs(viewerGamma - 1.0f) > 1.0e-6f) {
                float* data = image.rgba32fData();
                const size_t pixelCount = image.totalPixels();
                for (size_t i = 0; i < pixelCount; ++i) {
                    float* pixel = data + i * 4u;
                    pixel[0] = std::pow(std::max(0.0f, pixel[0] * exposureScale),
                                        1.0f / viewerGamma);
                    pixel[1] = std::pow(std::max(0.0f, pixel[1] * exposureScale),
                                        1.0f / viewerGamma);
                    pixel[2] = std::pow(std::max(0.0f, pixel[2] * exposureScale),
                                        1.0f / viewerGamma);
                }
            }
            return;
        } catch (const OCIO::Exception&) {
            // Keep the existing matrix path as a deterministic fallback when
            // a custom display/view cannot be resolved by the active config.
        }
    }

    const auto workingCS = Impl::mapOCIOColorSpaceToEnum(impl_->workingSpace_);
    const auto displayCS = Impl::mapOCIOColorSpaceToEnum(impl_->display_);

    if (workingCS == displayCS) {
        const float exposureScale = std::pow(2.0f, impl_->viewerExposure_);
        const float viewerGamma = impl_->viewerGamma_;
        if (std::abs(impl_->viewerExposure_) > 1.0e-6f ||
            std::abs(viewerGamma - 1.0f) > 1.0e-6f) {
            float* data = image.rgba32fData();
            for (size_t i = 0; i < image.totalPixels(); ++i) {
                float* pixel = data + i * 4u;
                pixel[0] = std::pow(std::max(0.0f, pixel[0] * exposureScale),
                                    1.0f / viewerGamma);
                pixel[1] = std::pow(std::max(0.0f, pixel[1] * exposureScale),
                                    1.0f / viewerGamma);
                pixel[2] = std::pow(std::max(0.0f, pixel[2] * exposureScale),
                                    1.0f / viewerGamma);
            }
        }
        return;
    }

    const auto matrix = ArtifactCore::ColorGamutConversion::getConversionMatrix(
        Impl::mapColorSpaceToGamut(workingCS), Impl::mapColorSpaceToGamut(displayCS));

    const int w = image.width();
    const int h = image.height();
    float* data = image.rgba32fData();
ArtifactCore::Parallel::For(0, h, w * h, [&](int y) {
        float* row = data + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
        for (int x = 0; x < w; ++x) {
            float* pixel = row + static_cast<size_t>(x) * 4u;
            const float r = pixel[0];
            const float g = pixel[1];
            const float b = pixel[2];
            const float a = pixel[3];

            // Color transforms operate on RGB only. Keep alpha independent and
            // preserve scene-linear/HDR values until an explicit display/output
            // encoding stage performs tone mapping or range limiting.
            pixel[0] = matrix(0, r, g, b);
            pixel[1] = matrix(1, r, g, b);
            pixel[2] = matrix(2, r, g, b);
            pixel[3] = a;
        }
    });
}

QString ArtifactOCIOManager::gpuViewTransformShader() const
{
    if (!impl_->ocioConfig_ || impl_->workingSpace_.isEmpty() ||
        impl_->display_.isEmpty() || impl_->view_.isEmpty()) {
        return {};
    }

    try {
        OCIO::ConstProcessorRcPtr processor;
        if (impl_->looks_.isEmpty()) {
            processor = impl_->ocioConfig_->getProcessor(
                impl_->workingSpace_.toUtf8().constData(),
                impl_->display_.toUtf8().constData(),
                impl_->view_.toUtf8().constData(),
                OCIO::TRANSFORM_DIR_FORWARD);
        } else {
            auto group = OCIO::GroupTransform::Create();
            auto look = OCIO::LookTransform::Create();
            look->setSrc(impl_->workingSpace_.toUtf8().constData());
            look->setDst(impl_->workingSpace_.toUtf8().constData());
            look->setLooks(impl_->looks_.toUtf8().constData());
            group->appendTransform(look);
            auto displayView = OCIO::DisplayViewTransform::Create();
            displayView->setSrc(impl_->workingSpace_.toUtf8().constData());
            displayView->setDisplay(impl_->display_.toUtf8().constData());
            displayView->setView(impl_->view_.toUtf8().constData());
            group->appendTransform(displayView);
            processor = impl_->ocioConfig_->getProcessor(group);
        }

        auto shader = OCIO::GpuShaderDesc::CreateShaderDesc();
        shader->setLanguage(OCIO::GPU_LANGUAGE_HLSL_SM_5_0);
        shader->setFunctionName("ArtifactOCIOViewTransform");
        shader->setPixelName("pixel");
        shader->setResourcePrefix("ArtifactOCIO_");
        processor->getDefaultGPUProcessor()->extractGpuShaderInfo(shader);
        return QString::fromUtf8(shader->getShaderText());
    } catch (const OCIO::Exception&) {
        return {};
    }
}

QJsonObject ArtifactOCIOManager::gpuViewTransformDescriptor() const
{
    QJsonObject result;
    const QString shaderText = gpuViewTransformShader();
    if (shaderText.isEmpty() || !impl_->ocioConfig_) {
        return result;
    }

    try {
        OCIO::ConstProcessorRcPtr processor;
        if (impl_->looks_.isEmpty()) {
            processor = impl_->ocioConfig_->getProcessor(
                impl_->workingSpace_.toUtf8().constData(),
                impl_->display_.toUtf8().constData(),
                impl_->view_.toUtf8().constData(),
                OCIO::TRANSFORM_DIR_FORWARD);
        } else {
            auto group = OCIO::GroupTransform::Create();
            auto look = OCIO::LookTransform::Create();
            look->setSrc(impl_->workingSpace_.toUtf8().constData());
            look->setDst(impl_->workingSpace_.toUtf8().constData());
            look->setLooks(impl_->looks_.toUtf8().constData());
            group->appendTransform(look);
            auto displayView = OCIO::DisplayViewTransform::Create();
            displayView->setSrc(impl_->workingSpace_.toUtf8().constData());
            displayView->setDisplay(impl_->display_.toUtf8().constData());
            displayView->setView(impl_->view_.toUtf8().constData());
            group->appendTransform(displayView);
            processor = impl_->ocioConfig_->getProcessor(group);
        }

        auto shader = OCIO::GpuShaderDesc::CreateShaderDesc();
        shader->setLanguage(OCIO::GPU_LANGUAGE_HLSL_SM_5_0);
        shader->setFunctionName("ArtifactOCIOViewTransform");
        shader->setPixelName("pixel");
        shader->setResourcePrefix("ArtifactOCIO_");
        processor->getDefaultGPUProcessor()->extractGpuShaderInfo(shader);

        QJsonArray uniforms;
        for (unsigned i = 0; i < shader->getNumUniforms(); ++i) {
            OCIO::GpuShaderDesc::UniformData data;
            const char* name = shader->getUniform(i, data);
            if (!name) continue;
            QJsonObject uniform;
            uniform[QStringLiteral("name")] = QString::fromUtf8(name);
            uniform[QStringLiteral("type")] = static_cast<int>(data.m_type);
            uniform[QStringLiteral("bufferOffset")] =
                static_cast<qint64>(data.m_bufferOffset);
            switch (data.m_type) {
            case OCIO::UNIFORM_DOUBLE:
                if (data.m_getDouble)
                    uniform[QStringLiteral("value")] = data.m_getDouble();
                break;
            case OCIO::UNIFORM_BOOL:
                if (data.m_getBool)
                    uniform[QStringLiteral("value")] = data.m_getBool();
                break;
            case OCIO::UNIFORM_FLOAT3:
                if (data.m_getFloat3) {
                    const auto& value = data.m_getFloat3();
                    QJsonArray values;
                    values.append(value[0]);
                    values.append(value[1]);
                    values.append(value[2]);
                    uniform[QStringLiteral("value")] = values;
                }
                break;
            case OCIO::UNIFORM_VECTOR_FLOAT:
                if (data.m_vectorFloat.m_getSize &&
                    data.m_vectorFloat.m_getVector) {
                    const unsigned size = data.m_vectorFloat.m_getSize();
                    const float* values = data.m_vectorFloat.m_getVector();
                    QJsonArray array;
                    for (unsigned j = 0; values && j < size; ++j)
                        array.append(values[j]);
                    uniform[QStringLiteral("value")] = array;
                }
                break;
            case OCIO::UNIFORM_VECTOR_INT:
                if (data.m_vectorInt.m_getSize &&
                    data.m_vectorInt.m_getVector) {
                    const unsigned size = data.m_vectorInt.m_getSize();
                    const int* values = data.m_vectorInt.m_getVector();
                    QJsonArray array;
                    for (unsigned j = 0; values && j < size; ++j)
                        array.append(values[j]);
                    uniform[QStringLiteral("value")] = array;
                }
                break;
            default:
                break;
            }
            uniforms.append(uniform);
        }
        QJsonArray textures;
        for (unsigned i = 0; i < shader->getNumTextures(); ++i) {
            const char* textureName = nullptr;
            const char* samplerName = nullptr;
            unsigned width = 0;
            unsigned height = 0;
            OCIO::GpuShaderDesc::TextureType channel =
                OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
            OCIO::GpuShaderDesc::TextureDimensions dimensions =
                OCIO::GpuShaderDesc::TEXTURE_2D;
            OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
            shader->getTexture(i, textureName, samplerName, width, height,
                               channel, dimensions, interpolation);
            QJsonObject texture;
            texture[QStringLiteral("name")] = textureName
                ? QString::fromUtf8(textureName) : QString();
            texture[QStringLiteral("sampler")] = samplerName
                ? QString::fromUtf8(samplerName) : QString();
            texture[QStringLiteral("width")] = static_cast<int>(width);
            texture[QStringLiteral("height")] = static_cast<int>(height);
            texture[QStringLiteral("dimensions")] = static_cast<int>(dimensions);
            texture[QStringLiteral("bindingIndex")] =
                static_cast<int>(shader->getTextureShaderBindingIndex(i));
            textures.append(texture);
        }
        QJsonArray textures3D;
        for (unsigned i = 0; i < shader->getNum3DTextures(); ++i) {
            const char* textureName = nullptr;
            const char* samplerName = nullptr;
            unsigned edge = 0;
            OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
            shader->get3DTexture(i, textureName, samplerName, edge,
                                 interpolation);
            QJsonObject texture;
            texture[QStringLiteral("name")] = textureName
                ? QString::fromUtf8(textureName) : QString();
            texture[QStringLiteral("sampler")] = samplerName
                ? QString::fromUtf8(samplerName) : QString();
            texture[QStringLiteral("edgeLength")] = static_cast<int>(edge);
            texture[QStringLiteral("bindingIndex")] = static_cast<int>(
                shader->get3DTextureShaderBindingIndex(i));
            textures3D.append(texture);
        }
        result[QStringLiteral("shader")] = shaderText;
        result[QStringLiteral("uniforms")] = uniforms;
        result[QStringLiteral("textures")] = textures;
        result[QStringLiteral("textures3D")] = textures3D;
        result[QStringLiteral("uniformBufferSize")] =
            static_cast<qint64>(shader->getUniformBufferSize());
    } catch (const OCIO::Exception&) {
        result = {};
    }
    return result;
}

bool ArtifactOCIOManager::bakeViewTransformLUT(
    const int size, QVector<float>& rgbValues,
    const float domainMin, const float domainMax) const
{
    if (size < 2 || size > 256 ||
        !std::isfinite(domainMin) || !std::isfinite(domainMax) ||
        domainMax <= domainMin ||
        impl_->workingSpace_.isEmpty() || impl_->display_.isEmpty() ||
        impl_->view_.isEmpty()) {
        return false;
    }

    try {
        if (!impl_->ocioConfig_) {
            const auto working = Impl::mapOCIOColorSpaceToEnum(
                impl_->workingSpace_);
            const auto display = Impl::mapOCIOColorSpaceToEnum(
                impl_->display_);
            const auto matrix =
                ArtifactCore::ColorGamutConversion::getConversionMatrix(
                    Impl::mapColorSpaceToGamut(working),
                    Impl::mapColorSpaceToGamut(display));
            rgbValues.resize(size * size * size * 3);
            const float exposureScale =
                std::pow(2.0f, impl_->viewerExposure_);
            const float inverseGamma =
                1.0f / std::max(0.001f, impl_->viewerGamma_);
            int index = 0;
            for (int b = 0; b < size; ++b) {
                for (int g = 0; g < size; ++g) {
                    for (int r = 0; r < size; ++r) {
                        const float red = domainMin + (domainMax - domainMin) *
                            static_cast<float>(r) / static_cast<float>(size - 1);
                        const float green = domainMin + (domainMax - domainMin) *
                            static_cast<float>(g) / static_cast<float>(size - 1);
                        const float blue = domainMin + (domainMax - domainMin) *
                            static_cast<float>(b) / static_cast<float>(size - 1);
                        float pixel[3] = {
                            matrix(0, red, green, blue),
                            matrix(1, red, green, blue),
                            matrix(2, red, green, blue)};
                        for (float& channel : pixel) {
                            channel = std::pow(
                                std::max(0.0f, channel * exposureScale),
                                inverseGamma);
                        }
                        rgbValues[index++] = pixel[0];
                        rgbValues[index++] = pixel[1];
                        rgbValues[index++] = pixel[2];
                    }
                }
            }
            return true;
        }
        OCIO::ConstProcessorRcPtr processor;
        if (impl_->looks_.isEmpty()) {
            processor = impl_->ocioConfig_->getProcessor(
                impl_->workingSpace_.toUtf8().constData(),
                impl_->display_.toUtf8().constData(),
                impl_->view_.toUtf8().constData(),
                OCIO::TRANSFORM_DIR_FORWARD);
        } else {
            auto group = OCIO::GroupTransform::Create();
            auto look = OCIO::LookTransform::Create();
            look->setSrc(impl_->workingSpace_.toUtf8().constData());
            look->setDst(impl_->workingSpace_.toUtf8().constData());
            look->setLooks(impl_->looks_.toUtf8().constData());
            group->appendTransform(look);
            auto displayView = OCIO::DisplayViewTransform::Create();
            displayView->setSrc(impl_->workingSpace_.toUtf8().constData());
            displayView->setDisplay(impl_->display_.toUtf8().constData());
            displayView->setView(impl_->view_.toUtf8().constData());
            group->appendTransform(displayView);
            processor = impl_->ocioConfig_->getProcessor(group);
        }
        const auto cpu = processor->getDefaultCPUProcessor();
        rgbValues.resize(size * size * size * 3);
        int index = 0;
        for (int b = 0; b < size; ++b) {
            for (int g = 0; g < size; ++g) {
                for (int r = 0; r < size; ++r) {
                    float pixel[3] = {
                        domainMin + (domainMax - domainMin) * static_cast<float>(r) /
                            static_cast<float>(size - 1),
                        domainMin + (domainMax - domainMin) * static_cast<float>(g) /
                            static_cast<float>(size - 1),
                        domainMin + (domainMax - domainMin) * static_cast<float>(b) /
                            static_cast<float>(size - 1)};
                    cpu->applyRGB(pixel);
                    const float exposureScale =
                        std::pow(2.0f, impl_->viewerExposure_);
                    const float inverseGamma =
                        1.0f / std::max(0.001f, impl_->viewerGamma_);
                    for (float& channel : pixel) {
                        channel = std::pow(
                            std::max(0.0f, channel * exposureScale),
                            inverseGamma);
                    }
                    rgbValues[index++] = pixel[0];
                    rgbValues[index++] = pixel[1];
                    rgbValues[index++] = pixel[2];
                }
            }
        }
        return true;
    } catch (const OCIO::Exception&) {
        rgbValues.clear();
        return false;
    }
}

void ArtifactOCIOManager::applyInputTransformToWorkingImage(
    ArtifactCore::ImageF32x4_RGBA& image,
    const QString& sourceColorSpace,
    const QString& sourceTransferFunction) const
{
    if (!impl_->config_.isValid() || !image.rgba32fData()) {
        return;
    }

    const QString normalizedSourceColorSpace = sourceColorSpace.trimmed();
    const QString normalizedTransferFunction = sourceTransferFunction.trimmed();

    if (impl_->ocioConfig_ && !normalizedSourceColorSpace.isEmpty() &&
        !impl_->workingSpace_.isEmpty()) {
        try {
            const auto processor = impl_->ocioConfig_->getProcessor(
                normalizedSourceColorSpace.toUtf8().constData(),
                impl_->workingSpace_.toUtf8().constData());
            const auto cpuProcessor = processor->getDefaultCPUProcessor();
            OCIO::PackedImageDesc pixels(image.rgba32fData(), image.width(),
                                          image.height(), 4);
            cpuProcessor->apply(pixels);
            return;
        } catch (const OCIO::Exception&) {
            // Fall through to the legacy transfer-function/matrix path.
        }
    }

    const auto sourceCS = Impl::mapOCIOColorSpaceToEnum(normalizedSourceColorSpace);
    const auto workingCS = Impl::mapOCIOColorSpaceToEnum(impl_->workingSpace_);
    const QString transfer = normalizedTransferFunction.toLower();
    const auto transferFunction = [&]() {
        using ArtifactCore::TransferFunction;
        if (transfer == QLatin1String("srgb")) return TransferFunction::sRGB;
        if (transfer == QLatin1String("gamma22")) return TransferFunction::Gamma22;
        if (transfer == QLatin1String("gamma24")) return TransferFunction::Gamma24;
        if (transfer == QLatin1String("gamma26")) return TransferFunction::Gamma26;
        if (transfer == QLatin1String("rec709")) return TransferFunction::Rec709;
        if (transfer == QLatin1String("rec2020") ||
            transfer == QLatin1String("rec2020_10") ||
            transfer == QLatin1String("bt2020")) {
            return TransferFunction::Rec2020_10;
        }
        if (transfer == QLatin1String("pq") ||
            transfer == QLatin1String("st2084") ||
            transfer == QLatin1String("rec2084_pq")) {
            return TransferFunction::Rec2084_PQ;
        }
        if (transfer == QLatin1String("hlg")) return TransferFunction::HLG;
        if (transfer == QLatin1String("acescc")) return TransferFunction::ACEScc;
        if (transfer == QLatin1String("acescct")) return TransferFunction::ACEScct;
        if (transfer == QLatin1String("slog3") ||
            transfer == QLatin1String("sony_slog3")) {
            return TransferFunction::SonySLog3;
        }
        if (transfer == QLatin1String("cineon") ||
            transfer == QLatin1String("dpx")) {
            return TransferFunction::Cineon;
        }
        if (transfer == QLatin1String("canonlog2") ||
            transfer == QLatin1String("canon log 2") ||
            transfer == QLatin1String("canon_log2")) {
            return TransferFunction::CanonLog2;
        }
        if (transfer == QLatin1String("canonlog3") ||
            transfer == QLatin1String("canon log 3") ||
            transfer == QLatin1String("canon_log3")) {
            return TransferFunction::CanonLog3;
        }
        return TransferFunction::Linear;
    }();
    const auto matrix = ArtifactCore::ColorGamutConversion::getConversionMatrix(
        Impl::mapColorSpaceToGamut(sourceCS), Impl::mapColorSpaceToGamut(workingCS));

    const int w = image.width();
    const int h = image.height();
    float* data = image.rgba32fData();
    ArtifactCore::Parallel::For(0, h, w * h, [&](int y) {
        float* row = data + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
        for (int x = 0; x < w; ++x) {
            float* pixel = row + static_cast<size_t>(x) * 4u;
            const float r = ArtifactCore::ColorTransferFunction::decode(pixel[0], transferFunction);
            const float g = ArtifactCore::ColorTransferFunction::decode(pixel[1], transferFunction);
            const float b = ArtifactCore::ColorTransferFunction::decode(pixel[2], transferFunction);
            pixel[0] = matrix(0, r, g, b);
            pixel[1] = matrix(1, r, g, b);
            pixel[2] = matrix(2, r, g, b);
        }
    });
}

QJsonObject ArtifactOCIOManager::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("activePresetName")] = impl_->activePresetName_;
    obj[QStringLiteral("workingSpace")] = impl_->workingSpace_;
    obj[QStringLiteral("display")] = impl_->display_;
    obj[QStringLiteral("view")] = impl_->view_;
    obj[QStringLiteral("looks")] = impl_->looks_;
    obj[QStringLiteral("viewerExposure")] = impl_->viewerExposure_;
    obj[QStringLiteral("viewerGamma")] = impl_->viewerGamma_;
    obj[QStringLiteral("config")] = impl_->config_.toJson();
    return obj;
}

bool ArtifactOCIOManager::fromJson(const QJsonObject& obj)
{
    const auto normalizedIdentifier = [](const QJsonValue& value,
                                         const qsizetype maximum) {
        return value.toString().trimmed().left(maximum);
    };
    impl_->activePresetName_ = normalizedIdentifier(
        obj.value(QStringLiteral("activePresetName")), 256);
    impl_->workingSpace_ = normalizedIdentifier(
        obj.value(QStringLiteral("workingSpace")), 1024);
    impl_->display_ = normalizedIdentifier(obj.value(QStringLiteral("display")), 1024);
    impl_->view_ = normalizedIdentifier(obj.value(QStringLiteral("view")), 1024);
    impl_->looks_ = normalizedIdentifier(obj.value(QStringLiteral("looks")), 4096);
    const float storedExposure = static_cast<float>(
        obj.value(QStringLiteral("viewerExposure")).toDouble(0.0));
    const float storedGamma = static_cast<float>(
        obj.value(QStringLiteral("viewerGamma")).toDouble(1.0));
    impl_->viewerExposure_ = std::isfinite(storedExposure)
                                 ? std::clamp(storedExposure, -10.0f, 10.0f)
                                 : 0.0f;
    impl_->viewerGamma_ = std::isfinite(storedGamma)
                              ? std::clamp(storedGamma, 0.1f, 4.0f)
                              : 1.0f;

    const QJsonObject configJson = obj.value(QStringLiteral("config")).toObject();
    if (!configJson.isEmpty()) {
        impl_->config_.loadFromJson(configJson);
    }

    impl_->ocioConfig_.reset();
    if (!impl_->config_.configFilePath().isEmpty()) {
        try {
            impl_->ocioConfig_ = OCIO::Config::CreateFromFile(
                impl_->config_.configFilePath().toUtf8().constData());
        } catch (const OCIO::Exception&) {
            impl_->ocioConfig_.reset();
        }
    }

    if (impl_->config_.isValid()) {
        const auto colorSpaces = impl_->config_.colorSpaces();
        const bool hasWorkingSpace = std::any_of(
            colorSpaces.cbegin(), colorSpaces.cend(),
            [this](const ArtifactCore::OCIOColorSpace& colorSpace) {
                return colorSpace.name == impl_->workingSpace_;
            });
        if (!colorSpaces.isEmpty() && !hasWorkingSpace) {
            impl_->workingSpace_ = impl_->config_.workingSpace();
            if (impl_->workingSpace_.isEmpty()) {
                impl_->workingSpace_ = colorSpaces.first().name;
            }
        }
        const QStringList displays = impl_->config_.displays();
        if (!displays.isEmpty() && !displays.contains(impl_->display_)) {
            impl_->display_ = displays.first();
        }
        const QStringList views = impl_->config_.viewsForDisplay(impl_->display_);
        if (!views.isEmpty() && !views.contains(impl_->view_)) {
            impl_->view_ = views.first();
        }
        impl_->config_.setWorkingSpace(impl_->workingSpace_);
        impl_->config_.setActiveDisplay(impl_->display_);
        impl_->config_.setActiveView(impl_->view_);
    }

    configChanged();
    return true;
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactOCIOManager)
