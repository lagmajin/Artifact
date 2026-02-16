module;
module Audio.AudioClockProvider;

import std;
import Audio.AudioClockProvider;

namespace Artifact {

class AudioClockProvider::Impl {
public:
    AudioClockFn fn_ = nullptr;
};

AudioClockProvider::AudioClockProvider(): impl_(new Impl()) {}
AudioClockProvider::~AudioClockProvider(){ delete impl_; }

void AudioClockProvider::setProvider(const AudioClockFn& fn) { impl_->fn_ = fn; }

double AudioClockProvider::now() const {
    if (impl_->fn_) return impl_->fn_();
    return 0.0;
}

}
