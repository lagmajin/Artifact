module;
#include <cstring>
#include <algorithm>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

#include <QList>
#include <QString>
module Artifact.VST.Effect;




import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Artifact.VST.Host;
import VST3.Interfaces;
import Core.ArtifactString;

namespace Artifact {

namespace {

QString vst3ParameterTitle(const Steinberg::Vst::ParameterInfo& info) {
    int length = 0;
    while (length < 128 && info.title[length] != u'\0') ++length;
    return QString::fromUtf16(
        reinterpret_cast<const ushort*>(info.title), length);
}

} // namespace

// 内部実装
class VSTEffect::Impl {
public:
    int pluginId = -1;
    std::string pluginPath;
    std::string pluginName;
    double sampleRate = 44100.0;
    int blockSize = 512;
    bool isLoaded = false;
    bool isVST3 = false;
    std::unique_ptr<Steinberg::Vst::VST3EffectHost> vst3Host;
    std::array<std::vector<float>, 2> vst3InputScratch;
    std::array<std::vector<float>, 2> vst3OutputScratch;
    
    // パラメータ管理
    std::map<std::string, float> parameters;
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createVSTEffect(const std::string& path) {
    auto effect = std::make_unique<VSTEffect>();
    if (!path.empty()) {
        effect->loadPlugin(path);
    }
    return effect;
}

VSTEffect::VSTEffect() : impl_(new Impl()) {
}

VSTEffect::~VSTEffect() {
    unloadPlugin();
}

bool VSTEffect::loadPlugin(const std::string& path) {
    auto& host = VSTHost::getInstance();
    
    // 既にロードされている場合は閉じる
    if (impl_->isLoaded) {
        unloadPlugin();
    }
    
    impl_->isVST3 = VSTPluginLoader::isVST3Plugin(path);
    if (impl_->isVST3) {
        auto vst3 = std::make_unique<Steinberg::Vst::VST3EffectHost>();
        if (!vst3->load(ArtifactCore::String(path)) ||
            !vst3->setup(impl_->sampleRate, impl_->blockSize) ||
            !vst3->setProcessing(true)) {
            return false;
        }
        try {
            for (auto& channel : impl_->vst3InputScratch) {
                channel.resize(static_cast<size_t>(impl_->blockSize));
            }
            for (auto& channel : impl_->vst3OutputScratch) {
                channel.resize(static_cast<size_t>(impl_->blockSize));
            }
        } catch (...) {
            return false;
        }
        impl_->pluginName = vst3->className();
        impl_->pluginId = -1;
        impl_->vst3Host = std::move(vst3);
    } else {
        if (!host.loadPlugin(path)) return false;
        impl_->pluginPath = path;
        impl_->pluginId = host.getLoadedPluginIdByPath(path);
        if (impl_->pluginId < 0) {
            impl_->pluginId = host.getLoadedPluginCount() - 1;
        }
        host.setSampleRate(impl_->sampleRate);
        host.setBlockSize(impl_->blockSize);
        host.resume(impl_->pluginId);
        impl_->pluginName = host.getPluginInfo(impl_->pluginId).name;
    }

    impl_->pluginPath = path;
    impl_->isLoaded = true;
    
    return true;
}

void VSTEffect::unloadPlugin() {
    if (!impl_->isLoaded) return;

    if (impl_->vst3Host) {
        impl_->vst3Host->closeEditor();
        impl_->vst3Host->setProcessing(false);
        impl_->vst3Host->setActive(false);
        impl_->vst3Host->unload();
        impl_->vst3Host.reset();
        for (auto& channel : impl_->vst3InputScratch) channel.clear();
        for (auto& channel : impl_->vst3OutputScratch) channel.clear();
    } else {
        auto& host = VSTHost::getInstance();
        if (impl_->pluginId >= 0) host.unloadPlugin(impl_->pluginId);
    }
    
    impl_->pluginId = -1;
    impl_->pluginPath.clear();
    impl_->pluginName.clear();
    impl_->isLoaded = false;
    impl_->isVST3 = false;
}

std::string VSTEffect::getPluginName() const {
    return impl_->pluginName;
}

bool VSTEffect::isPluginLoaded() const {
    return impl_->isLoaded;
}

void VSTEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!impl_->isLoaded) {
        return;
    }
    
