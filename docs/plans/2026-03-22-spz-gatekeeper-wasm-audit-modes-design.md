# SPZ Gatekeeper WASM 通用审查 Skill 与双模式审查设计

## 1. 背景

`spz_gatekeeper` 已经具备 3 个可继续扩展为工程化审查体系的基础：

1. 共享核心校验链路：`InspectSpzBlob(...)`、`GateReport`、`ExtensionSpecRegistry`、`ExtensionValidatorRegistry`。
2. 原生命令行入口：`check-spz`、`dump-trailer`、`compat-check`、`compat-board`、`gen-fixture`。
3. 浏览器侧 WASM 入口：`inspectSpz`、`dumpTrailer`、`listRegisteredExtensions`、`describeExtension`、`getCompatibilityBoard`。

最近新增的 `wasm_quality_gate` 已经证明，门卫不仅可以做格式合法性检查，还可以进一步演进为“工程可上线性审查器”。

但如果继续无边界扩展，会出现两个问题：

1. 浏览器端会被错误期待为“大资产审查器”，而浏览器内存限制天然不适合承接重型产物审查。
2. `WASM` 工程问题与 `SPZ` 成品问题会混在一起，导致规则漂移、输出语义混乱。

因此，本设计将门卫明确拆成两种模式，并把其背后的能力抽象为通用 `WASM` 工程优化/审查 skill。

---

## 2. 设计结论摘要

### 2.1 总结论

1. 将 `WASM` 审查沉淀为通用工程 skill，而不是临时的 `SPZ` 专用补丁。
2. `spz_gatekeeper` 作为该通用 skill 的一个 `SPZ profile` 落地。
3. 门卫只服务 `SPZ` 审查边界，不扩展到 `GLB` 或 `spz2glb`。
4. 审查模式固定拆分为：
   - **浏览器端**：`browser_lightweight_wasm_audit`
   - **命令行端**：`local_cli_spz_artifact_audit`
5. 浏览器端只审查**纯算法 WASM 审查包**；命令行端只审查**用户算法产出的 SPZ 成品**。

### 2.2 推荐原因

这样拆分后：

- 浏览器端负责“轻量、低风险、工程质量前置筛查”；
- CLI 端负责“真实产物、真实预算、真实兼容性审查”；
- 两者共享统一审查 schema，但不再强行承担彼此不适合的职责。

---

## 3. 范围边界

### 3.1 负责范围

门卫本阶段只负责以下两类对象：

1. **WASM 算法审查包**
   - 用于浏览器端轻量工程审查
   - 核心问题是：这个 WASM 包是否具备工程上线资格、是否值得进入更重的本地审查

2. **SPZ 成品文件**
   - 用于本地 CLI 端真实产物审查
   - 核心问题是：用户算法产出的 `.spz` 是否合规、可读、兼容、可交付

### 3.2 明确排除项

本设计明确排除以下对象：

1. `GLB`
2. `spz2glb`
3. 服务器端远程审查平台
4. 浏览器端大体积真实资产性能基准
5. 算法优劣排行榜或压缩率排行榜

### 3.3 浏览器端边界

浏览器端不能回答以下问题：

- 大规模真实资产能否稳定处理
- 真实大文件峰值内存是否可控
- 最终 `SPZ` 成品是否可交付

浏览器端只能回答：

> 这份 WASM 算法包是否通过轻量工程门禁，是否值得进入本地深审。

---

## 4. 通用 Skill 定位

## 4.1 推荐名称

推荐将该能力抽象为通用 skill：

- `wasm-engineering-audit-skill`

其中：

- `spz_gatekeeper` 是 `SPZ` 领域的一个应用 profile；
- 后续若 `SPZ` 全链路更多模块上 `WASM`，仍复用同一套工程审查规则；
- 未来其他 `WASM` 项目也可借用同一 skill，只需替换 profile 与 fixture。

## 4.2 固定审查维度

该 skill 固定审查 8 个维度：

1. **空壳审查**
   - 导出函数是否真实接到主路径
   - 是否存在假成功、假结果、空转接口

2. **API / ABI Surface 审查**
   - 导出是否最小化
   - JS loader、manifest、WASM export 是否一致

3. **多拷贝预算审查**
   - 是否出现多余的 `ArrayBuffer` / `Uint8Array` / `vector<uint8_t>` 往返复制

4. **内存模型审查**
   - 初始内存、增长次数、峰值占用、对象复用、内存池

5. **指针 / 所有权 / Embind 限制审查**
   - 裸指针暴露
   - 生命周期不清
   - `Embind` 默认复制/所有权陷阱

6. **Warning Clean 审查**
   - 构建期与运行期警告是否受控

7. **性能预算审查**
   - 冷启动时间
   - 微型样本耗时
   - 重复调用退化
   - 小规模吞吐稳定性

8. **Artifact 审计能力**
   - 该维度主要在 CLI 产物审查模式启用
   - 浏览器端只做存在性声明，不承担重型资产审计

## 4.3 Skill 输出原则

skill 输出不应直接给“优化建议大全”，而应先给结构化结论：

- `pass`
- `review_required`
- `block`

