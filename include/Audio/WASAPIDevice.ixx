module;
#include <cstdint>
export module Audio.WASAPIDevice;

import std;
import Audio.IAudioDevice;

export namespace Artifact {

// WASAPI device uses Pimpl to keep implementation private
export class WASAPIDevice : public IAudioDevice {
private:
    class Impl;
    Impl* impl_;
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
};

}
