module;
#ifdef USE_PORTAUDIO
#include <portaudio.h>
#endif
module Audio.PortAudioDevice;

import std;
import <cstdint>;
import Audio.IAudioDevice;

namespace Artifact {

class PortAudioDevice::Impl {
public:
#ifdef USE_PORTAUDIO
    PaStream* stream_ = nullptr;
#else
    void* stream_ = nullptr;
#endif
    int sampleRate_ = 48000;
    int channels_ = 2;
    int framesPerBuffer_ = 512;
    std::uint64_t framesWritten_ = 0;
    AudioDeviceState state_ = AudioDeviceState::Closed;
};

PortAudioDevice::PortAudioDevice() : impl_(new Impl()) {}
PortAudioDevice::~PortAudioDevice() { close(); delete impl_; impl_ = nullptr; }

bool PortAudioDevice::open(int sampleRate, int channels, int framesPerBuffer) {
    impl_->sampleRate_ = sampleRate; impl_->channels_ = channels; impl_->framesPerBuffer_ = framesPerBuffer;
#ifdef USE_PORTAUDIO
    PaError err = Pa_Initialize();
    if (err != paNoError) return false;
    PaStreamParameters outParams;
    outParams.device = Pa_GetDefaultOutputDevice();
    if (outParams.device == paNoDevice) return false;
    outParams.channelCount = impl_->channels_;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&impl_->stream_, nullptr, &outParams, impl_->sampleRate_, impl_->framesPerBuffer_, paNoFlag, nullptr, nullptr);
    if (err != paNoError) return false;
    impl_->state_ = AudioDeviceState::Opened;
    return true;
#else
    (void)sampleRate; (void)channels; (void)framesPerBuffer;
    return false;
#endif
}

void PortAudioDevice::close() {
#ifdef USE_PORTAUDIO
    if (impl_->stream_) {
        Pa_CloseStream(impl_->stream_);
        impl_->stream_ = nullptr;
    }
    Pa_Terminate();
    impl_->state_ = AudioDeviceState::Closed;
#else
    impl_->state_ = AudioDeviceState::Closed;
#endif
}

bool PortAudioDevice::start() {
#ifdef USE_PORTAUDIO
    if (!impl_->stream_) return false;
    PaError err = Pa_StartStream(impl_->stream_);
    if (err != paNoError) return false;
    impl_->state_ = AudioDeviceState::Started;
    return true;
#else
    return false;
#endif
}

void PortAudioDevice::stop() {
#ifdef USE_PORTAUDIO
    if (!impl_->stream_) return;
    Pa_StopStream(impl_->stream_);
    impl_->state_ = AudioDeviceState::Stopped;
#else
    impl_->state_ = AudioDeviceState::Stopped;
#endif
}

void PortAudioDevice::write(const float* interleaved, size_t frames) {
#ifdef USE_PORTAUDIO
    if (!impl_->stream_) return;
    Pa_WriteStream(impl_->stream_, interleaved, static_cast<unsigned long>(frames));
    impl_->framesWritten_ += frames;
#else
    // No-op when PortAudio not present
    (void)interleaved; (void)frames;
#endif
}

std::uint64_t PortAudioDevice::position() const { return impl_->framesWritten_; }

AudioDeviceState PortAudioDevice::state() const { return impl_->state_; }

}
