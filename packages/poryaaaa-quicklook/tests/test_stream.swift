import CryptoKit
import Darwin
import Foundation
import QuickLookPlayer
import Testing

private struct Fixture {
    let directory: URL
    let midi: URL
    let wav: URL

    init(bytes: [UInt8]) throws {
        directory = FileManager.default.temporaryDirectory.appendingPathComponent(
            "pql-test-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: false)
        midi = directory.appendingPathComponent("input.mid", isDirectory: false)
        wav = directory.appendingPathComponent("output.wav", isDirectory: false)
        try Data(bytes).write(to: midi)
    }

    func remove() { try? FileManager.default.removeItem(at: directory) }
}

private func be16(_ value: UInt16) -> [UInt8] {
    [UInt8(truncatingIfNeeded: value >> 8), UInt8(truncatingIfNeeded: value)]
}

private func be32(_ value: UInt32) -> [UInt8] {
    [
        UInt8(truncatingIfNeeded: value >> 24), UInt8(truncatingIfNeeded: value >> 16),
        UInt8(truncatingIfNeeded: value >> 8), UInt8(truncatingIfNeeded: value),
    ]
}

private func smf(_ tracks: [[UInt8]], format: UInt16 = 1, division: UInt16 = 96) -> [UInt8] {
    var bytes: [UInt8] = Array("MThd".utf8) + be32(6) + be16(format) + be16(UInt16(tracks.count)) + be16(division)
    for track in tracks { bytes += Array("MTrk".utf8) + be32(UInt32(track.count)) + track }
    return bytes
}

private func synthetic(tempoChange: Bool = false) -> [UInt8] {
    var conductor: [UInt8] = [0, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20]
    var notes: [UInt8] = [0, 0xC0, 5, 0, 0x90, 60, 100, 96, 0x80, 60, 0]
    if tempoChange {
        conductor += [96, 0xFF, 0x51, 3, 0x03, 0xD0, 0x90]
        notes += [0, 0x90, 64, 100, 96, 0x80, 64, 0]
    }
    conductor += [0, 0xFF, 0x2F, 0]
    notes += [0, 0xFF, 0x2F, 0]
    return smf([conductor, notes])
}

private func events(_ path: String) throws -> [StreamEvent] {
    let arena = try MidiArena(path: path)
    var reader = try MidiStreamReader(arena: arena)
    var result: [StreamEvent] = []
    while let event = try reader.next() { result.append(event) }
    return result
}

/// Headless checks run the producer and consumer cooperatively on one thread.
private func pullBlock(_ core: StreamEngineCore, left: inout [Float], right: inout [Float]) throws -> Int {
    precondition(!left.isEmpty && left.count == right.count)
    try core.fillAhead()
    let frames = left.count
    return left.withUnsafeMutableBufferPointer { l in
        right.withUnsafeMutableBufferPointer { r in
            guard let lp = l.baseAddress, let rp = r.baseAddress else {
                preconditionFailure("nonempty test buffers")
            }
            return core.renderNext(left: lp, right: rp, frames: frames)
        }
    }
}

@Test private func noteIndexPairsRetriggersAcrossTempoChangesAndClosesOpenNotes() throws {
    let conductor: [UInt8] = [
        0, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20,
        96, 0xFF, 0x51, 3, 0x03, 0xD0, 0x90, 0, 0xFF, 0x2F, 0,
    ]
    let notes: [UInt8] = [
        0, 0x90, 60, 100, 48, 0x90, 60, 100,
        48, 0x80, 60, 0, 0, 0x90, 60, 0,
        0, 0x90, 64, 100, 96, 0x80, 64, 0,
        0, 0x90, 67, 100, 48, 0xB0, 7, 100, 0, 0xFF, 0x2F, 0,
    ]
    let otherTrack: [UInt8] = [0, 0x90, 60, 100, 48, 0x80, 60, 0, 0, 0xFF, 0x2F, 0]
    let fixture = try Fixture(bytes: smf([conductor, notes, otherTrack]))
    defer { fixture.remove() }
    let index = try MidiNoteIndex(path: fixture.midi.path)
    #expect(index.totalDurationSamples == 38_587)
    #expect(index.notes == [
        MidiNoteIndex.Note(engineTrack: 1, key: 60, startSample: 0, endSample: 22_050),
        MidiNoteIndex.Note(engineTrack: 2, key: 60, startSample: 0, endSample: 11_025),
        MidiNoteIndex.Note(engineTrack: 1, key: 60, startSample: 11_025, endSample: 22_050),
        MidiNoteIndex.Note(engineTrack: 1, key: 64, startSample: 22_050, endSample: 33_075),
        MidiNoteIndex.Note(engineTrack: 1, key: 67, startSample: 33_075, endSample: 38_587),
    ])
}

