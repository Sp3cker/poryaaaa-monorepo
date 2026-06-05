// Pure path utilities used by the voicegroup service. No Max globals, so they
// can be unit-tested under tsx + node:test without bundling.

// "Macintosh HD:/Users/..." -> "/Users/..."; POSIX paths pass through. macOS
// opendialog emits HFS form, the wrapper external wants POSIX.
export function hfsToPosix(p: string): string {
    const i = p.indexOf(":/");
    return i > -1 ? p.slice(i + 1) : p;
}

export function trimTrailingSlash(s: string): string {
    while (s.length > 1 && s.endsWith("/")) {
        s = s.slice(0, -1);
    }
    return s;
}

export function normalizeRoot(raw: string): string {
    return trimTrailingSlash(hfsToPosix(String(raw || "")));
}

// Sorts and trims ".inc" suffixes from a list of filenames (already filtered).
export function sortBankNames(filenames: string[]): string[] {
    return filenames
        .filter((f) => f.endsWith(".inc"))
        .map((f) => f.slice(0, -".inc".length))
        .sort((a, b) => a.localeCompare(b));
}
