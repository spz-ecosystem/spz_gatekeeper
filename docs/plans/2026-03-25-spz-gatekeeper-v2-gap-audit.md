# SPZ Gatekeeper v2 门卫收口计划（修订版，2026-03-25）

> 本文替代“纯问题清单”写法，改为**可执行的收口计划**：明确已解决项、未解决项、剩余差距、执行路径与验收目标。
> 对齐基线：`docs/plans/2026-03-23-spz-gatekeeper-v2-hybrid-implementation-plan.md`（仅覆盖门卫主线，不包含数据集扩充实施细节）。

---

## 1. 目标与范围

### 1.1 目标
把当前 `feature/spz-v2-profile-core` 从“已有骨架”收口到“满足 v2 门卫验收线”的工程状态。

### 1.2 覆盖范围（门卫主线）
- 统一 `policy/profile` 规则层（`dev/release/challenge`）
- Browser `copy budget`
- CLI `memory budget`
- `final_verdict / release_ready` 单一语义
- `compat-check --manifest` 的 challenge 闭环
- 测试、CI、README、registry 收口

### 1.3 非范围
- 排行榜前端、远程评测服务
- GLB/spz2glb 扩展
- 与门卫无关的大规模 UI 改造

---

## 2. 基线与现状对齐

### 2.1 计划基线
- `docs/plans/2026-03-23-spz-gatekeeper-v2-hybrid-implementation-plan.md`
- `docs/plans/2026-03-22-spz-gatekeeper-wasm-audit-modes-design.md`

### 2.2 当前实现范围
- `cpp/include/spz_gatekeeper/audit_summary.h`
- `cpp/src/audit_summary.cc`
- `cpp/src/main.cc`
- `cpp/src/wasm_main.cc`
- `web/spz_gatekeeper.js`
- `cpp/tests/audit_summary_test.cc`
- `cpp/tests/compat_check_test.cc`
- `cpp/tests/registry_cli_test.cc`
- `tests/wasm_smoke_test.mjs`
- `.github/workflows/ci.yml`
- `README.md` / `README-zh.md`
- `docs/extension_registry.json`

---

## 3. 已解决问题（本分支已落地）

> 说明：以下为“已修复/已变更到位”，但不等于整体验收已完成。

### 3.1 manifest item 级 `policy_mode` 透传
- 已在 CLI 审查路径显式传递 `policy_mode` 到 item 级 JSON builder。
- 当前 `RunCompatAuditForPath(...)` 构建 item 输出时传入 `policy_mode`，避免默认回落 `release`。

### 3.2 CLI memory budget 从“观测字段”升级为“影响 verdict”
- 引入 `ResolveArtifactBudgetThreshold(...)`，并在 `release/challenge` 给出 declared 阈值（当前：`peak_memory_mb=256`、`memory_growth_count=1`）。
- `not_collected / over_budget` 会写入 issue，并在必要时把 `pass` 升级为 `review_required`。

### 3.3 compat-board 成熟度快照语义修正
- `compat-board` 快照不再输出“`pass + release_ready=true`”误导组合，当前收敛为成熟度语义：`review_required + false`。
- CI 对应检查已同步成该成熟度口径（仍是快照检查，见未解决项）。

---

## 4. 仍未解决的问题（阻塞/关键缺口）

### 4.1 Phase 1 / 4：`release_ready` 仍未彻底单源化（P0）
- Browser 侧仍存在 legacy 局部语义路径（`final_verdict` 与 `release_ready` 可能由局部输入直接决定）。
- `wasm_quality_gate.release_ready` 仍带兼容层语义，易与“最终发布语义”混淆。

### 4.2 Phase 1 / 2 / 3：统一 declared budget policy 表未完全收口（P0）
- 当前 memory 已部分 policy 化，copy/perf/artifact 尚未在 Browser/CLI 形成同一 policy 表驱动。
- `within_budget / over_budget / not_collected / observed_without_budget` 在多 profile 下仍缺系统性覆盖。

### 4.3 Phase 5：challenge 全链路一致性测试不足（P0）
- 顶层与 item 级口径虽已修复，但仍缺“single/dir/manifest/handoff 全路径一致性”测试矩阵。
- legacy text-scan fallback 过宽，挑战模式主路径约束仍不够严格。

### 4.4 Phase 6：CI 仍是快照校验，不是 profile-aware 发布门禁（P1，若对外发布则升 P0）
- 当前 CI 仅校验 `compat-board` 快照字段。
- 尚未实现 `release/challenge` 实际审查命令的阻断条件。

### 4.5 Phase 6：README/registry 仍需“实现态 vs baseline-only vs 未完成项”分层（P1）
- 文案已同步现状，但尚未形成可直接对外声明“已完成 v2 收口”的证据结构。

---

## 5. Phase 对照状态（门卫主线）

