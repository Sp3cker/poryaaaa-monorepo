import AVFoundation
import Foundation
import os
import Synchronization

// SPSC FIFO/watermark and the atomic render gate synchronize ownership of the C core.
// Sole unchecked transfer: main-side resets require producer and consumer quiescence.
private final class LiveSession: @unchecked Sendable {
    let core: StreamEngineCore
    let cancelled = Atomic<Bool>(false)
    // Bit 0 gates callbacks to silence; bit 1 marks the sole active core consumer.
    let renderGate = Atomic<UInt8>(0)
    let firstBlock = Atomic<Bool>(false)
    let producerError = Mutex<(any Error)?>(nil)
    let producer = DispatchGroup()

    init(core: StreamEngineCore) { self.core = core }
}

/// Main-thread lifetime owner. The source callback owns rendering; a dedicated
/// producer queue owns parsing. Neither accesses AVAudioEngine lifecycle state.
@MainActor
public final class LivePlayer {
    private static let log = Logger(subsystem: "com.poryaaaa.quicklook", category: "playback")
    // Finder retains preview controllers across selection changes. Keep only
    // the most recently prepared preview audible, without extending its lifetime.
    private static weak var activePlayer: LivePlayer?
    private let audioEngine = AVAudioEngine()
    private let path: String
    private let producerQueue = DispatchQueue(label: "com.poryaaaa.quicklook.producer", qos: .userInitiated)
    private var session: LiveSession?
    private var source: AVAudioSourceNode?
    private var timer: Timer?
    private var loggedFirstBlock = false
    private var finishTime: ContinuousClock.Instant?
    private var stoppedPosition: UInt64 = 0

    public var isRunning: Bool { audioEngine.isRunning }

    /// Atomic audio-clock snapshot while playing; the final position remains readable after stop.
    public var currentPositionSamples: UInt64 { session?.core.renderedFrames ?? stoppedPosition }

    /// Actual source identity for lifecycle diagnostics, without exposing the audio node.
    public var sourceNodeIdentity: ObjectIdentifier? { source.map { ObjectIdentifier($0) } }

    public init(path: String) throws {
        self.path = path
        session = LiveSession(core: try StreamEngineCore(path: path))
    }

    /// Maps and validates MIDI off the main actor before creating the audio host.
    public static func prepare(path: String) async throws -> LivePlayer {
        let session = try await prepareSession(path: path)
        return LivePlayer(path: path, session: session)
    }

    private init(path: String, session: LiveSession) {
        self.path = path
        self.session = session
    }

    @concurrent
    private static func prepareSession(path: String) async throws -> LiveSession {
        LiveSession(core: try StreamEngineCore(path: path))
    }

    isolated deinit { stop() }

    // AVAudioSourceNode's callback is not imported as @Sendable. Construct it
    // outside MainActor so Swift does not insert a main-queue assertion on audio.
    nonisolated private static func makeSource(format: AVAudioFormat, session: LiveSession) -> AVAudioSourceNode {
        AVAudioSourceNode(format: format) { _, _, frameCount, audioBufferList in
            let buffers = UnsafeMutableAudioBufferListPointer(audioBufferList)
            guard buffers.count == 2,
                let left = buffers[0].mData?.assumingMemoryBound(to: Float.self),
                let right = buffers[1].mData?.assumingMemoryBound(to: Float.self)
            else { return kAudio_ParamError }
            guard session.renderGate.compareExchange(
                expected: 0, desired: 2, ordering: .acquiringAndReleasing).exchanged else {
                left.update(repeating: 0, count: Int(frameCount))
                right.update(repeating: 0, count: Int(frameCount))
                return noErr
            }
            // Preserve a mute request that arrived while this block was rendering.
            defer { _ = session.renderGate.bitwiseAnd(1, ordering: .releasing) }
            _ = session.core.renderNext(left: left, right: right, frames: Int(frameCount))
            session.firstBlock.store(true, ordering: .releasing)
            return noErr
        }
    }

