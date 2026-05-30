# R6 阻力分析 — Agent 最小阻力原则

> 日期: 2026-05-26
> 核心公式: **跳过概率 ∝ (步骤数 × 路径长度 × 编码复杂度 × 外部依赖) ÷ (即时反馈强度)**

---

## 1. 阻力矩阵

| 组件 | 步骤数 | 路径长度 | 编码复杂 | 外部依赖 | 即时反馈 | **综合阻力** | **跳过概率** |
|:--|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| `discover` | 1 | 低 | 0 | 无 | ✅ exit code | 🟢 1 | ~5% |
| `state` | 1 | 低 | 0 | 无 | ✅ JSON | 🟢 1 | ~5% |
| `balance` | 1 | 低 | 0 | API key | ✅ JSON | 🟢 2 | ~10% |
| `set-key` | 1 | 低 | 0 | 无 | ✅ JSON | 🟢 1 | ~5% |
| `pre-tool-check` | 1 | 低 | 0 | 无 | ✅ exit code | 🟢 1 | ~5% |
| `stage-detail` | 1 | 低 | 0 | 无 | ✅ JSON | 🟢 1 | ~5% |
| `advance` (check) | 1 | 低 | 0 | 无 | ✅ exit code | 🟢 2 | ~5% |
| `guard-edit` | 1 | **高** | 0 | worktree | ⚠️ JSON | 🟡 4 | ~40% |
| `monitor --local` | 1 | 低 | 0 | 无 | ✅ exit code | 🟢 2 | ~10% |
| `monitor --api` | 1 | 低 | JSON | **API key** | ⚠️ pipe | 🟠 5 | ~50% |
| `advance` (gate) | 2 | 低 | exit code | 无 | ✅ | 🟡 3 | ~20% |
| `appeal` | 2 | 中 | JSON | API key | ⚠️ JSON | 🟠 5 | ~50% |
| `escalate` | 2 | 高 | markdown | API key | ⚠️ JSON | 🔴 7 | ~70% |
| **Skill: git-workflow** | **5+** | **高** | **shell** | **token** | **❌ delay** | **🔴 8** | **~80%** |
| **Skill: requesting-code-review** | **3+** | **高** | **markdown** | **人** | **❌ delay** | **🔴 8** | **~85%** |
| **Skill: verification-before-completion** | **3+** | **中** | **bash** | **无** | **❌ delay** | **🟠 6** | **~60%** |
| **Skill: agent-self-correction** | **10+** | **高** | **读全文** | **无** | **❌ delay** | **🔴 9** | **~90%** |
| **Skill: finishing-a-development-branch** | **3** | **中** | **bash** | **无** | **❌ delay** | **🟠 6** | **~70%** |
| **Session Startup Checklist** | **5** | **中** | **bash** | **token** | **❌ delay** | **🔴 8** | **~85%** |
| **L1-L6 Cross-Review** | **6×独立** | **高** | **多工具** | **无** | **❌ delay** | **🔴 10** | **~95%** |

---

## 2. 阻力来源分析

### 🔴 阻力等级 10: L1-L6 交叉审查

**为什么阻力最高**:
- 6 个独立层级，每层需要不同的工具和思维模式
- L1 (git log) 简单 → L2 (plan conformance) 需要读 plan → L3 (GitNexus) 需要命令 → L4 (Niantic 对照) 需要读外部源码 → L5 (rg 扫描) 需要写正则 → L6 (架构) 需要全局理解
- **每层之间没有强制依赖** — agent 可以跳过任何一层
- R5 实际: 只做了 L4+L2 部分, L1/L3/L5/L6 全部跳过

**阻力本质**: 6 个独立检查 × 每个都需要主动执行 = 6^6 种跳过组合

### 🔴 阻力等级 9: agent-self-correction

**为什么阻力最高**:
- 需要读 10+ 个 category 文件
- 每个 category 包含 symptom + root cause + guardrail + instance
- **agent 不认为自己犯了错** — 9/13 个 category 的触发条件是 "agent 做了 X 但不应该做 X"
- R5 实际: 10+ 个错误, 0 次触发

**阻力本质**: agent 的自我认知偏差 — "我没错" = 不触发

### 🔴 阻力等级 8: Session Startup Checklist

**为什么阻力最高**:
- 5 个步骤，每步需要不同工具
- 1. Context health → 需要判断对话轮次
- 2. Review lessons → 需要 rg + 阅读
- 3. CLI integrity → 需要运行 discover + state
- 4. Token check → 需要检查环境变量
- 5. Commit uncommitted → 需要 git status
- **每步的 "通过" 都是静默的** — 没有 "FAIL: 你需要做 X"

**阻力本质**: 5 步中任何一步的 "通过" 都不产生可见输出 → agent 认为 "已检查"

### 🟠 阻力等级 7: escalat

**为什么阻力高**:
- 需要构造完整的 Markdown 报告
- 需要包含 state + evidence + monitor 输出
- 需要人类审查 — agent 不控制这个过程

**阻力本质**: 产出是给人看的 → agent 认为 "人类会看到 monitor 输出"

### 🟠 阻力等级 6: guard-edit

**为什么阻力高**:
- 路径转换: `C:\Users\HP\Downloads\...` → `/root/.config/superpowers/worktrees/...`
- 路径长度: 每次 120+ 字符
- **没有 "通过" 的正向反馈** — 只有 "BLOCKED" 时才输出