并按问题类别输出证据与下一步动作。

---

## 5. 双模式总体架构

### 5.1 设计原则

1. **共享核心，不共享错误职责**
   - 共享：解析、validator、registry、报告 schema
   - 分离：浏览器轻审与 CLI 重审

2. **浏览器轻、CLI 重**
   - Browser 面向审查包与合成 fixture
   - CLI 面向真实 `.spz` 文件与批量资产

3. **先工程门禁，后产物门禁**
   - 浏览器先判断算法包是否值得继续
   - CLI 再判断真实 `.spz` 产物是否可交付

### 5.2 逻辑结构

统一结构如下：

`WASM 审查包 / SPZ 文件 -> Core Inspect / Registry / Validator -> 审查摘要 Schema -> Verdict`

其中：

- 浏览器模式输入为 **WASM 审查包**；
- CLI 模式输入为 **SPZ 成品文件**；
- 输出统一落到同一个上层审查摘要 schema。

---

## 6. 浏览器端模式：`browser_lightweight_wasm_audit`

## 6.1 目标

浏览器端只做**轻量 WASM 工程审查**，不做最终 `SPZ` 产物交付判断。

目标是：

1. 快速确认一个 WASM 包不是空壳；
2. 快速确认导出 API、loader、manifest 能否正常协同；
3. 用微型合成 fixture 验证最小可用主路径；
4. 用轻量预算筛掉明显不合格的工程实现。

## 6.2 输入格式

浏览器端只支持 **一个标准 zip 审查包**。

推荐包结构如下：

```text
<algo-name>-wasm-audit.zip
├─ manifest.json
├─ module.wasm
├─ loader.mjs
└─ tiny_fixtures/
   ├─ valid_case.json
   └─ invalid_case.json
```

其中：

- `manifest.json`：声明 profile、导出 API、预算、版本、生成信息
- `module.wasm`：待审查主模块
- `loader.mjs`：浏览器加载器
- `tiny_fixtures/`：可选微型样本，仅用于轻量 smoke

### 6.2.1 `manifest.json` 最小字段建议

```json
{
  "profile": "spz",
  "package_type": "wasm_audit_bundle",
  "entry": "loader.mjs",
  "module": "module.wasm",
  "exports": ["init", "selfCheck", "runTinyFixture"],
  "budgets": {
    "cold_start_ms": 1500,
    "tiny_case_ms": 500,
    "peak_memory_mb": 256,
    "copy_pass_limit": 2
  }
}
```

## 6.3 审查项

浏览器端固定执行以下项目：

1. **包结构检查**
   - zip 是否完整
   - manifest 是否可读
   - entry/module 是否存在

2. **模块加载检查**
   - loader 是否能初始化模块
   - export 是否与 manifest 声明一致

3. **空壳检查**
   - `selfCheck` / `runTinyFixture` 是否真实触发主路径
   - 不能只返回固定成功值

4. **微型正确性检查**
   - valid tiny fixture 通过
   - invalid tiny fixture 稳定失败

5. **轻量 copy / memory / perf 检查**
   - copy 次数是否超阈值
   - 峰值内存是否超过轻量预算
   - 冷启动与 tiny case 是否超预算

6. **风险检查**
   - 明显 `Embind` 风险
   - 警告预算超标
   - manifest 与实际导出不一致

## 6.4 输出结论

浏览器端只输出三档结论：

- `pass`
- `review_required`
- `block`

建议含义如下：

- `pass`：通过轻量工程门禁，可进入本地 CLI 深审或集成测试
- `review_required`：可运行，但存在明显工程风险，需要人工复核
- `block`：包结构、主路径、预算或正确性存在阻断问题，不应继续

## 6.5 浏览器端限制

浏览器模式不得承担：

1. 大文件性能评估
2. 真实产物内存峰值认证
3. 大批量 corpus 审查
4. 最终 `SPZ` 成品兼容性签发

---

## 7. 命令行端模式：`local_cli_spz_artifact_audit`

## 7.1 目标

命令行端只审查**用户算法实际产出的 `SPZ` 成品**。

目标是：

1. 检查 `.spz` 文件是否合法、可读、兼容；
2. 检查 trailer/TLV/扩展 validator 覆盖情况；
3. 检查 strict / non-strict 双结论；
4. 检查真实产物预算、批量资产汇总和交付风险。

## 7.2 输入形式

建议 CLI 最终支持三种输入：

1. 单文件
   - `compat-check file.spz --json`
2. 目录
   - `compat-check --dir <dir> --json`
3. manifest
   - `compat-check --manifest <manifest.json> --json`

其中：

- 单文件是基础能力；
- `--dir` 适合本地批量回归；
- `--manifest` 适合规范化数据集与 CI 批量产物审查。

## 7.3 审查项

CLI 侧固定执行以下项目：

1. **格式合法性**
   - gzip 解压
   - header/version/flags/reserved
   - base payload 尺寸推导

2. **TLV / trailer 审查**
   - trailer 是否存在
   - TLV 长度是否合法
   - 未知 type 是否可跳过

