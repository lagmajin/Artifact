module;
#include <cstdint>

export module Artifact.Ofx.Isolation.Protocol;

export namespace Artifact::Ofx::Isolation {

// Wire protocol between the host application and the out-of-process OFX runner.
//
// Every field is a fixed-width integer or a byte count so the struct can be
// memcpy'd into a shared-memory slot without alignment surprises. Variable data
// (strings, parameter values, pixel payloads) travels as a byte count plus that
// many bytes immediately after the header in the same shared-memory block.
//
// Handles are opaque to plugins but are raw pointers to host structs in the
// in-process host. Across a process boundary a handle cannot be the host's
// address, so each side maps a small integer to its own object. The plugin
// only ever passes a handle back to a suite function, so a small integer cast to
// the opaque pointer type is indistinguishable from an address as far as the
// plugin is concerned.

inline constexpr std::uint32_t kProtocolMagic = 0x4F465850; // OFXP
inline constexpr std::uint32_t kProtocolVersion = 1;

// Role of a process participating in the protocol.
enum class EndpointRole : std::uint32_t {
    Host = 0,
    Runner = 1,
};

// Which side currently owns the channel and may write into it.
enum class ChannelState : std::uint32_t {
    Idle = 0,
    HostHasPayload = 1,
    RunnerHasPayload = 2,
    Closed = 3,
};

enum class MessageKind : std::uint32_t {
    // Handshake
    Hello = 1,
    HelloAck = 2,

    // Plugin discovery (runner -> host is a response, host -> runner is a request)
    GetNumberOfPlugins = 10,
    GetPlugin = 11,

    // Plugin lifecycle: these carry the action name as the payload.
    ActionLoad = 20,
    ActionDescribe = 21,
    ActionDescribeInContext = 22,
    ActionCreateInstance = 23,
    ActionDestroyInstance = 24,
    ActionBeginSequenceRender = 25,
    ActionRender = 26,
    ActionEndSequenceRender = 27,
    ActionGetClipPreferences = 28,
    ActionIsIdentity = 29,

    // Suite calls the plugin makes back into the host.
    SuiteFetch = 40,
    PropertyCall = 41,
    ClipCall = 42,
    ParamCall = 43,
    MessageCall = 44,
    ProgressCall = 45,
    TimelineCall = 46,
    MemoryCall = 47,

    // Status
    StatusOk = 60,
    StatusReplyDefault = 61,
    StatusFailed = 62,
    StatusErrUnsupported = 63,
    StatusErrBadHandle = 64,
    StatusErrValue = 65,
    StatusErrMemory = 66,
};

// Fixed header at the start of every message. `payloadOffset` is a byte offset
// from the start of the whole message to the variable-length payload.
struct MessageHeader {
    std::uint32_t magic = kProtocolMagic;
    std::uint32_t version = kProtocolVersion;
    std::uint32_t kind = 0; // MessageKind
    std::uint32_t status = 0; // OfxStatus for responses
    std::uint64_t sequence = 0;
    std::uint32_t payloadBytes = 0;
    std::uint32_t flags = 0;
};

// The two fixed-size slots the pixel buffers occupy inside the shared block.
// The host writes the source frame here, the plugin writes the output frame
// into the destination slot, and the host reads it back after the action
// returns. Base addresses differ per process, so only offsets travel on the
// wire and each side adds its own mapping base.
struct PixelSlotHeader {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t rowBytes = 0;
    std::uint32_t components = 4; // RGBA
    std::uint32_t bytesPerComponent = 4; // float
    std::uint64_t byteSize = 0;
};

// Everything both processes agree on, published at a fixed offset in the shared
// block. The runner reads this on attach; the host writes it before starting the
// runner.
struct SharedBlockHeader {
    std::uint32_t magic = kProtocolMagic;
    std::uint32_t version = kProtocolVersion;
    std::uint32_t endpointRole = 0; // EndpointRole
    std::uint32_t reserved = 0;

    // Absolute address of this process's mapping of the pixel region. Only
    // meaningful locally; the other process has a different value.
    std::uint64_t localPixelBase = 0;

    std::uint64_t sourceOffset = 0; // offset of the source slot from the block base
    std::uint64_t destinationOffset = 0;
    PixelSlotHeader sourceSlot{};
    PixelSlotHeader destinationSlot{};

    // Single request/response slot. The writer fills the message, publishes
    // state, and the reader polls it. Kept simple on purpose: OFX is a strictly
    // synchronous, single-threaded protocol per effect instance.
    std::uint64_t messageOffset = 0;
    std::uint32_t messageCapacity = 0;

    // Heartbeat the runner updates so the host can detect a hang.
    std::uint64_t runnerHeartbeat = 0;
    std::uint64_t runnerState = 0;
};

} // namespace Artifact::Ofx::Isolation