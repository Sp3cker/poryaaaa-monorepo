// Cross-device IPC facade for v8 scripts. Each [v8] object is its own
// JS runtime, so an in-process EventEmitter cannot be shared across devices.
// This module exposes EventEmitter-shaped on()/emit() ergonomics on top of
// Max's `messnamed` (push) and `Dict` (snapshot) primitives — see
// docs/Max9-JS-API-en.md sections on messnamed (4962) and Dict (333).
//
// Transport contract:
//   send(channel, encoded)                — fan-out via messnamed("---channel", encoded)
//   subscribe(channel, onEncoded)         — installs the inbound dispatcher;
//                                          called once per channel. Wiring on the
//                                          patcher side is a [r ---channel] cord
//                                          into the v8's first inlet which dispatches
//                                          messages by leading symbol.
//   readSnapshot(name)                    — read a value previously stored via writeSnapshot
//   writeSnapshot(name, value)            — store a replay snapshot for late joiners

// Runtime contract: payloads must be JSON-serializable. Types are advisory —
// we don't force callers to declare index signatures.
export type Json = unknown;

export interface BusTransport {
    send: (channel: string, encoded: string) => void;
    subscribe: (channel: string, onEncoded: (encoded: string) => void) => void;
    readSnapshot: (name: string) => unknown;
    writeSnapshot: (name: string, value: unknown) => void;
}

export type Handler<T = unknown> = (payload: T) => void;

export interface Bus {
    on: <T = unknown>(channel: string, fn: Handler<T>) => () => void;
    emit: (channel: string, payload: unknown) => void;
    readSnapshot: <T = unknown>(name: string) => T | null;
    writeSnapshot: (name: string, value: unknown) => void;
}

export function createBus(transport: BusTransport, onError?: (err: string) => void): Bus {
    const handlers = new Map<string, Set<Handler>>();

    function dispatch(channel: string, encoded: string): void {
        const set = handlers.get(channel);
        if (!set || set.size === 0) return;
        let payload: unknown;
        try {
            payload = JSON.parse(decodeURIComponent(encoded));
        } catch (_) {
            onError?.(`bus: failed to decode payload on ${channel}`);
            return;
        }
        for (const h of set) {
            try {
                h(payload);
            } catch (_) {
                onError?.(`bus: handler for ${channel} threw`);
            }
        }
    }

    return {
        on(channel, fn) {
            let set = handlers.get(channel);
            if (!set) {
                set = new Set();
                handlers.set(channel, set);
                transport.subscribe(channel, (encoded) => dispatch(channel, encoded));
            }
            set.add(fn as Handler);
            return () => {
                set!.delete(fn as Handler);
            };
        },
        emit(channel, payload) {
            const encoded = encodeURIComponent(JSON.stringify(payload));
            transport.send(channel, encoded);
        },
        readSnapshot(name) {
            return (transport.readSnapshot(name) ?? null) as never;
        },
        writeSnapshot(name, value) {
            transport.writeSnapshot(name, value);
        },
    };
}
