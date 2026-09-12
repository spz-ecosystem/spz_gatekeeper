# 变更日志

本文件仅记录当前主线的重要变更，不保留已废弃路线的细节。版本按时间倒序排列（最新在上）。

## [v2.0.5.3] - 2026-09-11

### CI 安全网加固 (R7.2, #81)

落实 R7 计划 PRE 系列**剩余未落地项**（spz2glb 经验吸收，PRE.20 索引标注的 📌 待落地条目）：

- **CI 管道掩盖修复（PRE.14）**：`| tee` 步骤补显式 `shell: bash` + `set -euo pipefail` —— 此前不写 `shell` 时 GitHub 默认仅 `bash -e`（**无 pipefail**），管道退出码取 `tee`(=0)，编译/校验失败被掩盖
- **修真实缺陷**：WASM 浏览器冒烟测试的 `if node … | tee …; then` 条件**恒真**（管道无 pipefail）⇒ 冒烟测试永远"通过"、重试与失败分支均为死代码；补 pipefail 后恢复真实判定
- **关键产物缺失禁止静默（PRE.14）**：3 处 `upload-artifact` 补 `if-no-files-found: error`（native 二进制 / WASM 站点 ×2）
- **移除假绿 step（PRE.19 A）**：删除恒 ENOLOCK 且 `continue-on-error: true` 吞结果的 `npm audit` step；供应链面由 security-audit job（zizmor）+ `npm install --ignore-scripts` 承担
- **macOS 双架构（PRE.12）**：`macos-latest` 实为 ARM64 却产出 `-macos-x64` 名实错配 ⇒ 拆为 `-macos-arm64`（macos-latest）+ `-macos-x64`（macos-15-intel）；相关条件改 `startsWith(matrix.os,'macos')`
- **MinGW 语法守卫（PRE.10）**：新增 `mingw-syntax` job —— `x86_64-w64-mingw32-g++ -fsyntax-only` 动态校验全部含 `_WIN32` 的翻译单元，拦截条件编译守卫漂移
- **native 双覆盖（PRE.10）**：新增 `verify-native` job（**不 needs build**）复用 `wasm-pre-check.sh --native-only`，并补 CMake `set(::)` 语法守卫（`::` 为 ALIAS/IMPORTED 保留符号）
- **三引擎冒烟（PRE.13）**：Playwright `1.52.0 → 1.62.0`；`wasm_smoke_test.mjs` 支持 `SPZ_WASM_SMOKE_BROWSER`（chromium/firefox/webkit，未知值 fail-loud），CI 逐引擎轮跑并逐引擎独立日志
- **CHANGELOG 发布门禁（PRE.16 + PRE.18）**：新增 `scripts/changelog-check.sh` + release.yml `changelog-check` job（`release` 依赖之）—— tag 区间内每个 commit 必须已在 CHANGELOG 有记录，否则**阻断发布**；含 `docs(scope):` **自指豁免**（前置，防"记录者自身无法被记录"的无限回归）
- **Release 正文来源修正（PRE.19 E.3）**：`generate_release_notes` 改为从权威 CHANGELOG 提取该版本段落（`body_path`），并给 release job 补 `fetch-depth: 0`

### 文档与治理
- README/README-zh 版本标签更新至 v2.0.5.3

## [v2.0.5.2] - 2026-09-11

### WASM 运行时完整性 (R7.1, #78)
- 前端 wasm 指纹运行时校验（**SHA-384**）：信任根内嵌 `web/index.html`（CI 构建时注入 `spz-wasm-fp` meta），加载字节哈希不符即拒绝加载（fail-closed）；`wasm-fingerprint.json` 仅作审计辅助，不作校验依据
- 校验与加载复用同一份 fetch 字节（`wasmBinary` 直传），消除"先算哈希再二次下载"的替换窗口
- 静态 import 与动态 import 统一为同一 `?v=<fp>` URL，加载链唯一化
- 降级显式化（fail-loud）：wasm 加载/校验失败在 UI 标注原因，不再静默回退纯 JS 路径
- 预检增强：cache-busting 链检查（P4）+ `rg` 确定性检测（回退 `grep -aFq`）+ 严格锚定的 sed 自动修复
- CI 回环：`--auto-fix` 生效 + 指纹注入 step（sha384 → `__WASM_FP__`/`__FP__` 替换）；`npm install --ignore-scripts` 供应链加固（防 preinstall 钩子投毒）

