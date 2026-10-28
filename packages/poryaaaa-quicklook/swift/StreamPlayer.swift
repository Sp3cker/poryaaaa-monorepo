import Darwin

/// Streaming render measurements, also useful to the headless verification tool.
public struct RenderResult: Sendable {
    public let frames: UInt64
    public let peak: Float
    public let droppedTracks: Int
    public var duration: Double { Double(frames) / Double(StreamPlayer.sampleRate) }
}

/// Offline PCM16 WAV adapter over the same pull engine used by live playback.
public final class StreamPlayer {
    public static let sampleRate = StreamEngineCore.sampleRate
    public static let maxDurationSeconds = StreamEngineCore.maxDurationSeconds
    private let arena: MidiArena
    private let core: StreamEngineCore
    private var rendered = false

    public init(arena: MidiArena) throws {
        self.arena = arena
        core = try StreamEngineCore(arena: arena)
    }

    public convenience init(path: String) throws { try self.init(arena: MidiArena(path: path)) }

    /// Writes bounded stereo PCM16, including up to two seconds of tail within the absolute duration cap.
    /// The destination must not already exist. Any newly created partial WAV is removed on failure.
    public func render(to path: String) throws -> RenderResult {
        guard !rendered else { throw MidiStreamError.noEvents }
        rendered = true
        var sink = try WAVSink(path: path)
        defer { sink.close() }
        var succeeded = false
        defer { if !succeeded { unlink(path) } }
        var peak: Float = 0
        // Stack scratch keeps the core's parity-preserving internal cache disjoint.
        try withUnsafeTemporaryAllocation(of: Float.self, capacity: MidiArena.blockFrames * 2) { buffer in
            buffer.initialize(repeating: 0)
            guard let left = buffer.baseAddress else { preconditionFailure("nonempty audio scratch") }
            let right = left.advanced(by: MidiArena.blockFrames)
            while !core.isFinished {
                try core.fillAhead()
                let count = core.renderNext(left: left, right: right, frames: MidiArena.blockFrames)
                for index in 0..<count {
                    peak = max(peak, max(abs(left[index]), abs(right[index])))
                    arena.pcm[index * 2] = Int16((max(-1, min(1, left[index])) * 32767).rounded()).littleEndian
                    arena.pcm[index * 2 + 1] = Int16((max(-1, min(1, right[index])) * 32767).rounded()).littleEndian
                }
                try sink.writePCM(arena.pcm, frames: count)
            }
        }
        try sink.finish(frames: core.renderedFrames)
        succeeded = true
        return RenderResult(frames: core.renderedFrames, peak: peak, droppedTracks: core.droppedTracks)
    }
}

// The sink shares the player's lifetime, but not the parser's responsibilities.
// POSIX writes avoid allocating a Foundation Data object for every audio slice.
private struct WAVSink {
    private var descriptor: Int32

    init(path: String) throws {
        descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0o600)
        guard descriptor >= 0 else { throw MidiStreamError.io(operation: "create WAV", code: errno) }
        do {
            try writeHeader(frames: 0)
        } catch {
            Darwin.close(descriptor)
            unlink(path)
            throw error
        }
    }

    mutating func close() {
        if descriptor >= 0 { Darwin.close(descriptor) }
        descriptor = -1
    }

    func writePCM(_ samples: UnsafePointer<Int16>, frames: Int) throws {
        try writeAll(UnsafeRawPointer(samples), count: frames * 4)
    }

    func finish(frames: UInt64) throws {
        guard frames <= UInt64(UInt32.max - 36) / 4 else { throw MidiStreamError.wavTooLarge }
        guard lseek(descriptor, 0, SEEK_SET) == 0 else {
            throw MidiStreamError.io(operation: "seek WAV header", code: errno)
        }
        try writeHeader(frames: UInt32(frames))
    }

    private func writeHeader(frames: UInt32) throws {
        var header = WAVHeader(frames: frames)
        try withUnsafeBytes(of: &header) { bytes in
            guard let base = bytes.baseAddress else { preconditionFailure("nonempty WAV header") }
            try writeAll(base, count: bytes.count)
        }
    }

    private func writeAll(_ bytes: UnsafeRawPointer, count: Int) throws {
        var offset = 0
        while offset < count {
            let written = Darwin.write(descriptor, bytes.advanced(by: offset), count - offset)
            if written < 0 && errno == EINTR { continue }
            guard written > 0 else { throw MidiStreamError.io(operation: "write WAV", code: errno) }
            offset += written
        }
    }
}

// Integer-only, 44-byte RIFF header; no array or Data storage is needed.
private struct WAVHeader {
    let riff: UInt32 = 0x4646_4952
    var riffSize: UInt32
    let wave: UInt32 = 0x4556_4157
    let fmt: UInt32 = 0x2074_6D66
    let fmtSize: UInt32 = 16
    let format: UInt16 = 1
    let channels: UInt16 = 2
    let sampleRate: UInt32 = StreamPlayer.sampleRate
    let bytesPerSecond: UInt32 = StreamPlayer.sampleRate * 4
    let blockAlign: UInt16 = 4
    let bits: UInt16 = 16
    let data: UInt32 = 0x6174_6164
    var dataSize: UInt32

    init(frames: UInt32) {
        precondition(MemoryLayout<Self>.size == 44, "PCM WAV header layout")
        dataSize = (frames * 4).littleEndian
        riffSize = (frames * 4 + 36).littleEndian
    }
}