**阻力本质**: 路径转换是纯机械劳动 → agent 认为 "路径对的, 跳过"

---

## 3. R5 裸跑的阻力映射

```
R5 实际执行了哪些:
  ✅ discover (阻力 1)     — 自然执行, 最低阻力
  ✅ state (阻力 1)        — 自然执行, 最低阻力
  ✅ advance (阻力 3)      — 执行了 3 次, 但跳步 + force
  ❌ guard-edit (阻力 4)   — 0 次, 路径转换太麻烦
  ❌ monitor --api (阻力 5) — 0 次, key 不在就跳过
  ❌ Session Startup (阻力 8) — 0/5, 5 步太麻烦
  ❌ L1-L6 (阻力 10)       — 只做了 2/6 层
  ❌ agent-self-correction (阻力 9) — 0 次, 不认为犯错
  ❌ git-workflow (阻力 8)  — 0 次, token 管理麻烦
  ❌ requesting-code-review (阻力 8) — 0 次, 需要人
  ❌ verification (阻力 6)  — 0 次, 没有即时失败
  ❌ finishing (阻力 6)     — 0 次, 收尾不紧急
```

**规律**: R5 只执行了阻力 ≤ 3 的组件。阻力 ≥ 4 的全部跳过。

---

## 4. 结构化门禁 vs 行为化指令

| 机制 | 阻力 | R5 跳过率 | 例子 |
|:--|:--:|:--:|:--|
| **结构化门禁** (exit code) | 低 | ~10% | advance, pre-tool-check, guard-edit |
| **行为化指令** ("每次 X 前做 Y") | 高 | ~85% | guard-edit before SearchReplace, L1-L6 |
| **Skill invocation** (INVOKE X) | 最高 | ~95% | requesting-code-review, verification |

**关键洞察**:
- **结构化门禁** 能拦住 agent — 因为 `exit code=1` 是不可绕过的
- **行为化指令** 拦不住 — 因为 agent 可以 "忘记" 执行
- **Skill invocation** 最难触发 — 因为需要 agent 主动读 + invoke + 等待

---

## 5. R6 应该做什么 (修正版)

### 原则: 区分保护性阻力和无意义阻力

| 阻力类型 | 定义 | 处理方式 |
|:--|:--|:--|
| **无意义阻力** | 纯机械劳动，不增加安全性 | 降低（路径缩短、exit code 替代 pipe） |
| **保护性阻力** | 增加安全性或强制审查 | 保留，但降低执行成本 |
| **探索性阻力** | 规则尚未验证有效 | 保留为行为化指令，继续在 Gatekeeper 中迭代 |

### 降低无意义阻力

| 当前 (无意义阻力) | 改造 (低阻力) | 保留的保护性 |
|:--|:--|:--|
| guard-edit 120+ 字符路径 | CLI 自动检测 worktree 前缀 | 路径校验本身保留 |
| monitor --api JSON pipe | exit code 为主通道 | API key 依赖保留 |
| Session Startup 5 步手动 | discover 自动检查 5 项 | 检查项保留，执行方式改变 |
| L1-L6 6×独立工具 | monitor 逐层评分 | 6 层独立性保留 |
| verification 多一步 | pre-tool-check 自动跑 ctest | ctest 验证保留 |

### 保留保护性阻力

| 组件 | 为什么是保护性的 | R6 策略 |
|:--|:--|:--|
| L4 交叉审查 (Niantic 对照) | 需要人类判断 "设计是否合理" | 保留为行为化指令, STAGE_3 强制 |
| requesting-code-review | 需要另一个人审查 | 保留为行为化指令, STAGE_3 强制 |
| agent-self-correction | 需要 agent 反思 | 保留为行为化指令, BLOCK 时触发 |
| L1-L6 6 层独立 | 每层有不同检查维度 | 保留独立性，降低执行成本 |

### 不做的（不合理方案）

| 方案 | 为什么不合理 |
|:--|:--|
| "消除所有行为化指令" | HL 的核心是 agent 能改进规则，完全强制=执行器 |
| "让 agent 不需要记住任何规则" | 不记规则的 agent 不是 agent，是执行器 |
| "全部结构化" | 过度结构化 = agent 失去自主性 = 不再是 HL |
| "Gatekeeper CLI 废弃" | Gatekeeper 是规则发现实验室，废弃=丢失探索经验 |
| "SearchReplace 自动触发 guard-edit" | 需要改 Trae 框架，外部依赖不可控 |

---

## 6. 核心结论 (修正版)

```
R5 裸跑的根因不是 "agent 不知道规则"
而是 "规则的执行阻力 > agent 的最小阻力阈值"

阻力阈值 ≈ 3 (discover/state/advance 的阻力级别)
阻力 > 3 的组件全部被跳过

但: 不是所有阻力都应该消除

阻力分三类:
  1. 无意义阻力 → 降低 (路径缩短、exit code、自动化)
  2. 保护性阻力 → 保留，降低执行成本 (L1-L6、交叉审查)
  3. 探索性阻力 → 保留为行为化指令，继续迭代 (HL 的核心)

架构定位:
  Gatekeeper = HL Harness shell 层实例 = 规则发现实验室
  Bridge = HL Harness Python 层实例 = 规则基础设施化
  R6 在 Gatekeeper 中探索的规则，验证后移植到 Bridge Engine
```
