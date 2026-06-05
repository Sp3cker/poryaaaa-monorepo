// Probe recorder-output event density and pitch-bend thinning candidates.
//
// Usage:
//   npm run probe:recorder-size
//   npm run probe:recorder-size -- test_midi/other.mid

import { readFileSync } from "node:fs";
import path from "node:path";

import { parseSmf, voiceEvents, type VoiceEvent } from "../test/_smf_parser";

interface BendPoint {
    tick: number;
    value: number;
}

function eventKind(e: VoiceEvent): string {
    switch (e.status & 0xF0) {
        case 0x80: return "noteOff";
        case 0x90: return "noteOn";
        case 0xB0: return "cc";
        case 0xC0: return "pc";
        case 0xE0: return "bend";
        default:   return "other";
    }
}

function inc(map: Map<string, number>, key: string): void {
    map.set(key, (map.get(key) ?? 0) + 1);
}

function pitchBendValue(e: VoiceEvent): number {
    return ((e.d2 & 0x7F) << 7) | (e.d1 & 0x7F);
}

function sameTickLastWins(points: BendPoint[]): BendPoint[] {
    const out: BendPoint[] = [];
    for (const point of points) {
        const prev = out[out.length - 1];
        if (prev && prev.tick === point.tick) {
            out[out.length - 1] = point;
        } else {
            out.push(point);
        }
    }
    return out;
}

function deadband(points: BendPoint[], minDelta: number): BendPoint[] {
    const out: BendPoint[] = [];
    for (let i = 0; i < points.length; i++) {
        const point = points[i];
        const isEdge = i === 0 || i === points.length - 1;
        const isCenterReset = point.value === 8192;
        const last = out[out.length - 1];
        if (isEdge || isCenterReset || !last || Math.abs(point.value - last.value) >= minDelta) {
            out.push(point);
        }
    }
    return out;
}

function perpendicularError(point: BendPoint, start: BendPoint, end: BendPoint): number {
    if (start.tick === end.tick) return Math.abs(point.value - start.value);
    const frac = (point.tick - start.tick) / (end.tick - start.tick);
    const interpolated = start.value + frac * (end.value - start.value);
    return Math.abs(point.value - interpolated);
}

function rdp(points: BendPoint[], epsilon: number): BendPoint[] {
    if (points.length <= 2) return points.slice();

    const start = points[0];
    const end = points[points.length - 1];
    let maxError = -1;
    let splitIndex = -1;

    for (let i = 1; i < points.length - 1; i++) {
        const error = perpendicularError(points[i], start, end);
        if (error > maxError) {
            maxError = error;
            splitIndex = i;
        }
    }

    if (maxError <= epsilon) return [start, end];

    const left = rdp(points.slice(0, splitIndex + 1), epsilon);
    const right = rdp(points.slice(splitIndex), epsilon);
    return left.slice(0, -1).concat(right);
}

function printProjection(label: string, originalCount: number, projectedCount: number): void {
    const saved = originalCount - projectedCount;
    const pct = originalCount === 0 ? 0 : (saved / originalCount) * 100;
    console.log(`${label.padEnd(24)} ${String(projectedCount).padStart(5)} events  saved ${String(saved).padStart(5)} (${pct.toFixed(1)}%)`);
}

function main(): void {
    const inputPath = process.argv[2] ?? "test_midi/mus_sadpop_many_cc.mid";
    const absolutePath = path.resolve(process.cwd(), inputPath);
    const bytes = readFileSync(absolutePath);
    const parsed = parseSmf(bytes);
    const voices = voiceEvents(parsed);

    const counts = new Map<string, number>();
    const ccCounts = new Map<string, number>();
    const bendByChannel = new Map<number, BendPoint[]>();

    for (const event of voices) {
        const kind = eventKind(event);
        const channel = event.status & 0x0F;
        inc(counts, kind);

        if (kind === "cc") {
            inc(ccCounts, `ch${channel}:cc${event.d1}`);
        } else if (kind === "bend") {
            const points = bendByChannel.get(channel) ?? [];
            points.push({ tick: event.tick, value: pitchBendValue(event) });
            bendByChannel.set(channel, points);
        }
    }

    console.log(`file: ${inputPath}`);
    console.log(`bytes: ${bytes.length}`);
    console.log(`format: ${parsed.header.format}  tracks: ${parsed.header.tracks}  division: ${parsed.header.division}`);
    console.log(`voice events: ${voices.length}`);
    console.log("event counts:");
    for (const [kind, count] of [...counts].sort()) {
        console.log(`  ${kind.padEnd(8)} ${count}`);
    }

    const denseCcs = [...ccCounts]
        .sort((a, b) => b[1] - a[1])
        .slice(0, 12);
    if (denseCcs.length > 0) {
        console.log("densest CC streams:");
        for (const [key, count] of denseCcs) {
            console.log(`  ${key.padEnd(10)} ${count}`);
        }
    }

    for (const [channel, bends] of [...bendByChannel].sort((a, b) => a[0] - b[0])) {
        const sameTickCollapsed = sameTickLastWins(bends);
        const tickCounts = new Map<number, number>();
        for (const bend of bends) {
            tickCounts.set(bend.tick, (tickCounts.get(bend.tick) ?? 0) + 1);
        }
        const duplicateTickGroups = [...tickCounts.values()].filter((count) => count > 1);

        console.log(`pitch bend ch${channel}:`);
        console.log(`  original events: ${bends.length}`);
        console.log(`  unique ticks:     ${sameTickCollapsed.length}`);
        console.log(`  duplicate ticks:  ${duplicateTickGroups.length}`);
        console.log(`  max same-tick:    ${Math.max(0, ...duplicateTickGroups)}`);
        printProjection("  same-tick last wins", bends.length, sameTickCollapsed.length);

        for (const minDelta of [16, 32, 64, 128]) {
            printProjection(`  deadband ${minDelta}`, bends.length, deadband(sameTickCollapsed, minDelta).length);
        }

        for (const epsilon of [8, 16, 32]) {
            printProjection(`  rdp epsilon ${epsilon}`, bends.length, rdp(sameTickCollapsed, epsilon).length);
        }
    }
}

main();