@Test private func noteIndexCapsDurationAndOpenNotesAtThreeHundredSeconds() throws {
    let track: [UInt8] = [
        0, 0xFF, 0x51, 3, 0x0F, 0x42, 0x40,
        0, 0x90, 60, 100, 0x82, 0x2D, 0x80, 60, 0, 0, 0xFF, 0x2F, 0,
    ]
    let fixture = try Fixture(bytes: smf([track], format: 0, division: 1))
    defer { fixture.remove() }
    let index = try MidiNoteIndex(path: fixture.midi.path)
    #expect(index.totalDurationSamples == 13_230_000)
    #expect(index.notes == [MidiNoteIndex.Note(engineTrack: 0, key: 60, startSample: 0, endSample: 13_230_000)])
}

@Test private func optionalRealMIDINoteIndexReportsBuildTime() throws {
    guard let value = getenv("PQL_TEST_MIDI"), access(value, R_OK) == 0 else {
        print("SKIP note index timing: PQL_TEST_MIDI is unset or absent")
        return
    }
    let index = try MidiNoteIndex(path: String(cString: value))
    #expect(!index.notes.isEmpty)
    #expect(index.totalDurationSamples == 2_914_045)
    print("mus_shop note index: notes=\(index.notes.count) durationSamples=\(index.totalDurationSamples) build=\(index.buildDuration)")
}

private func seekFixtureBytes() -> [UInt8] {
    let track: [UInt8] = [
        0, 0xC0, 5, 96, 0x90, 60, 100, 96, 0x80, 60, 0,
        96, 0x90, 64, 100, 96, 0x80, 64, 0, 0, 0xFF, 0x2F, 0,
    ]
    return smf([track], format: 0)
}

@Test private func seekForwardChasesProgramDropsStraddlingNotesAndPlaysFutureNotes() throws {
    let fixture = try Fixture(bytes: seekFixtureBytes())
    defer { fixture.remove() }
    let core = try StreamEngineCore(path: fixture.midi.path)
    try core.seek(toSample: 33_075)
    #expect(core.renderedFrames == 33_075)
    #expect(core.currentProgram(forTrack: 0) == 5)
    var left = [Float](repeating: 1, count: 512)
    var right = [Float](repeating: 1, count: 512)
    #expect(try pullBlock(core, left: &left, right: &right) == 512)
    #expect(left.allSatisfy { $0 == 0 } && right.allSatisfy { $0 == 0 })
    var peak: Float = 0
    while core.renderedFrames < 72_000 && !core.isFinished {
        let count = try pullBlock(core, left: &left, right: &right)
        for index in 0..<count { peak = max(peak, max(abs(left[index]), abs(right[index]))) }
    }
    #expect(peak > 0.01)
    print("forward seek: program=\(core.currentProgram(forTrack: 0)) position=\(core.renderedFrames) futurePeak=\(peak)")
}

@Test private func seekBackwardResetsSoundingVoicesAndReverbWithoutGhostAudio() throws {
    let fixture = try Fixture(bytes: seekFixtureBytes())
    defer { fixture.remove() }
    let core = try StreamEngineCore(path: fixture.midi.path)
    var left = [Float](repeating: 0, count: 512)
    var right = [Float](repeating: 0, count: 512)
    var peak: Float = 0
    while core.renderedFrames < 33_075 {
        let count = try pullBlock(core, left: &left, right: &right)
        for index in 0..<count { peak = max(peak, max(abs(left[index]), abs(right[index]))) }
    }
    #expect(peak > 0.01)
    try core.seek(toSample: 0)
    #expect(core.renderedFrames == 0)
    #expect(try pullBlock(core, left: &left, right: &right) == 512)
    #expect(core.renderedFrames == 512)
    #expect(left.allSatisfy { $0 == 0 } && right.allSatisfy { $0 == 0 })
    print("backward seek: preSeekPeak=\(peak) position=\(core.renderedFrames) first-block peak=0")
}

