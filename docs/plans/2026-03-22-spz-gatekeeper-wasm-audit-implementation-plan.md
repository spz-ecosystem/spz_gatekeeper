# SPZ Gatekeeper WASM 双模式审查实施计划

> **Goal:** 将 `spz_gatekeeper` 的双模式审查设计落地为可执行实施计划：浏览器端实现 `browser_lightweight_wasm_audit`，命令行端实现 `local_cli_spz_artifact_audit`，并补充一个**可选**的本地双端协同能力。
>
> **Architecture:** 继续复用 `InspectSpzBlob(...)`、`GateReport`、`ExtensionSpecRegistry`、`ExtensionValidatorRegistry` 等现有核心；新增统一的审查摘要构造层，让 CLI、WASM、Web 共用同一套 verdict / budgets / issues / next_action schema。浏览器端只负责 `WASM` 审查包轻量门禁，CLI 端只负责真实 `.spz` 产物审查。
>
> **Tech Stack:** C++17、CMake、zlib、Emscripten、原生 HTML/CSS/JS、CTest、Node（已有 `tests/wasm_smoke_test.mjs`）

---

## 0. 先决边界

### 0.1 固定边界

1. 门卫只审查 `SPZ`。
2. 不扩展到 `GLB`。
3. 不扩展到 `spz2glb`。
4. 不引入后台服务器作为本期前提。
5. 浏览器端输入固定为**一个标准 `zip` 审查包**。

### 0.2 双端协同的定位

这里的“双端协同”不是服务端同步能力，而是**同机本地工作流协同**：

- 浏览器端运行在用户本地；
- CLI 也运行在用户本地；
- 两者天然可以通过本地文件、JSON 摘要、手工导入导出完成串联；
- 因此“浏览器先审、CLI 再审”的基本协同**默认就存在**。

本期新增的“可选双端协同功能”不应理解为“再造一个后台”，而应理解为：

- 是否提供一个标准化的 `handoff` 文件；
- 是否让 CLI 能消费浏览器导出的轻审结果；
- 是否减少用户重复填写 package/profile/budget 元信息；
- 是否让最终报告能同时包含 browser + CLI 两段证据链。

### 0.3 本期非目标

1. 不做在线账号体系。
2. 不做远程任务队列。
3. 不做浏览器端大资产性能基准。
4. 不做算法排行榜。
5. 不在本期引入新的第三方下载依赖；如浏览器 `zip` 解包必须引依赖，需单独审批。

---

## 1. 目标交付物

本实施计划完成后，项目应新增或完成以下交付物：

1. 一套共享的审查摘要 schema 构造逻辑。
2. 一个浏览器端 `browser_lightweight_wasm_audit` 入口。
3. 一个 CLI 端 `local_cli_spz_artifact_audit` 完整入口。
4. 一个可选的 `browser -> CLI` 本地 `handoff` 协议。
5. 一组覆盖双模式与协同流程的测试。
6. 一份对外稳定的输出契约，保证 Web / CLI / CI 一致。

---

## 2. 总体实施顺序

### Phase 1：冻结统一 schema 与模式常量
### Phase 2：落地浏览器端轻量审查模式
### Phase 3：落地 CLI 端真实产物审查模式
### Phase 4：增加可选本地 `handoff` 协同能力
### Phase 5：收口 UI、测试与文档

说明：

- **Phase 1-3** 是主线，必须完成；
- **Phase 4** 是可选增强，不应阻塞主线交付；
- **Phase 5** 负责把双端体验与输出格式真正收口，避免漂移。

---

## 3. Task 1：抽离统一审查摘要层

**目标：** 让 CLI、WASM、Web 不再各自拼 JSON，而是复用同一套上层审查摘要生成逻辑。

**Files:**
- Create: `cpp/include/spz_gatekeeper/audit_summary.h`
- Create: `cpp/src/audit_summary.cc`
- Modify: `cpp/CMakeLists.txt`
- Modify: `cpp/src/main.cc`
- Modify: `cpp/src/wasm_main.cc`
- Test: `cpp/tests/audit_summary_test.cc`

### Step 1：先写失败测试，冻结顶层字段

测试应覆盖：

- `audit_profile`
- `audit_mode`
- `verdict`
- `summary`
- `budgets`
- `issues`
- `next_action`