| Phase | 目标 | 当前状态 | 结论 |
| --- | --- | --- | --- |
| Phase 1 | 统一规则内核、三档 profile、单一 readiness | 字段与常量到位；policy 表与单源 readiness 未彻底完成 | 部分完成 |
| Phase 2 | Browser copy budget 真接线并参与门禁 | 已有 copy 观测与 over-budget 报告；profile 级矩阵不足 | 部分完成 |
| Phase 3 | CLI memory budget 真采集+fallback | 已采集且进入 verdict；跨 profile 完整矩阵仍不足 | 部分完成 |
| Phase 4 | 统一 bundle/artifact/final 与 readiness | 结构已到位；Browser/兼容层语义仍需继续收口 | 部分完成 |
| Phase 5 | manifest challenge 固定顺序/标签/聚合 | 主体完成；全链路一致性与 fallback 约束不足 | 大体完成（待收口） |
| Phase 6 | 测试/CI/文档/registry 全收口 | 当前仍以快照验证为主，发布门禁不足 | 未完成 |

---

## 6. 剩余差距（按验收项拆解）

### 6.1 规则层差距
- 统一 policy 表需覆盖 `warning/copy/memory/perf/artifact` 全量维度。
- Browser/CLI 对预算状态枚举与判定动作需完全同构。

### 6.2 语义层差距
- `release_ready` 必须只由最终汇总路径生成；兼容层字段不得反向定义最终态。

### 6.3 测试层差距
- 必须补齐 `dev/release/challenge` x `copy/memory/perf/artifact` x `within/over/not_collected/observed_without_budget`。
- 必须补齐 `single/dir/manifest/handoff` 的 `policy_mode` 一致性断言。

### 6.4 CI 层差距
- 需要真实 `release` gate（关键预算缺失或 `block` 即失败）。
- 需要真实 `challenge` gate（批量 manifest 稳定输出 + 聚合断言）。

### 6.5 文档层差距
- README/registry/compat-board 统一分层：
  1) 已实现；2) baseline-only；3) 未完成；4) 发布判定依据。

---

## 7. 具体执行路径（按优先级）

## 7.1 执行批次 A（P0，先完成）

### A1：规则内核收口（Phase 1）
- 文件：
  - `cpp/include/spz_gatekeeper/audit_summary.h`
  - `cpp/src/audit_summary.cc`
  - `web/spz_gatekeeper.js`
- 动作：
  - 固化三档 profile 的统一 policy 表（含 copy/memory/perf/artifact）。
  - 统一 budget 状态机与判定入口。

### A2：单一 `release_ready` 语义（Phase 4）
- 文件：
  - `cpp/src/audit_summary.cc`
  - `web/spz_gatekeeper.js`
  - `cpp/src/wasm_main.cc`
- 动作：
  - 移除/收敛 Browser legacy 局部 readiness 生成路径。
  - 明确 `bundle_verdict / artifact_verdict / final_verdict / release_ready` 的不可逆关系。

### A3：测试矩阵补齐（Phase 2/3/5/6）
- 文件：
  - `cpp/tests/audit_summary_test.cc`
  - `cpp/tests/compat_check_test.cc`
  - `cpp/tests/registry_cli_test.cc`
  - `tests/wasm_smoke_test.mjs`
- 动作：
  - 补全 profile x budget x 状态矩阵断言。
  - 增加 `policy_mode` 在 single/dir/manifest/handoff 全路径一致性断言。

## 7.2 执行批次 B（P1，紧随其后）

### B1：CI 升级为 profile-aware gate（Phase 6）
- 文件：`.github/workflows/ci.yml`
- 动作：
  - 增加 release gate 任务：运行真实 `compat-check`，关键预算缺失或 `block` 失败。
  - 增加 challenge gate 任务：运行 `--manifest` 样例并断言聚合输出稳定。

  落地步骤（最小改动）
保留现有 build/ctest，不动矩阵。
在 ubuntu-latest 增加两个 gate 步骤（先不扩到 Win/mac，避免壳差异）：
release gate：对单个真实 fixture 跑 compat-check --json，要求 exit code == 0。
challenge gate：生成多样本 + 临时 manifest.json，跑 compat-check --manifest ... --json，要求聚合字段稳定且无 block。
fixture 来源用现有 gen-fixture，不引入新依赖、不下载外部数据。
失败条件明确化：
命令非 0 直接 fail；
JSON 中出现 block 直接 fail；
challenge 聚合缺关键字段直接 fail。
关键障碍与对应清除
障碍：目前只查快照，不查真实审计流。
清除：新增 compat-check gate 步骤替代快照作为主门禁。
障碍：缺 challenge 资产。
清除：CI 内动态 gen-fixture + manifest 生成。
障碍：跨平台 shell 风险。
清除：先只在 Ubuntu gate，后续再平移。

