module;
#include <Audioclient.h>
#include <Mmdeviceapi.h>
module Audio.WASAPIDevice;

import std;
import <cstdint>;
import Audio.IAudioDevice;

namespace Artifact {

class WASAPIDevice : public IAudioDevice {
public:
    WASAPIDevice();
    ~WASAPIDevice();

    bool open(int sampleRate, int channels, int framesPerBuffer) override;
    void close() override;
    bool start() override;
    void stop() override;
    void write(const float* interleaved, size_t frames) override;
    std::uint64_t position() const override;
    AudioDeviceState state() const override;

private:
    // Minimal members for a simple blocking render client
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

WASAPIDevice::WASAPIDevice() {}
WASAPIDevice::~WASAPIDevice() { close(); }

bool WASAPIDevice::open(int sampleRate, int channels, int framesPerBuffer) {
    sampleRate_ = sampleRate; channels_ = channels; framesPerBuffer_ = framesPerBuffer;
    HRESULT hr;

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || !enumerator) return false;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    enumerator->Release();
    if (FAILED(hr) || !device_) return false;

    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient_);
    if (FAILED(hr) || !audioClient_) return false;

    hr = audioClient_->GetMixFormat(&mixFormat_);
    if (FAILED(hr) || !mixFormat_) return false;

    // Initialize audio client for shared mode
    REFERENCE_TIME bufferDuration = (REFERENCE_TIME)((framesPerBuffer_ * 10000000LL) / sampleRate_);
    hr = audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, mixFormat_, nullptr);
    if (FAILED(hr)) return false;

    hr = audioClient_->GetService(__uuidof(IAudioRenderClient), (void**)&renderClient_);
    if (FAILED(hr) || !renderClient_) return false;

    state_ = AudioDeviceState::Opened;
    return true;
}

void WASAPIDevice::close() {
    if (renderClient_) { renderClient_->Release(); renderClient_ = nullptr; }
    if (audioClient_) { audioClient_->Release(); audioClient_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
    if (mixFormat_) { CoTaskMemFree(mixFormat_); mixFormat_ = nullptr; }
    CoUninitialize();
    state_ = AudioDeviceState::Closed;
}

bool WASAPIDevice::start() {
    if (!audioClient_) return false;
    HRESULT hr = audioClient_->Start();
    if (FAILED(hr)) return false;
    state_ = AudioDeviceState::Started;
    return true;
}

void WASAPIDevice::stop() {
    if (!audioClient_) return;
    audioClient_->Stop();
    state_ = AudioDeviceState::Stopped;
}

void WASAPIDevice::write(const float* interleaved, size_t frames) {
    if (!renderClient_ || !audioClient_) return;
    // Simple blocking write: get buffer and copy
    UINT32 numFramesAvailable = 0;
    HRESULT hr = audioClient_->GetBufferSize(&numFramesAvailable);
    if (FAILED(hr)) return;
    BYTE* data = nullptr;
    hr = renderClient_->GetBuffer(frames, &data);
    if (FAILED(hr)) return;
    // Assuming float32 planar interleaved matches mixFormat_ (platform dependent)
    memcpy(data, interleaved, frames * channels_ * sizeof(float));
    renderClient_->ReleaseBuffer(frames, 0);
    framesWritten_ += frames;
}

std::uint64_t WASAPIDevice::position() const { return framesWritten_; }

AudioDeviceState WASAPIDevice::state() const { return state_; }

}
