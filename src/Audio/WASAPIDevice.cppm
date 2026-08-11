module;
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <utility>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

module Audio.WASAPIDevice;

import std;
import Audio.IAudioDevice;

namespace Artifact {

class WASAPIDevice::Impl {
public:
    IAudioClient* audioClient_ = nullptr;
    IAudioRenderClient* renderClient_ = nullptr;
    IMMDevice* device_ = nullptr;
    WAVEFORMATEX* mixFormat_ = nullptr;
    std::uint64_t framesWritten_ = 0;
    int sampleRate_ = 48000;
    int channels_ = 2;
    int framesPerBuffer_ = 512;
    AudioDeviceState state_ = AudioDeviceState::Closed;
    bool comInitialized_ = false;
};

WASAPIDevice::WASAPIDevice() : impl_(new Impl()) {}
WASAPIDevice::~WASAPIDevice() { close(); delete impl_; impl_ = nullptr; }

bool WASAPIDevice::open(int sampleRate, int channels, int framesPerBuffer) {
    if (sampleRate <= 0 || sampleRate > 384000 ||
        channels <= 0 || channels > 64 ||
        framesPerBuffer <= 0 || framesPerBuffer > (1 << 20)) {
        return false;
    }
    close();
    impl_->sampleRate_ = sampleRate; impl_->channels_ = channels; impl_->framesPerBuffer_ = framesPerBuffer;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;
    impl_->comInitialized_ = true;
    const auto fail = [this]() {
        close();
        return false;
    };

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || !enumerator) return fail();

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &impl_->device_);
    enumerator->Release();
    if (FAILED(hr) || !impl_->device_) return fail();

    hr = impl_->device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&impl_->audioClient_);
    if (FAILED(hr) || !impl_->audioClient_) return fail();

    hr = impl_->audioClient_->GetMixFormat(&impl_->mixFormat_);
    if (FAILED(hr) || !impl_->mixFormat_) return fail();

    const REFERENCE_TIME bufferDuration = (REFERENCE_TIME)((impl_->framesPerBuffer_ * 10000000LL) / impl_->sampleRate_);
    hr = impl_->audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, impl_->mixFormat_, nullptr);
    if (FAILED(hr)) return fail();

    hr = impl_->audioClient_->GetService(__uuidof(IAudioRenderClient), (void**)&impl_->renderClient_);
    if (FAILED(hr) || !impl_->renderClient_) return fail();

    impl_->state_ = AudioDeviceState::Opened;
    return true;
}

void WASAPIDevice::close() {
    if (!impl_) return;
    if (impl_->renderClient_) { impl_->renderClient_->Release(); impl_->renderClient_ = nullptr; }
    if (impl_->audioClient_) { impl_->audioClient_->Release(); impl_->audioClient_ = nullptr; }
    if (impl_->device_) { impl_->device_->Release(); impl_->device_ = nullptr; }
    if (impl_->mixFormat_) { CoTaskMemFree(impl_->mixFormat_); impl_->mixFormat_ = nullptr; }
    if (impl_->comInitialized_) {
        CoUninitialize();
        impl_->comInitialized_ = false;
    }
    impl_->state_ = AudioDeviceState::Closed;
}

bool WASAPIDevice::start() {
    if (!impl_ || !impl_->audioClient_) return false;
    const HRESULT hr = impl_->audioClient_->Start();
    if (FAILED(hr)) return false;
    impl_->state_ = AudioDeviceState::Started;
    return true;
}

void WASAPIDevice::stop() {
    if (!impl_ || !impl_->audioClient_) return;
    impl_->audioClient_->Stop();
    impl_->state_ = AudioDeviceState::Stopped;
}

void WASAPIDevice::write(const float* interleaved, size_t frames) {
    if (!impl_ || !impl_->renderClient_ || !impl_->audioClient_) return;
    if (frames == 0) return;
    UINT32 bufferSize = 0;
    HRESULT hr = impl_->audioClient_->GetBufferSize(&bufferSize);
    if (FAILED(hr)) return;
    UINT32 currentPadding = 0;
    hr = impl_->audioClient_->GetCurrentPadding(&currentPadding);
    if (FAILED(hr) || currentPadding >= bufferSize) return;
    const size_t boundedFrames = std::min(
        frames, static_cast<size_t>(std::numeric_limits<UINT32>::max()));
    const UINT32 framesToWrite = std::min(
        static_cast<UINT32>(boundedFrames), bufferSize - currentPadding);
    if (framesToWrite == 0 ||
        static_cast<size_t>(framesToWrite) >
            std::numeric_limits<size_t>::max() /
                (static_cast<size_t>(impl_->channels_) * sizeof(float))) {
        return;
    }
    BYTE* data = nullptr;
    hr = impl_->renderClient_->GetBuffer(framesToWrite, &data);
    if (FAILED(hr)) return;
    const size_t bytes = static_cast<size_t>(framesToWrite) *
                         static_cast<size_t>(impl_->channels_) * sizeof(float);
    if (interleaved) {
        std::memcpy(data, interleaved, bytes);
    } else {
        std::memset(data, 0, bytes);
    }
    if (SUCCEEDED(impl_->renderClient_->ReleaseBuffer(framesToWrite, 0))) {
        impl_->framesWritten_ += framesToWrite;
    }
}

std::uint64_t WASAPIDevice::position() const { return impl_ ? impl_->framesWritten_ : 0; }
AudioDeviceState WASAPIDevice::state() const { return impl_ ? impl_->state_ : AudioDeviceState::Closed; }

}
