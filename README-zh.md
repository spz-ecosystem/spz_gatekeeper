# SPZ Gatekeeper

> 仅做 L2 的 SPZ 合法性检查器，用于审计 header、flags 和 TLV trailer 扩展，同时不破坏原始 SPZ 的基础解码行为。

`spz_gatekeeper` 是一个 **纯 C++17** 的 `.spz` 校验工具。它**不**检查 GLB 容器，也**不**实现压缩、渲染或效果评测；它只负责判断一个 SPZ 文件在携带可选厂商扩展时，是否仍保持与上游 SPZ packed format 的兼容性。

## 作者与版权
- 版权所有者：**Pu Junhan**
- 起始年份：**2026**

## 个人独立作品声明
本仓库为 **Pu Junhan** 独立完成的净室实现（clean-room implementation）。SPZ 文件格式规范（原由 Niantic, Inc. 开发并发布于 https://github.com/nianticlabs/spz）仅作为实现技术互操作性的参考依据。与 "SPZ" 及 "Niantic" 相关的所有商标、商号及知识产权归其各自权利人专有。

## 组织与政治立场分离声明

### 无隶属关系
**本项目与 Niantic, Inc. 或其任何子公司、母公司、关联实体不存在任何隶属、代言或关联关系。**

作者明确且不可撤销地与 Niantic, Inc. 及其关联方所涉及的任何政治立场、言论、行动或争议划清界限。该分离声明涵盖所有政治、意识形态及组织层面的立场。

### 技术独立性
本门卫项目在独立治理框架下运作，并自行制定 SPZ 文件兼容性校验标准。本项目所实现的技术规范仅源于公开文档与独立分析。

## 治理权与衍生合规声明

### 管辖权
作为唯一创建者与维护者，**Pu Junhan** 依据本项目的开源协议主张以下权利：
1. **定义权**：定义何为合规的 SPZ 衍生项目或扩展规范的权利
2. **背书裁量权**：对任何声称 SPZ 兼容的项目给予或拒绝背书的裁量权
3. **标准演进权**：为维护技术完整性与社区标准而更新校验标准的权利

### 衍生合法性
**SPZ 衍生项目的合法性仅以其是否符合本项目发布的规范与协议为判定依据。** Niantic, Inc. 或任何其他外部实体均无权在本治理框架下确认或否定衍生项目的合法性。

### 免责声明
通过本门卫项目的校验或纳入，不构成对任何底层政治、组织或商业实体的背书。

## 工具边界

### 本工具负责
- 校验 SPZ header：magic、version、点数、SH degree、flags、reserved
- 校验 base payload 大小与截断情况
- 校验 base payload 之后的 TLV trailer
- 校验已知厂商扩展，目前内置 Adobe Safe Orbit Camera（`0xADBE0002`）
- 对更高版本 SPZ 发出 warning，并继续 best-effort 校验

### 本工具不负责
- 校验 GLB/glTF 结构
- 检查 SPZ 到 GLB 转换过程是否二进制无损
- 实现压缩、熵编码或渲染

## 主线协议口径
- 官方扩展存在位：`0x02`（`has extensions`）
- 抗锯齿位保持 `0x01`
- 未识别 flags 位按可忽略处理
- 扩展记录采用 TLV：`[u32 type][u32 length][payload...]`
- 未知 TLV 类型必须能通过 `length` 跳过
- 版本策略：
  - `version < 1` => error
  - `version 1..4` => 正常校验
  - `version > 4` => warning，但继续校验

## Web 界面

SPZ Validator 提供在线 Web 界面，无需安装即可使用：

🔗 **在线访问 (GitHub Pages)**: https://spz-ecosystem.github.io/spz_gatekeeper/
🔗 **在线访问 (腾讯云 CloudBase)**: https://openclaw-spz-3gt7x2sya7c10ef2-1355411679.tcloudbaseapp.com/

### 云端部署

本项目已部署至 **腾讯云 CloudBase** 静态托管，用于演示：
- **环境**: `openclaw-spz` (EnvId: `openclaw-spz-3gt7x2sya7c10ef2`)
- **区域**: `ap-shanghai`
- **类型**: 静态网站托管 (HTML + WASM，无需后端服务器)
- **数据隐私**: 所有验证均在浏览器本地运行，文件不上传服务器

功能特点：
- 纯浏览器端运行，WASM 本地执行
- 拖拽上传 `.spz` 文件即时验证
- 支持 SPZ v4 格式完整性检查
- 文件不上传服务器，零隐私泄露风险