### Web UI 批处理队列 (R7_1g, #79)
- 前端队列与 spz2glb `MAX_PARALLEL=1` 对齐：并发常量 `2 → 1`（WASM 单实例串行，排队等待不计入处理计时）
- 并发控制由布尔闩改为**计数器 + while 循环**：原布尔闩在任何时刻至多 1 项，`kQueueMax > 1` 时无法扩展（"假并发"）
- 修复队列 fail-open：`handleFile` 失败（超推荐上限 / 超 30MB 硬顶 / 引擎未就绪 / 解压失败）不再被吞成完成态 —— 队列条正确显示失败并可重投（此前失败项恒显示 ✅，且同名文件重投被永久跳过）

### 文档与治理
- README/README-zh 版本标签更新至 v2.0.5.2；补 v2.0.4 WASM 工程优化段与 v2.0.5 Web UI 段
- 术语更正：`TLV` → `ILV`；JSON 示例字段 `tlv_records` → `ilv_records`
- 删除 README-zh 空 section；补齐中文压缩格式说明

## [v2.0.5.1] - 2026-07-30

### 文档与治理
- README 新增 Future Plans 章节，宣布向开放原子开源基金会捐赠意向
- 补写 v2.0.3-v2.0.5 缺失更新日志
- 新增 .gitattributes 统一 CRLF/LF 换行规范

### CI 修复
- 注册 feature/spz-gatekeeper-2.0.5.1 分支到 push 触发器
- 构建日志 tee + 依赖解析摘要 + wasm-build.log 上传，失败时保留现场

## [v2.0.5] - 2026-07-29