建议同时冻结两个模式常量：

- `browser_lightweight_wasm_audit`
- `local_cli_spz_artifact_audit`

建议数据结构：

```cpp
struct AuditIssue {
  std::string code;
  std::string severity;
  std::string message;
};

struct AuditSummary {
  std::string audit_profile;
  std::string audit_mode;
  std::string verdict;
  std::string next_action;
  std::map<std::string, std::string> summary;
  std::map<std::string, double> budgets;
  std::vector<AuditIssue> issues;
};
```

### Step 2：实现共享 builder

要求：

- 文本 / JSON 输出都能从统一结构生成；
- CLI 与 WASM 侧只填充证据，不自己重复拼装 verdict 语义；
- 保留当前 `wasm_quality_gate`，但将其作为兼容字段，而不是未来唯一出口。

### Step 3：验证共享 builder 被 CLI 与 WASM 共同消费

Run:
```bash
wsl bash -lc "source /home/linuxmmlsh/.venv/hunyuan/bin/activate && cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake -S cpp -B build-wsl -DCMAKE_BUILD_TYPE=Release && cmake --build build-wsl --parallel && ctest --test-dir build-wsl -R 'audit_summary_test|compat_check_test|registry_cli_test' --output-on-failure"
```

Expected: PASS

---

## 4. Task 2：落地浏览器端 `browser_lightweight_wasm_audit`

**目标：** 浏览器端接收一个标准 `zip` 审查包，完成轻量工程门禁，并输出统一摘要。

**Files:**
- Modify: `cpp/src/wasm_main.cc`
- Modify: `web/index.html`
- Modify: `web/spz_gatekeeper.js`
- Modify: `tests/wasm_smoke_test.mjs`
- Test: `cpp/tests/compat_check_test.cc`（如复用共享 schema）
- Possibly Create: `tests/fixtures/wasm_audit_bundle/`（如需最小样本）

### Step 1：定义浏览器端 API 契约

建议新增一个高层 WASM 导出，例如：

```cpp
std::string auditWasmBundle(const std::string& bundle_bytes_json);
```

或者等价的 JS 封装入口：

```js
await module.auditWasmBundle(bundle);
```

要求最终输出统一 schema，并至少包含：

- 包结构检查结果
- manifest 与导出一致性
- 空壳风险
- tiny fixture 正确性
- copy / memory / perf 轻量预算结论
- `next_action=allow_local_cli_audit` 或等价结论

### Step 2：先解决 `zip` 输入策略

由于本期禁止擅自下载依赖，必须先在实现层做一个二选一确认：

1. **优先**：仓内零依赖实现最小 `zip` 读取；
2. **否则**：提交单独审批，引入明确、可审计的轻量解包依赖。

该步骤的验收标准不是“功能全做完”，而是：

- 明确浏览器端 `zip` 解包路径；
- 明确不会偷偷引入额外下载依赖；
- 明确失败时的阻塞点。

### Step 3：在 Web UI 中增加轻审工作流

页面需要新增：

- `zip` 文件上传入口；
- manifest 摘要展示；
- `pass / review_required / block` 显示；
- 问题列表展示；
- `导出 handoff` 按钮（若 Task 4 实现）。

### Step 4：写浏览器轻审测试

至少覆盖：

- 合法 bundle 返回 `pass` 或 `review_required`
- 缺少 `manifest.json` 返回 `block`
- manifest 声明导出与实际不一致返回 `block`
- invalid tiny fixture 无法正确失败时返回 `review_required` 或 `block`

Run:
```bash
wsl bash -lc "source /home/linuxmmlsh/.venv/hunyuan/bin/activate && cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && ctest --test-dir build-wsl -R compat_check_test --output-on-failure && node tests/wasm_smoke_test.mjs"
```

Expected: PASS

---

## 5. Task 3：落地 CLI 端 `local_cli_spz_artifact_audit`

**目标：** CLI 侧对真实 `.spz` 产物输出稳定、可批量化的审查结果。

**Files:**
- Modify: `cpp/src/main.cc`
- Modify: `cpp/include/spz_gatekeeper/report.h`
- Modify: `cpp/src/report.cc`
- Modify: `cpp/src/spz.cc`
- Modify: `cpp/tests/compat_check_test.cc`
- Modify: `cpp/tests/gen_fixture_test.cc`
- Possibly Create: `cpp/tests/compat_manifest_test.cc`