    if (impl_->vst3Host) {
        const int requiredInputChannels = impl_->vst3Host->inputChannelCount();
        const int outputChannels = impl_->vst3Host->outputChannelCount();
        const int frameCount = segment.frameCount();
        const int hostChannels = segment.channelCount();
        if ((hostChannels != 1 && hostChannels != 2) ||
            requiredInputChannels < 1 || requiredInputChannels > 2 ||
            outputChannels < 1 || outputChannels > 2 || frameCount <= 0 ||
            frameCount > impl_->blockSize ||
            segment.channelData.size() < hostChannels) {
            return;
        }
        for (int channel = 0; channel < hostChannels; ++channel) {
            if (segment.channelData[channel].size() < frameCount) return;
        }
        std::array<float*, 2> inputSources{nullptr, nullptr};
        std::array<float*, 2> outputTargets{nullptr, nullptr};
        if (requiredInputChannels == hostChannels) {
            for (int channel = 0; channel < requiredInputChannels; ++channel) {
                inputSources[channel] = const_cast<float*>(
                    segment.channelData[channel].constData());
            }
        } else if (requiredInputChannels == 1) {
            const auto& left = segment.channelData[0];
            const auto& right = segment.channelData[1];
            float* mono = impl_->vst3InputScratch[0].data();
            for (int frame = 0; frame < frameCount; ++frame) {
                mono[frame] = 0.5f * left[frame] + 0.5f * right[frame];
            }
            inputSources[0] = mono;
        } else {
            const float* mono = segment.channelData[0].constData();
            for (int channel = 0; channel < 2; ++channel) {
                float* stereo = impl_->vst3InputScratch[channel].data();
                std::copy_n(mono, frameCount, stereo);
                inputSources[channel] = stereo;
            }
        }
        for (int channel = 0; channel < outputChannels; ++channel) {
            outputTargets[channel] = impl_->vst3OutputScratch[channel].data();
        }
        if (impl_->vst3Host->processFloat(
                inputSources.data(), requiredInputChannels,
                outputTargets.data(), outputChannels, frameCount)) {
            if (outputChannels == hostChannels) {
                for (int channel = 0; channel < hostChannels; ++channel) {
                    std::copy_n(impl_->vst3OutputScratch[channel].data(),
                                frameCount, segment.channelData[channel].data());
                }
            } else if (outputChannels == 1) {
                const float* mono = impl_->vst3OutputScratch[0].data();
                for (int channel = 0; channel < hostChannels; ++channel) {
                    std::copy_n(mono, frameCount,
                                segment.channelData[channel].data());
                }
            } else {
                const float* left = impl_->vst3OutputScratch[0].data();
                const float* right = impl_->vst3OutputScratch[1].data();
                float* mono = segment.channelData[0].data();
                for (int frame = 0; frame < frameCount; ++frame) {
                    mono[frame] = 0.5f * left[frame] + 0.5f * right[frame];
                }
            }
        }
        return;
    }

    auto& host = VSTHost::getInstance();
    host.processInPlace(impl_->pluginId, segment);
}

std::vector<AudioEffectParameter> VSTEffect::getUiParameters() const {
    std::vector<AudioEffectParameter> params;
    
    if (!impl_->isLoaded) {
        return params;
    }
    
    if (impl_->vst3Host) {
        params.reserve(static_cast<size_t>(impl_->vst3Host->parameterCount()));
        for (Steinberg::int32 index = 0;
             index < impl_->vst3Host->parameterCount(); ++index) {
            Steinberg::Vst::ParameterInfo info{};
            if (!impl_->vst3Host->getParameterInfo(index, info)) continue;
            QString name = vst3ParameterTitle(info);
            if (name.isEmpty()) name = QStringLiteral("Parameter %1").arg(index + 1);
            AudioEffectParameter effectParam;
            effectParam.name = ArtifactCore::String(name.toStdString());
            effectParam.minValue = 0.0f;
            effectParam.maxValue = 1.0f;
            effectParam.defaultValue = static_cast<float>(info.defaultNormalizedValue);
            params.push_back(std::move(effectParam));
        }
        return params;
    }

    auto& host = VSTHost::getInstance();
    auto pluginParams = host.getPluginParameters(impl_->pluginId);

    for (const auto& param : pluginParams) {
        AudioEffectParameter effectParam;
        effectParam.name = param.name;
        effectParam.minValue = param.minValue;
        effectParam.maxValue = param.maxValue;
        effectParam.defaultValue = param.defaultValue;

        params.push_back(effectParam);
    }
    
    return params;
}