    /// Starts real hardware output; failures propagate without a silent fallback.
    public func start() throws {
        if isRunning { return }
        guard let session else { throw LivePlayerError.alreadyStopped }
        guard let format = AVAudioFormat(
            commonFormat: .pcmFormatFloat32, sampleRate: Double(StreamEngineCore.sampleRate),
            channels: 2, interleaved: false) else { throw LivePlayerError.invalidFormat }
        Self.activePlayer?.stop()
        Self.activePlayer = self
        let source = Self.makeSource(format: format, session: session)
        self.source = source
        audioEngine.attach(source)
        audioEngine.connect(source, to: audioEngine.mainMixerNode, format: format)
        startProducer(session)
        do {
            audioEngine.prepare()
            try audioEngine.start()
        } catch {
            Self.log.error("start failed file=\(self.path, privacy: .public) error=\(String(describing: error), privacy: .public)")
            stop()
            throw error
        }
        let device = audioEngine.outputNode.outputFormat(forBus: 0)
        Self.log.notice("started file=\(self.path, privacy: .public) source=44100Hz stereo float device=\(device.description, privacy: .public)")
        timer = Timer.scheduledTimer(withTimeInterval: 0.05, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.observePlayback() }
        }
    }

    /// Idempotent, including partially failed starts. Stop callbacks before silencing
    /// the engine, then cancel/join the producer before releasing mapped storage.
    public func stop() {
        if Self.activePlayer === self { Self.activePlayer = nil }
        guard let session else { return }
        timer?.invalidate()
        timer = nil
        audioEngine.stop()
        stoppedPosition = session.core.renderedFrames
        session.core.allSoundOff()
        session.cancelled.store(true, ordering: .releasing)
        session.producer.wait()
        if let source { audioEngine.detach(source) }
        source = nil
        Self.log.notice("stopped file=\(self.path, privacy: .public) renderedSamples=\(session.core.renderedFrames)")
        self.session = nil
    }

    /// Running seeks gate the existing callback to silence; the AVAudioEngine and
    /// source stay attached/running. Already stopped previews take a real cold start.
    public func seek(toSample sample: UInt64) throws {
        let began = ContinuousClock.now
        let keepEngineRunning = isRunning
        let current: LiveSession
        if let session, keepEngineRunning {
            current = session
            _ = current.renderGate.bitwiseOr(1, ordering: .acquiringAndReleasing)
            current.cancelled.store(true, ordering: .releasing)
            current.producer.wait()
            // Main-side bounded yields only. New callbacks cannot enter the core;
            // wait for the one that entered before muting to publish its exit.
            while current.renderGate.load(ordering: .acquiring) & 2 != 0 {
                Thread.sleep(forTimeInterval: 0.0001)
            }
        } else {
            let previous = session
            stop()
            if let previous {
                current = previous
            } else {
                current = LiveSession(core: try StreamEngineCore(path: path))
            }
            current.renderGate.store(1, ordering: .releasing)
            session = current
            loggedFirstBlock = false
            current.firstBlock.store(false, ordering: .releasing)
        }
        do {
            try current.core.seek(toSample: sample)
            try current.core.fillAhead()
            current.cancelled.store(false, ordering: .releasing)
            current.producerError.withLock { $0 = nil }
            finishTime = nil
            if keepEngineRunning {
                startProducer(current)
            } else {
                try start()
            }
            let position = current.core.renderedFrames
            current.renderGate.store(0, ordering: .releasing)
            let elapsed = (ContinuousClock.now - began).components
            let elapsedMs = Double(elapsed.seconds) * 1000 + Double(elapsed.attoseconds) / 1e15
            Self.log.notice("seek file=\(self.path, privacy: .public) targetSample=\(position) seconds=\(Double(position) / Double(StreamEngineCore.sampleRate)) elapsedMs=\(elapsedMs)")
        } catch {
            stop()
            throw error
        }
    }

    private func startProducer(_ session: LiveSession) {
        session.producer.enter()
        producerQueue.async {
            defer { session.producer.leave() }
            do {
                while !session.cancelled.load(ordering: .acquiring) && !session.core.isProducerFinished {
                    try session.core.fillAhead()
                    // Only the producer sleeps; the audio callback never waits.
                    Thread.sleep(forTimeInterval: 0.002)
                }
            } catch {
                session.producerError.withLock { $0 = error }
            }
        }
    }

    private func observePlayback() {
        guard let session else { return }
        if !loggedFirstBlock && session.firstBlock.load(ordering: .acquiring) {
            loggedFirstBlock = true
            Self.log.notice("first rendered block file=\(self.path, privacy: .public) renderedSamples=\(session.core.renderedFrames)")
        }
        if let error = session.producerError.withLock({ $0 }) {
            Self.log.error("producer failed error=\(String(describing: error), privacy: .public)")
            stop()
            return
        }
        if session.core.isFinished {
            if let finishTime {
                if ContinuousClock.now - finishTime >= .milliseconds(500) { stop() }
            } else {
                finishTime = .now
                Self.log.notice("finished file=\(self.path, privacy: .public) renderedSamples=\(session.core.renderedFrames)")
            }
        }
    }
}

private enum LivePlayerError: Error {
    case invalidFormat
    case alreadyStopped
}
