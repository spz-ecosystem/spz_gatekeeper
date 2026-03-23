# SPZ Gatekeeper Wiki

> `spz_gatekeeper` is a local-only SPZ audit tool. It validates `SPZ` legality and optional trailer extensions without expanding into `GLB` or `spz2glb` auditing.

## Fixed boundaries
- Audit target: `SPZ` only
- Not audited: `GLB`
- Not audited: `spz2glb`
- Browser stage: standard zip audit bundle only
- CLI stage: real `.spz` artifact only
- Final decision: always from `local_cli_spz_artifact_audit`

## Dual audit modes
- `browser_lightweight_wasm_audit`
  - Input: one standard `.zip` audit bundle
  - Goal: lightweight browser gate for manifest, loader, exports, tiny fixtures, and basic budgets
- `local_cli_spz_artifact_audit`
  - Input: one `.spz`, one directory, or one manifest of `.spz` files
  - Goal: real artifact audit with strict/non-strict compatibility evidence
- `browser_to_cli_handoff`
  - Optional JSON exported by the browser
  - Can be merged into CLI JSON output
  - Never replaces the real CLI audit

## Recommended local workflow
1. Upload one standard `.zip` audit bundle in the browser.
2. Review the browser verdict: `pass`, `review_required`, or `block`.
3. If needed, export the optional `browser_to_cli_handoff` JSON.
4. Run `compat-check` locally against the real `.spz` artifact.
5. If you exported handoff, pass it to CLI with `--handoff`.
6. Treat the CLI result as the final release decision.

## Standard browser bundle
```text
<algo-name>-wasm-audit.zip
├─ manifest.json
├─ module.wasm
├─ loader.mjs
└─ tiny_fixtures/
```

## Common CLI commands
```bash
spz_gatekeeper compat-check model.spz --json
spz_gatekeeper compat-check model.spz --handoff browser_audit.json --json
spz_gatekeeper compat-check --dir ./fixtures --json
spz_gatekeeper compat-check --manifest ./fixtures/manifest.json --json
spz_gatekeeper compat-board --json
```

## Extension-author quick loop
```bash
spz_gatekeeper registry show 0xADBE0002 --json
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture_valid.spz
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode invalid-size --out fixture_invalid.spz
spz_gatekeeper check-spz fixture_valid.spz --json
spz_gatekeeper dump-trailer fixture_valid.spz --json
spz_gatekeeper compat-check fixture_valid.spz --json
```

## References
- `README.md`
- `README-zh.md`
- `docs/Implementing_Custom_Extension.md`
- `docs/extension_registry.json`
- `docs/plans/2026-03-22-spz-gatekeeper-wasm-audit-implementation-plan.md`
