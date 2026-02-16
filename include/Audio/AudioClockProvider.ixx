module;
export module Audio.AudioClockProvider;

import std;

export namespace Artifact {

using AudioClockFn = std::function<double()>;

export class AudioClockProvider {
private:
    class Impl;
    Impl* impl_;
public:
    AudioClockProvider();
    ~AudioClockProvider();

    void setProvider(const AudioClockFn& fn);
    double now() const; // seconds
};

}
