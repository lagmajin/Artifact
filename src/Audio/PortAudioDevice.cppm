module;
#ifdef USE_PORTAUDIO
#include <portaudio.h>
#endif
module Audio.PortAudioDevice;

import std;
import <cstdint>;
import Audio.IAudioDevice;

namespace Artifact {

class PortAudioDevice : public IAudioDevice {
public:
    PortAudioDevice();
    ~PortAudioDevice();

    bool open(int sampleRate, int channels, int framesPerBuffer) override;
    void close() override;
    bool start() override;
    void stop() override;
    void write(const float* interleaved, size_t frames) override;
    std::uint64_t position() const override;
    AudioDeviceState state() const override;

private:
#ifdef USE_PORTAUDIO
    PaStream* stream_ = nullptr;
#else
    void* stream_ = nullptr; // placeholder when PortAudio not available
#endif
    int sampleRate_ = 48000;
    int channels_ = 2;
    int framesPerBuffer_ = 512;
    std::uint64_t framesWritten_ = 0;
    AudioDeviceState state_ = AudioDeviceState::Closed;
};

PortAudioDevice::PortAudioDevice() {}
PortAudioDevice::~PortAudioDevice() { close(); }

bool PortAudioDevice::open(int sampleRate, int channels, int framesPerBuffer) {
    sampleRate_ = sampleRate; channels_ = channels; framesPerBuffer_ = framesPerBuffer;
#ifdef USE_PORTAUDIO
    PaError err = Pa_Initialize();
    if (err != paNoError) return false;
    PaStreamParameters outParams;
    outParams.device = Pa_GetDefaultOutputDevice();
    if (outParams.device == paNoDevice) return false;
    outParams.channelCount = channels_;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&stream_, nullptr, &outParams, sampleRate_, framesPerBuffer_, paNoFlag, nullptr, nullptr);
    if (err != paNoError) return false;
    state_ = AudioDeviceState::Opened;
    return true;
#else
    // PortAudio not available in this build; fail gracefully
    (void)sampleRate; (void)channels; (void)framesPerBuffer;
    return false;
#endif
}

void PortAudioDevice::close() {
#ifdef USE_PORTAUDIO
    if (stream_) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    Pa_Terminate();
    state_ = AudioDeviceState::Closed;
#else
    state_ = AudioDeviceState::Closed;
#endif
}

bool PortAudioDevice::start() {
#ifdef USE_PORTAUDIO
    if (!stream_) return false;
    PaError err = Pa_StartStream(stream_);
    if (err != paNoError) return false;
    state_ = AudioDeviceState::Started;
    return true;
#else
    return false;
#endif
}

void PortAudioDevice::stop() {
#ifdef USE_PORTAUDIO
    if (!stream_) return;
    Pa_StopStream(stream_);
    state_ = AudioDeviceState::Stopped;
#else
    state_ = AudioDeviceState::Stopped;
#endif
}

void PortAudioDevice::write(const float* interleaved, size_t frames) {
#ifdef USE_PORTAUDIO
    if (!stream_) return;
    Pa_WriteStream(stream_, interleaved, frames);
    framesWritten_ += frames;
#else
    // No-op when PortAudio not present
    (void)interleaved; (void)frames;
#endif
}

std::uint64_t PortAudioDevice::position() const { return framesWritten_; }

AudioDeviceState PortAudioDevice::state() const { return state_; }

}
