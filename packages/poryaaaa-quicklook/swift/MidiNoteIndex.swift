/// Immutable display data built with its own mapping and reader, never the live FIFO.
public struct MidiNoteIndex: Sendable {
    public struct Note: Equatable, Sendable {
        public let engineTrack: UInt8
        public let key: UInt8
        public let startSample: UInt64
        public fileprivate(set) var endSample: UInt64

        public init(engineTrack: UInt8, key: UInt8, startSample: UInt64, endSample: UInt64) {
            self.engineTrack = engineTrack
            self.key = key
            self.startSample = startSample
            self.endSample = endSample
        }
    }

    public let notes: [Note]
    public let totalDurationSamples: UInt64
    public let buildDuration: Duration

    /// Pairs retriggers FIFO per (engine track, key). Open notes close at the final
    /// channel event; notes and musical duration are bounded by the playback cap.
    /// Call off the main actor after starting playback.
    public init(path: String) throws {
        let started = ContinuousClock.now
        let arena = try MidiArena(path: path)
        var reader = try MidiStreamReader(arena: arena, sampleRate: StreamEngineCore.sampleRate)
        let cap = UInt64(StreamEngineCore.sampleRate) * StreamEngineCore.maxDurationSeconds
        var notes: [Note] = []
        var pending: [UInt16: PendingNotes] = [:]
        var duration: UInt64 = 0
        while let event = try reader.next() {
            try Task.checkCancellation()
            guard event.sample < cap else {
                duration = cap
                break
            }
            if event.kind != .tempo { duration = event.sample }
            let identity = UInt16(event.track) * 128 + UInt16(event.data0)
            switch event.kind {
            case .noteOn:
                let index = notes.count
                notes.append(Note(engineTrack: event.track, key: event.data0,
                                  startSample: event.sample, endSample: event.sample))
                pending[identity, default: PendingNotes()].indices.append(index)
            case .noteOff:
                if let index = pending[identity]?.popFirst() { notes[index].endSample = event.sample }
            case .tempo, .programChange, .controlChange, .pitchBend:
                break
            }
        }
        for queue in pending.values {
            for index in queue.indices.dropFirst(queue.head) { notes[index].endSample = duration }
        }
        self.notes = notes
        totalDurationSamples = duration
        buildDuration = ContinuousClock.now - started
    }
}

private struct PendingNotes {
    var indices: [Int] = []
    var head = 0

    mutating func popFirst() -> Int? {
        guard head < indices.count else { return nil }
        let index = indices[head]
        head += 1
        if head == indices.count {
            indices.removeAll(keepingCapacity: true)
            head = 0
        }
        return index
    }
}
