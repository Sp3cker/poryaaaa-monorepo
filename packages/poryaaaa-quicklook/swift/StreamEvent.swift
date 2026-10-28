/// A translated SMF event. Payload bytes are values; no mapped MIDI payload is retained.
public struct StreamEvent: Sendable, Equatable {
    public enum Kind: UInt8, Sendable {
        case noteOff = 0x8
        case noteOn = 0x9
        case controlChange = 0xB
        case programChange = 0xC
        case pitchBend = 0xE
        case tempo = 0x51
    }

    public var sample: UInt64 = 0
    public var tick: UInt32 = 0
    public var kind: Kind = .noteOff
    public var track: UInt8 = 0
    public var data0: UInt8 = 0
    public var data1: UInt8 = 0
    public var microsecondsPerQuarter: UInt32 = 0

    public init(
        sample: UInt64 = 0, tick: UInt32 = 0, kind: Kind = .noteOff,
        track: UInt8 = 0, data0: UInt8 = 0, data1: UInt8 = 0,
        microsecondsPerQuarter: UInt32 = 0
    ) {
        self.sample = sample
        self.tick = tick
        self.kind = kind
        self.track = track
        self.data0 = data0
        self.data1 = data1
        self.microsecondsPerQuarter = microsecondsPerQuarter
    }
}

/// Failures reported by the streaming reader, engine setup, and WAV writer.
public enum MidiStreamError: Error, Equatable {
    case io(operation: String, code: Int32)
    case truncated
    case badMagic
    case invalidHeader
    case unsupportedFormat(UInt16)
    case smpteDivision
    case malformedTrack(Int)
    case tickOverflow
    case sampleOverflow
    case noEvents
    case bankLoad
    case engineCreation
    case wavTooLarge
}

// Only offsets and values live here, never pointers into a temporary buffer.
struct TrackCursor {
    var start = 0
    var offset = 0
    var end = 0
    var tick: UInt32 = 0
    var runningStatus: UInt8 = 0
    var hasPeek = false
    var finished = false
    var peek = StreamEvent()
}
