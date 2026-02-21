module;
#include <cstdint>
export module Audio.PortAudioDevice;

import std;
import Audio.IAudioDevice;

export namespace Artifact {

// Forward-declared PortAudio-based device implementation interface
export class PortAudioDevice : public IAudioDevice {
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
    class Impl;
    Impl* impl_;
};

}
