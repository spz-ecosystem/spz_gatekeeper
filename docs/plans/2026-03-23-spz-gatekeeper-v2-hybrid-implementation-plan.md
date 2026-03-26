# SPZ Gatekeeper v2.0.0 Hybrid 实施计划

> **Version Goal:** 将 `spz_gatekeeper` 从“已打通的 Browser + CLI 双端流程”收口为“一套单一规则内核 + 三档 profile（`dev/release/challenge`）+ 可阻断 CI + 可复用基线数据集”的正式工程版本。
>
> **定位:** `v2.0.0` 同时服务两类场景：
> 1. 工程上线门卫：对 WASM 审查包与真实 `.spz` 产物执行稳定、可复验、可阻断的工程审查；
> 2. 挑战赛评测内核：在不引入远程平台的前提下，提供固定规则、固定顺序、可批量复验的本地评测入口。
>
> **Main Workspace:** `c:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project`
>
> **Baseline Dataset Root:** `c:/Users/HP/Downloads/HunYuan3D_test_cases/test_cases/fastgs_ccby4_gs_spz_input`

---

## 0. 固定边界

### 0.1 本版负责范围

`v2.0.0` 只做以下 6 项：

1. 建立统一 `policy/profile` 规则层；
2. 补齐浏览器端 `copy budget`；
3. 补齐 CLI 端 `memory budget`；
4. 统一 `final_verdict / release_ready` 语义；
5. 让 `--manifest` 具备 challenge 最小支撑；
6. 补齐测试与 CI 阻断。

### 0.2 本版非目标

1. 不做排行榜前端；
2. 不做远程评测服务；
3. 不改成多产品平台；
4. 不扩展到 `GLB` 或 `spz2glb`；
5. 不新增与主线无关的 UI 大改；
6. 不在本版引入额外下载依赖作为前提。

### 0.3 版本一句话定义

`v2.0.0` 的核心不是新增更多入口，而是把现有双端能力收口成：

`单一规则内核 -> Browser 轻审 / CLI 实审 -> Final Verdict -> CI 阻断 / 本地 challenge 批量复验`

---

## 1. 当前状态摘要

### 1.1 已具备的能力

项目当前已经具备：

1. 浏览器端 `browser_lightweight_wasm_audit` 主链路；
2. CLI 端 `compat-check` 的单文件、`--dir`、`--manifest`、`--handoff` 能力；
3. 统一审查摘要构造层；
4. Web / CLI / smoke tests 的基础覆盖；
5. Browser -> CLI 本地协同的基础证据链。

### 1.2 当前缺口

`v2.0.0` 真正需要补的是收口，不是重构：

1. 浏览器 `copy budget` 未真正接线；
2. CLI 的 `peak_memory_mb / memory_growth_count` 只有 schema，没有稳定采集；
3. `release_ready` 仍存在运行态与 baseline 态双口径；
4. `--manifest` 还缺固定顺序与标签化 corpus 语义；
5. 文档与 registry 仍保留部分旧口径。

---

## 2. 目标交付物

完成后应得到以下交付物：

1. 一套统一的 `policy/profile` 规则定义；
2. Browser / CLI 共用的 budget 判定语义；
3. 单一最终裁决：`final_verdict`、`release_ready`；
4. `dev / release / challenge` 三档模式；
5. 支持 challenge 最小闭环的 `--manifest` 批量评测；
6. 可阻断发布的 `release` profile CI；
7. 与门卫主线配套的最低可用 SPZ 基线数据集工程目录。

---

## 3. 总体实施顺序

### Phase 1：统一规则内核
### Phase 2：补齐 Browser `copy budget`
### Phase 3：补齐 CLI `memory budget`
### Phase 4：统一最终裁决与 readiness 语义
### Phase 5：增强 `--manifest` 以支撑 challenge 模式
### Phase 6：测试、CI、文档与 registry 收口

说明：

