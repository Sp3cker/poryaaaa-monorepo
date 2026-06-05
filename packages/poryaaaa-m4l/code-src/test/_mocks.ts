// Minimal stubs for the v8 service tests. The services are dependency-injected
// via ServiceDeps, so each harness is a fake outlet recorder, a no-op post
// sink, plus an in-memory bus + snapshot store so we can simulate inter-device
// IPC without a Max process.

import type { Bus, BusTransport, Json } from "../bus";
import { createBus } from "../bus";
import type { ServiceDeps as VoicesDeps } from "../ccomidi_voices";

export interface OutletCall {
    outlet?: number;
    args: unknown[];
}

export interface MockBus {
    bus: Bus;
    sent: { channel: string; payload: Json }[];
    snapshots: Map<string, Json>;
    deliver: (channel: string, payload: Json) => void;
    seedSnapshot: (name: string, value: Json) => void;
}

// Symmetric in-process bus + snapshot. send() captures emit calls; deliver()
// fires inbound messages to whatever subscribed via on(). Snapshot store is a
// plain Map.
export function makeMockBus(): MockBus {
    const sent: { channel: string; payload: Json }[] = [];
    const snapshots = new Map<string, Json>();
    const subscribers = new Map<string, (encoded: string) => void>();

    const transport: BusTransport = {
        send: (channel, encoded) => {
            sent.push({
                channel,
                payload: JSON.parse(decodeURIComponent(encoded)),
            });
        },
        subscribe: (channel, onEncoded) => {
            subscribers.set(channel, onEncoded);
        },
        readSnapshot: (name) => snapshots.get(name) ?? null,
        writeSnapshot: (name, value) => {
            snapshots.set(name, value);
        },
    };
    const bus = createBus(transport);
    return {
        bus,
        sent,
        snapshots,
        deliver: (channel, payload) => {
            const sub = subscribers.get(channel);
            if (!sub) return;
            sub(encodeURIComponent(JSON.stringify(payload)));
        },
        seedSnapshot: (name, value) => {
            snapshots.set(name, value);
        },
    };
}

export interface MockVoicesDeps extends VoicesDeps {
    outletCalls: OutletCall[];
    postCalls: string[];
    busMock: MockBus;
    reset: () => void;
}

export function makeMockVoicesDeps(): MockVoicesDeps {
    const outletCalls: OutletCall[] = [];
    const postCalls: string[] = [];
    const busMock = makeMockBus();
    const harness = {
        outlet: (...args: unknown[]) => {
            outletCalls.push({ args });
        },
        post: (msg: string) => {
            postCalls.push(msg);
        },
        bus: busMock.bus,
        routeTrack: () => {},
        outletCalls,
        postCalls,
        busMock,
        reset: () => {
            outletCalls.length = 0;
            postCalls.length = 0;
            busMock.sent.length = 0;
        },
    };
    return harness;
}
