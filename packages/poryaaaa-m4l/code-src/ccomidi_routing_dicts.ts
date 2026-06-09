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


export function parseDictLike(value: unknown) {
    if (typeof Dict !== "undefined" && value instanceof Dict) {
        try {
            return JSON.parse(value.stringify());
        } finally {
            value.freepeer();
        }
    }
    if (Array.isArray(value)) {
        if (value.length === 1 && typeof value[0] === "string") {
            const trimmed = value[0].trim();
            if (trimmed.startsWith("{")) return JSON.parse(trimmed);
        }
        for (let i = 0; i < value.length - 1; i += 1) {
            if ((value[i] === "dictionary" || value[i] === "dict")
                && typeof value[i + 1] === "string") {
                const d = new Dict(value[i + 1]);
                try {
                    return JSON.parse(d.stringify());
                } finally {
                    d.freepeer();
                }
            }
        }
    }
    if (typeof value === "string") {
        const trimmed = value.trim();
        if (trimmed.startsWith("{")) return JSON.parse(trimmed);
        const d = new Dict(trimmed);
        try {
            return JSON.parse(d.stringify());
        } finally {
            d.freepeer();
        }
    }
    return value;
}


export function routingChoice(value: DictLikeInput){
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

export function routingChoices(value: DictLikeInput, key: string) {
    const parsed = parseDictLike(value) as RoutingDict;
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
        throw new Error(`${key} must be the dictionary returned by track.get("${key}")`);
    }
    const list = parsed[key];
    if (!Array.isArray(list)) throw new Error(`${key} dictionary missing ${key} list`);
    return list.map(routingChoice);
}