@Test(arguments: [UInt64(100_000), UInt64.max])
private func seekBeyondEndFinishesAndPullsSilence(target: UInt64) throws {
    let fixture = try Fixture(bytes: seekFixtureBytes())
    defer { fixture.remove() }
    let core = try StreamEngineCore(path: fixture.midi.path)
    try core.seek(toSample: target)
    #expect(core.isFinished)
    let position = core.renderedFrames
    var left = [Float](repeating: 1, count: 512)
    var right = [Float](repeating: 1, count: 512)
    #expect(try pullBlock(core, left: &left, right: &right) == 0)
    #expect(core.renderedFrames == position)
    #expect(left.allSatisfy { $0 == 0 } && right.allSatisfy { $0 == 0 })
}

@Test private func seekChasePreservesTheSoloPianoRule() throws {
    let fixture = try Fixture(bytes: synthetic())
    defer { fixture.remove() }
    let core = try StreamEngineCore(path: fixture.midi.path)
    try core.seek(toSample: 11_025)
    #expect(core.currentProgram(forTrack: 1) == 1)
    #expect(core.renderedFrames == 11_025)
}

@Test private func optionalRealMIDICoreSeekToNinetySecondsStaysBelowLatencyBudget() throws {
    guard let value = getenv("PQL_TEST_MIDI"), access(value, R_OK) == 0 else {
        print("SKIP seek latency: PQL_TEST_MIDI is unset or absent")
        return
    }
    let core = try StreamEngineCore(path: String(cString: value))
    let began = ContinuousClock.now
    try core.seek(toSample: 90 * UInt64(StreamEngineCore.sampleRate))
    let elapsed = ContinuousClock.now - began
    // Ample scheduling allowance around cursor rewind, state chase, and the C reset.
    #expect(elapsed < .milliseconds(250))
    #expect(core.renderedFrames == 3_969_000)
    #expect(core.isFinished) // mus_shop ends before this target.
    print("mus_shop core seek target=90s elapsed=\(elapsed) budget=250ms")
}

private struct WAVMeasurements {
    let frames: UInt64
    let peak: Double
    var duration: Double { Double(frames) / 44_100 }
}

private func measureWAV(_ url: URL) throws -> WAVMeasurements {
    let data = try Data(contentsOf: url, options: .mappedIfSafe)
    return try data.withUnsafeBytes { (bytes: UnsafeRawBufferPointer) in
        try #require(bytes.count >= 44)
        func le16(_ offset: Int) -> UInt16 { UInt16(bytes[offset]) | UInt16(bytes[offset + 1]) << 8 }
        func le32(_ offset: Int) -> UInt32 {
            UInt32(bytes[offset]) | UInt32(bytes[offset + 1]) << 8
                | UInt32(bytes[offset + 2]) << 16 | UInt32(bytes[offset + 3]) << 24
        }
        #expect(le32(0) == 0x4646_4952)
        #expect(le32(8) == 0x4556_4157)
        #expect(le32(12) == 0x2074_6D66)
        #expect(le32(16) == 16)
        #expect(le16(20) == 1)
        #expect(le16(22) == 2)
        #expect(le32(24) == 44_100)
        #expect(le16(32) == 4)
        #expect(le16(34) == 16)
        #expect(le32(36) == 0x6174_6164)
        let size = Int(le32(40))
        try #require(size == bytes.count - 44)
        #expect(Int(le32(4)) == bytes.count - 8)
        #expect(size % 4 == 0)
        var peak: Double = 0
        for offset in stride(from: 44, to: bytes.count, by: 2) {
            peak = max(peak, abs(Double(Int16(bitPattern: le16(offset)))) / 32768)
        }
        return WAVMeasurements(frames: UInt64(size / 4), peak: peak)
    }
}

