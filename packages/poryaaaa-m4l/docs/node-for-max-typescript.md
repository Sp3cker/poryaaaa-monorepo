# Node For Max TypeScript

Node for Max voicegroup (and recorder) scripts are authored in TypeScript under `code-src/` and shipped as bundled JavaScript in `javascript/`.

The hand-maintained devices load the built `.js` files via `[v8]` and `[node.script]` entries so Max sees the same runtime shape after `npm run build`. Use native TypeScript loading only for local experiments.
