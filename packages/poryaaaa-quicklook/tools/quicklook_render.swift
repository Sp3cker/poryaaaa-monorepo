import Darwin
import QuickLookPlayer

@main
private enum QuickLookRender {
    static func main() {
        guard CommandLine.arguments.count == 3 else {
            print("usage: quicklook_render <in.mid> <out.wav>")
            exit(2)
        }
        do {
            let player = try StreamPlayer(path: CommandLine.arguments[1])
            let result = try player.render(to: CommandLine.arguments[2])
            print(
                "frames=\(result.frames) sampleRate=\(StreamPlayer.sampleRate) duration=\(result.duration)s peak=\(result.peak) droppedTracks=\(result.droppedTracks)"
            )
        } catch {
            print("quicklook_render: \(error)")
            exit(1)
        }
    }
}
