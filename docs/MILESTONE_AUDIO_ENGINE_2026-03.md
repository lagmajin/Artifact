# Audio Engine Implementation Milestone

**Status:** M-AUDIO-1〜3 partial/implemented, M-AUDIO-4 and runtime verification pending

Date: 2026-03-22

## Goal

- Implement real-time audio playback for the timeline and composition renderer.
- Enable `ArtifactVideoLayer` and `ArtifactAudioLayer` to output their decoded audio to a centralized system audio device (speakers/headphones).
- Support basic mixing (volume, mute, solo, panning) via `ArtifactAudioMixer`.
- Ensure Audio-Video Sync (A/V Sync) by using the audio playhead as the master clock during video playback.

## Scope

- `ArtifactCore/src/Media` (Audio Decoding buffer handling)
- `Artifact/src/Audio` (Audio Mixing, Routing, and Output Sink generation)
- `Artifact/src/Widgets/Render` (AV Sync handling)
- `Artifact/src/Widgets` (Timeline playhead integration)

## Milestones

### M-AUDIO-1 Audio Sink Foundation (Device Output)
- Implement an audio player class (e.g., using Qt6 `QAudioSink` or a cross-platform library like `RtAudio`/`miniaudio`) that can accept raw PCM buffers.
- Setup a continuous background thread or audio callback that requests audio samples uniformly.

### M-AUDIO-2 Decoding and Ring Buffer (Producer)
- Make `MediaPlaybackController` decode audio packets effectively via `getNextAudioFrame()`.
- Push the decoded PCM bytes continuously into a lock-free Ring Buffer per layer during playback.
- Handle resampling/format conversion so that all layers match the master output format (e.g., 48kHz, Stereo, Float32).

### M-AUDIO-3 Mixing and Routing
- Connect the layer ring buffers to the `ArtifactAudioMixer`.
- Calculate mixed output frames continuously within the audio callback.
- Apply Volume, Pan, Mute, and Solo properties managed by the mixer before sending samples to the master `QAudioSink`.

### M-AUDIO-4 Real-time A/V Synchronization
- During composition playback, query the audio engine for the true "played" hardware time.
- Drive the timeline playhead (and the video renderer) strictly based on the audio system's master clock to prevent drift.

## Validation Checklist

- Audio can be heard when playing back a composition with a VideoLayer containing an audio track.
- Volume slider in the property widget alters the playback loudness in real-time.
- Mute/Solo buttons work as expected and stop/start the audio dispatch for specific tracks.
- A 5-minute video plays perfectly in sync without accumulating lip-sync delay.

## Implementation Status (2026-07-25)

- Audio output backends exist for WASAPI and PortAudio, with `AudioRenderer` / `AudioScrubController` providing PCM device output paths.
- Core audio processing includes `AudioRingBuffer`, decoding/cache primitives, resampling/downmix helpers, and `AudioMixer` bus processing.
- `ArtifactAudioMixer` synchronizes layer strips with core buses and exposes volume, pan, mute, solo, and master controls.
- `AudioClockProvider` and scrub playback provide clock infrastructure, but the full Composition playback path has not been verified as hardware-audio-clock-driven end to end.
- The validation checklist (audible full playback, real-time controls, and long-run A/V sync) remains pending because build/runtime testing is intentionally not run yet.