@Test private func soloNoteDetectionCountsOnlyRetainedPositiveVelocityTypeOneChunks() throws {
    let conductor: [UInt8] = [0, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20, 0, 0xFF, 0x2F, 0]
    let notes: [UInt8] = [0, 0x90, 60, 100, 96, 0x80, 60, 0, 0, 0xFF, 0x2F, 0]
    let velocityZero: [UInt8] = [0, 0x90, 64, 0, 0, 0xFF, 0x2F, 0]
    let empty: [UInt8] = [0, 0xFF, 0x2F, 0]
    let cases: [(format: UInt16, tracks: [[UInt8]], expected: Int?)] = [
        (1, [conductor, notes], 1),
        (1, [notes], 0),
        (1, [conductor, notes, velocityZero], 1),
        (1, [conductor, notes, notes], nil),
        (1, [conductor, velocityZero], nil),
        (0, [notes], nil),
        (1, [conductor, notes] + Array(repeating: empty, count: 14) + [notes], 1),
    ]
    for testCase in cases {
        let fixture = try Fixture(bytes: smf(testCase.tracks, format: testCase.format))
        defer { fixture.remove() }
        let reader = try MidiStreamReader(arena: MidiArena(path: fixture.midi.path))
        #expect(reader.soloNoteTrack == testCase.expected)
    }
}

@Test(arguments: [false, true])
private func typeOneSoloTrackForcesPianoWhileMultipleNoteTracksKeepPrograms(hasSecondNoteTrack: Bool) throws {
    let conductor: [UInt8] = [0, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20, 0, 0xFF, 0x2F, 0]
    // Both initial and later program changes must be suppressed for a solo track.
    let notes: [UInt8] = [
        0, 0xC0, 5, 0, 0x90, 60, 100, 48, 0xC0, 5,
        48, 0x80, 60, 0, 0, 0xFF, 0x2F, 0,
    ]
    let secondNotes: [UInt8] = [0, 0x90, 64, 100, 96, 0x80, 64, 0, 0, 0xFF, 0x2F, 0]
    var tracks = [conductor, notes]
    if hasSecondNoteTrack { tracks.append(secondNotes) }
    let fixture = try Fixture(bytes: smf(tracks))
    defer { fixture.remove() }
    let core = try StreamEngineCore(path: fixture.midi.path)
    #expect(core.currentProgram(forTrack: 0) == 0)
    #expect(core.currentProgram(forTrack: 1) == (hasSecondNoteTrack ? 0 : 1))
    var left = [Float](repeating: 0, count: 512)
    var right = [Float](repeating: 0, count: 512)
    var peak: Float = 0
    let expectedProgram: UInt8 = hasSecondNoteTrack ? 5 : 1
    while !core.isFinished {
        let count = try pullBlock(core, left: &left, right: &right)
        #expect(core.currentProgram(forTrack: 1) == expectedProgram)
        for index in 0..<count { peak = max(peak, max(abs(left[index]), abs(right[index]))) }
    }
    #expect(peak > 0.01)
    print("type-1 secondNoteTrack=\(hasSecondNoteTrack) currentProgram=\(core.currentProgram(forTrack: 1)) peak=\(peak)")
}

@Test private func readerMergesTracksAndConvertsQuarterNote() throws {
    let fixture = try Fixture(bytes: synthetic())
    defer { fixture.remove() }
    let result = try events(fixture.midi.path)
    let playable = result
    #expect(playable.map(\.kind) == [.tempo, .programChange, .noteOn, .noteOff])
    #expect(playable.map(\.tick) == [0, 0, 0, 96])
    #expect(playable.map(\.track) == [0, 1, 1, 1])
    #expect(playable[1].data0 == 5)
    #expect(playable[2].data0 == 60 && playable[2].data1 == 100)
    #expect(playable[3].sample == 22_050)
}

@Test private func tempoChangeFoldsTheCurrentAnchor() throws {
    let fixture = try Fixture(bytes: synthetic(tempoChange: true))
    defer { fixture.remove() }
    let result = try events(fixture.midi.path)
    let changes = result.filter { $0.kind == .tempo }
    #expect(changes.map(\.microsecondsPerQuarter) == [500_000, 250_000])
    #expect(changes.map(\.sample) == [0, 22_050])
    let offs = result.filter { $0.kind == .noteOff }
    #expect(offs.map(\.sample) == [22_050, 33_075])
    let at96 = result.filter { $0.tick == 96 }
    #expect(at96.first?.kind == .tempo)
}