### B2：文档与 registry 收口（Phase 6）
- 文件：
  - `README.md`
  - `README-zh.md`
  - `docs/extension_registry.json`
- 动作：
  - 明确“成熟度看板”与“发布门禁”边界。
  - 给出 v2 剩余缺口与完成判据，不再仅做状态快照。

落地步骤（最小改动）
README 中新增固定小节（中英一致）：
Maturity Board vs Release Gate（边界）
v2 Remaining Gaps（剩余缺口）
v2 Done Criteria（完成判据）
docs/extension_registry.json 增加轻量元信息区（不破坏现有 schema）：
v2_gap_status
v2_done_criteria
release_gate_contract（指向 CI gate 判据）
保持“现状快照”和“发布门禁”分层，不混写：
compatibility_board 只表示成熟度；
release gate 只由 CI 的 compat-check 判定。
关键障碍与对应清除
障碍：现在是状态描述，缺可执行完成定义。
清除：三处同步补 done criteria。
障碍：release_ready 易被误解为最终发布结论。
清除：明确“board readiness ≠ release gate decision”。
执行顺序建议
先改 ci.yml（B1）并本地最小验证；
再改 README.md / README-zh.md / extension_registry.json（B2）；
最后一次性自检并提交，保证变更边界干净。

## 7.3 执行批次 C（P2，可后移）

### C1：handoff 结构化解析替换（P1→P2 过渡项）
- 文件：
  - `cpp/src/audit_summary.cc`
  - `cpp/include/spz_gatekeeper/audit_summary.h`
- 动作：
  - 将 `ParseBrowserAuditHandoffJson` 从“字符串提取”切到结构化 JSON 校验；
  - 固定必填字段（`schema_version/audit_profile/policy_mode/final_verdict/release_ready/issues`）；
  - 缺失或类型错误返回可审计错误码，禁止静默容错。

### C2：版本语义与文档引用收口（P2）
- 文件：
  - `cpp/CMakeLists.txt`
  - `README.md`
  - `README-zh.md`
  - `docs/extension_registry.json`
- 动作：
  - 对齐版本口径（工程版本、策略版本、文档声明一致）；
  - 修复不存在的计划文档引用，避免审计断链；
  - 明确 `compat-board` 是成熟度视图，不是发布门禁结论。

### C3：双端协同回归脚本固化（P2）
- 文件：
  - `tests/wasm_smoke_test.mjs`
  - `.github/workflows/ci.yml`
- 动作：
  - 固定 browser→handoff→CLI roundtrip 断言模板；
  - 固定三项一致性断言：`final_verdict`、`release_ready`、`policy_mode`。

---

## 8. P0-P2 修补验收门禁（完成判定）

满足以下全部条件，才可宣称“门卫侧符合 v2 计划”：

1. **P0 完成**：
   - Browser `copy_breakdown` 统计链完整且可复验；
   - `challenge` 禁止无开关 text-scan fallback 绕过；
   - 本地最小双端回归（CLI + WASM + handoff）可执行。
2. **P1 完成**：
   - handoff 结构化校验落地；
   - 预算状态机在 `dev/release/challenge` 语义一致。
3. **P2 完成**：
   - 版本与文档口径一致；
   - README/registry/CLI/Web 声明边界一致。

### 8.1 验收前检查 Checklist

#### A. P0（功能正确性）
- [ ] `web/spz_gatekeeper.js` 中 `copy_breakdown` 按阶段输出稳定且总量守恒。
- [ ] `cpp/src/main.cc` 中 challenge 默认不走宽松 text-scan fallback。
- [ ] `tests/wasm_smoke_test.mjs` 可完成 browser→handoff→CLI roundtrip。

#### B. P1（鲁棒性与一致性）
- [ ] `ParseBrowserAuditHandoffJson` 已改为结构化校验。
- [ ] `within_budget / over_budget / not_collected / observed_without_budget` 四态在三档 profile 下有断言覆盖。
- [ ] `single/dir/manifest/handoff` 四路径 `policy_mode` 输出一致。

#### C. P2（语义与文档）
- [ ] 版本口径在 CMake/README/registry 中一致。
- [ ] 文档无失效引用，验收链路可追溯。
- [ ] 对外口径严格区分“成熟度看板”与“发布门禁”。

#### D. 发布门禁
- [ ] CI 运行真实 profile-aware gate（非仅快照）。
- [ ] `release` 遇关键预算缺失、over-budget 或 `block` 会真实失败。

> 只有当 P0/P1/P2 与本 Checklist 同时满足，才能发起最终验收。

---

## 9. 交付口径（对外声明约束）

在第 8 节全部达成前，只能使用以下口径：
- “v2 门卫骨架已完成，正在进行规则层与发布门禁收口”；
- 不能声明“v2 已完成可发布”。

达到第 8 节后，才可升级口径为：
- “v2 门卫主线已收口，具备可阻断 CI 的发布门禁能力”。
