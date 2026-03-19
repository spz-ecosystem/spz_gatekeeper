# SPZ Vendor Extension Guide

> Use `spz_gatekeeper` to validate L2 SPZ legality for optional vendor extensions.

## Quick start
1. Set the official extension flag: `flags |= 0x02`
2. Append TLV records after the base payload
3. Run `spz_gatekeeper check-spz your.spz`
4. Verify upstream compatibility with `spz_info your.spz && spz_to_ply your.spz`

## Compatibility rules
- Never modify standard SPZ field layout
- Only append extension data after the base payload
- Unknown TLV types must remain skippable by `length`
- Standard-field decode results must remain unchanged even if trailer parsing fails

## TLV layout
```text
[u32 type][u32 length][payload...]
```

- High 16 bits of `type`: `vendor_id`
- Low 16 bits of `type`: `extension_id`

## Known vendor IDs
- `0xADBE`: Adobe
- `0x4E41`: Niantic

## Built-in sample extension
Adobe Safe Orbit Camera (`0xADBE0002`)

Payload:
```text
float32 minElevation   // radians, [-pi/2, pi/2]
float32 maxElevation   // radians, [-pi/2, pi/2]
float32 minRadius      // >= 0
```

## Planned extension route
`spz-entropy` should be implemented as a vendor extension, not as a core-header modification.

Recommended placeholder:
- vendor: `0x4E41` (`Niantic`)
- extension: `TBD`

## Registration flow
1. Define `type = (vendor_id << 16) | extension_id`
2. Implement a validator derived from `SpzExtensionValidator`
3. Register it in `ExtensionValidatorRegistry`
4. Add TLV integration tests and compatibility checks

## References
- Upstream SPZ: https://github.com/nianticlabs/spz
- Repository: https://github.com/spz-ecosystem/spz_gatekeeper
- Public overview: `README.md`