@Test private func readerRewindReusesPrepassAndMatchesFreshTempoChangedStream() throws {
    let fixture = try Fixture(bytes: synthetic(tempoChange: true))
    defer { fixture.remove() }
    let fresh = try events(fixture.midi.path)
    let arena = try MidiArena(path: fixture.midi.path)
    var reader = try MidiStreamReader(arena: arena)
    while try reader.next() != nil {}
    #expect(reader.channelEventCount > 0)
    try reader.rewind()
    #expect(reader.channelEventCount == 0)
    #expect(reader.soloNoteTrack == 1)
    var replayed: [StreamEvent] = []
    while let event = try reader.next() { replayed.append(event) }
    #expect(replayed == fresh)
}

@Test private func runningStatusVelocityZeroAndSkippedPayloads() throws {
    let track: [UInt8] = [
        0, 0x90, 60, 100, 96, 60, 0,
        0, 0xFF, 6, 4, 0x6C, 0x6F, 0x6F, 0x70,
        0, 0xF0, 3, 1, 2, 0xF7,
        0, 0xC0, 5, 0, 0xFF, 0x2F, 0,
    ]
    let fixture = try Fixture(bytes: smf([track], format: 0))
    defer { fixture.remove() }
    let result = try events(fixture.midi.path)
    #expect(result.map(\.kind) == [.noteOn, .noteOff, .programChange])
    #expect(result[1].sample == 22_050)
}

@Test private func endOfTrackPaddingIsNotScheduled() throws {
    let track: [UInt8] = [
        0, 0xC0, 5, 0, 0x90, 60, 100, 96, 0x80, 60, 0,
        0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0x2F, 0,
    ]
    let fixture = try Fixture(bytes: smf([track], format: 0))
    defer { fixture.remove() }
    let result = try events(fixture.midi.path)
    #expect(result.map(\.tick) == [0, 0, 96])
    let rendered = try StreamPlayer(path: fixture.midi.path).render(to: fixture.wav.path)
    #expect(rendered.frames == 110_250)
}

@Test(arguments: [UInt32(299), 300, 301])
private func durationCapIncludesNaturalTailAndStopsAtBoundary(lastTick: UInt32) throws {
    var track: [UInt8] = [
        0, 0xFF, 0x51, 3, 0x0F, 0x42, 0x40,
        0, 0xC0, 5, 0, 0x90, 60, 100,
    ]
    track += [UInt8(lastTick >> 7) | 0x80, UInt8(lastTick & 0x7F), 0x80, 60, 0, 0, 0xFF, 0x2F, 0]
    let fixture = try Fixture(bytes: smf([track], format: 0, division: 1))
    defer { fixture.remove() }
    let parsed = try events(fixture.midi.path)
    #expect(parsed.last?.sample == UInt64(lastTick) * UInt64(StreamPlayer.sampleRate))
    let rendered = try StreamPlayer(path: fixture.midi.path).render(to: fixture.wav.path)
    let wav = try measureWAV(fixture.wav)
    #expect(rendered.frames == 13_230_000)
    #expect(wav.frames == rendered.frames)
    #expect(wav.duration == Double(StreamPlayer.maxDurationSeconds))
    #expect(wav.peak > 0.01)
    print("capped render lastTick=\(lastTick): duration=\(wav.duration)s frames=\(wav.frames) peak=\(wav.peak)")
}

@Test private func trailingTempoMetadataIsBoundedEvenBeyondFIFOCapacity() throws {
    var conductor: [UInt8] = [0, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20]
    conductor += [0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0x51, 3, 0x07, 0xD3, 0x35]
    for _ in 0..<(MidiArena.fifoCapacity + 100) {
        conductor += [0, 0xFF, 0x51, 3, 0x07, 0xD3, 0x35]
    }
    conductor += [0, 0xFF, 0x2F, 0]
    let notes: [UInt8] = [0, 0xC0, 5, 0, 0x90, 60, 100, 96, 0x80, 60, 0, 0, 0xFF, 0x2F, 0]
    let fixture = try Fixture(bytes: smf([conductor, notes]))
    defer { fixture.remove() }
    let result = try events(fixture.midi.path)
    #expect(result.map(\.kind) == [.tempo, .programChange, .noteOn, .noteOff])
    let rendered = try StreamPlayer(path: fixture.midi.path).render(to: fixture.wav.path)
    #expect(rendered.frames == 110_250)
}

