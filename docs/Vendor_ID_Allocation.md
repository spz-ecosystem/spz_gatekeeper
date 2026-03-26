# Vendor ID Allocation / Vendor ID 分配

## Purpose / 目的
Define a stable `u16 vendor_id` strategy for TLV extension types.
定义 TLV 扩展类型中的 `u16 vendor_id` 分配策略，避免冲突。

## Type Layout / 类型布局
`type = (vendor_id << 16) | extension_id`

- High 16 bits: vendor ID / 高 16 位：厂商 ID
- Low 16 bits: extension ID / 低 16 位：扩展 ID

## Known Vendor IDs / 已知厂商 ID
- `0xADBE`: Adobe
- `0x4E41`: Niantic

## Current / Planned Extensions / 当前与规划中的扩展
| Vendor | Vendor ID | Extension | Extension ID | Status | Validator | Compatibility Board |
|---|---:|---|---:|---|---|---|
| Adobe | `0xADBE` | Safe Orbit Camera | `0x0002` | stable | yes | included |
| Niantic | `0x4E41` | `spz-entropy` (planned) | `TBD` | planned | no | not yet |

## Registry / Self-Test / Compatibility Board Roles / 分工说明
- `registry` is the canonical built-in catalog for shipped extension specs.
- `--self-test` only checks the gatekeeper's own invariants; it is not an extension leaderboard.
- `compat-board` reports extension integration maturity (`fixture_valid_pass`, `fixture_invalid_pass`, `strict_check_pass`, `non_strict_check_pass`) instead of algorithm ranking.
- `docs/extension_registry.json` is the machine-readable mirror for docs and Web display, while runtime registration remains in C++.

- `registry` 是已发布扩展规范的内置目录。
- `--self-test` 只验证门卫自身的不变量，不承担扩展排行职责。
- `compat-board` 只报告扩展接入成熟度（`fixture_valid_pass`、`fixture_invalid_pass`、`strict_check_pass`、`non_strict_check_pass`），不做算法排名。
- `docs/extension_registry.json` 是文档与 Web 展示用的机器可读镜像，运行时注册仍以 C++ 内置逻辑为准。

## Best Practices / 最佳实践

1. Use one fixed vendor ID per organization.
2. Keep public, stable extensions in a small documented range.
3. Maintain an internal ownership registry for `extension_id`.
4. Unknown types must be skippable by `length`.
5. Planned extensions should be documented before payload design diverges.

1. 每个组织固定使用一个 vendor ID。
2. 对外稳定扩展应保持在可文档化的小范围内。
3. 维护内部 `extension_id` 归属表。
4. 未知类型必须可按 `length` 跳过。
5. 在 payload 设计发散前，先把规划中的扩展写入文档。
