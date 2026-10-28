import Darwin

/// Owns the read-only MIDI mapping and the pipeline's single working-storage allocation.
public final class MidiArena {
    public static let trackCapacity = 16
    public static let fifoCapacity = 8192
    public static let blockFrames = 2048

    let midi: UnsafeRawBufferPointer
    private let allocation: UnsafeMutableRawBufferPointer
    private let mappedAddress: UnsafeMutableRawPointer
    let cursors: UnsafeMutablePointer<TrackCursor>
    let events: UnsafeMutablePointer<StreamEvent>
    let left: UnsafeMutablePointer<Float>
    let right: UnsafeMutablePointer<Float>
    let pcm: UnsafeMutablePointer<Int16>

    /// Maps `path` without copying the MIDI bytes into a heap buffer.
    public init(path: String) throws {
        let descriptor = open(path, O_RDONLY | O_CLOEXEC)
        guard descriptor >= 0 else { throw MidiStreamError.io(operation: "open MIDI", code: errno) }
        defer { close(descriptor) }
        var info = stat()
        guard fstat(descriptor, &info) == 0 else {
            throw MidiStreamError.io(operation: "stat MIDI", code: errno)
        }
        guard info.st_size >= 14, info.st_size <= Int.max else { throw MidiStreamError.truncated }
        let size = Int(info.st_size)
        guard let mapping = mmap(nil, size, PROT_READ, MAP_PRIVATE, descriptor, 0), mapping != MAP_FAILED else {
            throw MidiStreamError.io(operation: "mmap MIDI", code: errno)
        }
        midi = UnsafeRawBufferPointer(start: mapping, count: size)
        mappedAddress = mapping

        // All region starts are 16-byte aligned; WAV headers use a fixed-size stack value.
        let cursorBytes = Self.aligned(MemoryLayout<TrackCursor>.stride * Self.trackCapacity)
        let eventBytes = Self.aligned(MemoryLayout<StreamEvent>.stride * Self.fifoCapacity)
        let planeBytes = Self.aligned(MemoryLayout<Float>.stride * Self.blockFrames)
        let pcmBytes = Self.aligned(MemoryLayout<Int16>.stride * Self.blockFrames * 2)
        allocation = UnsafeMutableRawBufferPointer.allocate(
            byteCount: cursorBytes + eventBytes + planeBytes * 2 + pcmBytes, alignment: 16)
        guard let base = allocation.baseAddress else { preconditionFailure("nonempty arena allocation") }
        cursors = base.bindMemory(to: TrackCursor.self, capacity: Self.trackCapacity)
        events = base.advanced(by: cursorBytes).bindMemory(to: StreamEvent.self, capacity: Self.fifoCapacity)
        left = base.advanced(by: cursorBytes + eventBytes).bindMemory(to: Float.self, capacity: Self.blockFrames)
        right = base.advanced(by: cursorBytes + eventBytes + planeBytes).bindMemory(
            to: Float.self, capacity: Self.blockFrames)
        pcm = base.advanced(by: cursorBytes + eventBytes + planeBytes * 2).bindMemory(
            to: Int16.self, capacity: Self.blockFrames * 2)
        cursors.initialize(repeating: TrackCursor(), count: Self.trackCapacity)
        events.initialize(repeating: StreamEvent(), count: Self.fifoCapacity)
        left.initialize(repeating: 0, count: Self.blockFrames)
        right.initialize(repeating: 0, count: Self.blockFrames)
        pcm.initialize(repeating: 0, count: Self.blockFrames * 2)
    }

    deinit {
        cursors.deinitialize(count: Self.trackCapacity)
        events.deinitialize(count: Self.fifoCapacity)
        left.deinitialize(count: Self.blockFrames)
        right.deinitialize(count: Self.blockFrames)
        pcm.deinitialize(count: Self.blockFrames * 2)
        allocation.deallocate()
        munmap(mappedAddress, midi.count)
    }

    private static func aligned(_ count: Int) -> Int { (count + 15) & ~15 }
}
