# SPZ Gatekeeper

> L2-only SPZ legality checker for validating headers, flags, and TLV trailer extensions without changing baseline SPZ decoding behavior.

`spz_gatekeeper` is a **pure C++17** validator for `.spz` files. It does **not** validate GLB containers, and it does **not** implement compression or rendering. Its job is to audit whether an SPZ file stays compatible with the upstream SPZ packed format while carrying optional vendor extensions.

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

### Show extension-development guide
```bash
spz_gatekeeper guide [--json]
```

### Run built-in self-test
```bash
spz_gatekeeper --self-test
```

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
ext type=2914908162 vendor="Adobe" name="Adobe Safe Orbit Camera" valid=true
```

### JSON
```json
{
  "asset_path": "extended.spz",
  "issues": [],
  "extension_reports": [
    {
      "type": 2914908162,
      "vendor_name": "Adobe",
      "extension_name": "Adobe Safe Orbit Camera",
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
      {"type": 2914908162, "length": 12, "offset": 28000000}
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