- `Phase 1-4` 是主线；
- `Phase 5` 是 `hybrid` 版本成立的必要条件；
- `Phase 6` 负责把“可运行”收口成“可发布”。

---

## 4. Milestone 1：统一规则内核

**目标：** 让 budget 阈值、状态机、裁决逻辑从分散判断变成集中配置。

**Files:**
- Modify: `cpp/include/spz_gatekeeper/audit_summary.h`
- Modify: `cpp/src/audit_summary.cc`
- Modify: `web/spz_gatekeeper.js`
- Test: `cpp/tests/audit_summary_test.cc`

### Step 1：定义三档 profile

固定三档：

- `dev`
- `release`
- `challenge`

每档 profile 统一声明以下 budget：

- `warning`
- `copy`
- `memory`
- `perf`
- `artifact`

### Step 2：冻结统一字段

最终所有模式都应共享：

- `policy_name`
- `policy_version`
- `policy_mode`
- `budgets`
- `issues`
- `next_action`
- `final_verdict`
- `release_ready`

### Step 3：收口判定入口

要求：

- Browser 不再自己定义最终发布语义；
- CLI 不再使用与 Browser 不同的 budget 状态含义；
- 所有 readiness 字段只由统一 builder 生成；
- `wasm_quality_gate` 可保留为兼容层，但不再是唯一出口。

### 完成定义

1. 三档 profile 有固定默认值；
2. 同一 budget 在 Browser / CLI 中状态枚举一致；
3. `release_ready` 只剩单一含义。

---

## 5. Milestone 2：补齐 Browser `copy budget`

**目标：** 浏览器轻审真正具备工程预算门禁，而不只是占位字段。

**Files:**
- Modify: `web/spz_gatekeeper.js`
- Possibly Modify: `cpp/src/wasm_main.cc`
- Test: `tests/wasm_smoke_test.mjs`

### Step 1：定义低成本计数策略

本版只做稳定、可重复的低成本方案，优先统计：

1. JS -> WASM 边界的大对象传递次数；
2. `ArrayBuffer / Uint8Array` 的关键复制次数；
3. 从 bundle 解析到报告生成的核心 copy 路径。

### Step 2：接入 budget 结果

要求：

- `copy_pass_limit` 不再长期为 `not_collected`；
- `copy_budget_wired=true`；
- Browser 侧能给出 `within_budget / over_budget`。

### Step 3：补测试

至少覆盖：

- 正常 bundle 在 `dev` 下不过度误报；
- 超预算时 `release` / `challenge` 不得仍给 `pass`；
- 缺少采集时状态必须显式可见，不允许静默补 0。

### 完成定义

1. Browser 报告出现真实 copy 观测值；
2. smoke test 能稳定复验 copy 预算结果；
3. 浏览器工程门禁不再缺这一维。

---

## 6. Milestone 3：补齐 CLI `memory budget`

**目标：** 把 CLI 已有 schema 变成真实工程指标。

**Files:**
- Modify: `cpp/src/main.cc`
- Modify: `cpp/src/audit_summary.cc`
- Test: `cpp/tests/compat_check_test.cc`

### Step 1：补采集字段

优先采集：

- `peak_memory_mb`
- `memory_growth_count`

### Step 2：明确 fallback 语义

若平台无法稳定采集：

- 必须显式输出 `not_collected` 或 `observed_without_budget`；
- 禁止用 `0` 或伪造数值占位；
- `release / challenge` 下应触发至少 `review_required`。

### Step 3：补测试

至少覆盖：

- 有采集且在预算内；
- 有采集且超预算；
- 无法采集但状态明确可解释。

### 完成定义

1. CLI 报告中的 memory budget 不再是空壳；
2. `compat-check` 单文件和批量模式都能带出 memory 状态；
3. schema 与运行时一致。

---

## 7. Milestone 4：统一最终裁决

**目标：** 把双端协同收口为一条明确证据链。