### 本地构建 Web 版本

```bash
# 在已配置 Emscripten 的 WSL 环境中执行
emcmake cmake -S cpp -B build-pages -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
emmake cmake --build build-pages --parallel

# 构建产物会保留在 build-pages/site，不污染 web/
cd build-pages/site && python3 -m http.server 8080
# 打开 http://localhost:8080
```

### v1.1.1 浏览器 smoke 基线（仅合成 fixture）

`v1.1.1` 起默认采用“合成 `.spz` 夹具”进行浏览器验收，避免真实样例的协议风险：

```bash
# 1）先构建本地 CLI（用于生成 fixture）
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel

# 2）构建 WASM 站点
emcmake cmake -S cpp -B build-pages -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
emmake cmake --build build-pages --parallel

# 3）生成合成合法样例（仓库内可控路径）
./build/spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out build-pages/site/synthetic_valid.spz

# 4）启动本地服务并跑浏览器 smoke
python3 -m http.server 4173 --directory build-pages/site
node tests/wasm_smoke_test.mjs http://127.0.0.1:4173 build-pages/site/synthetic_valid.spz
```

策略说明：CI/Web smoke 默认只依赖合成 fixture；真实本地样例仅可选，不作为发布门禁必需输入。

### v1.1.2 WASM 审查模式状态
- Web UI 已启用浏览器门禁 `browser_lightweight_wasm_audit`。
- 浏览器报告可导出 `browser_to_cli_handoff`，并在 CLI `compat-check --handoff ... --json` 中并入证据链。
- 最终发布结论仍以本地 CLI 成品审查（`local_cli_spz_artifact_audit`）为准。

## 快速开始

### 在 WSL/Linux/macOS 构建
```bash
git clone https://github.com/spz-ecosystem/spz_gatekeeper.git
cd spz_gatekeeper
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/spz_gatekeeper check-spz model.spz
```

### 从 Windows 通过 WSL 构建
```bash
wsl bash -lc "
  cd /path/to/spz_gatekeeper && \
  cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release && \
  cmake --build build -j && \
  ctest --test-dir build --output-on-failure
"
```

### 依赖
```bash
# Ubuntu / Debian
sudo apt-get install -y zlib1g-dev
```

## CLI

### 校验 SPZ
```bash
spz_gatekeeper check-spz <file.spz> [--strict|--no-strict] [--json]
```

### 查看已登记扩展
```bash
spz_gatekeeper registry [--json]
spz_gatekeeper registry show 0xADBE0002 [--json]
```

### 导出 trailer TLV 记录
```bash
spz_gatekeeper dump-trailer <file.spz> [--strict|--no-strict] [--json]
```

### 查看单个资产的兼容性摘要
```bash
spz_gatekeeper compat-check <file.spz> [--json]
```

### 查看兼容性成熟度看板
```bash
spz_gatekeeper compat-board [--json]
```

### 生成最小 SPZ 夹具
```bash
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture.spz
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode invalid-size --out fixture_bad.spz
```

### 查看扩展开发指南
```bash
spz_gatekeeper guide [--json]
```

### 运行内置自测
```bash
spz_gatekeeper --self-test
```

### 命令分工速览
- `check-spz`：主入口的 L2 合法性校验，适合 CI、发布前检查和人工审计。
- `dump-trailer`：查看 base payload 之后的原始 TLV 记录、offset 与 length。
- `registry`：浏览内置扩展规范；需要单条契约细节时用 `registry show <type>`。
- `gen-fixture`：为扩展验证器开发生成最小合法/非法 `.spz` 样例。
- `compat-check`：对单个资产执行 strict / non-strict 的快速兼容性摘要。
- `compat-board`：查看扩展接入成熟度看板，不是算法性能排行榜。
- `guide`：输出扩展作者开发清单与治理说明。
- `--self-test`：不依赖外部资产，直接验证二进制内置假设。

## Registry、自测与兼容性看板的分工
- `registry` 是内置扩展契约的机器可读目录。
- `--self-test` 用于验证门卫自身的 header / TLV 假设，不依赖外部资产。
- `compat-board` 展示的是扩展接入成熟度，不是算法排行榜。
- `docs/extension_registry.json` 提供当前内置 registry 与 compatibility board 的文档镜像，供文档或 Web 页面复用。
- Web/WASM 与 CLI 复用同一套 registry / report 字段口径，保证终端、浏览器和文档里的 JSON 结构一致。
- 当前注册表模型已经明确拆成双层：
  - `ExtensionSpecRegistry`：负责扩展契约元数据，如厂商、状态、类别、规范链接、`min_spz_version`、`requires_has_extensions_flag`
  - `ExtensionValidatorRegistry`：负责按 `type` 挂接运行时 payload 校验器