void VSTEffect::setParameter(const String& name, float value) {
    const std::string key = ArtifactCore::toStdString(name);
    if (!impl_->isLoaded) {
        impl_->parameters[key] = value;
        return;
    }
    
    if (impl_->vst3Host) {
        for (Steinberg::int32 index = 0;
             index < impl_->vst3Host->parameterCount(); ++index) {
            Steinberg::Vst::ParameterInfo info{};
            if (!impl_->vst3Host->getParameterInfo(index, info)) continue;
            const QString paramName = vst3ParameterTitle(info);
            if (paramName.toStdString() != key) continue;
            const double clamped = std::clamp(
                static_cast<double>(value), 0.0, 1.0);
            if (impl_->vst3Host->setParameterNormalized(info.id, clamped)) {
                impl_->parameters[key] = static_cast<float>(clamped);
            }
            return;
        }
        return;
    }

    auto& host = VSTHost::getInstance();
    auto pluginParams = host.getPluginParameters(impl_->pluginId);
    
    for (size_t i = 0; i < pluginParams.size(); i++) {
        if (pluginParams[i].name == key) {
            const float clamped = std::clamp(value, pluginParams[i].minValue,
                                             pluginParams[i].maxValue);
            if (host.setParameter(impl_->pluginId, static_cast<int>(i), clamped)) {
                impl_->parameters[key] = clamped;
            }
            return;
        }
    }
    impl_->parameters[key] = value;
}

float VSTEffect::getParameter(const String& name) const {
    const std::string key = ArtifactCore::toStdString(name);
    if (impl_->vst3Host) {
        for (Steinberg::int32 index = 0;
             index < impl_->vst3Host->parameterCount(); ++index) {
            Steinberg::Vst::ParameterInfo info{};
            if (!impl_->vst3Host->getParameterInfo(index, info)) continue;
            if (vst3ParameterTitle(info).toStdString() == key) {
                const double normalized =
                    impl_->vst3Host->parameterNormalized(info.id);
                return std::isfinite(normalized)
                    ? static_cast<float>(std::clamp(normalized, 0.0, 1.0))
                    : 0.0f;
            }
        }
    }
    if (impl_->isLoaded && impl_->pluginId >= 0) {
        auto& host = VSTHost::getInstance();
        const auto pluginParams = host.getPluginParameters(impl_->pluginId);
        for (size_t i = 0; i < pluginParams.size(); ++i) {
            if (pluginParams[i].name == key) {
                return host.getParameter(impl_->pluginId, static_cast<int>(i));
            }
        }
    }
    const auto it = impl_->parameters.find(key);
    if (it != impl_->parameters.end()) {
        return it->second;
    }
    return 0.0f;
}

void VSTEffect::openEditor(void* parentWindow) {
    int width = 0;
    int height = 0;
    bool resizable = false;
    (void)openEditorWindow(parentWindow, nullptr, nullptr, width, height,
                           resizable);
}

bool VSTEffect::openEditorWindow(
    void* nativeParent, void* resizeContext,
    bool (*resizeCallback)(void*, int, int), int& width, int& height,
    bool& resizable) {
    width = 0;
    height = 0;
    resizable = false;
    if (!impl_->isLoaded || !nativeParent) return false;
    if (impl_->vst3Host) {
        const bool opened = impl_->vst3Host->openEditor(
            nativeParent, resizeContext, resizeCallback, width, height,
            resizable);
        return opened;
    }
    if (impl_->pluginId < 0) return false;

    auto& host = VSTHost::getInstance();
    const bool opened = host.openEditor(impl_->pluginId, nativeParent,
                                        resizeContext, resizeCallback,
                                        width, height);
    resizable = false;
    return opened;
}

bool VSTEffect::resizeEditor(int width, int height) {
    return impl_->vst3Host && impl_->vst3Host->resizeEditor(width, height);
}

void VSTEffect::closeEditor() {
    if (!impl_->isLoaded) return;

    if (impl_->vst3Host) {
        impl_->vst3Host->closeEditor();
        return;
    }
    if (impl_->pluginId < 0) return;
    
    auto& host = VSTHost::getInstance();
    host.closeEditor(impl_->pluginId);
}

bool VSTEffect::hasEditor() const {
    if (!impl_->isLoaded) return false;

    if (impl_->vst3Host) return impl_->vst3Host->hasEditor();
    if (impl_->pluginId < 0) return false;
    
    auto& host = VSTHost::getInstance();
    return host.getPluginInfo(impl_->pluginId).hasEditor;
}

void VSTEffect::setSampleRate(int sampleRate) {
    impl_->sampleRate = std::max(1, sampleRate);

    if (impl_->isLoaded && impl_->pluginId >= 0) {
        auto& host = VSTHost::getInstance();
        host.setSampleRate(impl_->sampleRate);
    }
    if (impl_->vst3Host) {
        impl_->vst3Host->setProcessing(false);
        impl_->vst3Host->setActive(false);
        if (impl_->vst3Host->setup(impl_->sampleRate, impl_->blockSize)) {
            impl_->vst3Host->setProcessing(true);
        }
    }
}

int VSTEffect::getSampleRate() const {
    return impl_->sampleRate;
}

}