### Step 1：补齐单文件报告字段

在现有 `compat-check` 基础上，确保单文件报告至少稳定输出：

- `audit_profile=spz`
- `audit_mode=local_cli_spz_artifact_audit`
- `strict_ok`
- `non_strict_ok`
- `artifact_summary`
- `budgets`
- `issues`
- `verdict`

### Step 2：新增批量输入模式

建议在 `main.cc` 中逐步新增：

- `compat-check --dir <dir> --json`
- `compat-check --manifest <manifest.json> --json`

要求：

- 批量模式输出 summary + top_issues；
- 目录模式只递归处理 `.spz`；
- manifest 模式支持固定顺序与标签化 corpus。

### Step 3：加入 artifact budget 语义

CLI 侧 budget 重点包括：

- 文件大小
- 解压体积
- 处理耗时
- 峰值内存 / 增长次数（若本期已有稳定采样路径）

注意：

- 若某预算当前不能稳定测量，应显式标记 `unsupported` 或 `not_collected`；
- 不允许伪造或静默补 0。

### Step 4：写批量模式测试

至少覆盖：

- 单文件 `.spz`
- `--dir` 含 1 个 pass、1 个 block
- `--manifest` 可输出稳定汇总
- strict / non-strict 双结论在 JSON 中共存

Run:
```bash
wsl bash -lc "source /home/linuxmmlsh/.venv/hunyuan/bin/activate && cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake --build build-wsl --parallel && ctest --test-dir build-wsl -R 'compat_check_test|gen_fixture_test|compat_manifest_test' --output-on-failure"
```

Expected: PASS

---

## 6. Task 4：增加可选本地双端协同能力

**目标：** 在不引入后台服务的前提下，让浏览器轻审结果可被 CLI 复用或挂接，减少重复录入与证据割裂。

**重要说明：**

- 这不是主线必需功能；
- 因为浏览器与 CLI 都在本地，所以“协同”默认就可以通过人工导出/导入完成；
- 本任务只是把这种天然协同**标准化**。

**Files:**
- Modify: `cpp/include/spz_gatekeeper/audit_summary.h`
- Modify: `cpp/src/audit_summary.cc`
- Modify: `cpp/src/main.cc`
- Modify: `cpp/src/wasm_main.cc`
- Modify: `web/index.html`
- Test: `cpp/tests/audit_summary_test.cc`
- Possibly Create: `cpp/tests/handoff_cli_test.cc`
- Modify: `tests/wasm_smoke_test.mjs`

### Step 1：冻结 `handoff` JSON 契约

建议浏览器导出一个标准文件，例如：

```json
{
  "audit_profile": "spz",
  "audit_mode": "browser_lightweight_wasm_audit",
  "bundle_id": "sha256:...",
  "tool_version": "0.1.0",
  "verdict": "pass",
  "summary": {},
  "budgets": {},
  "issues": [],
  "next_action": "allow_local_cli_audit"
}
```

### Step 2：CLI 新增可选参数

建议新增：

- `compat-check <file.spz> --handoff <browser_audit.json> --json`

语义：

- 若提供 `--handoff`，CLI 在最终报告中附带 browser 证据链；
- 若未提供，CLI 仍可独立运行；
- handoff 只能增强体验，不能替代 CLI 对 `.spz` 的真实审查。

### Step 3：最终报告增加双段证据视图

建议最终 JSON 增加：

```json
{
  "browser_stage": {},
  "artifact_stage": {},
  "final_verdict": "pass"
}
```

或者保持现有顶层不变，只增加：

- `handoff`
- `upstream_audit`
- `evidence_chain`

二选一即可，但必须保持顶层 schema 可演进。

### Step 4：协同能力验收标准

必须满足：

1. 没有后台服务也可完整工作。
2. 浏览器导出的 JSON 可以被 CLI 消费。
3. handoff 缺失不会影响 CLI 主线能力。
4. handoff 不会被误解为“CLI 可以跳过真实 `.spz` 审查”。

