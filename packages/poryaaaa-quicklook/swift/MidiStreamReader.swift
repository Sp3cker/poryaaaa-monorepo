/// A bounded cursor parser and stable k-way merger over read-only mapped SMF bytes.
public struct MidiStreamReader {
    private let arena: MidiArena
    public let division: UInt16
    public let trackCount: Int
    public let droppedTrackCount: Int
    /// The sole retained chunk with a positive-velocity note-on, for SMF 1 only.
    public private(set) var soloNoteTrack: Int?
    private let sampleRate: UInt64
    private var originTick: UInt32 = 0
    private var originSample: UInt64 = 0
    private var tempo: UInt32 = 500_000
    private var maxChannelTick: UInt32 = .max
    public private(set) var channelEventCount: UInt64 = 0

    /// Accepts SMF 0/1 with PPQN timing; only the first 16 SMF chunks become engine tracks.
    public init(arena: MidiArena, sampleRate: UInt32 = 44_100) throws {
        self.arena = arena
        self.sampleRate = UInt64(sampleRate)
        guard sampleRate > 0 else { throw MidiStreamError.invalidHeader }
        guard Self.u32(arena, 0) == 0x4D54_6864 else { throw MidiStreamError.badMagic }
        guard Self.u32(arena, 4) == 6 else { throw MidiStreamError.invalidHeader }
        let format = Self.u16(arena, 8)
        guard format <= 1 else { throw MidiStreamError.unsupportedFormat(format) }
        let chunks = Int(Self.u16(arena, 10))
        division = Self.u16(arena, 12)
        guard division & 0x8000 == 0 else { throw MidiStreamError.smpteDivision }
        guard division > 0, chunks > 0, format != 0 || chunks == 1 else {
            throw MidiStreamError.invalidHeader
        }
        trackCount = min(chunks, MidiArena.trackCapacity)
        droppedTrackCount = max(0, chunks - MidiArena.trackCapacity)
        var offset = 14
        for index in 0..<chunks {
            guard offset <= arena.midi.count - 8 else { throw MidiStreamError.truncated }
            guard Self.u32(arena, offset) == 0x4D54_726B else { throw MidiStreamError.badMagic }
            let length = Int(Self.u32(arena, offset + 4))
            offset += 8
            guard length <= arena.midi.count - offset else { throw MidiStreamError.truncated }
            if index < trackCount {
                arena.cursors[index] = TrackCursor(start: offset, offset: offset, end: offset + length)
            }
            offset += length
        }
        // One zero-allocation prepass bounds musical duration without buffering arbitrarily many trailing tempos.
        var finalChannelTick: UInt32 = 0
        var noteTrackCount = 0
        var noteTrack: Int?
        for index in 0..<trackCount {
            var hasNoteOn = false
            try advance(index)
            while arena.cursors[index].hasPeek {
                let event = arena.cursors[index].peek
                if event.kind != .tempo { finalChannelTick = max(finalChannelTick, event.tick) }
                if event.kind == .noteOn && event.data1 > 0 { hasNoteOn = true }
                try advance(index)
            }
            if hasNoteOn {
                noteTrackCount += 1
                noteTrack = index
            }
            let cursor = arena.cursors[index]
            arena.cursors[index] = TrackCursor(start: cursor.start, offset: cursor.start, end: cursor.end)
        }
        maxChannelTick = finalChannelTick
        soloNoteTrack = format == 1 && noteTrackCount == 1 ? noteTrack : nil
        for index in 0..<trackCount { try advance(index) }
    }

    /// Rewinds cursors and tempo state while retaining validated layout, duration
    /// bounds, and solo-track detection. Does not repeat the initial prepass.
    public mutating func rewind() throws {
        originTick = 0
        originSample = 0
        tempo = 500_000
        channelEventCount = 0
        for index in 0..<trackCount {
            let cursor = arena.cursors[index]
            arena.cursors[index] = TrackCursor(start: cursor.start, offset: cursor.start, end: cursor.end)
            try advance(index)
        }
    }

    /// Produces one event, updating the tempo anchor only when that event wins the merge.
    public mutating func next() throws -> StreamEvent? {
        var winner = -1
        for index in 0..<trackCount {
            let cursor = arena.cursors[index]
            if cursor.hasPeek && (winner < 0 || cursor.peek.tick < arena.cursors[winner].peek.tick) {
                winner = index
            }
        }
        guard winner >= 0 else { return nil }
        var event = arena.cursors[winner].peek
        let numerator = UInt64(event.tick - originTick) * UInt64(tempo)
        let product = numerator.multipliedFullWidth(by: sampleRate)
        let denominator = UInt64(division) * 1_000_000
        guard product.high < denominator else { throw MidiStreamError.sampleOverflow }
        let elapsed = denominator.dividingFullWidth(product).quotient
        let (sample, overflow) = originSample.addingReportingOverflow(elapsed)
        guard !overflow else { throw MidiStreamError.sampleOverflow }
        event.sample = sample
        if event.kind == .tempo {
            originTick = event.tick
            originSample = sample
            tempo = event.microsecondsPerQuarter
        } else {
            channelEventCount += 1
        }
        try advance(winner)
        return event
    }

