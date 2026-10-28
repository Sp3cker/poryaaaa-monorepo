import QuickLookEngine
import Synchronization

/// Pull-based SPSC MIDI engine. Call `fillAhead` only on the producer and
/// `renderNext` only on the consumer; quiesce both before `seek`.
/// All storage and engine voices are prepared before rendering starts.
public final class StreamEngineCore {
    public static let sampleRate: UInt32 = 44_100
    public static let maxDurationSeconds: UInt64 = 300
    private static let capFrames = UInt64(sampleRate) * maxDurationSeconds

    // Outside a quiescent seek, these isolated regions have fixed roles:
    // reader/queuedThrough: producer; engine/cache/clocks: consumer.
    // The arena's cursor, FIFO, and audio regions are disjoint allocations slices.
    private let arena: MidiArena
    private var reader: MidiStreamReader
    private let fifo: EventFIFO
    private let bank: OpaquePointer
    private let engine: UnsafeMutablePointer<M4AEngine>
    private let soloNoteTrack: Int?
    private var queuedThrough: UInt64 = 0
    private let availableThrough = Atomic<UInt64>(0)
    private let capped = Atomic<Bool>(false)
    private let consumed = Atomic<UInt64>(0)
    private let done = Atomic<Bool>(false)
    private var clock: UInt64 = 0
    private var lastEventSample: UInt64 = 0
    private var blockEnd: UInt64 = 0
    private var cacheCount = 0
    private var cacheOffset = 0
    private var terminalBlock = false
    public let droppedTracks: Int
    public var renderedFrames: UInt64 { consumed.load(ordering: .acquiring) }
    public var isFinished: Bool { done.load(ordering: .acquiring) }
    public var isProducerFinished: Bool { fifo.isEOF }

    public init(arena: MidiArena) throws {
        self.arena = arena
        reader = try MidiStreamReader(arena: arena, sampleRate: Self.sampleRate)
        soloNoteTrack = reader.soloNoteTrack
        droppedTracks = reader.droppedTrackCount
        fifo = EventFIFO(arena: arena)
        var size = 0
        guard let blob = pql_embedded_hanabi(&size), let bank = pql_bank_load(blob, size) else {
            throw MidiStreamError.bankLoad
        }
        guard let engine = m4a_engine_create(Float(Self.sampleRate)) else {
            pql_bank_free(bank)
            throw MidiStreamError.engineCreation
        }
        self.bank = bank
        self.engine = engine
        configureEngine()
        try fillAhead()
    }

    public convenience init(path: String) throws { try self.init(arena: MidiArena(path: path)) }

    deinit {
        m4a_engine_free(engine)
        pql_bank_free(bank)
    }

    /// Reads the actual C-engine program state. Consumer-thread-only: inspect after
    /// rendering or after the consumer has stopped, never during live playback.
    public func currentProgram(forTrack track: Int) -> UInt8 {
        precondition((0..<MidiArena.trackCapacity).contains(track), "engine track index out of range")
        return withUnsafePointer(to: &engine.pointee.tracks) { tracks in
            tracks.withMemoryRebound(to: M4ATrack.self, capacity: MidiArena.trackCapacity) {
                $0[track].currentProgram
            }
        }
    }

    /// Rewinds and chases state strictly before the target, dropping historical notes.
    /// Producer and consumer must be quiescent. Reset destroys/recreates all C
    /// driver, hardware, voice, and reverb storage; no pre-seek sound can survive.
    /// Notes straddling the target deliberately stay silent.
    public func seek(toSample requestedSample: UInt64) throws {
        allSoundOff()
        guard m4a_engine_reset(engine) else { throw MidiStreamError.engineCreation }
        configureEngine()
        try reader.rewind()
        fifo.reset()
        let target = min(requestedSample, Self.capFrames)
        clock = target
        blockEnd = target
        consumed.store(target, ordering: .releasing)
        queuedThrough = 0
        availableThrough.store(0, ordering: .releasing)
        capped.store(false, ordering: .releasing)
        done.store(false, ordering: .releasing)
        lastEventSample = 0
        cacheCount = 0
        cacheOffset = 0
        terminalBlock = false
        if target == Self.capFrames {
            fifo.markEOF()
            done.store(true, ordering: .releasing)
            return
        }
        while let event = try reader.next() {
            if event.sample >= Self.capFrames {
                capped.store(true, ordering: .releasing)
                fifo.markEOF()
                return
            }
            if event.sample >= target {
                precondition(fifo.push(event), "seek starts with an empty FIFO")
                queuedThrough = event.sample
                availableThrough.store(event.sample, ordering: .releasing)
                return
            }
            switch event.kind {
            case .noteOn, .noteOff:
                break
            case .tempo, .programChange, .controlChange, .pitchBend:
                dispatch(event)
            }
            lastEventSample = event.sample
        }
        fifo.markEOF()
        done.store(true, ordering: .releasing)
    }

