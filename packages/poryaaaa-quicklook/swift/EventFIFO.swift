import Synchronization

/// Single producer/single consumer queue. Publish storage with release, observe with acquire.
/// The noncopyable queue retains its arena; producer and consumer must not change roles concurrently.
public struct EventFIFO: ~Copyable {
    private let arena: MidiArena
    private let head = Atomic<UInt64>(0)
    private let tail = Atomic<UInt64>(0)
    private let eof = Atomic<Bool>(false)

    public init(arena: MidiArena) { self.arena = arena }

    /// Resets an empty generation. Both producer and consumer must be stopped.
    public borrowing func reset() {
        head.store(0, ordering: .relaxed)
        tail.store(0, ordering: .relaxed)
        eof.store(false, ordering: .releasing)
    }

    /// Returns false without modifying the queue when all 8192 slots are occupied.
    public borrowing func push(_ event: StreamEvent) -> Bool {
        let write = head.load(ordering: .relaxed)
        let read = tail.load(ordering: .acquiring)
        guard write &- read < UInt64(MidiArena.fifoCapacity) else { return false }
        arena.events[Int(write & UInt64(MidiArena.fifoCapacity - 1))] = event
        head.store(write &+ 1, ordering: .releasing)
        return true
    }

    /// Consumer-only lookahead; does not remove the event.
    public borrowing func peek() -> StreamEvent? {
        let read = tail.load(ordering: .relaxed)
        guard read != head.load(ordering: .acquiring) else { return nil }
        return arena.events[Int(read & UInt64(MidiArena.fifoCapacity - 1))]
    }

    /// Consumer-only removal.
    public borrowing func pop() -> StreamEvent? {
        let read = tail.load(ordering: .relaxed)
        guard read != head.load(ordering: .acquiring) else { return nil }
        let value = arena.events[Int(read & UInt64(MidiArena.fifoCapacity - 1))]
        tail.store(read &+ 1, ordering: .releasing)
        return value
    }

    /// EOF is orthogonal to queue occupancy: queued events remain readable after this call.
    public borrowing func markEOF() { eof.store(true, ordering: .releasing) }
    public var isEOF: Bool { borrowing get { eof.load(ordering: .acquiring) } }
    public var isFull: Bool {
        borrowing get {
            head.load(ordering: .acquiring) &- tail.load(ordering: .acquiring) == UInt64(MidiArena.fifoCapacity)
        }
    }
}
