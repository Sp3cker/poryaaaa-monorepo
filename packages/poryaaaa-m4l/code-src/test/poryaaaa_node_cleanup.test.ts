import assert from "node:assert/strict";
import test from "node:test";

import { unlinkTempFile } from "../cleanup";

test("unlinkTempFile removes the requested temp file", () => {
  const unlinked: string[] = [];
  const posts: string[] = [];

  const ok = unlinkTempFile("/tmp/poryaaaa-probe.bin", {
    unlinkSync: (path) => {
      unlinked.push(path);
    },
    post: (msg) => {
      posts.push(msg);
    },
  });

  assert.equal(ok, true);
  assert.deepEqual(unlinked, ["/tmp/poryaaaa-probe.bin"]);
  assert.deepEqual(posts, []);
});
