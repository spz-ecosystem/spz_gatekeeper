# SPZ Gatekeeper 2.0.3 — 2.0.2 → 2.1 桥接版

> 创建: 2026-05-26 | 状态: 框架草案  
> 定位: 不新增功能。做 2.0.2 尾债清理 + 2.1 plan-refinement 前置准备。  
> 前置: 2.0.2 R6 完成并 merge 到 main。  
> 预计: 1-2 天。

---

## 零、为什么需要 2.0.3

2.0.2 做完后 main 分支状态 vs 2.1 需要的起点之间存在落差：

| 2.1 需要 | 2.0.2 结束时 | 谁做 |
|:---|:---|:---|
| 9-agent constitution 已更新 | ❌ 202 plan 明确"暂缓" | 2.0.3 |
| plan 文档已迁移 | ❌ 只有 202 plan 被 `git add -f` 强推 | 2.0.3 |
| 2.1 migration checklist Q1/Q2 已决策 | ❌ 悬而未决 | 2.0.3 |
| extension_validator.h TLV 残留已修复 | ❌ "下个 PR 统一" | 2.0.3 |
| 202 plan §10.2 审查不纳入项已分类 | ❌ 未做 | 2.0.3 |
| 2.1 计划已根据 Bridge §9 新事实重构 | ❌ 2.1 计划写于 3-4 月，早于 Bridge 集群框架 | 2.1 (不在 2.0.3) |

另外，2.1 计划的相当一部分内容已被 Bridge 吸收：

| 原 2.1 范围 | 现在在哪 | 状态 |
|:---|:---|:---|
| HandoffProtocol dataclass | Bridge §9 M1 | 已定义，待实现 |
| ContextPacket / BudgetSnapshot / LintReport / LedgerEntry / StageEvent / Checkpoint | Bridge §9 G1-G5 + C1 | 已定义，待实现 |
| ClusterContract + register_cluster + JSON 加载器 | Bridge §9 F1-F6 | 已定义，待实现 |
| Serial Task Card Pipeline + spawn_agent | Bridge R8 T30-T31 | 计划中 |
| 4→9 agent owner/handoff/escalation 冻结 | Gatekeeper 2.5 §5 | 待 2.1 完成后 |
| C++ 模块 (dual_audit_orchestrator 等) | 仍在 Gatekeeper | 2.1 本体 |
| Web/WASM 消费层 (handoff/board/CI) | 仍在 Gatekeeper | 2.1 本体 |

结论: 2.1 计划需要**大幅重构**——Bridge 已经拿走了 agent 框架层，Gatekeeper 2.1 只做 C++ 工程 + Web/WASM 消费。但重构之前必须先把 2.0.2 的尾债清掉、constitution 更新、plan 文档就位。

---

## 一、任务列表

### T01: 9-agent constitution 更新 → 对齐 Bridge §9

**为什么现在做**: Bridge constitution.py 的 spz_gatekeeper 集群 section 已经定义了 9 个 agent profile + 4 个集群规划。Gatekeeper 的 `agent_contract_v1.json` 和旧版文档需要对齐到当前实际状态。不更新的话 2.1 plan-refinement 会基于过时信息。

**操作**:
1. 读 Bridge `constitution.py` spz_gatekeeper 集群 section → 导出当前 9-agent 的实际 profile（primary_model, spawn_role, permission_tier, budget_gate）
2. 对照 `docs/agent_contract_v1.json` → 标出差异
3. 更新 contract json 到当前实际状态
4. 更新 2.1 agent infra blueprint 的版本边界声明（注明哪些已移入 Bridge）

**产出**: 更新后的 `agent_contract_v1.json`（v0.4.0） + 2.1 blueprint 版本边界段

### T02: extension_validator.h TLV→ILV 残留修复

**为什么现在做**: 202 R5 做了全局 TLV→ILV 重命名（16 文件），但 `extension_validator.h` 注释中还有 TLV 引用。202 plan §12 明确写了"下个 PR 统一"——这就是下个 PR。

**操作**:
1. `rg -i "tlv" cpp/include/spz_gatekeeper/extension_validator.h` → 定位残留
2. 全部改为 ILV
3. `rg -i "tlv" cpp/` 全局确认零残留（除 legacy path 注释中刻意保留的）

**产出**: 1 commit, extension_validator.h + main.cc + wasm_main.cc 注释

### T03: 202 plan §10.2 审查不纳入项分类

**为什么现在做**: 202 plan §10.2 列了 5 条"不纳入"——有些是真的不需要（MIN-2 命名、MIN-5 include guard），有些有模糊性。需要在 2.1 plan-refinement 之前给每条一个明确的去向。

