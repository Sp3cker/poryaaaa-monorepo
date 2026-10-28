import AppKit
import os
import QuickLookPlayer
import QuickLookUI

/// Finder supplies navigation; this controller owns playback and the asynchronous roll index.
@MainActor
@objc(PoryaaaaQuickLookController)
final class QuickLookController: NSViewController, QLPreviewingController {
    private static let log = Logger(subsystem: "com.poryaaaa.quicklook", category: "preview")
    private var player: LivePlayer?
    private var preparation: UInt64 = 0
    private var indexTask: Task<Void, Never>?

    deinit { indexTask?.cancel() }

    override func loadView() {
        let roll = PianoRollView(frame: NSRect(x: 0, y: 0, width: 480, height: 160))
        roll.onDisappearance = { [weak self] in self?.stopPlayback() }
        roll.position = { [weak self] in self?.player?.currentPositionSamples ?? 0 }
        roll.onSeek = { [weak self] sample in
            guard let self, let player = self.player else { return }
            do {
                try player.seek(toSample: sample)
            } catch {
                Self.log.error("seek failed error=\(String(describing: error), privacy: .public)")
                player.stop()
            }
        }
        view = roll
        preferredContentSize = NSSize(width: 480, height: 160)
    }

    func preparePreviewOfFile(at url: URL) async throws {
        preparation &+= 1
        let generation = preparation
        indexTask?.cancel()
        indexTask = nil
        player?.stop()
        player = nil
        (view as? PianoRollView)?.noteIndex = nil
        let path = url.path
        let prepared = try await LivePlayer.prepare(path: path)
        guard generation == preparation else { throw CancellationError() }
        try Task.checkCancellation()
        try prepared.start()
        player = prepared
        // Starting this independent scan never delays audio or the preview completion.
        indexTask = Task { [weak self] in
            do {
                let index = try await Self.buildIndex(path: path)
                try Task.checkCancellation()
                guard let self, self.preparation == generation else { return }
                (self.view as? PianoRollView)?.noteIndex = index
                Self.log.notice("note index ready notes=\(index.notes.count) durationSamples=\(index.totalDurationSamples) build=\(String(describing: index.buildDuration), privacy: .public)")
            } catch is CancellationError {
                // Disappearing previews discard their independent indexing work.
            } catch {
                Self.log.error("note index failed error=\(String(describing: error), privacy: .public)")
            }
        }
    }

    @concurrent
    private static func buildIndex(path: String) async throws -> MidiNoteIndex {
        try MidiNoteIndex(path: path)
    }

    override func viewWillDisappear() {
        super.viewWillDisappear()
        stopPlayback()
    }

    private func stopPlayback() {
        // Deliberately synchronous: engine stop/all-sound-off finish before the hook returns.
        preparation &+= 1
        indexTask?.cancel()
        indexTask = nil
        (view as? PianoRollView)?.stopPolling()
        player?.stop()
        // Retain only the stopped host/position so an explicit scrub can restart it.
        // LivePlayer.stop has already released the core, bank, mapping, and producer.
    }
}