@Test private func engineTracksUseChunkIndexAndDropBeyondSixteen() throws {
    let fixture = try Fixture(bytes: smf(Array(repeating: [0, 0xC0, 5, 0, 0xFF, 0x2F, 0], count: 18)))
    defer { fixture.remove() }
    let arena = try MidiArena(path: fixture.midi.path)
    var reader = try MidiStreamReader(arena: arena)
    #expect(reader.trackCount == 16)
    #expect(reader.droppedTrackCount == 2)
    var programs: [UInt8] = []
    while let event = try reader.next() { if event.kind == .programChange { programs.append(event.track) } }
    #expect(programs == Array(UInt8(0)..<UInt8(16)))
}

@Test private func fifoWrapsWithoutDroppingAndKeepsEOFSeparate() throws {
    let fixture = try Fixture(bytes: synthetic())
    defer { fixture.remove() }
    let arena = try MidiArena(path: fixture.midi.path)
    let fifo = EventFIFO(arena: arena)
    let capacity = MidiArena.fifoCapacity
    var sequence: UInt64 = 0
    for _ in 0..<3 {
        for offset in 0..<capacity {
            #expect(fifo.push(StreamEvent(sample: sequence + UInt64(offset), kind: .noteOn)) == true)
        }
        #expect(fifo.isFull == true)
        #expect(fifo.push(StreamEvent(sample: UInt64.max)) == false)
        #expect(fifo.isEOF == false)
        for offset in 0..<capacity {
            #expect(fifo.peek()?.sample == sequence + UInt64(offset))
            #expect(fifo.pop()?.sample == sequence + UInt64(offset))
        }
        #expect(fifo.pop() == nil)
        sequence += UInt64(capacity)
    }
    #expect(fifo.push(StreamEvent(sample: sequence)) == true)
    fifo.markEOF()
    #expect(fifo.isEOF == true)
    #expect(fifo.pop()?.sample == sequence)
    #expect(fifo.pop() == nil)
    #expect(fifo.isEOF == true)
}

@Test private func denseSameTickBurstDoesNotAdvanceAudioBeforeRefill() throws {
    var track: [UInt8] = []
    for _ in 0..<(MidiArena.fifoCapacity + 100) { track += [0, 0xC0, 0] }
    track += [0, 0xC0, 5, 0, 0x90, 60, 100, 96, 0x80, 60, 0, 0, 0xFF, 0x2F, 0]
    let conductor: [UInt8] = [0, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20, 0, 0xFF, 0x2F, 0]
    let dense = try Fixture(bytes: smf([conductor, track]))
    defer { dense.remove() }
    let baseline = try Fixture(bytes: synthetic())
    defer { baseline.remove() }
    let denseResult = try StreamPlayer(path: dense.midi.path).render(to: dense.wav.path)
    let baselineResult = try StreamPlayer(path: baseline.midi.path).render(to: baseline.wav.path)
    #expect(denseResult.frames == baselineResult.frames)
    #expect(try Data(contentsOf: dense.wav) == Data(contentsOf: baseline.wav))
}

@Test private func twoSequentialRendersReleaseArenaAndPlayer() throws {
    let fixture = try Fixture(bytes: synthetic())
    defer { fixture.remove() }
    for index in 0..<2 {
        weak var releasedArena: MidiArena?
        weak var releasedPlayer: StreamPlayer?
        let output = fixture.directory.appendingPathComponent("render-\(index).wav")
        do {
            let arena = try MidiArena(path: fixture.midi.path)
            let player = try StreamPlayer(arena: arena)
            releasedArena = arena
            releasedPlayer = player
            let result = try player.render(to: output.path)
            let wav = try measureWAV(output)
            #expect(result.frames == 110_250)
            #expect(wav.frames == result.frames)
            #expect(wav.peak > 0.01)
            #expect(result.peak > 0.01)
            print("synthetic render \(index): duration=\(wav.duration)s peak=\(wav.peak) frames=\(wav.frames)")
        }
        #expect(releasedPlayer == nil)
        #expect(releasedArena == nil)
    }
}

@Test private func optionalRealMIDIReaderMaintainsSampleOrder() throws {
    guard let value = getenv("PQL_TEST_MIDI") else { return }
    let path = String(cString: value)
    guard access(path, R_OK) == 0 else { return }
    let arena = try MidiArena(path: path)
    var reader = try MidiStreamReader(arena: arena)
    #expect(reader.soloNoteTrack == nil)
    var previous: UInt64 = 0
    var finalEvent = StreamEvent()
    var lastNoteSample: UInt64 = 0
    var count = 0
    while let event = try reader.next() {
        #expect(event.sample >= previous)
        previous = event.sample
        finalEvent = event
        if event.kind == .noteOn || event.kind == .noteOff { lastNoteSample = event.sample }
        count += 1
    }
    #expect(count > 0)
    print(
        "mus_shop reader: events=\(count) final=\(finalEvent) lastNoteSample=\(lastNoteSample) duration=\(Double(previous) / 44_100)s"
    )
}

