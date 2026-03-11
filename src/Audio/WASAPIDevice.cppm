module;
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <cstdint>

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
};

WASAPIDevice::WASAPIDevice() : impl_(new Impl()) {}
WASAPIDevice::~WASAPIDevice() { close(); delete impl_; impl_ = nullptr; }

bool WASAPIDevice::open(int sampleRate, int channels, int framesPerBuffer) {
    impl_->sampleRate_ = sampleRate; impl_->channels_ = channels; impl_->framesPerBuffer_ = framesPerBuffer;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || !enumerator) return false;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &impl_->device_);
    enumerator->Release();
    if (FAILED(hr) || !impl_->device_) return false;

    hr = impl_->device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&impl_->audioClient_);
    if (FAILED(hr) || !impl_->audioClient_) return false;

    hr = impl_->audioClient_->GetMixFormat(&impl_->mixFormat_);
    if (FAILED(hr) || !impl_->mixFormat_) return false;

    const REFERENCE_TIME bufferDuration = (REFERENCE_TIME)((impl_->framesPerBuffer_ * 10000000LL) / impl_->sampleRate_);
    hr = impl_->audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, impl_->mixFormat_, nullptr);
    if (FAILED(hr)) return false;

    hr = impl_->audioClient_->GetService(__uuidof(IAudioRenderClient), (void**)&impl_->renderClient_);
    if (FAILED(hr) || !impl_->renderClient_) return false;

    impl_->state_ = AudioDeviceState::Opened;
    return true;
}

void WASAPIDevice::close() {
    if (!impl_) return;
    if (impl_->renderClient_) { impl_->renderClient_->Release(); impl_->renderClient_ = nullptr; }
    if (impl_->audioClient_) { impl_->audioClient_->Release(); impl_->audioClient_ = nullptr; }
    if (impl_->device_) { impl_->device_->Release(); impl_->device_ = nullptr; }
    if (impl_->mixFormat_) { CoTaskMemFree(impl_->mixFormat_); impl_->mixFormat_ = nullptr; }
    CoUninitialize();
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
    UINT32 numFramesAvailable = 0;
    HRESULT hr = impl_->audioClient_->GetBufferSize(&numFramesAvailable);
    if (FAILED(hr)) return;
    BYTE* data = nullptr;
    hr = impl_->renderClient_->GetBuffer(static_cast<UINT32>(frames), &data);
    if (FAILED(hr)) return;
    memcpy(data, interleaved, frames * impl_->channels_ * sizeof(float));
    impl_->renderClient_->ReleaseBuffer(static_cast<UINT32>(frames), 0);
    impl_->framesWritten_ += frames;
}

std::uint64_t WASAPIDevice::position() const { return impl_ ? impl_->framesWritten_ : 0; }
AudioDeviceState WASAPIDevice::state() const { return impl_ ? impl_->state_ : AudioDeviceState::Closed; }

}