3. **扩展兼容性审查**
   - registry 中是否已登记
   - validator 是否存在
   - validator 结果是否通过

4. **strict / non-strict 双结论**
   - 严格模式是否通过
   - 非严格模式是否通过

5. **真实产物预算审查**
   - 文件大小
   - 解压后体积
   - 处理耗时
   - 峰值内存/增长次数

6. **批量汇总审查**
   - 通过数
   - review_required 数
   - block 数
   - 高频 issue 汇总

## 7.4 输出结论

CLI 侧建议输出两层结果：

### 7.4.1 单文件报告

```json
{
  "audit_profile": "spz",
  "audit_mode": "local_cli_spz_artifact_audit",
  "verdict": "pass",
  "strict_ok": true,
  "non_strict_ok": true,
  "artifact_summary": {},
  "budgets": {},
  "issues": []
}
```

### 7.4.2 批量汇总报告

```json
{
  "audit_profile": "spz",
  "audit_mode": "local_cli_spz_artifact_audit",
  "summary": {
    "total": 128,
    "pass": 120,
    "review_required": 6,
    "block": 2
  },
  "top_issues": []
}
```

## 7.5 命令行端限制

CLI 侧仍然不负责：

1. `GLB` 审查
2. `spz2glb` 审查
3. 远端托管服务化审查
4. 算法排名或 leaderboard

---

## 8. 统一审查 Schema

## 8.1 顶层统一字段

建议统一顶层输出为：

```json
{
  "audit_profile": "spz",
  "audit_mode": "browser_lightweight_wasm_audit",
  "verdict": "pass",
  "summary": {},
  "budgets": {},
  "issues": [],
  "next_action": "allow_local_cli_audit"
}
```

统一字段含义：

- `audit_profile`：当前领域配置，先固定为 `spz`
- `audit_mode`：当前审查模式
- `verdict`：三档结论
- `summary`：高层摘要
- `budgets`：预算对比
- `issues`：结构化问题列表
- `next_action`：引导后续流程

## 8.2 与现有字段兼容

现有 `wasm_quality_gate` 不应立即移除，而应：

1. 保留现有字段，保证兼容已有 UI / JSON / 测试
2. 在上层新增更中性的模式化字段，例如：
   - `audit_modes.browser_lightweight`
   - `audit_modes.local_cli_artifact`
3. 逐步把 `wasm_quality_gate` 从“单一对象”演进为“统一审查体系的一部分”

---

## 9. 浏览器端与 CLI 端的关系

建议流程固定为：

1. **浏览器端先审查 WASM 包**
   - 目标：筛掉空壳、导出错误、轻量预算问题
2. **通过后进入本地 CLI 审查**
   - 目标：检查真实 `.spz` 成品与批量资产
3. **最终交付结论以 CLI 为准**
   - 浏览器结论只代表“允许进入下一阶段”
   - CLI 结论才代表“真实产物是否可交付”

即：

- Browser 负责 **前置工程门禁**
- CLI 负责 **最终产物门禁**

---

## 10. 推荐实施顺序

## Phase 1：冻结边界与术语

1. 固定门卫边界：只审 `SPZ`
2. 固定双模式命名：
   - `browser_lightweight_wasm_audit`
   - `local_cli_spz_artifact_audit`
3. 固定浏览器输入：只允许一个标准 zip 审查包

## Phase 2：补浏览器端上层摘要能力

1. 在 WASM 侧新增浏览器版 `compat-check` 摘要接口
2. 让浏览器不再只有 `inspectSpz` 级别的底层结果
3. 输出与 CLI 同构的高层审查摘要

## Phase 3：抽离共享审查构件

1. 将 CLI / WASM 重复的 gate / board / schema 构造逻辑抽到 core
2. 消除双端漂移风险
3. 让 Web、CLI、CI 共用同一套摘要生成逻辑

## Phase 4：补 CLI 批量产物审查

1. 新增 `--dir`
2. 新增 `--manifest`
3. 增加 corpus 汇总报告
4. 增加 artifact budget 审查

## Phase 5：固化通用 skill

1. 抽出 `wasm-engineering-audit-skill`
2. 固定审查维度、判定级别、输出格式
3. 将 `SPZ` 作为首个标准 profile

---

## 11. 验收标准

本设计落地后，应满足以下验收标准：

1. 浏览器端不会再被设计成大资产审查器。
2. 浏览器端只接收标准 `zip` 审查包。
3. CLI 端只面向 `.spz` 真实产物。
4. 门卫全局不扩展到 `GLB` / `spz2glb`。
5. 两种模式都输出统一的高层审查 schema。
6. `pass / review_required / block` 语义在双端一致。
7. 通用 `WASM` 工程审查 skill 可以脱离当前仓库独立复用。

---

## 12. 最终建议

本设计推荐的最终形态是：

- **一个通用 skill**：`wasm-engineering-audit-skill`
- **一个 SPZ profile**：`spz_gatekeeper`
- **两种固定审查模式**：
  - 浏览器：轻量 WASM 工程门禁
  - CLI：真实 `SPZ` 产物门禁

这是目前最稳、最清晰、最容易长期固化的演进路径。