@Test private func rendersOptionalRealMIDI() throws {
    guard let value = getenv("PQL_TEST_MIDI") else {
        print("SKIP real MIDI: PQL_TEST_MIDI is not set")
        return
    }
    let path = String(cString: value)
    guard access(path, R_OK) == 0 else {
        print("SKIP real MIDI: \(path) is absent")
        return
    }
    let fixture = try Fixture(bytes: synthetic())
    defer { fixture.remove() }
    let player = try StreamPlayer(path: path)
    let result = try player.render(to: fixture.wav.path)
    let wav = try measureWAV(fixture.wav)
    #expect(wav.frames == result.frames)
    #expect(wav.peak > 0.01)
    #expect(wav.duration > 5)
    print("mus_shop: duration=\(wav.duration)s peak=\(wav.peak) frames=\(wav.frames)")
}

@Test private func rejectsMalformedInputsWithoutPartialWAV() throws {
    var badMagic = synthetic()
    badMagic[0] = 0
    let cases: [([UInt8], MidiStreamError)] = [
        ([0, 1, 2], .truncated),
        (badMagic, .badMagic),
        (smf([[0, 0xFF, 0x2F, 0]], division: 0xE728), .smpteDivision),
        (smf([[0, 0xFF, 0x2F, 0]], format: 2), .unsupportedFormat(2)),
        (smf([[0, 0x90, 60]]), .malformedTrack(0)),
        (smf([[0, 0xFF, 0x2F, 0]]), .noEvents),
        (smf([[0, 0x90, 60, 100, 96, 0x80, 60, 0, 0, 0xC0]]), .malformedTrack(0)),
    ]
    for (bytes, expected) in cases {
        let fixture = try Fixture(bytes: bytes)
        defer { fixture.remove() }
        do {
            let player = try StreamPlayer(path: fixture.midi.path)
            _ = try player.render(to: fixture.wav.path)
            Issue.record("render unexpectedly succeeded for \(expected)")
        } catch let error as MidiStreamError {
            #expect(error == expected)
        }
        #expect(access(fixture.wav.path, F_OK) != 0)
    }
}

/// Independent test WAV writer: exercise the public planar pull seam, not the offline adapter.
private func pullWAV(path: String, output: URL, chunkFrames: Int = 512) throws -> UInt64 {
    let core = try StreamEngineCore(path: path)
    var left = [Float](repeating: 0, count: chunkFrames)
    var right = [Float](repeating: 0, count: chunkFrames)
    var pcm = Data()
    while !core.isFinished {
        let count = try pullBlock(core, left: &left, right: &right)
        for index in 0..<count {
            for sample in [left[index], right[index]] {
                var value = Int16((max(-1, min(1, sample)) * 32767).rounded()).littleEndian
                withUnsafeBytes(of: &value) { pcm.append(contentsOf: $0) }
            }
        }
    }
    func le16(_ value: UInt16) -> Data {
        var value = value.littleEndian
        return withUnsafeBytes(of: &value) { Data($0) }
    }
    func le32(_ value: UInt32) -> Data {
        var value = value.littleEndian
        return withUnsafeBytes(of: &value) { Data($0) }
    }
    var wav = Data("RIFF".utf8)
    wav.append(le32(UInt32(pcm.count + 36)))
    wav.append(Data("WAVEfmt ".utf8))
    wav.append(le32(16))
    wav.append(le16(1))
    wav.append(le16(2))
    wav.append(le32(44_100))
    wav.append(le32(176_400))
    wav.append(le16(4))
    wav.append(le16(16))
    wav.append(Data("data".utf8))
    wav.append(le32(UInt32(pcm.count)))
    wav.append(pcm)
    try wav.write(to: output)
    // Finished pulls must return silence without moving the musical clock.
    left.withUnsafeMutableBufferPointer { l in
        right.withUnsafeMutableBufferPointer { r in
            guard let lp = l.baseAddress, let rp = r.baseAddress else { return }
            #expect(core.renderNext(left: lp, right: rp, frames: chunkFrames) == 0)
        }
    }
    #expect(left.allSatisfy { $0 == 0 } && right.allSatisfy { $0 == 0 })
    return core.renderedFrames
}

