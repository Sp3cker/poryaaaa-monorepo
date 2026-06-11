// Minimal stubs for the v8 service tests. The services are dependency-injected
// via ServiceDeps, so each harness is a fake outlet recorder and a no-op post
// sink.

import type { ServiceDeps as VoicesDeps } from "../ccomidi_voices";

export interface OutletCall {
    outlet?: number;
    args: unknown[];
}

export interface MockVoicesDeps extends VoicesDeps {
    outletCalls: OutletCall[];
    postCalls: string[];
    reset: () => void;
}

export function makeMockVoicesDeps(): MockVoicesDeps {
    const outletCalls: OutletCall[] = [];
    const postCalls: string[] = [];
    const harness = {
        outlet: (...args: unknown[]) => {
            outletCalls.push({ args });
        },
        post: (msg: string) => {
            postCalls.push(msg);
        },
        routeTrack: () => {},
        getInstanceId: () => "mock-device",
        outletCalls,
        postCalls,
        reset: () => {
            outletCalls.length = 0;
            postCalls.length = 0;
        },
    };
    return harness;
}

export function routeWithBroadcast(svc: {
    route: () => void;
}): string {
    svc.route();
    return "reroute-signal";
}