| 项 | 当前状态 | 2.0.3 决策 |
|:---|:---|:---|
| MIN-2: SpzHeaderV4 vs NgspFileHeader 命名 | 保持门卫风格 | ✅ 不变 |
| MIN-5: include guard 重命名 | tlv.h 已删，问题已消失 | ✅ 已自动解决 |
| MIN-6: WASM export 风格 | R6 不新增 | ✅ 留给 2.1 或 2.5 |
| 遗漏 #3: ComputeBasePayloadSize 废弃 | 标记 legacy，不删除 | ✅ 不变 |
| 遗漏 #5: antialiased flag 验证 | 常量已定义，验证逻辑未实现 | 🔶 → 转到 2.0.3 T04 |

**操作**: 更新 202 plan §10.2，每条标注"2.0.3 决策: 转到 2.1 / 转到 2.5 / 已自动解决 / 不变"

**产出**: 202 plan 更新 1 段

### T04: antialiased flag 验证逻辑

**为什么现在做**: 202 plan R2 T08 已定义 `kFlagAntialiased = 0x01` 常量，但未实现验证逻辑。这是 202 自己的尾债——2.0.2 新增了常量但没写完后续逻辑。体量极小（~15 行 C++），适合 2.0.3 收尾。

**操作**:
1. 在 `ParseHeaderV4()` 或 `InspectSpzBlobV4()` 中加 `if (header.flags & kFlagAntialiased) { ... }` 
2. 将 antialiased 信息写入 `SpzL2Info`（新增 bool 字段或 flags 字段）
3. CTest 加 1 个 case

**产出**: spz.cc +5 行, report.h +2 行, v4_format_test.cc +15 行

### T05: 计划文档迁移

**为什么现在做**: 2.1 migration checklist 有 386 个未跟踪文件 + 15+ plan 文档散落在 gitignored `docs/plans/` 中。2.1 plan-refinement 需要能访问所有历史计划文档。

**操作**:
1. 在门卫项目内创建 `docs/plan-backup/` 目录（已存在则跳过）
2. 将 `docs/plans/` 下的所有 .md 文件复制到 `docs/plan-backup/`（plan-backup 不受 gitignore 影响）
3. 202 plan 本身保持 `git add -f` 方式跟踪（不改已有机制）
4. 更新 202 plan §12.1 的实施前清理清单——补充 plan-backup 已就位的说明

**产出**: plan-backup/ 目录 + 202 plan 更新

### T06: 2.1 migration checklist Q1/Q2 决策

**为什么现在做**: Q1（main 上 468 行 audit_summary 改动归属）和 Q2（web 图片删除意图）阻塞了 2.1 的起点——不先决定哪条路，连 2.1 的第一个 commit 从哪开始都不确定。

**操作**:
1. 读当前 main 分支 `git status` + `git diff` → 确认那 468 行现在的状态
2. 读 `web/cat_left.png` / `web/cat_right.png` 是否存在
3. 如果 468 行改动仍在 main 上未提交 → stash → 切 2.0.3 分支 → commit
4. 如果图片已删除 → 确认有意 → commit
5. 更新 migration checklist → 标注 Q1/Q2 为"2.0.3 已决策"

**产出**: 2.0.3 commit 含 main 悬垂改动 + migration checklist 更新

### T07: GitNexus 全量基线

**为什么现在做**: 2.1 plan-refinement 的 L3 审查需要"当前 GitNexus 拓扑是否支撑目标范围"。2.0.3 结束后跑全量 analyze，作为 2.1 的前置基线。

**操作**:
1. `gitnexus analyze`（所有 202 R6 变更已并入）
2. `gitnexus context` 逐个新增符号 → 死代码检测
3. 记录 nodes/edges/clusters/flows 基线
4. 写入 202 plan 末尾（GitNexus 三项目基线更新）

**产出**: GitNexus baseline 数字 + 死代码审计记录

---

## 二、不做的

| 项 | 原因 |
|:---|:---|
| 运行 CI | 令牌未到位（202 §0.4），2.0.3 改动全是文档/注释/常量，不涉及编译 |
| 重写 2.1 plan | 这是 2.1 plan-refinement（STAGE -1 流程B）的事，不在 2.0.3 |
| 实现 Bridge §9 的 dataclass | 这些在 Bridge R8 里做，不是 gatekeeper |
| WASM 优化 | 202 §12 明确排除，留给 2.5 或 3.0 Phase 4 |
| 大文件拆分 | 202 §1.4.4 明确留给 2.5 |

---

## 三、外部风险与自举依赖

### 3.1 SPZ 上游在 R8-R10 期间出新版本

Niantic v3.0.0 是 latest stable，但可能发新 tag：

| 场景 | 触发 | 响应 | 影响 2.1? |
|:---|:---|:---|:--:|
| 新扩展 (v4.x 加 extension type ID) | SPZ 新 tag，新 `0xADBE0004` 等 | **STAGE -1 流程A**：登记 ExtensionSpec → CMake 更新参考版本 → CI 验证 | ❌ 不阻断。新扩展登记 ~12 行 |
| 破坏性 v5 格式 | header 结构变、压缩算法换 | **STAGE -1 流程B**：完整 plan-refinement 周期 | ❌ 低概率。v4 2025年5月才稳定，v5 至少明年 |
| zstd/zlib CVE | CVE 数据库触发 | **STAGE -1 流程A**：版本号 + SHA256 更新 → CI 验证 | ❌ 不阻断 |