@Test(arguments: [1, 512, 4096])
private func syntheticPullMatchesOfflineBytes(chunkFrames: Int) throws {
    let fixture = try Fixture(bytes: synthetic(tempoChange: true))
    defer { fixture.remove() }
    let pulled = fixture.directory.appendingPathComponent("pulled.wav")
    let offline = try StreamPlayer(path: fixture.midi.path).render(to: fixture.wav.path)
    let frames = try pullWAV(path: fixture.midi.path, output: pulled, chunkFrames: chunkFrames)
    #expect(frames == offline.frames)
    #expect(try Data(contentsOf: pulled) == Data(contentsOf: fixture.wav))
}

@Test private func realMIDIPullPreservesGoldenSHA256() throws {
    guard let value = getenv("PQL_TEST_MIDI") else {
        print("SKIP pull parity: PQL_TEST_MIDI is not set")
        return
    }
    let path = String(cString: value)
    guard access(path, R_OK) == 0 else {
        print("SKIP pull parity: \(path) is absent")
        return
    }
    let fixture = try Fixture(bytes: synthetic())
    defer { fixture.remove() }
    let frames = try pullWAV(path: path, output: fixture.wav)
    let digest = SHA256.hash(data: try Data(contentsOf: fixture.wav))
    let hash = digest.map { String(format: "%02x", $0) }.joined()
    #expect(frames == 3_002_245)
    #expect(hash == "8069a8327c15e8619d20634e770b6c6faaf09eeec6ed5993caf37c4a5167a66c")
    print("mus_shop pull 512: frames=\(frames) SHA-256=\(hash)")
}

@Test @MainActor private func livePlayerStartsOrExplicitlySkipsUnavailableDevice() throws {
    let fixture = try Fixture(bytes: synthetic())
    defer { fixture.remove() }
    let player = try LivePlayer(path: fixture.midi.path)
    do {
        try player.start()
    } catch {
        print("SKIP LivePlayer output device could not start: \(error)")
        #expect(!player.isRunning)
        player.stop()
        return
    }
    #expect(player.isRunning)
    let sourceIdentity = try #require(player.sourceNodeIdentity)
    try player.seek(toSample: 11_025)
    #expect(player.isRunning)
    #expect(player.currentPositionSamples >= 11_025)
    #expect(player.sourceNodeIdentity == sourceIdentity)
    try player.seek(toSample: 100_000)
    #expect(player.isRunning)
    #expect(player.sourceNodeIdentity == sourceIdentity)
    try player.seek(toSample: 0)
    #expect(player.isRunning)
    #expect(player.sourceNodeIdentity == sourceIdentity)
    player.stop()
    player.stop()
    #expect(!player.isRunning)
    try player.seek(toSample: 11_025) // Explicit scrub after a real stop takes the cold path.
    #expect(player.isRunning)
    player.stop()
    print("LivePlayer gated seeks kept engine running and source identity unchanged; cold seek restarted cleanly")
}

@Test @MainActor private func livePlayerReplacementKeepsExactlyOneActivePreview() throws {
    let fixture = try Fixture(bytes: synthetic())
    defer { fixture.remove() }
    let first = try LivePlayer(path: fixture.midi.path)
    let second = try LivePlayer(path: fixture.midi.path)
    let third = try LivePlayer(path: fixture.midi.path)
    defer {
        first.stop()
        second.stop()
        third.stop()
    }
    do {
        try first.start()
        #expect(first.isRunning)
        try second.start()
        #expect(!first.isRunning && second.isRunning)
        // A retained old controller can disappear after its replacement starts.
        first.stop()
        try third.start()
        #expect(!second.isRunning && third.isRunning)
        second.stop()
        #expect(third.isRunning)
    } catch {
        print("SKIP LivePlayer replacement: output device could not start: \(error)")
        return
    }
    print("LivePlayer replacement stopped each predecessor; stale stop preserved the active slot")
}

@main
private enum StreamTests {
    static func main() async {
        let status: CInt = await Testing.__swiftPMEntryPoint()
        exit(status)
    }
}