**Files:**
- Modify: `cpp/src/audit_summary.cc`
- Modify: `cpp/tests/audit_summary_test.cc`
- Modify: `cpp/tests/registry_cli_test.cc`
- Modify: `docs/extension_registry.json`

### 语义分层

- Browser：`bundle_verdict`
- CLI：`artifact_verdict`
- Final：`final_verdict`

### 统一规则

1. Browser 只决定是否进入下一阶段；
2. CLI 负责真实 `.spz` 产物判定；
3. `final_verdict` 由统一 builder 汇总生成；
4. `release_ready` 只能跟随最终汇总，而不是任何单端局部状态。

### 完成定义

1. registry、CLI JSON、Web 报告中的 readiness 语义一致；
2. `handoff` 只增强证据链，不改变 CLI 主责；
3. 不再出现“运行时 pass，但 baseline 仍 false”的双重真相。

---

## 8. Milestone 5：增强 `--manifest` 以支撑 challenge 模式

**目标：** 不做服务端，也要让本地批量评测具备挑战赛雏形。

**Files:**
- Modify: `cpp/src/main.cc`
- Modify: `cpp/tests/compat_check_test.cc`

### Step 1：固定顺序

`--manifest` 模式必须支持稳定顺序，保证多次运行结果可复验。

### Step 2：支持标签化 corpus

建议 manifest 允许最小标签信息，例如：

- `scene_id`
- `group`
- `split`
- `difficulty`

### Step 3：补聚合 summary

批量输出至少应包含：

- 总数
- `pass / review_required / block`
- top issues
- 按标签分组的统计摘要

### 完成定义

1. `challenge` profile 可以在本地单机跑完整批次；
2. 输出顺序稳定；
3. 汇总结果可直接作为评测证据。

---

## 9. Milestone 6：测试、CI、文档收口

**目标：** 让门卫从“功能存在”升级为“工程可发布”。

**Files:**
- Modify: `cpp/tests/audit_summary_test.cc`
- Modify: `cpp/tests/compat_check_test.cc`
- Modify: `cpp/tests/registry_cli_test.cc`
- Modify: `tests/wasm_smoke_test.mjs`
- Modify: `README.md`
- Modify: `README-zh.md`
- Modify: `docs/extension_registry.json`

### Step 1：补 profile 测试

必须覆盖：

- `dev`
- `release`
- `challenge`

### Step 2：补 budget 测试

必须覆盖：

- `copy`
- `memory`
- `perf`
- `artifact`
- budget 缺失 / 未采集 / 超阈值

### Step 3：接入 CI 阻断

建议：

- `dev`：允许 `review_required`
- `release`：关键预算缺失或 `block` 直接失败
- `challenge`：单独 workflow 或专项任务触发

### Step 4：同步文档口径

统一写清：

1. Browser 审的是 `zip` 审查包；
2. CLI 审的是真实 `.spz` 成品；
3. 最终结论以 CLI 汇总结果为准；
4. `release_ready` 是最终态字段；
5. `challenge` 是固定规则下的本地批量评测模式。

---

## 10. P0 / P1 / P2 优先级拆分

## P0（必须完成）

1. 三档 profile 定义；
2. Browser `copy budget`；
3. CLI `memory budget`；
4. 单一 `final_verdict / release_ready`；
5. 核心测试补齐。

## P1（推荐本版完成）

1. `--manifest` 稳定顺序；
2. 标签化 corpus；
3. challenge 聚合 summary；
4. CI 阻断接线；
5. registry / README 口径收口。

## P2（可延后）

1. 更细粒度的 copy 分类；
2. 更多 challenge 统计维度；
3. 更丰富的数据集可视化辅助；
4. 排行榜或远程评测服务。

---

## 11. SPZ 基线数据集并行工作流

`v2.0.0` 除代码主线外，还需要一条并行的数据集收口工作流。

### 11.1 当前状态

当前主目录：

