# Node For Max TypeScript

Node for Max voicegroup scripts are authored in TypeScript under `code-src/` and shipped as bundled JavaScript in `javascript/`.

Use native TypeScript loading only for local experiments. Generated devices should load the built `.js` files so Max sees the same runtime shape after `npm run build`, independent of local Node loader flags.
