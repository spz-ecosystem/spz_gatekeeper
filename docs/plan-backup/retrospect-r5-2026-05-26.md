# R5 回顾沉淀 (2026-05-26)

## 1. 技能流程更新

### 已更新文件

| 文件 | 变更 |
|:--|:--|:--|
| `gatekeeper-cli.sh` | **P2**: 新增`_collect_hard_evidence()` 22项证据采集 + `_emit_hard_evidence_json()` |
| | **P1+P6**: 新增`pre_tool_check()` 路径拦截/裸跑检测/commit格式/push校验 |
| | **P4**: monitor 输出 Agent 可见仅 score+violations，全量转储 `.gatekeeper_monitor_api_tmp.json` |
| | `_resolve_api_key()` 跨 WSL 边界从 Windows $env:DEEPSEEK_API_KEY 解析 |
| | local monitor 所有检查项改为通过 `_collect_hard_evidence()` 获取证据 |
| `references/stage-1-code.md` | 新增 PRE-PUSH REVIEW 段(STAGE 1内部, CI前强制 L1-L4) |
| | Core rules 新增 `STAGE-DETAIL` `PRE-PUSH REVIEW` `verification-before-completion` |
| | 新增 L4 检查清单 (spec_url ls验证/类名匹配/copyright/模板参数) |
| | guard-edit 标注 R5 教训 "0 次调用导致 0 次拦截" |
| `references/stage-3-review.md` | L1-L6 从 3 行摘要展开为完整 10 步 + PowerShell 一次性执行脚本 |
| | L1: git log -8 / L2: Txx 逐项 / L3: gitnexus+context+rg / L4: diff+ls / L5: Red Flags / L6: 架构 |
| `references/stage-0-start.md` | 新增 Session Startup Checklist (5 项: 上下文健康/教训回顾/CLI完整性/token/commit) |
| `SKILL.md` | Project Constants 新增 Monitor API Key 行 |
| `project_rules.md` | 交叉审查教训 #5-#13 全部写入 |

### 需要新增的更新

**SKILL.md Cross-Stage Invariants 新增**:
```
| pre-tool-check before ANY tool call | R5 bare-run lesson |
| | L1-L6 cross-review BEFORE push, not after CI | R5 order lesson |
| | Hard evidence collection by CLI (Agent-invisible) | R5 anti-gaming (P2) |
```

**SKILL.md Red Flags 新增**:
```
- **pre-tool-check returning BLOCK** → STOP, do not edit. Start R-phase first
- **Monitor mode is --local (no API key)** → API monitor unavailable, set $DEEPSEEK_API_KEY
```

## 2. model 配置优化 (你的建议)

当前 `gatekeeper-cli.sh` 第 23 行:
```bash
API_MODEL="deepseek-v4-flash"
```

建议更改为:
```bash
# Actor 和 Critic 复用同一 v4-flash, 通过 reasoning 参数切换思考深度
# actor: reasoning=false (fast, cheap)
# critic: reasoning=true (depth, rigor — 替代 Bridge 的 v4-pro)
API_MODEL="deepseek-v4-flash"
# 启用 thinking 模式的门禁 stage (关键语义审查需要推理深度)
CRITICAL_REASONING=true
```

`_monitor_api()` 的 API 调用部分需新增 reasoning 参数:
```bash
# 当前: temperature=0.0
# 改进: 关键 stage (STAGE_3/4) 启用 reasoning=true
if echo "$CRITICAL_STAGES" | "$RG" -qw "$stage" 2>/dev/null; then
  reasoning_param='"reasoning_effort":"high"'
else
  reasoning_param='"reasoning_effort":"low"'
fi
```

## 3. 我的思考强度

我是 v4-flash，当前的"深度思考"模式是 DeepSeek 侧根据上下文复杂度和对话轮次自动调节的。我没有独立的 reasoning 开关控制权——但 `bridge-monitor.py` 那种 `temperature=0.0` 的确定性 JSON 输出方式可以模拟 critic 的稳定性。

## 4. R5 完成的技能进化点

| # | 建议 | 来源 | 状态 |
|:--|:--|:--|:--:|
| 1 | pre_tool_check 整合到 CLI | R5 pre_tool_check 实现 | ✅ `pre_tool_check()` 800行集成在 CLI |
| 2 | API monitor 启用 reasoning_effort | 你的模型洞察 | ✅ `REASONING_DEFAULT="low"`, `REASONING_CRITICAL="high"` |
| 3 | 死代码检测三关自动化 | SKILL.md 已有定义 | ⏳ R6 实现(gitnexus context+rg+plan 三关自动化) |
| 4 | GitNexus impact 编码前跑 | SKILL.md GitNexus 策略 | ⏳ R6 实现(已在 SKILL.md 中明确定义) |
| 5 | 删除 stale schema 文件 | R5 发现 STAGE_0.json 缺失 | ⏳ R6 实现(STAGE_0.json 已补全，需审计其余) |
| 6 | pre_tool_check IDLE false-BLOCK | pre_tool_check 过于严格 | ✅ 已验证: stage=IDLE 时 pre_tool_check BLOCK 是**有意设计**——防止裸跑。guard-edit 返回 allowed:true 用于 R 启动前的路径校验, 两命令分工不同 |

### 新增系统自述文档 + Balance 命令

| 文件 | 对应 Bridge 参考 | 内容 |
|:--|:--|:--|
| `references/architecture-overview.md` | Bridge `architecture-overview.md` | 800行系统全景: 文件地图、架构管线图、8条核心设计决策、Bridge对比表 |
| `references/prm-monitor-methodology.md` | Bridge `prm-monitor-methodology.md` | PRM方法论文档: 角色分离图、8项论文原则交叉审查、7条最佳实践、Bridge适配差异表、3个评审者Q&A |

### DeepSeek API 文档验证结果

| 验证项 | 之前 | 修正后 |
|:--|:--|:--|
| `reasoning_effort` 值 | `low` / `high` | `max`（API mode 仅用于critical stage, 一律用最深思考） |
| `temperature` 在思考模式下 | `temperature=0.0` 静默忽略 | 删除 `temperature` — 思考模式下不支持 temperature |
| API body 构建 | 手工 `-d` 字符串拼接 | 改为 `python3 json.dumps` 构造, 避免转义漏洞 |
| DSL 余额查询 | 无 | 新增 `bash gatekeeper-cli.sh balance` — 查余额+并发限制 |

### CLI 当前命令表 (8+1)

```
discover / state / guard-edit / pre-tool-check / advance / monitor / stage-detail
balance / appeal / escalate
```