`c:/Users/HP/Downloads/HunYuan3D_test_cases/test_cases/fastgs_ccby4_gs_spz_input`

已有：

- 版本化数据集目录；
- `manifests/`、`reports/` 结构；
- `spz_baseline_pack/pack_spz_baseline.py` 打包脚本；
- 一次 3 样本 eval run。

当前缺口：

1. 只有 3 个有效样本条目；
2. 以最低工程基线 `>=20` 计，还差 17 个 `.ply`；
3. 当前 root 目录下原始 seed `.ply` 可能缺失；
4. `spz_dataset_v4_current` 仍是半成品，manifest 与 coverage 状态未完全一致；
5. provenance 信息仍待补齐。

### 11.2 数据集主线任务

#### D1：恢复现有 3 个 seed
至少先恢复：

- `bear.ply`
- `flowerbed.ply`
- `gardenvase.ply`

#### D2：补足到 20 个场景
以最低发布线为准，先扩到 20 个 3DGS `.ply`。

#### D3：补 provenance
每个场景补：

- `source_url`
- `author`
- `license`
- `attribution`

#### D4：重跑现有打包脚本
入口：

- `spz_baseline_pack/run_spz_baseline_pack.sh`

目标：

- 重建 `spz_dataset_v4_current`
- 让 `dataset_manifest.json`、`scenes_manifest.jsonl`、`coverage_status.json` 一致

### 11.3 数据集验收线

本版最低要求：

1. `scene_count >= 20`
2. `scenes_manifest.jsonl` 非空
3. `reports/*.tsv` 不再只有表头
4. provenance 不再全是 pending
5. `v4_current` 的 manifest 与 coverage 计数一致

---

## 12. 风险与阻塞点

### 风险 1：copy 统计方式过重

处理：
- 本版只做低成本稳定计数；
- 不追求全量 profiler。

### 风险 2：CLI 内存采集平台差异

处理：
- 明确 `not_collected` 语义；
- 禁止伪造数据。

### 风险 3：challenge 需求膨胀

处理：
- 本版仅做到本地批量评测最小闭环；
- 不提前做平台化。

### 风险 4：数据集样本补齐慢于代码主线

处理：
- 数据集与代码主线并行推进；
- 不互相阻塞开发，但发版前一起验收。

---

## 13. 验收标准

全部完成后，应满足：

1. `dev / release / challenge` 三档 profile 可切换；
2. Browser 端 `copy budget` 已接线；
3. CLI 端 `memory budget` 已接线；
4. `final_verdict / release_ready` 只有单一最终语义；
5. `--manifest` 可支撑 challenge 最小批量评测；
6. `release` profile 已接入 CI 阻断；
7. 门卫主线文档、registry、测试口径一致；
8. SPZ 基线数据集达到最低 20 场景工程基线。

---

## 14. 推荐执行策略

推荐的实际施工顺序：

1. 先做规则层收口；
2. 再补 Browser `copy budget`；
3. 再补 CLI `memory budget`；
4. 然后统一最终裁决；
5. 再增强 `--manifest` 与 challenge 模式；
6. 最后统一测试、CI、文档与数据集验收。

工程上建议拆成两条并行主线：

- **主线 A：门卫代码收口**
  - 规则内核
  - budget 接线
  - final verdict
  - tests / CI

- **主线 B：FastGS 基线数据集补齐**
  - 恢复 3 个 seed
  - 新增 17 个 `.ply`
  - 补 provenance
  - 重建 `spz_dataset_v4_current`

---

## 15. 最终结论

`v2.0.0` 不是一次大重构，而是一次高价值的“规则层收口 + 预算补齐 + 数据集落地”。

只要严格控制范围，本版可以在不引入额外平台复杂度的前提下，同时完成三件事：

1. 把门卫变成真正的工程发布生死线；
2. 把门卫变成算法开发/回归测试的稳定内核；
3. 为后续 challenge / benchmark 方向保留直接可扩展的入口。