Run:
```bash
wsl bash -lc "source /home/linuxmmlsh/.venv/hunyuan/bin/activate && cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake --build build-wsl --parallel && ctest --test-dir build-wsl -R 'audit_summary_test|handoff_cli_test' --output-on-failure && node tests/wasm_smoke_test.mjs"
```

Expected: PASS

---

## 7. Task 5：Web/CLI/UI/文档收口

**目标：** 防止模式边界在实现后再次漂移。

**Files:**
- Modify: `README.md`
- Modify: `README-zh.md`
- Modify: `docs/WIKI.md`
- Modify: `docs/Implementing_Custom_Extension.md`
- Modify: `web/index.html`
- Modify: `docs/extension_registry.json`（如需暴露审查模式元信息）

### Step 1：统一术语

所有对外文档统一为：

- 浏览器模式：`browser_lightweight_wasm_audit`
- CLI 模式：`local_cli_spz_artifact_audit`
- 可选协同：`browser_to_cli_handoff`

### Step 2：统一边界说明

所有文档明确写清：

- 门卫只审 `SPZ`
- 不审 `GLB`
- 不审 `spz2glb`
- 浏览器端只做轻量 `WASM` 审查包门禁
- CLI 端只做真实 `SPZ` 成品审查

### Step 3：统一用户操作流

最终推荐用户流：

1. 浏览器上传 `zip` 审查包
2. 轻审通过后，导出 `handoff`（可选）
3. 本地 CLI 对真实 `.spz` 跑 `compat-check`
4. 若有 `handoff`，则并入最终报告
5. 以 CLI 最终结论为准

---

## 8. 风险与阻塞点

### 风险 1：浏览器 `zip` 解包方案不明确

- 影响：浏览器端主线无法真正吃到标准 `zip` 审查包
- 处理：优先零依赖实现；若必须引依赖，先单独审批

### 风险 2：预算指标不稳定

- 影响：`memory/perf/copy` 字段可能语义失真
- 处理：不能稳定测量就显式标注 `not_collected`，不得伪造

### 风险 3：CLI/WASM 各自继续拼 JSON

- 影响：双端 schema 漂移
- 处理：Task 1 必须优先完成，共享 builder 后再扩功能

### 风险 4：可选协同被误解为主线依赖

- 影响：用户误以为没有 handoff 就不能跑 CLI
- 处理：所有文档与 UI 均明确标注 `handoff` 为 optional

---

## 9. 验收标准

全部完成后，应满足：

1. 浏览器端可以对一个标准 `zip` 审查包输出统一 schema。
2. CLI 可以对单个 `.spz` 输出统一 schema。
3. CLI 至少支持 `--dir` 或 `--manifest` 中的一种批量模式；理想情况两者都支持。
4. 双端的 `pass / review_required / block` 语义一致。
5. `wasm_quality_gate` 保持兼容，但不再是唯一顶层结论出口。
6. 可选 `handoff` 缺失时，CLI 主线仍可独立运行。
7. 最终交付结论以 CLI 为准。
8. 全局仍严格保持“只审 `SPZ`，不审 `GLB/spz2glb`”的边界。

---

## 10. 推荐执行策略

推荐按以下顺序实际施工：

1. **先做 Task 1**：否则双端一定漂移。
2. **再做 Task 3**：CLI 先稳住真实产物主线。
3. **然后做 Task 2**：浏览器端轻审补齐高层入口。
4. **最后做 Task 4**：把本地协同做成可选增强，而不是主线依赖。
5. **Task 5 收尾**：统一文案、UI、命令帮助与样例。

也就是说，虽然产品体验上是“浏览器先、CLI 后”，但工程实施上更稳的顺序是：

`共享 schema -> CLI 主线 -> Browser 轻审 -> Optional handoff -> 文档/UI 收口`

这样可以避免先做浏览器 UI，后面又因为 schema 漂移而返工。

---

## 11. 最终结论

这份实施计划建议将门卫能力拆成：

- **主线必做**
  - `browser_lightweight_wasm_audit`
  - `local_cli_spz_artifact_audit`
  - 共享审查摘要 schema

- **可选增强**
  - `browser_to_cli_handoff`

其中，所谓“双端协同”本质上已经因为“浏览器端与 CLI 都运行在用户本地”而天然具备；本期真正需要补的是**标准化协同协议**，而不是后台服务。