### Batch Queue + WASM 工程优化 (R7)
- 前端批处理队列 max=2，串行处理，自动推进下一文件 (#61)
- 从 spz2glb 移植 HotObjectPool、BumpAllocator、MemoryTracker 到 WASM C++ 层 (#61)
- 预留缓冲区 C API (gk_reserve_buffer 等 8 函数)，支持分块写入 64KB (#61)
- 设备感知 SmartMemoryManager，根据 UA/deviceMemory/hardwareConcurrency 自适应 (#61)
- 自动导出 handoff JSON：单文件 .spz 审查完成后自动生成浏览器审计报告 (#62)
- 队列滑动动画 (slideOut)，完成项自动滑出并移除 (#62)

### CI/CD 管线加固
- T44 Auto-Fix Loop：P1 cmake 失败自动重配、OOM 自动 --parallel 1 降级、P5 端口冲突自动换端口 (4173→4174→4175) (#52-#55)
- P2 Emscripten 符号下划线前缀自动检测修复 (gk_* → _gk_*) (#61)
- P6 工作流强制 lint：actionlint + zizmor --min-severity medium (#53)
- 预检系统检测缺失 .wasm 二进制文件 (#56)
- 预算控制 (diff limit + edit count gate) 后因过度设计回退 (#55)

### 安全性
- 5 个 workflow 安全修复：pages.yml 权限下放到 job 级、zizmor 过滤 info 级别噪音 (#54)
- 修复 PR #62 合并残留冲突标记导致 WASM 引擎卡加载 (#56)
- 移除冲突标记残留，确保 initializeWasm() 正常执行

## [v2.0.4] - 2026-07-26

### WASM 零拷贝检查路径
- 新增 inspectSpzPtr(ptr, size, strict)，绕过 std::vector 中间拷贝，直接操作 WASM 堆内存 (#46, #47, #51)
- 导出 _malloc/_free/HEAPU8，JS 侧手动管理 WASM 内存生命周期 (#46)
- WASM 初始化三阶段超时递增重试（30s → 45s → 60s），弱网兼容 (#51)

### JS+WASM 分离加载
- 移除 -sSINGLE_FILE=1，WASM 二进制从 JS 胶水代码中分离 (#47)
- JS 胶水从数 MB 降至 ~几十 KB，.wasm 由浏览器流式编译 (#47)
- 缓存破坏：`?v=Date.now()` 防止 CDN/浏览器缓存旧 WASM (#51)

### Emscripten 6.0.3 升级
- 从 3.1.56 升级到 6.0.3，享受 Emscripten 最新编译优化
- WASM 构建产物 -Oz 优化后仅 ~300KB+

### 源文件级预检系统
- P2 符号检查不再依赖 wasm-objdump，直接在 wasm_main.cc 和 CMakeLists.txt 中验证 (#49, #50)
- 支持 --auto-fix 自动安装缺失工具 (#50)
- P3 适配分离模式：检查 .wasm 二进制体积而非 JS 胶水 (#48)
- P5 Smoke 测试从 CJS require() 改为 ES module import()
- P7 文件完整性检查：UTF-8 有效性 + C++ 语法验证

### CI/CD 管线
- Pages 部署与 CI 集成，支持 workflow_dispatch 手动触发 (#40)
- npm audit 供应链检查、zizmor 工作流安全扫描
- SHA256 发布清单自动生成
- 构建日志上传（失败时保留现场）

### Adobe Coordinate System 扩展
- 新增 0xADBE0003 扩展验证器 (#44)
- SPZ v4/v3 格式兼容性检查
- License 统一 + TLV → ILV 重命名 (#39)

### 前端可靠性
- WASM 导出 Promise.resolve().then() 安全包装，避免 .catch() 漏捕获
- 修复 renderBudgets 中 undefined budgetsList 引用

## [v2.0.3.1] - 2026-07-03

### 修复
- 移除 --closure 1 标志（破坏 WASM 在 Pages 上的初始化）
- 移除 sync-gh-pages.yml（供应链安全风险）

### 前端修复
- 新增直接 .spz 上传路径 (inspectSpz) 支持 (#34)
- 修复 GateReport 格式 (ok/issues) 在 formatVerdict + renderSummary 中的处理 (#34)
- 正确渲染 GateReport summary/manifest/budgets 面板 (#35)
- 填充 .spz 面板的 registry/compat/trailer 数据 (#36)

## [v2.0.3] - 2026-07-02

### WASM 优化 (T52-T54)
- WASM 工程优化：构建缓存、内存上限 30MB、前端体积检查
- Pages 自动部署：tag push (v*) 触发 Pages 构建 (#27)
- 移除 sync-gh-pages.yml（legacy，供应链风险），pages.yml 通过 actions/deploy-pages 替换

## [v2.0.2] - 2026-06-26

### v4 format support
- New --format v4 flag for gen-fixture command
- ZSTD compression integration for v4 fixture blobs
- 6-category v4 format test suite (v4_format_test.cc)
- WASM build: ZstdCompress + v4 path support

### CI & build improvements
- zlib URL switched to GitHub releases (zlib.net unreliable)
- WASM flags cleanup (removed -sASSERTIONS=0, -sNO_EXIT_RUNTIME=1)
- Test timeout 60s for self_test (prevents Windows CI hang)
- v4_format_test excluded from default build (WASM-only)

### Documentation
- DOI badge added to README.md and README-zh.md
- Version bumped 2.0.0 -> 2.0.2 in CMakeLists.txt

## [v2.0.0] - 2026-03-26

- v2 profile core 收口，文档与摘要口径统一。
- release/challenge gate 语义闭环，测试通过。

## [v1.1.2] - 2026-03-23

### WASM 双模式落地
- 浏览器侧稳定开放 `browser_lightweight_wasm_audit`，用于标准 zip 审查包的轻量门禁。
- CLI 侧 `compat-check` 支持单文件、`--dir`、`--manifest` 三种入口，统一到 `local_cli_spz_artifact_audit` 结论口径。
- 支持 `--handoff` 合并浏览器导出的 `browser_to_cli_handoff`，在 JSON 报告中保留上游证据链。

### 共享报告契约
- Browser/CLI 对齐共享报告字段与结论语义（`pass` / `review_required` / `block`）。
- 浏览器 JS 优先走 wasm 导出的共享 C++ builder，保留 legacy fallback 作为兼容路径。

### CI 与回归
- CI 增加 browser smoke 与本地 CLI 回归串联，确保浏览器轻审到 CLI 深审链路可复现。
- 默认继续使用仓库内合成 fixture，不将真实 `.spz` 资产作为发布门禁输入。

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