- 常见扩展状态语义：
  - `known_extension=false`：未登记扩展
  - `known_extension=true && has_validator=false`：已登记但暂未实现 validator（`L2_EXT_REGISTERED_NO_VALIDATOR`）
  - `known_extension=true && has_validator=true`：已登记且已接入 validator
- 当需要查看完整契约字段，而不是只看成熟度结果时，优先使用 `registry show <type>`。
- 延伸阅读：
  - `docs/extension_registry.json`
  - `docs/Implementing_Custom_Extension.md`
  - `docs/plans/2026-03-20-spz-extension-registry-and-selftest-design.md`
  - `docs/plans/2026-03-20-spz-extension-registry-implementation-plan.md`

## WASM 质量审查模式
Web/WASM 侧现在统一为一套稳定的双模式契约：

- `browser_lightweight_wasm_audit`：浏览器端轻量门禁，输入为标准 zip 审查包
- `local_cli_spz_artifact_audit`：本地 CLI 深审，输入为真实 `.spz` 产物、目录或 manifest
- `browser_to_cli_handoff`：浏览器可选导出的 JSON handoff，仅在 CLI 的 JSON 输出中并入证据链

### 固定边界
- `spz_gatekeeper` 只审 `SPZ`。
- 不审 `GLB`。
- 不审 `spz2glb`。
- 浏览器端只审一个标准 zip 审查包。
- 只有 CLI 阶段才审真实 `.spz` 成品。
- 最终发布结论仍以 `local_cli_spz_artifact_audit` 为准。

### 浏览器端审查包结构
浏览器端固定接收一个标准审查包：

```text
<algo-name>-wasm-audit.zip
├─ manifest.json
├─ module.wasm
├─ loader.mjs
└─ tiny_fixtures/
```

最小约束如下：
- `manifest.json`：声明 profile、导出 API、预算和包元数据
- `module.wasm`：待审查主模块
- `loader.mjs`：浏览器加载入口
- `tiny_fixtures/`：可选微型样本，仅用于轻量 smoke

### 统一审查摘要字段
两种模式对齐到同一套顶层字段：
- `audit_profile`
- `policy_name`
- `policy_version`
- `policy_mode`
- `audit_mode`
- `verdict`
- `final_verdict`
- `release_ready`
- `summary`
- `budgets`
- `issues`
- `next_action`

三档结论语义保持固定：
- `pass`：可继续进入本地 CLI 深审或集成测试
- `review_required`：可运行，但存在明显工程风险，需要人工复核
- `block`：包结构、正确性或预算存在阻断问题，不应继续推进

### 推荐本地用户流
1. 先在浏览器上传标准 `.zip` 审查包。
2. 若浏览器轻审通过或可复核，则导出可选的 `browser_to_cli_handoff`。
3. 在本地对真实 `.spz` 运行 `compat-check`。
4. 若已有 handoff，则通过 `--handoff` 并入 CLI JSON 报告。
5. 以 CLI 最终结论为准。

### CLI 示例
```bash
spz_gatekeeper compat-check model.spz --json
spz_gatekeeper compat-check model.spz --handoff browser_audit.json --json
spz_gatekeeper compat-check --dir ./fixtures --json
spz_gatekeeper compat-check --manifest ./fixtures/manifest.json --json
```

固定的 `policy_mode` 约定如下：
- 单文件 `compat-check` 与 `--dir` 默认使用 `policy_mode="release"`
- `--manifest` 批量评测默认使用 `policy_mode="challenge"`
- 浏览器导出的 `browser_to_cli_handoff` 会保留自身 `policy_mode`，用于证据链对齐

### 当前 `wasm_quality_gate` 状态
`docs/extension_registry.json` 现已对齐当前 compatibility-board 快照：
- 已接线：`validator_coverage_ok`、`api_surface_wired`、`browser_smoke_wired`、`empty_shell_guard_wired`
- 子 gate 仍是 baseline 状态：`warning_budget_wired`、`copy_budget_wired`、`memory_budget_wired`、`performance_budget_wired`、`artifact_audit_wired`
- 当前 compatibility-board 结论：`final_verdict="review_required"`、`release_ready=false`

