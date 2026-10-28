import AppKit

@main
@MainActor
private enum HostApp {
    static func main() {
        let application = NSApplication.shared
        application.setActivationPolicy(.accessory)
        application.run()
    }
}