    private mutating func advance(_ index: Int) throws {
        var cursor = arena.cursors[index]
        cursor.hasPeek = false
        defer { arena.cursors[index] = cursor }
        while !cursor.finished && cursor.offset < cursor.end {
            let delta = try vlq(&cursor, index)
            let (tick, overflow) = cursor.tick.addingReportingOverflow(delta)
            guard !overflow else { throw MidiStreamError.tickOverflow }
            cursor.tick = tick
            let first = try byte(&cursor, index)
            if first == 0xFF {
                cursor.runningStatus = 0
                let type = try byte(&cursor, index)
                let length = Int(try vlq(&cursor, index))
                guard length <= cursor.end - cursor.offset else { throw MidiStreamError.malformedTrack(index) }
                let payload = cursor.offset
                cursor.offset += length
                if type == 0x2F {
                    guard length == 0 else { throw MidiStreamError.malformedTrack(index) }
                    cursor.finished = true
                    return
                }
                if type == 0x51 {
                    guard length == 3 else { throw MidiStreamError.malformedTrack(index) }
                    let value =
                        UInt32(arena.midi[payload]) << 16
                        | UInt32(arena.midi[payload + 1]) << 8 | UInt32(arena.midi[payload + 2])
                    guard value > 0 else { throw MidiStreamError.malformedTrack(index) }
                    if tick > maxChannelTick { continue }
                    cursor.peek = StreamEvent(
                        tick: tick, kind: .tempo, track: UInt8(index), microsecondsPerQuarter: value)
                    cursor.hasPeek = true
                    return
                }
                // Meta text, loop markers, and other payloads are skipped in-place.
            } else if first == 0xF0 || first == 0xF7 {
                cursor.runningStatus = 0
                let length = Int(try vlq(&cursor, index))
                guard length <= cursor.end - cursor.offset else { throw MidiStreamError.malformedTrack(index) }
                cursor.offset += length
            } else {
                let status: UInt8
                let data0: UInt8
                if first & 0x80 != 0 {
                    guard first >= 0x80 && first < 0xF0 else { throw MidiStreamError.malformedTrack(index) }
                    status = first
                    cursor.runningStatus = first
                    data0 = try byte(&cursor, index)
                } else {
                    guard cursor.runningStatus != 0 else { throw MidiStreamError.malformedTrack(index) }
                    status = cursor.runningStatus
                    data0 = first
                }
                let type = status >> 4
                let data1 = type == 0xC || type == 0xD ? 0 : try byte(&cursor, index)
                guard data0 < 0x80 && data1 < 0x80 else { throw MidiStreamError.malformedTrack(index) }
                let translated = type == 0x9 && data1 == 0 ? UInt8(0x8) : type
                if let kind = StreamEvent.Kind(rawValue: translated) {
                    cursor.peek = StreamEvent(tick: tick, kind: kind, track: UInt8(index), data0: data0, data1: data1)
                    cursor.hasPeek = true
                    return
                }
                // Poly/channel pressure have no engine equivalent, but still consume their bytes.
            }
        }
        cursor.finished = true
    }

    private func byte(_ cursor: inout TrackCursor, _ index: Int) throws -> UInt8 {
        guard cursor.offset < cursor.end else { throw MidiStreamError.malformedTrack(index) }
        defer { cursor.offset += 1 }
        return arena.midi[cursor.offset]
    }

    private func vlq(_ cursor: inout TrackCursor, _ index: Int) throws -> UInt32 {
        var result: UInt32 = 0
        for _ in 0..<4 {
            let value = try byte(&cursor, index)
            result = result << 7 | UInt32(value & 0x7F)
            if value & 0x80 == 0 { return result }
        }
        throw MidiStreamError.malformedTrack(index)
    }

    private static func u16(_ arena: MidiArena, _ offset: Int) -> UInt16 {
        UInt16(arena.midi[offset]) << 8 | UInt16(arena.midi[offset + 1])
    }

    private static func u32(_ arena: MidiArena, _ offset: Int) -> UInt32 {
        UInt32(arena.midi[offset]) << 24 | UInt32(arena.midi[offset + 1]) << 16
            | UInt32(arena.midi[offset + 2]) << 8 | UInt32(arena.midi[offset + 3])
    }
}
