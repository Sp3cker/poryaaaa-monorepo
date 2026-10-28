import AppKit
import QuickLookPlayer

/// Drawing and interaction stay on the main actor, separate from audio rendering.
@MainActor
final class PianoRollView: NSView {
    var onDisappearance: (@MainActor () -> Void)?
    var onSeek: (@MainActor (UInt64) -> Void)?
    var position: (@MainActor () -> UInt64)?
    var noteIndex: MidiNoteIndex? {
        didSet { refreshDisplay() }
    }

    private static let trackColors: [NSColor] = [
        NSColor.systemBlue.withAlphaComponent(0.65),
        NSColor.systemOrange.withAlphaComponent(0.65),
        NSColor.systemGreen.withAlphaComponent(0.65),
        NSColor.systemPurple.withAlphaComponent(0.65),
        NSColor.systemTeal.withAlphaComponent(0.65),
        NSColor.systemPink.withAlphaComponent(0.65),
    ]
    private let timeLabel: NSTextField = {
        let label = NSTextField(labelWithString: "0:00 / 0:00")
        label.font = .monospacedDigitSystemFont(ofSize: 11, weight: .regular)
        label.textColor = .labelColor
        label.autoresizingMask = [.width]
        return label
    }()
    private weak var observedWindow: NSWindow?
    private var timer: Timer?
    private var scrubSample: UInt64?
    private var lastSeekSample: UInt64?
    private var lastSeekTime: ContinuousClock.Instant?
    private var displayedSeconds: UInt64 = .max
    private var displayedTotalSeconds: UInt64 = .max

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        addSubview(timeLabel)
        needsLayout = true
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        addSubview(timeLabel)
        needsLayout = true
    }

    isolated deinit {
        timer?.invalidate()
        NotificationCenter.default.removeObserver(self, name: NSWindow.didResignKeyNotification, object: nil)
    }

    override func layout() {
        super.layout()
        timeLabel.frame = NSRect(x: 8, y: 5, width: max(0, bounds.width - 16), height: 18)
    }

    private var rollBounds: NSRect {
        NSRect(x: 8, y: 28, width: max(0, bounds.width - 16), height: max(0, bounds.height - 36))
    }

    override func draw(_ dirtyRect: NSRect) {
        NSColor.windowBackgroundColor.setFill()
        dirtyRect.fill()
        let roll = rollBounds
        guard let noteIndex, noteIndex.totalDurationSamples > 0, roll.width > 0, roll.height > 0 else { return }
        NSGraphicsContext.saveGraphicsState()
        defer { NSGraphicsContext.restoreGraphicsState() }
        NSBezierPath(rect: roll).addClip()
        let scale = roll.width / CGFloat(noteIndex.totalDurationSamples)
        let pitchHeight = roll.height / 128
        for note in noteIndex.notes {
            let x = roll.minX + CGFloat(note.startSample) * scale
            let width = max(1, CGFloat(note.endSample - note.startSample) * scale)
            let y = roll.minY + CGFloat(note.key) * pitchHeight
            Self.trackColors[Int(note.engineTrack) % Self.trackColors.count].setFill()
            NSRect(x: x, y: y, width: width, height: max(1, pitchHeight)).fill()
        }
        let sample = min(scrubSample ?? position?() ?? 0, noteIndex.totalDurationSamples)
        let x = roll.minX + CGFloat(sample) * scale
        NSColor.labelColor.setFill()
        NSRect(x: min(x, roll.maxX - 1), y: roll.minY, width: 1, height: roll.height).fill()
    }

    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    override func mouseDown(with event: NSEvent) {
        guard rollBounds.contains(convert(event.locationInWindow, from: nil)),
            let sample = sample(at: event) else { return }
        scrubSample = sample
        refreshDisplay()
        commitSeek(sample, force: true)
        startPolling()
    }

    override func mouseDragged(with event: NSEvent) {
        guard scrubSample != nil else { return }
        scrubSample = sample(at: event)
        refreshDisplay()
        if let scrubSample { commitSeek(scrubSample, force: false) }
    }

    override func mouseUp(with event: NSEvent) {
        guard scrubSample != nil else { return }
        let target = sample(at: event) ?? scrubSample
        scrubSample = nil
        if let target { commitSeek(target, force: true) }
        startPolling()
        refreshDisplay()
    }

    private func sample(at event: NSEvent) -> UInt64? {
        guard let noteIndex, noteIndex.totalDurationSamples > 0, rollBounds.width > 0 else { return nil }
        let point = convert(event.locationInWindow, from: nil)
        let fraction = max(0, min(1, (point.x - rollBounds.minX) / rollBounds.width))
        return UInt64((Double(fraction) * Double(noteIndex.totalDurationSamples)).rounded())
    }

    /// The callback is synchronous/main-actor, so commits cannot overlap. The
    /// current scrub sample coalesces pending motion until a threshold is reached.
    private func commitSeek(_ sample: UInt64, force: Bool) {
        let now = ContinuousClock.now
        if !force, let previous = lastSeekSample, let committed = lastSeekTime {
            guard sample != previous else { return }
            let distance = sample >= previous ? sample - previous : previous - sample
            guard distance >= UInt64(StreamEngineCore.sampleRate) / 4
                || now - committed >= .milliseconds(100) else { return }
        }
        lastSeekSample = sample
        lastSeekTime = now
        onSeek?(sample)
    }

    private func pollPlayback() {
        if let scrubSample { commitSeek(scrubSample, force: false) }
        refreshDisplay()
    }

    private func refreshDisplay() {
        needsDisplay = true
        let sample = min(scrubSample ?? position?() ?? 0, noteIndex?.totalDurationSamples ?? .max)
        let elapsed = sample / UInt64(StreamEngineCore.sampleRate)
        let total = (noteIndex?.totalDurationSamples ?? 0) / UInt64(StreamEngineCore.sampleRate)
        guard elapsed != displayedSeconds || total != displayedTotalSeconds else { return }
        displayedSeconds = elapsed
        displayedTotalSeconds = total
        timeLabel.stringValue = String(format: "%llu:%02llu / %llu:%02llu", elapsed / 60, elapsed % 60, total / 60, total % 60)
    }

    private func startPolling() {
        guard timer == nil, window != nil, !isHiddenOrHasHiddenAncestor else { return }
        let timer = Timer(timeInterval: 0.05, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.pollPlayback() }
        }
        self.timer = timer
        RunLoop.main.add(timer, forMode: .common)
        refreshDisplay()
    }

    func stopPolling() {
        timer?.invalidate()
        timer = nil
        scrubSample = nil
        lastSeekSample = nil
        lastSeekTime = nil
    }

    private func disappear() {
        stopPolling()
        onDisappearance?()
    }

    override func viewWillMove(toWindow newWindow: NSWindow?) {
        if observedWindow !== newWindow {
            NotificationCenter.default.removeObserver(self, name: NSWindow.didResignKeyNotification, object: nil)
            observedWindow = nil
        }
        if window != nil && newWindow == nil { disappear() }
        super.viewWillMove(toWindow: newWindow)
    }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if let window, observedWindow !== window {
            observedWindow = window
            NotificationCenter.default.addObserver(
                self, selector: #selector(windowDidResignKey(_:)),
                name: NSWindow.didResignKeyNotification, object: window)
        }
        startPolling()
    }

    @objc private func windowDidResignKey(_ notification: Notification) {
        guard let observedWindow,
            let resigningWindow = notification.object as? NSWindow,
            resigningWindow === observedWindow, resigningWindow === window
        else { return }
        disappear()
    }

    override func viewWillMove(toSuperview newSuperview: NSView?) {
        if superview != nil && newSuperview == nil { disappear() }
        super.viewWillMove(toSuperview: newSuperview)
    }

    override func viewDidHide() {
        super.viewDidHide()
        disappear()
    }

    override func viewDidUnhide() {
        super.viewDidUnhide()
        startPolling()
    }
}
