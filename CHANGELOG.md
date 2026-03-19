# 变更日志

本文件仅记录当前主线的重要变更，不保留已废弃路线的细节。

## [当前主线]

### 协议对齐
- 主线扩展存在位与 upstream SPZ 对齐：`0x02`（`has extensions`）
- SPZ 版本策略：`version < 1` 为 error，`1..4` 正常，`>4` 为 warning 且继续校验
- Adobe Safe Orbit Camera 按弧度制校验：`minElevation/maxElevation ∈ [-pi/2, pi/2]`，`minRadius >= 0`

### 范围收敛
- `spz_gatekeeper` 定位为 **L2-only SPZ validator**
- CLI 仅保留 `check-spz` / `dump-trailer` / `guide` / `--self-test`
- 主线不再承担 `check-glb` / `check-gltf` / GLB 容器校验

### 扩展路线
- TLV 结构固定为 `[u32 type][u32 length][payload...]`
- 未知 TLV 类型必须可跳过
- `spz-entropy` 作为 vendor extension 规划项推进，不修改 core header

### 文档与验证
- `README.md` 与 `README-zh.md` 对齐为 L2-only 口径
- 扩展文档统一到 `0x02 + TLV + vendor extension`
- WSL 构建与测试作为发布前验证基线
