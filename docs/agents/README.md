# SPZ Agent 快照

## 概述

本目录包含 SPZ Gatekeeper 2.1 体系中 9 个 agent 的定义文件快照。这些文件是 `~/.codebuddy/agents/` 目录下对应文件的副本，用于版本控制和项目内参考。

## 文件列表

| 文件名 | Agent ID | 角色 | 模型 |
|--------|----------|------|------|
| `SPZ 主控编排代理（SPZ Prime Controller）.md` | spz-prime-controller | 主控编排 | GPT-5.4 |
| `SPZ Context Packer.md` | spz-context-packer | 上下文压缩 | mimo2.5 |
| `SPZ Schema Linter.md` | spz-schema-linter | 结构校验 | mimo2.5 |
| `SPZ Cost Sentinel.md` | spz-cost-sentinel | 预算门禁 | mimo2.5 |
| `SPZ Decision Recorder.md` | spz-decision-recorder | 记录写入 | mimo2.5 |
| `SPZ Contract 守门代理（SPZ Contract Guardian）.md` | spz-contract-guardian | 契约守门 | GPT-5.4 |
| `SPZ Artifact Builder 守门代理（SPZ Artifact Builder Guardian）.md` | spz-artifact-builder-guardian | 产物守门 | mimo2.5 |
| `SPZ 上半实施代理（SPZ Upper Implementer）.md` | spz-upper-runtime-integrator | 上半实施 | GPT-5.4 |
| `SPZ 下半实施代理（SPZ Lower Implementer）.md` | spz-lower-evidence-integrator | 下半实施 | GPT-5.4 |
| `SPZ Research Coordinator.md` | spz-research-coordinator | 研究协调 | mimo2.5 |
| `SPZ Technical Analyst.md` | spz-technical-analyst | 技术分析 | mimo2.5 |
| `SPZ Literature Reviewer.md` | spz-literature-reviewer | 文献综述 | mimo2.5 |
| `SPZ Research Agent Schema.md` | - | 研究代理规范 | - |

## 快照来源

- **源目录**: `~/.codebuddy/agents/`
- **快照时间**: 2026-05-06
- **快照版本**: 对应 agent_contract_v1.json v0.3.0

## 同步规则

1. **单向同步**: 从 `~/.codebuddy/agents/` 向本目录同步，不反向同步
2. **手动同步**: 修改 agent 定义后，需手动复制到本目录
3. **版本标记**: 每次同步时更新本文件的"快照时间"
4. **差异检查**: 同步前建议使用 diff 检查变更内容

## 使用场景

1. **版本控制**: 追踪 agent 定义的变更历史
2. **项目参考**: 在项目内快速查看 agent 定义
3. **团队协作**: 共享 agent 配置标准
4. **迁移参考**: 作为 agent 迁移的基准文件

## 注意事项

1. **文件名编码**: 文件名包含中文和特殊字符，确保 git 正确处理
2. **路径引用**: agent 定义中的路径引用基于 `~/.codebuddy/` 结构
3. **模型配置**: 模型名称（如 GPT-5.4、mimo2.5）需与实际环境匹配
4. **工具权限**: 工具列表和权限配置需与 CodeBuddy 环境一致

## 相关文件

- **Agent 契约**: `docs/agent_contract_v1.json`
- **执行覆盖**: `docs/plans/2026-04-16-spz-gatekeeper-2.1-execution-overlay.md`
- **迁移清单**: `docs/plans/2026-05-01-spz-gatekeeper-2.1-migration-checklist.md`

## 更新记录

- **2026-05-06**: 新增3个研究代理（Research Coordinator、Technical Analyst、Literature Reviewer）和研究代理规范
- **2026-05-05**: 初始快照，从 `~/.codebuddy/agents/` 复制 9 个 agent 定义文件