import test from "node:test";
import assert from "node:assert/strict";

import {
    hfsToPosix,
    normalizeRoot,
    sortBankNames,
    trimTrailingSlash,
} from "../scanner";

test("hfsToPosix strips macOS volume prefix; passes POSIX through", () => {
    assert.equal(hfsToPosix("Macintosh HD:/Users/me"), "/Users/me");
    assert.equal(hfsToPosix("Some Volume:/foo"), "/foo");
    assert.equal(hfsToPosix("/Users/me"), "/Users/me");
    assert.equal(hfsToPosix(""), "");
});

test("trimTrailingSlash strips one or many trailing slashes; preserves '/'", () => {
    assert.equal(trimTrailingSlash("/path/"), "/path");
    assert.equal(trimTrailingSlash("/path///"), "/path");
    assert.equal(trimTrailingSlash("/path"), "/path");
    assert.equal(trimTrailingSlash("/"), "/");
});

test("normalizeRoot composes hfsToPosix + trimTrailingSlash and tolerates falsy input", () => {
    assert.equal(
        normalizeRoot("Macintosh HD:/Users/me/proj/"),
        "/Users/me/proj",
    );
    assert.equal(normalizeRoot("/p/"), "/p");
    assert.equal(normalizeRoot(""), "");
    assert.equal(normalizeRoot(null as unknown as string), "");
    assert.equal(normalizeRoot(undefined as unknown as string), "");
});

test("sortBankNames keeps only *.inc, strips suffix, sorts alphabetically", () => {
    const out = sortBankNames([
        "zzz.inc",
        "aaa.inc",
        "bbb.txt",
        "ccc.inc",
        ".DS_Store",
        "README",
    ]);
    assert.deepEqual(out, ["aaa", "ccc", "zzz"]);
});

test("sortBankNames returns empty array when nothing matches", () => {
    assert.deepEqual(sortBankNames(["README", "junk"]), []);
    assert.deepEqual(sortBankNames([]), []);
});
