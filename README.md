# SPZ Gatekeeper

> L2-only SPZ legality checker for validating headers, flags, and TLV trailer extensions without changing baseline SPZ decoding behavior.

`spz_gatekeeper` is a **pure C++17** validator for `.spz` files. It does **not** validate GLB containers, and it does **not** implement compression or rendering. Its job is to audit whether an SPZ file stays compatible with the upstream SPZ packed format while carrying optional vendor extensions.

## Author & Copyright
- Copyright owner: **Pu Junhan**
- Start year: **2026**

## Independent Work Statement
This repository is an independent, clean-room implementation by **Pu Junhan**. The SPZ file format specification (originally developed by Niantic, Inc. and published at https://github.com/nianticlabs/spz) is referenced solely for the purpose of achieving technical interoperability. All trademarks, trade names, and intellectual property rights related to "SPZ" and "Niantic" remain the exclusive property of their respective owners.

## Organizational & Political Separation

### No Affiliation
**This project is NOT affiliated with, endorsed by, or connected to Niantic, Inc. or any of its subsidiaries, parent companies, or related entities.**

The author explicitly and irrevocably disassociates from any political positions, statements, actions, or controversies involving Niantic, Inc. or its affiliates. This separation extends to all political, ideological, and organizational matters.

### Technical Independence
This gatekeeper project operates under independent governance and establishes its own validation criteria for SPZ file compatibility. The technical specifications implemented herein are derived solely from publicly available documentation and independent analysis.

## Governance & Derivative Compliance

### Authority
As the sole creator and maintainer, **Pu Junhan** asserts the following rights under this project's open-source license:
1. **Definition Authority**: The right to define what constitutes a compliant SPZ derivative or extension
2. **Endorsement Discretion**: The right to grant or withhold endorsement for any project claiming SPZ compatibility
3. **Standard Evolution**: The right to update validation criteria to maintain technical integrity and community standards

### Derivative Legitimacy
**The legitimacy of SPZ derivative projects shall be determined solely by compliance with this project's published specifications and protocols.** Neither Niantic, Inc. nor any other external entity possesses authority to validate or invalidate derivatives under this governance framework.

### Non-Endorsement
Inclusion in, or validation by, this gatekeeper project does not constitute endorsement of any underlying political, organizational, or commercial entity.

## Scope

### What this tool does
- Validate SPZ header fields: magic, version, point count, SH degree, flags, reserved byte
- Validate base payload sizing and truncation
- Validate TLV trailer layout after the base payload
- Validate known vendor extensions, currently Adobe Safe Orbit Camera (`0xADBE0002`)
- Warn on newer SPZ versions while continuing best-effort validation

### What this tool does not do
- Validate GLB/glTF structure
- Check binary-losslessness of SPZ-to-GLB conversion
- Implement compression, entropy coding, or rendering

## Mainline protocol decisions
- Official extension-presence flag: `0x02` (`has extensions`)
- Antialiasing flag remains `0x01`
- Unknown flag bits are treated as ignorable
- Extension records use TLV layout: `[u32 type][u32 length][payload...]`
- Unknown TLV types must be skippable via `length`
- Version policy:
  - `version < 1` => error
  - `version 1..4` => normal validation
  - `version > 4` => warning, continue validation

## Web UI

SPZ Validator provides a web interface that runs entirely in your browser:

🔗 **Live Demo**: https://spz-ecosystem.github.io/spz_gatekeeper/

Features:
- Pure browser-side execution via WebAssembly
- Drag-and-drop `.spz` file validation
- SPZ v4 format integrity checking
- No file upload to servers — zero privacy risk

### Build Web Version Locally

```bash
# In a WSL shell with Emscripten already available
emcmake cmake -S cpp -B build-pages -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
emmake cmake --build build-pages --parallel

# The build stays in build-pages/site and does not dirty web/
cd build-pages/site && python3 -m http.server 8080
# Open http://localhost:8080
```

## Quick Start

### Build in WSL/Linux/macOS
```bash
git clone https://github.com/spz-ecosystem/spz_gatekeeper.git
cd spz_gatekeeper
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/spz_gatekeeper check-spz model.spz
```

### Build from Windows via WSL
```bash
wsl bash -lc "
  cd /path/to/spz_gatekeeper && \
  cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release && \
  cmake --build build -j && \
  ctest --test-dir build --output-on-failure
"
```

### Dependency
```bash
# Ubuntu / Debian
sudo apt-get install -y zlib1g-dev
```

## CLI

### Validate SPZ
```bash
spz_gatekeeper check-spz <file.spz> [--strict|--no-strict] [--json]
```

### Dump trailer TLV records
```bash
spz_gatekeeper dump-trailer <file.spz> [--strict|--no-strict] [--json]
```

### Browse registered extensions
```bash
spz_gatekeeper registry [--json]
spz_gatekeeper registry show 0xADBE0002 [--json]
```

### Run compatibility summary for one asset
```bash
spz_gatekeeper compat-check <file.spz> [--json]
```

### Show compatibility maturity board
```bash
spz_gatekeeper compat-board [--json]
```

### Generate a minimal SPZ fixture
```bash
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture.spz
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode invalid-size --out fixture_bad.spz
```

### Show extension-development guide
```bash
spz_gatekeeper guide [--json]
```

### Run built-in self-test
```bash
spz_gatekeeper --self-test
```

### Command guide
- `check-spz`: main L2 legality check for CI, release gating, and manual audits.
- `dump-trailer`: inspect raw TLV records, offsets, and lengths after the base SPZ payload.
- `registry`: browse built-in extension specs; use `registry show <type>` when you need one contract in detail.
- `gen-fixture`: generate the smallest valid or invalid `.spz` sample for extension-validator development.
- `compat-check`: run the fast strict/non-strict compatibility summary for one asset.
- `compat-board`: inspect extension integration maturity; this is a governance board, not a performance ranking.
- `guide`: print the extension-author checklist and governance notes.
- `--self-test`: verify the binary's built-in assumptions without needing an external asset.

## Registry, self-test, and compatibility board
- `registry` is the machine-readable catalog of built-in extension contracts.
- `--self-test` verifies the gatekeeper's own TLV/header assumptions without requiring external assets.
- `compat-board` reports extension integration maturity, not an algorithm leaderboard.
- `docs/extension_registry.json` mirrors the current built-in registry and compatibility-board snapshot for docs/Web use.
- The Web/WASM entry points reuse the same registry/report vocabulary as the CLI, so the JSON fields stay aligned across terminal, browser, and docs.

## Extension author quick loop
```bash
spz_gatekeeper registry show 0xADBE0002 --json
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture_valid.spz
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode invalid-size --out fixture_invalid.spz
spz_gatekeeper check-spz fixture_valid.spz --json
spz_gatekeeper dump-trailer fixture_valid.spz --json
spz_gatekeeper compat-check fixture_valid.spz --json
spz_gatekeeper compat-board --json
```

Use this loop to confirm, in order, the registered contract, a minimal valid/invalid sample pair, the raw TLV layout, the structured compatibility summary, and the current maturity-board status before cutting a release.

## Validation details

### Header checks
- `magic == NGSP`
- version policy described above
- `reserved == 0`
- `sh_degree <= 4`

### Trailer checks
- Trailer may only appear after standard SPZ fields
- If `flags & 0x02` is set, trailer must exist and parse as TLV
- If trailer exists without `0x02`, validation emits warning `L2_UNDECLARED_TRAILER`
- In `--no-strict` mode, TLV parse failures downgrade to warnings

### Adobe extension
Built-in validator: `0xADBE0002` (`Adobe Safe Orbit Camera`)

Payload:
```text
float32 minElevation   // radians, [-pi/2, pi/2]
float32 maxElevation   // radians, [-pi/2, pi/2]
float32 minRadius      // >= 0
```

## Example output

### Text
```text
asset: extended.spz
ext type=2914910210 vendor="Adobe" name="Adobe Safe Orbit Camera" valid=true
```

### JSON
```json
{
  "asset_path": "extended.spz",
  "issues": [],
  "extension_reports": [
    {
      "type": 2914910210,
      "vendor_name": "Adobe",
      "extension_name": "Adobe Safe Orbit Camera",
      "known_extension": true,
      "has_validator": true,
      "status": "stable",
      "category": "camera",
      "spec_url": "docs/Implementing_Custom_Extension.md",
      "short_description": "Constrains orbit elevation and minimum radius for safer camera control.",
      "validation_result": true,
      "error_message": ""
    }
  ],
  "spz_l2": {
    "header_ok": true,
    "version": 4,
    "num_points": 1000000,
    "sh_degree": 0,
    "flags": 2,
    "reserved": 0,
    "decompressed_size": 28000024,
    "base_payload_size": 28000000,
    "trailer_size": 24,
    "tlv_records": [
      {"type": 2914910210, "length": 12, "offset": 28000000}
    ]
  }
}
```

## Project layout
```text
spz_gatekeeper/
├── cpp/
│   ├── include/spz_gatekeeper/
│   │   ├── extension_validator.h
│   │   ├── json_min.h
│   │   ├── report.h
│   │   ├── safe_orbit_camera_validator.h
│   │   ├── spz.h
│   │   ├── tlv.h
│   │   └── validator_registry.h
│   ├── extensions/
│   │   ├── adobe/
│   │   │   └── safe_orbit_camera_validator.h
│   │   └── registry/
│   │       └── validator_registry.h
│   ├── src/
│   │   ├── json_min.cc
│   │   ├── main.cc
│   │   ├── report.cc
│   │   ├── spz.cc
│   │   └── tlv.cc
│   ├── tests/
│   └── CMakeLists.txt
├── docs/
│   └── WIKI.md
├── README.md
├── README-zh.md
├── CHANGELOG.md
└── RELEASE_CHECKLIST.md
```

## Planned extensions
- `spz-entropy` is planned as a **vendor extension**, not a core SPZ header modification.
- Recommended vendor space: `0x4E41` (`Niantic`) with `extension_id = TBD` until finalized.

## Related projects
- [nianticlabs/spz](https://github.com/nianticlabs/spz) - upstream SPZ library
- [spz2glb](https://github.com/spz-ecosystem/spz2glb) - SPZ to GLB conversion toolchain
- [KHR_gaussian_splatting](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_gaussian_splatting) - Khronos extension spec

## License
MIT. See `LICENSE`.
