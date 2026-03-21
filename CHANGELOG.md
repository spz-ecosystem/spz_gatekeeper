# 变更日志

本文件仅记录当前主线的重要变更，不保留已废弃路线的细节。

## [v1.1.0] - 2026-03-21

### 新增能力
- 新增 `registry` / `registry show <type>`，可枚举并查询内置扩展契约。
- 新增 `compat-check`，可输出单个 `.spz` 资产的 strict / non-strict 兼容性摘要。
- 新增 `compat-board`，用于展示扩展接入成熟度，而不是算法性能排行榜。
- 新增 `gen-fixture`，可生成最小合法 / 非法 TLV 扩展样例，缩短扩展验证闭环。
- 新增 `docs/extension_registry.json`，作为内置 registry 与 compatibility board 的文档镜像。

### 协议与报告对齐
- `check-spz --json` 的 `extension_reports` 已对齐 registry 元数据，可返回登记状态、类别、规范链接与说明摘要。
- 主线扩展存在位与 upstream SPZ 对齐：`0x02`（`has extensions`）。
- SPZ 版本策略保持：`version < 1` 为 error，`1..4` 正常，`>4` 为 warning 且继续校验。
- Adobe Safe Orbit Camera 继续按弧度制校验：`minElevation/maxElevation ∈ [-pi/2, pi/2]`，`minRadius >= 0`。

### 范围约束
- `spz_gatekeeper` 继续定位为 **L2-only SPZ validator**。
- 主线 CLI 口径为 `check-spz` / `dump-trailer` / `registry` / `compat-check` / `compat-board` / `gen-fixture` / `guide` / `--self-test`。
- 主线仍不承担 `check-glb` / `check-gltf` / GLB 容器校验。
- TLV 结构固定为 `[u32 type][u32 length][payload...]`，未知 TLV 类型必须可跳过。
- `spz-entropy` 仍作为 vendor extension 规划项推进，不修改 core header。

### 文档与发布
- `README.md` 与 `README-zh.md` 已同步当前 CLI 命令说明、Registry/WASM 口径和扩展作者快速自测闭环。
- 发布前检查清单已同步到当前命令面，并要求核对 `compat-board` 与 Pages 构建状态。
- WSL 构建与测试继续作为发布前验证基线。
