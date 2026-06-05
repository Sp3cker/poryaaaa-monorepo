export interface RoutingChoice {
    display_name: string;
    identifier: string | number;
}

type JsonPrimitive = string | number | boolean | null;
export type JsonValue = JsonPrimitive | JsonObject | JsonValue[];
export type JsonObject = { [key: string]: JsonValue };
export type RoutingDictStringAtom = readonly [string];
export type DictLikeInput = string | RoutingDictStringAtom | JsonValue;

interface RoutingDict {
    [key: string]: JsonValue;
}

function parseJson(text: string): JsonValue {
    return JSON.parse(text) as JsonValue;
}

export function parseDictLike(value: DictLikeInput): JsonValue {
    if (Array.isArray(value)) {
        const first = value[0];
        if (value.length === 1 && typeof first === "string" && first.trim().startsWith("{")) {
            return parseJson(first);
        }
    }
    if (typeof value === "string") {
        const trimmed = value.trim();
        if (trimmed.startsWith("{")) return parseJson(trimmed);
    }
    return value as JsonValue;
}

export function routingChoice(value: DictLikeInput): RoutingChoice {
    const parsed = parseDictLike(value);
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
        throw new Error("routing value is not a dictionary");
    }
    const display = parsed.display_name;
    const identifier = parsed.identifier;
    if (typeof display !== "string"
        || (typeof identifier !== "string" && typeof identifier !== "number")) {
        throw new Error("routing dictionary missing display_name/identifier");
    }
    return { display_name: display, identifier };
}

export function routingChoices(value: DictLikeInput, key: string): RoutingChoice[] {
    const parsed = parseDictLike(value) as RoutingDict;
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
        throw new Error(`${key} must be the dictionary returned by track.get("${key}")`);
    }
    const list = parsed[key];
    if (!Array.isArray(list)) throw new Error(`${key} dictionary missing ${key} list`);
    return list.map(routingChoice);
}
