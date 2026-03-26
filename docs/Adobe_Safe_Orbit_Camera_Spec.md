# Adobe Safe Orbit Camera Spec / 规范

## Extension Type / 扩展类型
- `type = 0xADBE0002`
- Vendor: Adobe (`0xADBE`)
- Extension ID: `0x0002`

## Payload Format / 负载格式
Length must be `12` bytes.
长度必须为 `12` 字节。

```text
float32 minElevation   // radians
float32 maxElevation   // radians
float32 minRadius
```

Little-endian / 小端序。

## Validation Rules / 校验规则
1. `size == 12`
2. `minElevation ∈ [-pi/2, pi/2]` radians
3. `maxElevation ∈ [-pi/2, pi/2]` radians
4. `minElevation <= maxElevation`
5. `minRadius >= 0`

## Error Behavior / 错误行为
- Validation failure => `L2_EXT_VALIDATION` error.
- Unknown extension type => warning only (`L2_EXT_UNKNOWN`).

- 校验失败 => `L2_EXT_VALIDATION` 错误。
- 未知扩展类型 => 仅 warning（`L2_EXT_UNKNOWN`）。