    /// Fills until full, EOF, the cap, or at least one second beyond consumption.
    /// A future-event watermark prevents the consumer advancing over unpublished events.
    public func fillAhead() throws {
        let horizon = min(renderedFrames + UInt64(Self.sampleRate), Self.capFrames)
        while !fifo.isEOF && queuedThrough < horizon && !fifo.isFull {
            guard let event = try reader.next() else {
                guard reader.channelEventCount > 0 else { throw MidiStreamError.noEvents }
                fifo.markEOF()
                break
            }
            guard event.sample < Self.capFrames else {
                capped.store(true, ordering: .releasing)
                fifo.markEOF()
                break
            }
            precondition(fifo.push(event), "single producer checked queue capacity")
            queuedThrough = event.sample
            availableThrough.store(queuedThrough, ordering: .releasing)
        }
    }

    /// Pulls up to `frames` into planar buffers and zeroes the remainder.
    /// Returns musical frames delivered (zero on underrun or after finish).
    /// No allocation, locks, parsing, logging, or Objective-C calls occur here.
    /// Internal 2048-frame/event slices preserve offline engine call boundaries
    /// regardless of the caller's buffer size, including the final natural tail.
    public func renderNext(left: UnsafeMutablePointer<Float>, right: UnsafeMutablePointer<Float>, frames: Int) -> Int {
        precondition(frames >= 0)
        var written = 0
        while written < frames && !isFinished {
            if cacheOffset == cacheCount {
                if terminalBlock {
                    done.store(true, ordering: .releasing)
                    break
                }
                // Resume a partially generated block after a producer underrun.
                if clock == blockEnd {
                    cacheOffset = 0
                    cacheCount = 0
                    blockEnd = min(clock + UInt64(MidiArena.blockFrames), Self.capFrames)
                }
                guard generateBlock() else { break }
            }
            let count = min(frames - written, cacheCount - cacheOffset)
            left.advanced(by: written).update(from: arena.left.advanced(by: cacheOffset), count: count)
            right.advanced(by: written).update(from: arena.right.advanced(by: cacheOffset), count: count)
            cacheOffset += count
            written += count
            consumed.wrappingAdd(UInt64(count), ordering: .releasing)
            if cacheOffset == cacheCount && terminalBlock { done.store(true, ordering: .releasing) }
        }
        if written < frames {
            left.advanced(by: written).update(repeating: 0, count: frames - written)
            right.advanced(by: written).update(repeating: 0, count: frames - written)
        }
        return written
    }

    /// Requires a quiescent consumer: audio stopped, or render gate held and drained.
    public func allSoundOff() { m4a_engine_all_sound_off(engine) }

    private func configureEngine() {
        m4a_engine_set_voicegroup(engine, pql_bank_voices(bank))
        m4a_engine_set_max_pcm_channels(engine, 15)
        for track in 0..<16 {
            m4a_engine_program_change(engine, Int32(track), track == soloNoteTrack ? 1 : 0)
        }
    }

    private func generateBlock() -> Bool {
        while true {
            // Observe EOF before the queue: its release publishes every final event.
            let eof = fifo.isEOF
            if let event = fifo.peek() {
                if event.sample <= blockEnd {
                    process(until: event.sample)
                    _ = fifo.pop()
                    dispatch(event)
                    lastEventSample = event.sample
                    continue
                }
                process(until: blockEnd)
                return true
            }
            if eof {
                let end = capped.load(ordering: .acquiring)
                    ? Self.capFrames : min(lastEventSample + UInt64(Self.sampleRate) * 2, Self.capFrames)
                process(until: min(blockEnd, end))
                terminalBlock = clock == end
                return true
            }
            if availableThrough.load(ordering: .acquiring) >= blockEnd {
                process(until: blockEnd)
                terminalBlock = clock == Self.capFrames
                return true
            }
            return false
        }
    }

    private func process(until end: UInt64) {
        guard clock < end else { return }
        let count = Int(end - clock)
        m4a_engine_process(engine, arena.left.advanced(by: cacheCount), arena.right.advanced(by: cacheCount), Int32(count))
        cacheCount += count
        clock = end
    }

    private func dispatch(_ event: StreamEvent) {
        let track = Int32(event.track)
        switch event.kind {
        case .noteOff:
            m4a_engine_note_off(engine, track, event.data0)
        case .noteOn:
            engine.pointee.polyEventClock = event.tick
            engine.pointee.auditionNote = false
            m4a_engine_note_on(engine, track, event.data0, event.data1)
        case .controlChange:
            if event.data0 == 0x78 {
                m4a_engine_all_sound_off(engine)
            } else if event.data0 == 0x7B {
                m4a_engine_all_notes_off(engine, track)
            } else {
                m4a_engine_cc(engine, track, event.data0, event.data1)
            }
        case .programChange:
            if Int(event.track) != soloNoteTrack {
                m4a_engine_program_change(engine, track, event.data0)
            }
        case .pitchBend:
            let bend = Int16((Int(event.data1) << 7 | Int(event.data0)) - 8192)
            m4a_engine_pitch_bend(engine, track, bend)
        case .tempo:
            m4a_engine_set_tempo_bpm(engine, 60_000_000 / Double(event.microsecondsPerQuarter))
        }
    }
}