这个 compatibility-board 快照表达的是成熟度视图，不是最终发布 gate。只要剩余子 gate 仍停留在 baseline-only，顶层就应保持 `review_required`。

### 成熟度看板 vs 发布门禁
- `compat-board` 与 `docs/extension_registry.json` 是**成熟度快照**（偏观测与接入状态）。
- CI 中的 `release/challenge` gate 步骤才是**发布决策路径**（基于运行时审查证据）。
- 看板里的 `release_ready` 不能反向覆盖 CI `compat-check` gate 的发布结论。

### v2 当前剩余缺口
- `feature/spz-v2-profile-core` 当前已无已知的 v2 门禁代码路径缺口。
- 当前本地阻塞点：本次 WSL 会话缺少 Node.js / Playwright / Emscripten，因此还不能在本机直接重跑 `tests/wasm_smoke_test.mjs`；该步骤仍以 CI 中的浏览器 gate 为准。

### v2 完成判据（必须同时满足）
1. `dev/release/challenge` 三档在策略行为与预算状态语义上保持一致。
2. Browser copy budget 与 CLI memory budget 都是可阻断的真实门禁，不是仅观测。
3. `final_verdict/release_ready` 仅由单一最终汇总路径产出。
4. challenge manifest 输出稳定、可复验，且 top-level 与 item-level 一致。
5. CI 运行真实 profile-aware gate，而不是只看 board 快照。
6. README、registry、CLI、Web 的边界口径与完成语义一致。

### 7.3 补强输出
- Browser bundle 报告现已输出 `copy_breakdown`，按阶段细分 copy 次数（当前包含 `zip_inflate` / `module_clone`）。
- challenge 批量输出现已增加 `challenge_stats` 与 `visualization` 辅助区块，便于分组复核与下游渲染。
- Web 摘要面板现已直接展示 `final_verdict`、`release_ready` 与 `Copy Breakdown`。

由于浏览器端和 CLI 端都运行在用户本地，所以项目默认就具备“本地双端协同”。`browser_to_cli_handoff` 只是一个可选的标准化能力，不是后台服务，也不能替代真实 CLI 成品审查。

延伸阅读：
- `docs/plans/2026-03-22-spz-gatekeeper-wasm-audit-modes-design.md`
- `docs/plans/2026-03-22-spz-gatekeeper-wasm-audit-implementation-plan.md`

## 扩展作者快速自测闭环
```bash
spz_gatekeeper registry show 0xADBE0002 --json
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture_valid.spz
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode invalid-size --out fixture_invalid.spz
spz_gatekeeper check-spz fixture_valid.spz --json
spz_gatekeeper dump-trailer fixture_valid.spz --json
spz_gatekeeper compat-check fixture_valid.spz --json
spz_gatekeeper compat-board --json
```

这条闭环路径依次确认：已登记契约、最小合法/非法样例、原始 TLV 布局、结构化兼容性摘要，以及当前成熟度看板状态，适合作为发布前的最短核对流程。

## 校验细节

### Header 校验
- `magic == NGSP`
- version 按上面的策略处理
- `reserved == 0`
- `sh_degree <= 4`

### Trailer 校验
- trailer 只能出现在标准 SPZ 字段之后
- 若 `flags & 0x02` 置位，则 trailer 必须存在且必须能按 TLV 解析
- 若存在 trailer 但未置 `0x02`，则发出 `L2_UNDECLARED_TRAILER` warning
- 在 `--no-strict` 模式下，TLV 解析失败降级为 warning

### Adobe 扩展
内置验证器：`0xADBE0002`（`Adobe Safe Orbit Camera`）

负载格式：
```text
float32 minElevation   // 弧度，范围 [-pi/2, pi/2]
float32 maxElevation   // 弧度，范围 [-pi/2, pi/2]
float32 minRadius      // >= 0
```

## 输出示例

### 文本
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

## 项目结构
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

## 规划中的扩展
- `spz-entropy` 规划为 **vendor extension**，不是 core SPZ header 改造。
- 推荐厂商空间：`0x4E41`（`Niantic`），`extension_id` 暂定 `TBD`，待正式定稿。

## 相关项目
- [nianticlabs/spz](https://github.com/nianticlabs/spz) - 上游 SPZ 库
- [KHR_gaussian_splatting](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_gaussian_splatting) - Khronos 扩展规范

## 许可证
MIT，详见 `LICENSE`。