**处理**: 2.0.3 不需要为 SPZ 上游预留代码改动。但需要在 `spz_gatekeeper.yaml` 的 blind_spots 里注册"SPZ v5 破坏性变更"为已识别风险。

### 3.2 gatekeeper 集群自举——用自己的 agent 开发自己的 2.1

SPZ 4 个集群的目标之一是开发 gatekeeper 2.1。但这是经典的狗粮循环：

```
gatekeeper 集群 9-agent ←── 用来开发 ──→ gatekeeper 2.1 C++ 代码
       │                                       │
       └── 全功能需要 Bridge R9 (Gateway 闭环) ──┘
```

**分阶段激活**:

| Bridge R | gatekeeper 集群能做什么 | 能辅助开发 2.1 的什么 |
|:---|:---|:---|
| pre-R8 (现在) | 研究人员集群 (coordinator + technical-analyst + lit-reviewer) 已激活 | L3/L4 交叉审查、SPZ v4 规范研究 |
| R8 完成 | orchestrator → U1-U4 串行调度 + spawn_agent → 4 domain agent 活跃 | upper 7 C++ 模块的编码 + CTest |
| R9 完成 | context-packer/cost-sentinel/schema-linter→contract-guardian 联动闭环 | lower 5 Web/WASM 模块 + 全量 CI/watch |
| R10 完成 | register_cluster + 全 14 spawn_role | 跨集群协作、供应链审计 |

**2.1 plan-refinement 时不假设 agent 全功能**——明确划分：哪段靠人 + IDE，哪段辅助 agent，哪段 agent 主导。

### 3.3 spz-gatekeeper-r-sequence 技能——保留、迁移、新增

**保留** (2.0.3 后继续用):

| 资产 | 用途 |
|:---|:---|
| STAGE -1 路由器 (flows-a-b-c.md) | 依赖升级 + 补丁 + 大版本 plan-refinement — 永不过时 |
| Red Flags + Cross-Stage Invariants | 2.0.2 踩坑资产，每个版本适用 |
| Design Provenance | 标注哪些 pattern 已被 Bridge HL Harness 吸收 |
| game-path-audit.md | gatekeeper 独有，Bridge 不替代 |

**迁移给 Bridge HL Harness** (R8-R10 期间逐步):

| 当前 | 迁移到 | 时机 |
|:---|:---|:---|
| gatekeeper-cli.sh (bash) | 统一 Python CLI + gatekeeper constitution YAML | Bridge R8 后 |
| monitor-schemas/ (JSON) | gate constitution 的 gate schema | 2.1 plan-refinement |
| STAGE 0-5 reference (历史流程) | 归档；2.1 用流程B 重新拆 | 2.1 plan-refinement |

**技能不变的核心**: 路由→门禁→计划质询→收尾。CLI 用什么语言、gate 几段，不影响这四个职责。
| 大文件拆分 | 202 §1.4.4 明确留给 2.5 |

---

## 三、任务汇总

| T# | 事项 | 改动量 | 产出 |
|:--|:---|:---|:---|
| T01 | 9-agent constitution 更新 | 1 文件 +30/-20 | agent_contract_v1.json v0.4.0 |
| T02 | extension_validator.h ILV 残留修复 | 3 文件, ~5 处 | cleanup commit |
| T03 | 202 plan §10.2 审查不纳入项分类 | 1 段 | 202 plan 更新 |
| T04 | antialiased flag 验证逻辑 | 3 文件, ~22 行 | 小功能收尾 |
| T05 | 计划文档迁移 | 15+ 文件复制 | plan-backup/ |
| T06 | migration checklist Q1/Q2 决策 | 1 文件 + 可能的 stash/commit | 解除 2.1 阻塞 |
| T07 | GitNexus 全量基线 | 命令输出 | 基线数字 |

---

## 四、与 2.1 的分界线

2.0.3 完成后的状态:

```
main 分支:
  ├─ 2.0.2 全部 R1-R6 代码 (merged)
  ├─ 9-agent constitution 对齐 Bridge (T01)
  ├─ extension_validator.h 干净 (T02)
  ├─ antialiased flag 验证逻辑完整 (T04)
  ├─ plan-backup/ 可访问 (T05)
  └─ migration Q1/Q2 已决策 (T06)

2.1 的起点:
  ├─ constitution 是最新的 → plan-refinement 不用猜状态
  ├─ plan 文档全部可读 → 流程B 的 L3/L4 审查有完整基线
  ├─ 没有 migration checklist 的悬疑项
  └─ 可以立即进入 STAGE -1 流程B:
      Step B1→B2→B3→B4→B5→B6→B7
      → 产出: 2.1 重构后的 v3 plan + 可执行 R 序列
```
