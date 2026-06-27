<!-- META
target_repo: skill
project_type: standard
worktree_needed: true
plan_id: batch8-python-migration
created: 2026-06-22
updated: 2026-06-27
authors: human + gatekeeper ECO loop + git-workflow F8.1
status: in_progress (R-T5.6+ F8.1 Phase 4 pre-req completed, R-T6 Phase 4 pending)
phases: R-T1 R-T2 R-T3 R-T4 R-T5 R-T5.5 R-T6 R-T7
commit_format: batch8:
review: L1-L7 cross-review each phase
-->

# Batch 8: Bash heredoc Python → 独立 scripts/gk_*.py 迁移计划

## R 相位映射

```
R-T1: Phase -1  (Baseline Capture)            ✅ 完成
R-T2: Phase 0   (gk_state.py + pytest 框架)   ✅ 完成
R-T3: Phase 1   (gk_adjudicate.py)            ✅ 完成
R-T4: Phase 2   (gk_monitor_api, gk_appeal,   ✅ 完成
                 gk_escalate_resolve, gk_reflect)
R-T5: Phase 3   (gate extraction + perf +      ✅ 完成
                 routing fix)
R-T5.5: Phase 3.5 (修复 + 压测 + 模板 + 提取)   ✅ 完成
                3.5a ✅  3.5b ✅  3.5c ✅  3.5d ✅  3.5e ✅  3.5f ✅  3.5g ✅  3.5h ✅
                3.5i: provider/gitnexus/jwt_signing 搬 Python 完成 (3自检+6测试)
                F8 事故补测: 11 tests, HOOK 3层防线验证通过
                RED 引擎 3 新阶段: RED-HINT/HOOK/PY (8 阶段全链路)
                RED 攻防: 28 发现 → 修复后 17, HIGH=0
                advance hint 结构修复: RED-HINT/HOOK/PY checklist + gates + AUDIT-*/ECC-* checklist
R-T5.6: gitnexus 调用统一化                      ✅ 完成
                P1(9 status) → P2(4 detect-changes) → P3(5 ctx/impact/query)
                P4(L3010 保留) → P5(L1031 转换) + L2417 原生调用
                总计 18 处 powershell.exe → _run_gitnexus()
                _run_gitnexus 加固: node.exe 3路径 fallback, detect-changes --skip-git, exit 1 on missing
                综合测试: 17/17 通过, 平均 317ms/次
R-T5.6+: F8.1 远端验证门禁 + git-workflow 增强       ✅ 完成
                A: _verify_remote + _ALLOWED_REMOTE_PATTERNS (gatekeeper-cli.sh)
                B: current_remote in advance hint JSON
                C: RunCommand in .trae/settings.json PreToolUse matcher
                git-workflow.sh v2.0.1: push gatekeeper CLI 委托 (L101-112)
                git-workflow.sh v2.0.1: pr-fallback 独立白名单检查 (L160-178)
                pre_tool_hook.py: STATE_FILE 环境变量优先 (L32-35)
                测试: 245/245 passed (新增 7 F8.1 项)
                评分: plan-refinement 83.8/100
R-T6: Phase 4-7 (audit + advance + finish)         ⏳ 待开始
R-T7: 收尾      (清理 + cron 调度器 +             ⏳ 待开始
                + RED 红队增强)
```

### R 序列执行流程

```
每 R 开始:  discover + guard-edit + pre-tool-check
每 R 编码:  Write/Edit → pytest → commit
每 R 审查:  L1-L7 cross-review (subagent via CLI gatekeeper-cli.sh monitor --subagent)
每 R 审计:  audit --all (state + docs + memory + crossr)
每 R 红队:  RED-SCOPE → SURFACE → CONSTRAINTS → EXPLOIT → POSTEX → REPORT
每 R 结束:  advance R-T(N+1)
```

## 目标

将 `gatekeeper-cli.sh`（~5400 行 bash）中的内嵌 Python heredoc 代码提取为独立 `scripts/gk_*.py` 文件，消除 CRLF/2>/dev/null/引号转义三大老问题。

## 架构

```
迁移前:
  gatekeeper-cli.sh (5400 行)
    ├── bash 函数 (case/shift 路由)
    ├── 内嵌 python3 -c "..." (heredoc, 2>/dev/null, CRLF 风险)
    └── 直接 curl API 调用

迁移后:
  gatekeeper-cli.sh (~600 行, 薄壳路由)
    ├── scripts/gk_adjudicate.py    (adjudicate 逻辑)
    ├── scripts/gk_appeal.py        (appeal 独立逻辑, 不在 monitor 内)
    ├── scripts/gk_monitor_api.py   (LLM API 统一入口: monitor/ci-diagnose)
    ├── scripts/gk_escalate_resolve.py (escalate + resolve)
    ├── scripts/gk_reflect.py       (reflect)
    ├── scripts/gk_audit_state.py   (17 项状态检查)
    ├── scripts/gk_audit_docs.py    (4 项文档检查)
    ├── scripts/gk_audit_crossr.py  (跨 R 分析)
    ├── scripts/gk_audit_depth.py   (GitNexus 死代码检测)
    ├── scripts/gk_advance.py       (advance 逻辑, 最复杂)
    └── scripts/gk_state.py         (state 读写 + SHA256 checksum, 共享库)

通信协议:
  bash → Python: sys.argv 传参, stdin 传 JSON
  Python → bash: stdout 输出 JSON, exit 0=OK / 1=ERROR
  Python → Python: --out-file 文件传递 (bridge 模式)

格式: JSON (not YAML) — 与 Gatekeeper 现有生态一致
```

## 27 命令迁移决策表

```
migrate (14): adjudicate appeal escalate resolve reflect
              monitor_api(state) audit(docs) audit(crossr) audit(depth)
              advance audit(report)

keep bash (8): discover state guard-edit pre-tool-check
               stage-detail push docgen search

already Python (5): cron replay context trace attribution
                  (cron_scheduler.py _replay.py subagent_context auto-inject)

delete (0): none — 所有命令保留
```

## 模板: bridge-cli.py 可复用模式

```
直接复用 (4):
  ✅ JSON stdout + exit code 协议
  ✅ --out-file 子进程通信 (绕过 pipe 编码)
  ✅ sys.argv 命令路由
  ✅ set-key 集中管理

适配复用 (3):
  ⚠️ SHA256 checksum (算法一致, 适配 gate_data 结构)
  ⚠️ Hard evidence 收集 (适配 27 命令的 stage 映射)
  ⚠️ Transition rule table (advance 的 6 阶段规则)

不适用 (3):
  ❌ YAML state → 用 JSON
  ❌ discover 模式 → 保持 bash
  ❌ 中断协议 → nice-to-have, 非 now
```

## Phase 划分

```
Phase -1: Baseline Capture
  目标: 迁移前捕获所有 27 个命令的 bash 输出基线
  产出: scripts/test_baselines/ (JSON snapshots)
  测试: 无 (纯采集)

Phase 0a: gk_state.py (基础设施)
  目标: 独立 state 读写 + SHA256 checksum 计算
  模板: bridge-cli.py _read_state / _write_state
  格式: JSON
  测试: test_gk_state.py (4 scenarios)

Phase 0b: .gitattributes + CRLF 标准化
  目标: 创建 .gitattributes 禁止 CRLF 转换
  内容: *.py text eol=lf, *.sh text eol=lf, *.json text eol=lf
  测试: 无

Phase 0c: pytest 框架搭建
  目标: 搭建 pytest + mock 基础设施
  产出: scripts/conftest.py, scripts/test_regression_harness.py
  测试: 自举

Phase 1: gk_adjudicate.py (修复 ECO.03)
  目标: 替换 cmd_adjudicate 中的内嵌 Python
  输入: --verdicts '...' --score 0.xx --decision '...' --concessions '...'
  输出: {"ok":true,...} + 写入 human_calibration_log
  单元测试: test_gk_adjudicate.py (4 scenarios: PASS/WARN/BLOCKED/异常)
  回归测试: 对比 baseline
  红队: ECO.01 验证

Phase 2a: gk_monitor_api.py (monitor + ci-diagnose 统一)
  目标: 统一 LLM 调用入口
  输入: --role monitor|diagnose --input-file <JSON>
  输出: {"overall_score":...,"checks":[...]}
  强制: 不包含 appeal (appeal 独立)
  单元测试: test_gk_monitor_api.py (mock API, 4 scenarios)

Phase 2b: gk_appeal.py (appel 独立)
  目标: appeal 独有调解逻辑
  输入: --gate ECO-AUDIT --appeal-notes-file <path> --mediation-context-file <path>
  独有逻辑: mediation_context (agent vs human divergence) + F1-F7 verdict 对比
  单元测试: test_gk_appeal.py (6 scenarios: clean/foreign_content/already_appealed/denied_permanent/mediation/API_unavailable)
  降级链: API → local structural → fail-closed

Phase 2c: gk_escalate_resolve.py + gk_reflect.py
  目标: escalate + resolve + reflect
  单元测试: 各 4 scenarios

Phase 3: cmd_advance 内联 Python 提取 + 性能优化 + 路由修复
  ⚡ 紧急 (P0): ECO-EDIT hint 路由修复
    根因: advance hint available 列表只能包含 CLI 命令
          plan-refinement 是 skill (brainstorming→plan-refinement→writing-plans)
          不在 CLI 命令注册表中，agent 看不到路由
    修复: hint 中新增 skill 调用指令，建立 CLI↔skill 命名空间桥接
    参考: meta-routing-protocol.md L143-164 双循环飞轮 — 计划/执行首尾衔接，不互嵌

  🔧 核心 (P1): cmd_advance 5 个内联 python3 块 → scripts/gk_gate_*.py
    (1) _state_checksum_verify  → gk_gate_checksum.py     (防篡改 BLOCK)
    (2) project_type read       → gk_gate_project_type.py  (standard/standing)
    (3) appeal_required gate    → gk_gate_appeal.py        (上诉门禁)
    (4) adjudicate gate         → gk_gate_adjudicate.py    (裁决门禁)
    (5) ECC incremental gate    → gk_gate_ecc.py           (ECC 增量)
    风险: 所有内联块通过 $STATE_FILE bash 变量与 shell 上下文耦合
          提取后通过 sys.argv[1] 传 STATE_FILE 路径（非环境变量）
    验证: checksum 防篡改回归测试 (tampered state → BLOCK, clean → PASS)

  ⏱️ 性能 (P2): 合并 python3 调用
    当前: 5 次独立 python3 spawn → ~4.2s per advance
    目标: 单次 python3 -c 处理所有 gate → <1.5s per advance
    验证: time advance ECO-READ 基准测量

Phase 4: audit 子命令 (已扩展 — 含 batch8 3.5 新增审计项)
  Phase 4a: gk_audit_state.py (17 项状态检查)
  Phase 4b: gk_audit_docs.py (4 项文档检查)
  Phase 4c: gk_audit_hint.py (新增 — 4 项 advance hint 审计)
    AUDIT-HINT/HINT-01~04: checklist 11 阶段覆盖/tool_policy/post_check/gates
  Phase 4d: gk_audit_self.py (新增 — 4 项自检审计)
    AUDIT-SELF/SELF-01~04: run_all 注册/try-except/bash for loop/gitnexus 路径
  Phase 4e: gk_audit_hook.py (新增 — 4 项 hook 配置审计)
    AUDIT-HOOK/HOOK-01~04: matcher 覆盖/WSL 路由/timeout/exit code
  Phase 4f: gk_audit_red.py (新增 — 8 项 RED 引擎审计)
    AUDIT-RED/RED-01~08: scope~py 全部阶段正确性
  注: Phase 4c-4f 可合并到 gk_audit_state.py 作为扩展检查链，不单独建文件。
  总计: Phase 4 覆盖 17+4+4+4+4+8 = 37 检查项 (原 17→37，+117%)

Phase 5: gk_audit_crossr.py (cross-R analysis)
Phase 6: gk_audit_depth.py (GitNexus dead code)

Phase 7: gk_advance.py (最复杂) + advance hint 更新
  目标: advance 逻辑迁移 + 3.5d hint 扩展
  hint 更新含:
    - RED-HINT/HOOK/PY 三阶段 checklist 条目
    - RED 过渡 gates (HINT→HOOK→PY→REPORT)
    - tool_policy 引用 gitnexus 统一调用
    - 注释从 "3.5d" 更新为 "3.5d+3.5i+RED 扩展"

══════════════════════════════════════════════════════════════
Phase 3.5: 缺陷修复 + 压测加固 + 结构化提示词 (2026-06-24 追加)
══════════════════════════════════════════════════════════════

  ✅ 3.5a: checksum 多写者修复 (FS3)
    根因: 20 处 STATE_FILE 直接 json.dump，仅 2 处调用 _state_checksum_write
    修复: _state_write() 追加 _state_checksum_write → 所有写入自动更新 checksum
    验证: ECO-EDIT advance 零 TAMPERED 误报, 109/109 tests ✅

  ✅ 3.5f: Ed25519 密钥无限旋转修复 (E1)
    根因: cmd_advance L1671 检测到 context_ed25519 过期 → rotate
          L4665 key_type="${2:-context}" 不 strip "_ed25519" 后缀
          → meta["context_ed25519_ed25519"] 新条目, 旧条目永不过期
          → 每次 advance 重复检测同一过期条目 → 无限旋转 (32 .bak 文件)
    修复: L4665 追加 key_type="${key_type%_ed25519}" strip 后缀
    验证: ECO-EDIT advance 无 rotate 输出, key-meta.json 正确覆盖

  ✅ 3.5g: RED 阶段 gitnexus 修复 (E2)
    根因: RED-SURFACE 重复 gitnexus analyze (ECO-SCAN 已有基线)
          RED-CONSTRAINTS/EXPLOIT/POSTEX 硬编码 -r spz_gatekeeper
          不适用于 skill worktree (target_repo=skill)
    修复: RED-SURFACE 替换为缓存声明 (复用 ECO-SCAN 基线)
          _run_gitnexus 新增 _GA_GITNEXUS_REPO 全局变量
          RED 阶段调用点: -r spz_gatekeeper → -r "$_GA_GITNEXUS_REPO"

  ✅ 3.5b: 红队静态攻击引擎组装 (2026-06-25 完成)
    背景: RED 架构 3 层已存在 (CLI 编排 + 静态引擎 + 动态引擎)，但层间未联动。
          16 个 _r7_redteam_*.sh 脚本 (~150 攻击向量) 独立执行，不参与 advance 管道。

    已完成:
      ① red_constraints() 函数 ✅ (cbce46e)
        → _red_attacks.py L96-127: 枚举 PROTECTED_REGION + gate_level + appeal_denied
        → 结果: 1 PROTECTED_REGION(gate), 1 gated + 14 ungated commands

      ② 静态→动态引擎联动 ✅ (cbce46e)
        → gatekeeper-cli.sh L4996-5006: context --update 运行 _red_attacks.py all
        → 18 red_findings 注入 subagent-context.json
        → LLM subagent 启动时自动获得静态引擎 findings

      ③ subagent-context.json 修复 ✅ (cbce46e + 829654b)
        → Bug 1: gatekeeper-cli.sh L4954: os.environ.get('SKILL_DIR','') → '$SKILL_DIR'
          根因: SKILL_DIR 是 bash 变量未 export，Python 子进程读到空字符串
          效果: context --update files:1 → files:11
        → Bug 2: gatekeeper-cli.sh L5002: print(sig) → base64.b64decode(sig)
          根因: 签名存 base64 文本，openssl pkeyutl -sigfile 期望二进制
          效果: context 验证 INVALID → valid JSON
        → Bug 3: worktree context 从 master 复制 + 重新签名

      ④ batch 脚本集成 — 跳过
        → I9 回写 (L1993) 已集成 _red_attacks.py all
        → 16 个 batch 脚本保持独立 (测试脚本，非生产代码)

      ⑤ subagent 角色 — 低优先级跳过
        → 4 个 RED 角色 (scanner/exploiter/auditor/reporter) 已够用

      Ed25519 密钥清理 (2026-06-25):
        → context_ed25519_ed25519 双后缀条目已从 key-meta.json 删除
        → context_ed25519 已轮换 (expires 2026-06-26)
        → integrity hash 已更新
        → key-rotate-check cron job 已存在 (every 1d, run_count=0)
        → 密钥路径: /mnt/c/Users/HP/.trae/skills/spz-gatekeeper-r-sequence/.keys/

  ✅ 3.5c: 集成测试 (2026-06-25 完成)
    目标: 验证 batch8 全部修复后完整 ECO 循环无功能漂移
    实现: scripts/test_eco_integration.py + test_red_attacks.py + test_replay.py
    结果: 178 passed, 0 failed, 69 new tests

    测试覆盖:
      test_eco_integration.py (38 tests):
        - AdvanceHintInjection: checklist/tool_policy/post_check (5 tests)
        - HookConfig: wsl bash -c + no CLAUDE_PROJECT_DIR + matcher (6 tests)
        - JsonLoadSafety: 4 files × 2 checks (8 tests)
        - RunGitnexusFix: no powershell + node.exe + wslpath (3 tests)
        - DeadCodeCleanup: ev_gitnexus_nodes_edges + rt_status (2 tests)
        - SubagentGitnexusGk: no powershell + node.exe + wslpath (1 test)
        - SelfCheckEnvIntegrity: HMAC/API/SUBAGENT_ROLE/NODE (5 tests)
        - PreToolHookGate0: keyword blocking (2 tests)

      test_red_attacks.py (35 tests):
        - red_scope: 7 tests (unsigned/signed/sealed/advance_log/missing)
        - red_surface: 3 tests (gate_level/no_gate/no_cli)
        - red_constraints: 4 tests (regions/gates/appeal_denied)
        - red_exploit: 5 tests (checksum/injection/clean/missing)
        - red_postex: 5 tests (unsigned/signed/entropy/missing)
        - red_report: 4 tests (no_findings/evidence_hash/severity)
        - PhaseAll: 2 tests (all_phase/individual)
        - FilePatterns: 2 tests (no_bare_open/uses_with)
        - integration: 3 tests (json_load/with_statement)

      test_replay.py (5 tests):
        - valid_history/empty_history/missing_state/no_args/entropy_rate

    Hook 消融测试 (2026-06-25):
      Gate 0 (Task 拦截):
        stdin 传递:     ✅ length=95, content 正确
        关键词匹配:     ✅ ['gatekeeper'] 命中
        hook 输出:      ✅ EXIT=2, "GATEKEEPER-BLOCK"
      Gate 3 (路径拦截):
        SearchReplace:  ✅ EXIT=2, "NOT in active worktree"
      Read 工具:        ✅ EXIT=0 (不拦截)
      非 gatekeeper:    ✅ EXIT=0 (不拦截)

    验证: python3 -m pytest scripts/test_*.py -q → 178 passed
    回归: python3 -m pytest scripts/test_*.py -q → 178 passed
    触发: python3 -m pytest scripts/test_eco_integration.py -v

    质询 (F1-F7):
      F1 范围: PASS — ECO 循环是 batch8 核心路径,必须验证
      F2 依赖: WARN — 依赖 state 文件写入 + CLI advance 逻辑
      F3 假验证: BLOCKED — 当前只有单元测试,无端到端集成测试
      F4 绕过: PASS — 用 pytest + subprocess 调 CLI,不绕过任何组件

    测试场景清单:
      T3.5c-01: ECO 完整循环 (SCAN→READ→EDIT→AUDIT→DOC→SCAN)
        → 每阶段 advance + discover 验证 stage 转换
        → 验证 checkpoint 写入 (3 处 state write)

      T3.5c-02: RED 完整循环 (SCOPE→SURFACE→CONSTRAINTS→EXPLOIT→POSTEX→REPORT)
        → 每阶段 advance + _red_attacks.py 验证 findings 输出
        → 验证 red_constraints() PROTECTED_REGION + gate_level 枚举

      T3.5c-03: ECO→RED 联动
        → ECO-AUDIT 后触发 RED-SCOPE
        → 验证 subagent-context.json 包含 red_findings

      T3.5c-04: Ed25519 密钥生命周期
        → advance 后检查 key-meta.json 无双后缀条目
        → 验证 context --update 签名有效

      T3.5c-05: checksum 完整性
        → ECO-EDIT advance 后验证 _checksum 非空
        → 手动修改 state → advance 应报 TAMPERED

      T3.5c-06: env-check self-check
        → self-check 输出包含 env_integrity 检查项
        → 验证 HMAC_KEY=MISSING 告警

      T3.5c-07: gitnexus 基线
        → monitor --gitnexus STAGE_0 返回 1,265+ nodes
        → context/impact/query 返回非空结果

      T3.5c-08: 结构门禁
        → IDLE/DONE stage 下 advance 应 BLOCKED
        → 无 active R-phase 时 guard-edit 应 BLOCKED

      T3.5c-09: json.load 资源泄漏
        → 验证 _red_attacks.py + _replay.py + sniffnet_webhook.py
          全部使用 with open() 语句 (grep 验证)

      T3.5c-10: pre_tool_hook 配置
        → .trae/settings.json 包含 wsl python3 (非 $CLAUDE_PROJECT_DIR)
        → matcher 包含 CreateFile|DeleteFile

    验证: python3 -m pytest scripts/test_eco_integration.py -v
    回归: python3 -m pytest scripts/test_*.py -q (109+ tests)

  ✅ 3.5d: 结构化提示词模版 (2026-06-25 完成)
    实现: gatekeeper-cli.sh L2012-2036 (cmd_advance hint_json 注入)
    结果: 11 个阶段各有独立 checklist + 全局 tool_policy + post_check
    背景: pre_tool_hook.py (Gate 0-4) 在 .trae/settings.json 中已配置 Task/SearchReplace/Write
          钩子，但 CLI 层面的提示词引导也是关键防线。agent 多次绕过 skill 直接调 Trae 
          Task tool → 需要 advance JSON 中嵌入强制性引导字段。

    **Hook 发现 (2026-06-25):**
      .trae/settings.json L2-25: PreToolUse hooks for Task + SearchReplace/Write/Edit
      → command: cd "$CLAUDE_PROJECT_DIR" && python3 scripts/pre_tool_hook.py
      → trae 已支持 hooks, Gate 0 拦截逻辑正确
      → 但 $CLAUDE_PROJECT_DIR + python3 在 Windows 环境可能不正确
      → 已验证: hooks 从未实际 BLOCK 过 Task 调用
      → 风险: F8 记录到风险表

    实现方式 — 不修改 SKILL.md，直接在 advance JSON hint 中注入:

    hint.checklist — 强制逐项确认 (Phase 入口):
    ```
    {"phase":"ECO-EDIT","desc":"编辑阶段",
     "checklist":[
       "□ gatekeeper-cli.sh discover (同步状态)",
       "□ gatekeeper-cli.sh guard-edit <file> (权限检查)",
       "□ gatekeeper-cli.sh pre-tool-check (门禁验证)",
       "□ gatekeeper-cli.sh advance ECO-EDIT (进入编辑模式)",
       "□ pytest (测试通过后再 commit)"
     ],
     "available":[...],"next":"ECO-AUDIT"}
    ```

    hint.tool_policy — 工具选择强制映射:
    ```
    {"tool_policy":{
       "分析代码结构": "gatekeeper-cli.sh search --repo skill <pattern>",
       "代码审查": "gatekeeper-cli.sh monitor --subagent \"<task>\"",
       "禁止": "Trae Task tool for gatekeeper code analysis",
       "阅读文件": "优先 gitnexus context (不截断) > rg grep > Read"
     }}
    ```

    hint.post_check — 操作后自检:
    ```
    {"post_check":[
       "bash gatekeeper-cli.sh self-check (env/gitnexus/key/tests)",
       "如果 gitnexus unavailable → 检查 NODE_PATH"
     ]}
    ```

    hint.gate — 序列完整性门禁:
    ```
    {"gates":{
       "ECO-EDIT→ECO-AUDIT": "必须先 adjudicate",
       "ECO-AUDIT→ECO-DOC": "必须先 audit --all"
     }}
    ```

    实现位置: gatekeeper-cli.sh cmd_advance L1904-1957 (hint_json case 语句)
    触发时机: 每次 advance 返回 JSON 时自动注入

    **质询 (F1-F7):**
      F1 范围: PASS — 提示词是 batch8 防线的核心组件
      F2 依赖: PASS — 不依赖外部 API,纯 CLI JSON 输出
      F3 假验证: WARN — 需要验证 agent 实际响应 checklist
      F4 绕过: PASS — 直接修改 CLI hint 输出,不绕过现有组件
      F5 过度假设: WARN — 假设 agent 会遵守 checklist (无法强制)
      F6 回退: PASS — 回退到当前 hint 格式 (无 checklist)

    **实现步骤:**
      Step 1: 在 cmd_advance L1904-1957 的每个 hint_json 中追加字段
        → checklist: 每个阶段独有的操作清单
        → tool_policy: 工具选择映射 (全局,不在 case 中)
        → post_check: self-check 命令 (全局)

      Step 2: 每个阶段的 checklist 定义:
        ECO-SCAN:  ["discover", "state", "self-check"]
        ECO-READ:  ["monitor --subagent \"分析参考项目\"", "gitnexus context"]
        ECO-EDIT:  ["guard-edit <file>", "pre-tool-check", "pytest"]
        ECO-AUDIT: ["self-check", "context --update", "reflect"]
        ECO-DOC:   ["docgen", "gitnexus analyze --embeddings"]
        RED-*:     ["monitor --subagent \"<red-phase-task>\""]

      Step 3: tool_policy 全局注入 (不在 case 中)
        在 hint_json 构建后,追加:
        ,"tool_policy":{"search":"gatekeeper-cli.sh search",
                        "review":"gatekeeper-cli.sh monitor --subagent",
                        "read":"gitnexus context > rg > Read"}

      Step 4: post_check 全局注入
        ,"post_check":"bash gatekeeper-cli.sh self-check"

      Step 5: 验证
        → advance ECO-EDIT 返回包含 checklist 的 JSON
        → 109/109 tests 通过
        → 手动验证 checklist 内容与阶段匹配

    **Hook 修复 (2026-06-25):**
      .trae/settings.json 的 hook command 有 3 个致死问题:
      ┌─────────────────────────────────────────────────────────────────────┐
      │ ① $CLAUDE_PROJECT_DIR 是 Claude Code 环境变量                     │
      │   Trae (Lingma AI) 不设置此变量 → cd 失败 → hook 静默无法执行     │
      │   修复: 替换为硬编码绝对路径 (worktree 专属 settings, 路径固定)    │
      ├─────────────────────────────────────────────────────────────────────┤
      │ ② python3 不存在于 Windows PowerShell PATH                        │
      │   Trae 在 Windows 上运行, 直接调 python3 找不到可执行文件          │
      │   修复: 改为 wsl python3 (通过 WSL 桥接)                           │
      ├─────────────────────────────────────────────────────────────────────┤
      │ ③ matcher 未覆盖 CreateFile/DeleteFile 工具                       │
      │   Gate 0-4 只拦截 SearchReplace/Write/Edit，CreateFile 逃逸        │
      │   修复: matcher 扩展到 CreateFile|DeleteFile                       │
      └─────────────────────────────────────────────────────────────────────┘
      修复后: cd "C:\Users\HP\.trae\worktrees\batch8-python-migration" && wsl python3 scripts/pre_tool_hook.py

    **Bridge pre_tool_hook 对比 (架构差异):**
      Bridge (engine.py) → on_tool_start() → pre_tool_hook
        → Python 代理层：所有工具调用经过 engine.py
        → pre_tool_hook 在 LLC (Language Layer Controller) 层面阻断
        → 是引擎强制 (engine-enforced)，agent 无法绕过
        参考: prm-monitor-methodology.md §Q2

      Gatekeeper (.trae/settings.json) → pre_tool_hook.py
        → 无 engine.py，CLI 是唯一执行层
        → hook 在 IDE 层面阻断 (IDE-enforced, 非引擎层)
        → agent 如果绕过 IDE (直接写文件系统) 仍可逃逸
        → .trae/settings.json hook 是 Bridge engine.py 的轻量替代
        约束: 这是架构已知限制 (prm-monitor-methodology.md L272)

    **后续验证:**
      修复后需要验证:
      - wsl python3 能从 Windows PowerShell 接收 stdin JSON ✅ (已知工作)
      - hook exit code 2 能正确阻断 Trae 工具调用
      - Gate 0 (Task 拦截) 能正确识别 gatekeeper 关键词
      - 若 Trae hook 线路不通 → 回退方案: agent 主动 pre_tool_check + advance hint

  ✅ 3.5e: self-check 内联 Python 提取 (2026-06-26 完成)
    背景: cmd_self_check 函数 ~210 行中有 6 个内联 python3 -c "..." 块。
          提取为独立 scripts/gk_self_check.py，替换为 bash wrapper 调用。

    实现:
      scripts/gk_self_check.py (339 行)
        - 7 个检查函数: state_file, gate_data, checksum, hmac_key,
          key_integrity, foreign_content_guard, subagent_runner
        - 使用 AST 静态解析 subagent_runner 常量，绕过 import hang
          (root cause: subagent_runner L94 sys.stdin.readline() + 模块级
           from openai import AsyncOpenAI → httpx 初始化挂死)
        - run_all() 聚合器 + CLI main()

      scripts/test_gk_self_check.py (27 tests, TDD 模式)
        - 覆盖全部 7 个 check 函数, 含边界/异常路径

      gatekeeper-cli.sh cmd_self_check (~210→~130 行, -38%)
        - Python 模块负责 7 项检查
        - bash wrapper 保留 6 项原生检查 (provider/gitnexus/subagent_python/
          jwt_signing/context_injection/env_integrity)

    验证: 202 tests passed (新增 27, 退化 0)

  ⏳ 3.5i: cmd_self_check 剩余 bash 检查迁移分析 (2026-06-26)
    现状: cmd_self_check 仍有 6 项 bash 原生检查:
      (1) provider       — curl API ping (curl → requests)
      (2) gitnexus       — powershell.exe gitnexus status
      (3) subagent_python— command -v python3
      (4) jwt_signing    — python3 -c "from joserfc import jwt"
      (5) context_injection — openssl pkeyutl -verify
      (6) env_integrity  — API_KEY/SUBAGENT_ROLE/NODE

    依赖审计结果 (2026-06-26):
      - requests 已是项目依赖: gk_reflect.py L19, gk_monitor_api.py L23,
        gk_appeal.py L24 均已 import requests
      - gitnexus 使用规模: 50+ 处调用 (status/analyze/context/impact/query/detect-changes)
        横跨所有 ECO/RED/AUDIT 阶段 + 5 个 advance gate + 2 个 monitor schema
        → 但 18 处调用点中仅 4 处使用正确的 _run_gitnexus() (L554-590 node.exe+JS)
        → 其余 14 处直接用 powershell.exe -Command "gitnexus ..." (PATH 上找不到)
        → 需要 "gitnexus 调用统一化" 而非仅仅修 self-check:
          1. 将所有 powershell.exe -Command "gitnexus ..." 替换为 _run_gitnexus()
          2. _run_gitnexus() 需扩展支持当前所有命令模式:
             - status (无 --repo 标志)
             - context/impact -r <repo> <symbol>
             - query -r <repo> <pattern>
             - detect-changes --repo <repo> --scope compare [--base-ref HEAD~1]
          3. _run_gitnexus() 当前只处理 analyze/status 的 --skip-git,
             需验证 detect-changes/context/impact/query 是否需要同等处理
        → 然后 self-check 的 6 层自检只作为验证工具,验证统一化是否完成
         → 不应自动重跑 analyze (self-check 是只读诊断)
         → 详细执行方案 (plan-refinement 质询, 总分 78/100):
           路线: 分批转换,由低风险→高风险,共 7 批次
           P1: 9 处 status (L1067/L1423/L1433/L2145/L2153/L2165/L2186/
               L669/L2153) — 简单替换,pipe chain 保持
           P2: 3 处 detect-changes (L680/L2125/L4876) — 需验证 --skip-git
           P3: 5 处 context/impact/query (L888/L1531/L1552/L3636/L4870)
               — 直接透传 args
           P4: L3010 (self-check) — 2>&1 语义关键,保留不转换或写专用变体
           P5: L1031 (monitor --gitnexus) — 2>&1 语义仅影响日志,可转换
           总预估: 分析验证 30min + 转换 30min + 验证 30min ≈ 1.5h
      - subagent_runner 导入挂死: 已知,使用 AST 绕开

    计划质询结论 (plan-refinement 魔鬼代言人, 总分 33.6→72/100):
      ┌─────────────────────────────────────────────────────────────────────┐
      │ T1 provider:   可搬, requests 零新依赖。注意 POST 保留/400=OK      │
      │ T2 gitnexus:   先做 14 处调用统一化 (powershell.exe→_run_gitnexus),             │
      │                再做 self-check 6 层自检验证。50+ 处调用的基础设施不能只修 1 处。   │
      │ T3 jwt_signing:最干净, try: from joserfc import jwt (不用 find_spec)│
      │ T4 subagent_python: 1 行 bash → 不搬                              │
      │ T5 context_injection: openssl 包装 → 不搬                         │
      │ T6 env_integrity: 纯 env 读取 → 不搬                              │
      └─────────────────────────────────────────────────────────────────────┘

    收敛方案 (砍掉 ~50%):
       只搬 T1+T2+T3, T4-T6 保留 bash。
       执行顺序: T3(零风险) → T2(已验证) → T1(需明确 API key 传递)
       交叉修复:
       - run_all() 每个 check 独立 try/except, 防止级联崩溃
       - bash L2977 `for check_name in ...` 列表需同步更新
       - 回退策略: 现有 || echo '{"ok":true,"issues":0}' 保留不动

     gitnexus 调用统一化执行方案 (plan-refinement 质询, 总分 78/100):
       ┌─────────────────────────────────────────────────────────────────┐
       │ 范围: 将 14 处 powershell.exe -Command "gitnexus ..." 替换为   │
       │       _run_gitnexus() (gatekeeper-cli.sh L554-590)             │
       │ 分批: 7 批次, 风险递进                                         │
       │ 工期: 分析验证 30min + 转换 30min + 验证 30min = ~1.5h        │
       └─────────────────────────────────────────────────────────────────┘

       执行批次:
         P1(9 status, 🟢低风险): L669/L1067/L1423/L1433/L2145/L2153
                                 L2165/L2186 — 直接替换,pipe chain 保持
         P2(3 detect-changes, 🟡中风险): L680/L2125/L4876
             — 需先手动验证 --skip-git 需求
         P3(5 ctx/impact/query, 🟡中风险): L888/L1531/L1552/L3636/L4870
             — 直接透传 args,注意符号引号
         P4(L3010 self-check, 🔴高风险): 暂缓,保留原生
             — 2>&1 混合 stderr 策略,_run_gitnexus 的 2>/dev/null 破坏语义
         P5(L1031 monitor --gitnexus, 🟢低风险): 可转换
             — 2>&1 仅影响日志记录,不影响控制流

       约束-难度检查:
         F1 范围 ✅ — 所有批次属同一重构范畴
         F2 依赖 ✅ — node.exe + gitnexus JS 已验证可执行
         F3 假验证 ⚠️ — detect-changes --skip-git 未验证
         F4 绕过 ✅ — _run_gitnexus 是统一入口
         F5 过度假设 ⚠️ — L3010/L1031 2>&1 处理未充分评估
         F6 回退 ⚠️ — L3010 保留原生为回退路径
         F7 锁定 ✅ — 无新框架依赖

       待修复项:
         ⬜ detect-changes --skip-git: 转换前手动跑一次验证
         ⬜ L3010: 保留原生,或写 _run_gitnexus_with_stderr() 变体
         ⬜ L1067 cd 路径: 转换前对比 _repo_dir vs gn_repo_path
         ⬜ L669 布尔回退: set -x 跟踪 subshell exit code

  🕐 R-T7: cron 调度器启动 + Ed25519 自动轮换
     当前状态:
       - 3 个 cron job 已注册 (cron list 返回)
       - cron_scheduler.py 从未持久化运行 (所有 job run_count=0)
       - Ed25519 context key rotation_days=1, 需要每日轮换
       - key-rotate-check job 已存在但从未执行

     任务清单:
       T7.1: 启动 cron_scheduler.py 持久进程
         → 方案: nohup python3 scripts/cron_scheduler.py &
           (WSL 可用, 终端关闭后继续, 最简单)
         → 确保 release-check (0 9 * * *) + key-rotate-check (every 1d) 自动执行
         → 验证: cron list 显示 run_count > 0

       T7.3: CI 监控自动化 (2026-06-26 新增)
         → 创建 scripts/gk_ci_monitor.py
           - GitHub API → 读取 run status + job steps
           - 写入 state.gate_data.ci_monitor (JSON)
           - 记录每次检查的: run_number, status, conclusion, timestamp
         → 注册 cron job: */2 * * * *
           (2 分钟一跳, 周期短因为 CI 跑一次只需 3-5 分钟)
         → 数据量: 每跳 ~500B, 30 条/小时, ~15KB/天
         → 产出: 可在 cron status 中查看 CI 历史, 无需手动 curl

       T7.2: 添加 ECO-SCAN 外部冲击检测 cron job
         → schedule: every 6h (或每日 2 次)
         → prompt: "run discover + monitor --align, check for external changes"
         → 产出: 自动检测上游项目变动 (spz/zstd 新版本等)

       T7.3: Ed25519 密钥生命周期自动化
         → key-rotate-check job 需要实际执行 audit --key rotate context
         → 当前 job prompt 只是 "check key expiration and rotate if needed"
         → 需要: prompt 中明确调用 gatekeeper-cli.sh audit --key rotate context
         → 或: 将 rotate 逻辑内联到 cron job 的 script 字段

       T7.4: 密钥清理自动化
         → 轮换后自动清理 .bak 文件 (保留最近 2 个)
         → 轮换后自动更新 key-integrity.sha256
         → 轮换后自动 re-sign subagent-context.json

     依赖: cron_scheduler.py 已存在于 scripts/ (Python, 需要 pip install croniter)
     验证: 4 个 cron job 全部 run_count > 0, Ed25519 key 无过期

     注意: cron job 的 script 字段在调用 gatekeeper-cli.sh 或 git 命令时，
           需要正确导入 git-workflow skill 的推送/重试逻辑。
           gatekeeper-cli.sh 的 cmd_push 依赖 git-workflow 的 retry/timeout 处理.
           cron job 的 prompt 字段应明确声明 "使用 git-workflow 推送逻辑"，
           避免 cron 环境下 2>/dev/null 吞错误或直接 git push 无重试。

  🕐 RED 红队覆盖分析 (2026-06-26)

     当前 RED 测试覆盖 (test_red_attacks.py, 35 tests):
       ├─ RED-SCOPE   (unsigned/signed resolution, sealed score, advance log)       ✅ 6 tests
       ├─ RED-SURFACE (gate_level command enumeration)                              ✅ 2 tests
       ├─ RED-CONSTRAINTS (protected regions, gate enumeration, appeal_denied)       ✅ 4 tests
       ├─ RED-EXPLOIT (checksum tamper, injection fields)                           ✅ 6 tests
       ├─ RED-POSTEX  (resolutions, entropy rate)                                   ✅ 5 tests
       ├─ RED-REPORT  (report generation)                                           ✅ 6 tests
       ├─ TestPhaseAll (phase orchestration)                                        ✅ 3 tests
       └─ TestFilePatterns (file patterns)                                          ✅ 3 tests

     ⚠️ 3.5 范围未覆盖的 RED 区域:
       ┌─────────────────────────────────────────────┬────────┐
       │ 3.5d: advance hint 注入 (checklist/tool_policy) │ ❌ 无  │
       │ 3.5e: self-check 提取 (10项自检绕过)            │ ❌ 无  │
       │ 3.5f: Ed25519 无限旋转 (密钥耗尽攻击)            │ ❌ 无  │
       │ 3.5g: gitnexus RED 修复 (错误仓库索引)          │ ❌ 无  │
       │ 3.5h: F8 三层防线 (pre_tool_hook 绕过)          │ ❌ 无  │
       │ 3.5i: 新Python自检 (jwt/provider/gitnexus绕过)  │ ❌ 无  │
       └─────────────────────────────────────────────┴────────┘

     新增 RED 测试需求 (6 个攻击面, ~30 tests):
       RED-HINT:     模拟 agent 忽略 checklist/tool_policy, 直接调 Task tool
       RED-SELF:     模拟 self-check 10 项中任意一项被绕过/篡改
       RED-KEY:      模拟 Ed25519 密钥重复旋转 (3.5f 场景复现)
       RED-GX:       模拟 gitnexus 索引过期/仓库名错误 (3.5g 场景)
       RED-HOOK:     模拟 pre_tool_hook Gate0-4 逐个绕过 (F8 复现)
       RED-PY:       模拟新 Python 自检被绕过/返回假 PASS

     专武集成: AI-Infra-Guard v4.1.14
       ┌─────────────────────────────────────────────────────────────────┐
       │ 来源: C:\Users\HP\Downloads\AI-Infra-Guard-4.1.14              │
       ├─────────────────────────────────────────────────────────────────┤
       │ AIG-PromptSecurity/    → 4.6k+ 漏洞指纹库, 红队提示词          │
       │   deepteam/red_team.py → 可直接作为 RED-CONSTRAINTS 数据源     │
       │   cli/                 → CLI 调用接口, 可 gatekeeper 编排       │
       ├─────────────────────────────────────────────────────────────────┤
       │ data/vuln/             → 1000+ CVE YAML 指纹                   │
       │   Chuanhugpt/          → 25 CVE (直接相关: gatekeeper C++ 生态) │
       │   Dify/                → 25 CVE (编排相关)                      │
       │   LiteLLM/             → 20 CVE (LLM API 相关)                 │
       │   OpenClaw/            → 29 CVE (CLI 相关)                     │
       ├─────────────────────────────────────────────────────────────────┤
       │ 集成方式: 不直接依赖 AIG 运行, 而是:                            │
       │ ① 从 data/vuln/ 导入 CVE 模式 → 映射为本 RED 攻击向量          │
       │ ② 从 deepteam/red_team.py 提取策略 → gatekeeper 红队编排        │
       │ ③ AIG 指纹库 → 验证 gatekeeper 自身是否暴露对应脆弱面           │
       └─────────────────────────────────────────────────────────────────┘

     计划:
       RED 增强分 2 批:
       P1 (3.5 覆盖优先): RED-HINT + RED-HOOK + RED-PY → ~15 tests
         → 依赖: advance hint 已实现, pre_tool_hook 已修复, 自检已提取
         → 可直接编写, 零外部依赖
       P2 (专武集成): RED-KEY + RED-GX + RED-SELF → ~15 tests
         → 需要: 从 AIG data/vuln/ 提取 CVE 模式
         → AI-Infra-Guard DeepTeam 策略作为 RED-CONSTRAINTS 外挂数据源

  ✅ 3.5h: Master 主分支批判性吸收 (20 commits) — 已完成审查
     背景: batch8 从 master HEAD (8f35eef) 创建，master 的 20 个 commit 已在 batch8 历史中。
           git log batch8..master = 0 (master 是 batch8 的子集)。
           不需要 cherry-pick，但需要推送到远端同步。

     排查结论 (2026-06-24):
     ┌─────────────────────────────────────────────────────────────────────┐
     │ A: ECO 审计修复 (5 commits)                              已覆盖 ✅ │
     │   ed8bcbf: ECO.03 adjudicate SyntaxError + ECO.13+14 resolve      │
     │   6c91086: ECO.03 json.loads .strip() + ECO.14 fail-closed        │
     │   3a985be: ECO.03 echo -n trailing newline + ECO.10 grep           │
     │   f6d9ed1: ECO.03 Python tmpfiles + ECO.14+13 explicit empty       │
     │   8f35eef: ECO.03 triple-quote variables (eliminate file IO race)  │
     │   结论: batch8 gk_adjudicate.py 已覆盖全部 ECO.03 修复。          │
     │         ECO.10/13/14 在 gk_escalate_resolve.py 中已覆盖。          │
     ├─────────────────────────────────────────────────────────────────────┤
     │ B: Functional Fixes (4 commits)                          已覆盖 ✅ │
     │   3326f9e: install.sh (cron dir + croniter dependency)    ✅ 已在  │
     │   9a8e847: G-17 block resolve force_advance              ✅ 已在  │
     │   a5aefb1: SKILL.md accuracy (T43/T48/T42)               ✅ 已在  │
     │   fdfdab3: adjudicate write failure + checksum update     ✅ E3覆盖│
     │   结论: install.sh/G-17/SKILL.md 均已在 batch8 中。               │
     │         checksum update 被 batch8 E3 统一写入覆盖。               │
     ├─────────────────────────────────────────────────────────────────────┤
     │ C: Batch 9 工作成果 (4 commits)                          已包含 ✅ │
     │   17c5296: feat: audit --crossr via subagent_runner       ✅ 已在  │
     │   35af40f: test: Batch 9 Phase 1 red team 9/10            ✅ 已在  │
     │   885c15b: fix: Phase 1 alignment --depth usage           ✅ 已在  │
     │   5e34e27: feat: audit --all parallel execution           ✅ 已在  │
     │   结论: Batch 9 功能已在 batch8 gatekeeper-cli.sh 中。            │
     │         batch8 Phase 4 时直接使用，不需要额外迁移。               │
     ├─────────────────────────────────────────────────────────────────────┤
     │ D: Docs + ECO docs (7 commits)                         已包含 ✅ │
     │   9277fa1: ECO cycle bug list                          ✅ 已在  │
     │   c4d18d8: ECO challenge conclusion                    ✅ 已在  │
     │   0addcec: Batch 8 engineering plan                    ✅ 已在  │
     │   2fc4f46: Batch 9 expanded                            ✅ 已在  │
     │   ae8425e: Superpowers skills absorption analysis      ✅ 已在  │
     │   6dd2ea2: Superpowers 6.0.3 analysis                  ✅ 已在  │
     │   675ae52: Superpowers critical absorption analysis     ✅ 已在  │
     │   结论: flywheel-implementation-plan.md 已包含全部文档。           │
     └─────────────────────────────────────────────────────────────────────┘

     远端同步 (2026-06-24):
        origin/master:               8f35eef ✅ 已推送
        origin/batch8-python-migration: 1bbd99d ✅ 已推送

      完整 20 commits 清单 (master 8f35eef..3326f9e):
        #   SHA      类型   描述                                                    batch8 状态
        ──  ───────  ────  ────────────────────────────────────────────────────────  ──────────
        1   8f35eef  fix   ECO.03 adjudicate triple-quote variables (IO race)       gk_adjudicate.py ✅
        2   f6d9ed1  fix   ECO.03 Python tmpfiles + ECO.14+13 explicit empty        gk_adjudicate.py ✅
        3   3a985be  fix   ECO.03 echo -n trailing newline + ECO.10 grep            gk_escalate_resolve.py ✅
        4   6c91086  fix   ECO.03 json.loads .strip() + ECO.14 fail-closed          gk_escalate_resolve.py ✅
        5   ed8bcbf  fix   ECO.03 SyntaxError + ECO.13+14 resolve + ECO.15 timeout  gatekeeper-cli.sh ✅
        6   fdfdab3  fix   adjudicate stderr + advance checksum write                E3 unified write ✅
        7   9277fa1  docs  ECO cycle bug list                                       flywheel-impl-plan ✅
        8   c4d18d8  docs  ECO challenge conclusion                                  flywheel-impl-plan ✅
        9   9a8e847  fix   G-17 block resolve force_advance when appeal_required     gatekeeper-cli.sh ✅
        10  5e34e27  feat  Batch 9 Phase 2: audit --all parallel execution           gatekeeper-cli.sh ✅
        11  885c15b  fix   Phase 1 alignment: --depth usage string                   gatekeeper-cli.sh ✅
        12  35af40f  test  Batch 9 Phase 1 red team 9/10                             scripts/_r7_redteam ✅
        13  17c5296  feat  Batch 9 Phase 1: audit --crossr/--depth                   gatekeeper-cli.sh ✅
        14  a5aefb1  fix   SKILL.md accuracy (T43/T48/T42/command count)             SKILL.md ✅
        15  675ae52  docs  Superpowers critical absorption analysis                   flywheel-impl-plan ✅
        16  6dd2ea2  docs  Superpowers 6.0.3 analysis                                 flywheel-impl-plan ✅
        17  ae8425e  docs  Superpowers skills absorption analysis                     flywheel-impl-plan ✅
        18  2fc4f46  docs  Batch 9 expanded plan                                     flywheel-impl-plan ✅
        19  0addcec  docs  Batch 8 engineering plan                                   flywheel-impl-plan ✅
        20  3326f9e  fix   install.sh (cron dir + croniter dependency)                install.sh ✅
  ```

## 每个 Phase 的 ECO 执行序列

> **每个 Phase 必须完整走完 ECO 5 步，禁止跳过。**

```
Phase N (gk_xxx.py 迁移):
  ECO-SCAN: gitnexus analyze (基线) + discover (状态同步)
    → CLI: gatekeeper-cli.sh discover
    → CLI: gatekeeper-cli.sh monitor --gitnexus STAGE_0

  ECO-READ: 深度阅读原始 bash 代码
    → Read gatekeeper-cli.sh L<start>-<end> (对应函数)
    → gitnexus context (死代码检测)
    → 交叉审查: 对比 Python 实现与 bash 原始逻辑

  ECO-EDIT: 通过 CLI gate 编辑
    → CLI: gatekeeper-cli.sh guard-edit scripts/gk_xxx.py
    → CLI: gatekeeper-cli.sh pre-tool-check
    → SearchReplace / Write (实际编辑)
    → pytest (单元测试)
    → git commit

  ECO-AUDIT: 审计检查
    → CLI: gatekeeper-cli.sh audit --all
    → CLI: gatekeeper-cli.sh monitor --local STAGE_0
    → CLI: gatekeeper-cli.sh advance ECO-AUDIT (门禁验证)

  ECO-DOC: 文档同步
    → 更新 architecture-overview.md (文件映射表)
    → CLI: gatekeeper-cli.sh advance ECO-DOC
```

## Phase → ECO 映射表

```
Phase   ECO-SCAN        ECO-READ              ECO-EDIT              ECO-AUDIT         ECO-DOC
────────────────────────────────────────────────────────────────────────────────────────────────
-1      discover        读取 27 命令基线        capture_baselines.sh   —                 —
0a      discover        读取 bridge-cli.py     gk_state.py           audit --all       更新架构
0b      discover        —                      .gitattributes        —                 —
0c      discover        —                      pytest 框架            —                 —
1       discover        bash L5218-5331        gk_adjudicate.py      audit --all       更新架构
2a      discover        bash L1480-1610        gk_monitor_api.py     audit --all       更新架构
2b      discover        bash L2441-2613        gk_appeal.py          audit --all       更新架构
2c      discover        bash L2631-3360        gk_escalate_resolve   audit --all       更新架构
3 P0   discover        bash L1949-2000 hint gen gk_gate_checksum.py  audit --all       更新架构
3 P1   discover        bash L1660-2100 gates  gk_gate_all5.py       audit --all       更新架构
3 P2   discover        trace 5 subprocs       merge python3        time benchmark    更新架构
4a     discover        bash audit state       gk_audit_state.py     audit --all       更新架构
4b     discover        bash audit docs        gk_audit_docs.py      audit --all       更新架构
5      discover        bash audit crossr      gk_audit_crossr.py    audit --all       更新架构
6      discover        bash audit depth       gk_audit_depth.py     audit --all       更新架构
7      discover        bash L1632-2438        gk_advance.py         audit --all       更新架构
```

## 迁移流程 (每个 Phase)

```
Step 1: git tag batch8-phaseN-before
Step 2: 创建 scripts/gk_xxx.py
Step 3: 创建 scripts/test_gk_xxx.py
Step 4: pytest scripts/test_gk_xxx.py → 必须通过
Step 5: 修改 gatekeeper-cli.sh, 替换内嵌 Python
Step 6: bash -n gatekeeper-cli.sh → 语法检查
Step 7: 运行对应功能测试 + 回归对比 baseline
Step 8: git commit + git tag batch8-phaseN-after
Step 9: 如 Step 4/7 失败 → git checkout tag → 修复 → 重试
```

## 测试策略

```
单元测试: pytest
  → gk_state.py: 4 scenarios
  → gk_adjudicate.py: 4 scenarios
  → gk_monitor_api.py: 4 scenarios (mock)
  → gk_appeal.py: 6 scenarios
  → gk_escalate_resolve.py: 4 scenarios
  → gk_reflect.py: 4 scenarios
  → Phase 3 gate extraction:
    → gk_gate_checksum.py: 3 scenarios (clean/tampered/missing)
    → gk_gate_all5.py: 5 scenarios (each gate + combined)
  → gk_audit_state.py: 17 scenarios
  → gk_advance.py: 20 scenarios (6 stages × 2 states + ECO/ECC)
  → 总计: ~75 test cases

回归测试: test_regression_harness.py
  → 设置相同预状态 (通过 gk_state.py)
  → 运行 bash 版本 → 保存 JSON A
  → 运行 Python 版本 → 保存 JSON B
  → json.loads(A) == json.loads(B) (语义等价, 非字符串 diff)
  → 同 key 集合, 同 value, 递归结构等价

红队:
  → 每个 Phase 后运行 _r7_redteam_eco_escape.sh
  → 适配: 直接 STATE_FILE 写入改为通过 gk_state.py
  → Phase 3.5b: 红队回归测试套件 (~15 scenarios)
    → PROTECTED_REGION 绕过、checksum 绕过、state 直接写入
    → appeal 绕过、advance 跳过裁决、ECC 边界条件
  → Phase 3.5c: 集成测试 (端到端 ECO 循环 ~8 scenarios)

## 结构化提示词模版 (Phase 3.5d)

> 目标: 嵌入 SKILL.md，在每次 Phase 开始时输出固定检查清单，抑制流程跳过。

```json
{
  "route_template": {
    "phase_start_checklist": [
      "[ ] gatekeeper-cli.sh guard-edit <target_file>",
      "[ ] gatekeeper-cli.sh pre-tool-check",
      "[ ] gatekeeper-cli.sh discover (stage sync)",
      "[ ] gatekeeper-cli.sh advance ECO-<NEXT>",
      "[ ] gatekeeper-cli.sh adjudicate (ECO-EDIT→ECO-AUDIT only)",
      "[ ] gatekeeper-cli.sh audit --all",
      "[ ] gatekeeper-cli.sh advance RED-SCOPE (then SURFACE/CONSTRAINTS/EXPLOIT/POSTEX/REPORT)",
      "[ ] gatekeeper-cli.sh advance ECO-DOC",
      "[ ] pytest (all tests pass before commit)",
      "[ ] git commit (after all gates, not before)"
    ],
    "phase_end_checklist": [
      "[ ] git status (clean worktree)",
      "[ ] audit --state (13/15 minimum)",
      "[ ] pytest (all tests pass)"
    ]
  }
}
```

触发: advance hint 输出中包含 `checklist` 字段，agent 看到后必须逐项确认。
```

## 风险与缓解

```
CRLF 污染: .gitattributes + encoding="utf-8" + newline="\n" 所有文件
checksum 损坏: SHA256 算法相同 (bash + Python 都用 json.dumps(sort_keys=True))
appeal 退化: 独立 gk_appeal.py, 保留完整 mediation_context + F1-F7 对比
回归失败: json.loads() 语义等价, 非字符串 diff
bash 内嵌残留: 每个 Phase 后 grep 'python3 -c' gatekeeper-cli.sh 确认减少
测试不足: 当前 ~109 用例全部是单元测试，缺少集成测试和红队回归测试
性能退化: 提取到独立脚本后，python3 spawn 开销不变 (每个脚本仍是一次进程启动)
跨分支隔离: worktree 的 hooks/gate 脚本不在 master 生效，需合并后才被 IDE 加载

2026-06-26 发现:
  F8: 主分支版 skill 推进门卫 202 计划 R6 时, 三层防线全部未触发
    ┌─────────────────────────────────────────────────────────────────────┐
    │ 场景: 用主分支版 spz-gatekeeper-r-sequence skill                   │
    │       (非 batch8 worktree 版本) 推进门卫 202 计划的 R6 阶段       │
    │       (回归测试 + WASM 构建验证 + 版本 bump)                      │
    │                                                                    │
    │ 现象: agent 直接 Write 到门卫主 repo worktree                      │
    │       (C:\Users\HP\Downloads\HunYuan3D_test_cases\                 │
    │        spz_gatekeeper_project\cpp\CMakeLists.txt +                 │
    │        cpp\tests\v4_format_test.cc)                                │
    │       pre_tool_hook 未 BLOCK, pre-commit hook 不存在,              │
    │       entropy monitor 从未调用                                    │
    │                                                                    │
    │ 根因: .trae/settings.json hook 加载的是主分支 skill 版本            │
    │       而非 batch8 worktree 版本 → hook 可能不完整或未更新          │
    │       state.worktree 指向 skill 目录而非 gatekeeper 主 repo       │
    │       → Gate 3 路径检查可能误判                                    │
    │                                                                    │
    │ 复现概率: 有概率 (取决于 Trae 加载哪个 .trae/settings.json)         │
    │                                                                    │
    │ 待排查 (Phase 3.5 或 Phase 4):                                     │
    │   1. hook 加载链路: Trae 从哪个路径读 .trae/settings.json?          │
    │   2. Gate 3 路径检查: state.worktree vs 实际写入路径               │
    │   3. entropy monitor 调用时机: 为什么 monitor --local 从未运行?    │
    │   4. pre-commit hook 与 worktree 的交互:                          │
    │      feature worktree 的 .git 是文件, hook 在主 repo 的 .git/     │
    │      → hook 对 feature worktree 的 commit 不生效?                 │
    │   5. batch8 worktree 中创建 .trae/settings.json 能否修复?         │
    │                                                                    │
    │ 测试方案:                                                          │
    │   T-F8-1: 在 batch8 worktree 中手动调 hook, 验证 Gate 0-4         │
    │   T-F8-2: 在主 worktree 中手动调 hook, 验证是否 BLOCK             │
    │   T-F8-3: 验证 pre-commit hook 对 feature worktree 的 commit      │
    │   T-F8-4: monitor --local 调用, 验证 entropy 熵值计算             │
    └─────────────────────────────────────────────────────────────────────┘

2026-06-24 发现:
  F1: 剩余 208 个 python3 -c 内联 (含 self-check 完整内联块)
  F2: 20 处 STATE_FILE 写入，仅 2 处调用 _state_checksum_write ✅ 已修复 (3.5a)
  F3: 单元测试未覆盖红队攻击路径 → 计划 3.5b 红队回归测试套件
  F4: 无集成测试验证 bash→Python 替换后的端到端 ECO 循环 → 计划 3.5c 集成测试
  F5: Skill 加载自主分支 master，worktree batch8 的 hooks/gate 脚本不对当前会话生效
  E1: Ed25519 context_ed25519 无限旋转 → 32 .bak 文件 ✅ 已修复 (3.5f)
  E2: RED 阶段 gitnexus 硬编码 -r spz_gatekeeper + 重复 analyze ✅ 已修复 (3.5g)

2026-06-26 CI 修复经验 (PR #21, 10 rounds):
  ┌─────────────────────────────────────────────────────────────────────┐
  │ 问题: Windows CI 全部失败 (CMake configure + test hang)            │
  │ 根因: GitHub Actions windows-latest runner 环境差异                │
  │ 教训:                                                            │
  │   1. windows-latest 无 VS 2022 IDE → 用 ilammy/msvc-dev-cmd@v1    │
  │   2. vcpkg zlib.net 404 → 用 FetchContent + GitHub mirror        │
  │   3. CTest TIMEOUT 用 SIGALRM → Windows 无此信号 → 不生效          │
  │   4. ctest --timeout 60 跨平台有效 (进程级 kill)                    │
  │   5. CLI 测试 (registry_cli/gen_fixture/compat_check/             │
  │      extension_integration) 在 Windows 上 subprocess 卡死         │
  │   6. ci.yml 默认 shell 是 PowerShell → 引号/转义问题              │
  │   7. micromamba 首次安装慢 → 不适合 CI (cache 也不够快)             │
  │   8. 10 轮 CI 失败才通过 → 暴露 CI/CD 盲区 (T42)                  │
  │   9. PR #21 squash merged → c10c206 (main)                      │
  │   10. 5 个测试在 Windows CI 排除: self_test + 4 CLI tests         │
  └─────────────────────────────────────────────────────────────────────┘
  F6: Master 20 commits 批判性吸收 → 全部已在 batch8 中 ✅ 已确认 (3.5h)
  F7: 两个分支远端同步 → origin/master 8f35eef + origin/batch8 1bbd99d ✅ 已推送
```

## 成功标准

```
Phase 0: gk_state.py 读写 + checksum 正确, .gitattributes 创建, pytest 可运行     ✅
Phase 1: ECO.03 修复, adjudicate 可通过 CLI 调用                                  ✅
Phase 2: appeal 独立 + monitor/ci-diagnose 统一入口, 降级链可用                   ✅
Phase 3: gate extraction + hint routing fix + perf optimization                    ✅
Phase 3.5: checksum/Ed25519/RED gitnexus 修复 + master 吸收审查                   ⏳ 部分
Phase 4-6: audit 子命令等价输出                                                   ⏳ 待开始
Phase 7: advance 所有阶段过渡正确, 20 case 全通过                                 ⏳ 待开始
全部: 回归对比 100% 语义等价

## 扩展审计序列覆盖范围 (2026-06-26 新增)

### 背景

Phase 3.5 完成后，大量新增基础设施需要纳入审计序列的覆盖范围：

| 新增组件 | 来源 | 当前审计覆盖 |
|---------|------|------------|
| advance hint gates/checklist/tool_policy | 3.5d | ❌ 无 |
| gk_self_check.py 10 项自检完整性 | 3.5e/3.5i | ❌ 无 |
| .trae/settings.json hook 配置 | 3.5d | ❌ 无 |
| RED 引擎 3 新阶段 (HINT/HOOK/PY) | RED 增强 | ❌ 无 |
| gitnexus 索引与节点基线 | 3.5g | ❌ 无 |
| Pre-tool hook 门禁版本 pinning | F8 | ❌ 无 |

### 扩展审计检查项 (新增 12 项)

```
AUDIT-HINT:     advance hint JSON 完整性
  HINT-01: checklist 覆盖所有 11 阶段
  HINT-02: tool_policy 存在且含 search/review/read 三层优先级
  HINT-03: post_check 存在且指向 self-check
  HINT-04: gates 存在且含关键阶段过渡约束

AUDIT-SELF:     gk_self_check.py 自检完整性
  SELF-01: 10 项 check 函数全部在 run_all() 中注册
  SELF-02: 每个 check 函数有独立 try/except 隔离
  SELF-03: bash for loop 包含所有 JSON check name
  SELF-04: gitnexus check 验证 node.exe+JS 路径 + --skip-git

AUDIT-HOOK:     .trae/settings.json hook 配置审计
  HOOK-01: PreToolUse hook 存在且 matcher 覆盖 Edit/Write/SearchReplace/CreateFile/DeleteFile
  HOOK-02: hook command 经过 WSL 路由
  HOOK-03: hook 超时配置 >=10s
  HOOK-04: hook exit code 2 正确阻断

AUDIT-RED:      RED 引擎完整性
  RED-01~08: scope/surface/constraints/exploit/postex/hint/hook/py 8 阶段全部正确
```

### 集成方式

- 新增 `audit --hint`, `audit --self`, `audit --hook`, `audit --red` 4 个子命令
- 或合并到 `audit --all` 作为新增检查链
- 实现位置: `scripts/gk_audit_state.py` 扩展 (Phase 4a 范畴)
- 阶段归属: **R-T6 Phase 4a** (与 audit_state 合并实现)
      红队 ECO escape 无新增 VULN
      gatekeeper-cli.sh 行数从 5393 → <800 (当前: 5393, 目标: <800)

当前状态 (2026-06-26):
  已提取脚本: 13 个 gk_*.py (state, adjudicate, monitor_api, appeal, escalate_resolve,
              reflect, gate_checksum, gate_project_type, gate_appeal, gate_adjudicate,
              gate_ecc, self_check, red_attacks)
  测试用例: 235/235 passed (batch8 worktree)
  门卫 C++ 测试: 22/26 passed (Windows CI, 5 个 CLI 测试排除)
  CLI 行数: 5359 (Phase 4-7 迁移后将大幅减少)
  PR #21: DOI badge + Windows CI 修复 → c10c206 (main) ✅
  R-T5.6: gitnexus 统一化 (18 处替换) ✅
  RED 引擎: 8 阶段全链路 (scope→py), 28→17 发现, HIGH=0 ✅
  advance hint: RED-HINT/HOOK/PY checklist + AUDIT-*/ECC-* checklist ✅

## Phase 4 前置准备

### 技术前置检查清单

```
依赖组件              就绪   说明
────────────────────────────────────────────────────────
gk_state.py          ✅     state 读写 + checksum
gk_self_check.py     ✅     10 项自检 (可复用为 AUDIT-SELF 数据源)
_red_attacks.py      ✅     8 阶段 RED 引擎 (可复用为 AUDIT-RED 数据源)
advance hint JSON    ✅     gates/checklist/tool_policy 已注入
gitnexus _run_*      ✅     统一调用, 18 处已替换
────────────────────────────────────────────────────────
cmd_audit bash 摸底   ❌     需要先确定 17 项检查的 bash 代码范围
test_gk_audit.py     ❌     TDD 先行
gk_audit_state.py    ❌     Phase 4a 主体
gk_audit_docs.py     ❌     Phase 4b (4 项文档检查)
gk_audit_crossr.py   ❌     Phase 5 (跨 R 分析)
gk_audit_depth.py    ❌     Phase 6 (GitNexus 死代码)
gk_advance.py        ❌     Phase 7 (~800 行状态机)
```

### 代码结构规划

```
scripts/gk_audit_state.py  (~300 行)
  ├── check_state_file()       # 17 项原有检查 (从 bash cmd_audit 迁移)
  ├── check_docs()             # 4 项文档检查 (Phase 4b)
  ├── check_hint()             # 4 项 AUDIT-HINT (新增)
  ├── check_self()             # 4 项 AUDIT-SELF (新增)
  ├── check_hook()             # 4 项 AUDIT-HOOK (新增)
  ├── check_red()              # 8 项 AUDIT-RED (新增)
  └── run_all()                # 聚合器, 37 项全量运行

scripts/test_gk_audit.py     (~150 行)
  ├── TestAuditState            # 17 test cases
  ├── TestAuditHint             # 4 test cases
  ├── TestAuditSelf             # 4 test cases
  ├── TestAuditHook              # 4 test cases
  └── TestAuditRed              # 8 test cases
```

## F8.1: worktree 结构性漏洞的根因

### 问题
2026-06-27 agent 通过 RunCommand 误将 batch8 分支推送到错误远端。核心问题不是单次误操作，而是 **IDE hook + CLI gate + advance hint 三层均不覆盖远端验证**：

```
IDE hook (PreToolUse matcher):  SearchReplace|Write|Edit|CreateFile|DeleteFile|Task
                                ❌ RunCommand 不在其中 → git 操作全部逃逸

CLI pre-tool-check:             stage ∈ {IDLE,DONE} 才 BLOCK
                                ❌ 不检查 git remote -v 是否匹配预期远端

advance hint injected context:  告知 agent 当前阶段
                                ❌ 不告知 agent 当前 remote 配置
```

### 修复方案 (Phase 4 前置)

```
A. cmd_push 远端验证门禁 (gatekeeper-cli.sh)
   新增 _verify_remote() 函数:
     - 读取 git remote -v 输出
     - 与 _ALLOWED_REMOTES 白名单 (atomgit.com/*, github.com/spz-ecosystem/*) 匹配
     - 不匹配则 BLOCK 并报告当前 remote 配置
   执行时机: cmd_push 入口, stage 检查之后, push 执行之前

B. advance hint remote 感知 (advance hint JSON)
   每个 hint 注入新增字段:
     "current_remote": "<git remote -v 摘要>"
   让 agent 在阶段切换时知道当前远端配置

C. (可选) pre_tool_hook RunCommand matcher
   .trae/settings.json 新增 PreToolUse matcher 匹配 git-push/git-remote 等子命令
   但在 WSL 内部 git 操作绕过 IDE hook 的情况下仍需 A 兜底
```

### 优先级
A > B > C。A 是程序化门禁，B 是信息提示，C 是可选项。Phase 4 开始前必须完成 A。

### F8.1 修复执行计划 (Phase 4 前置, 2026-06-27 计划质询通过, 评分 83.8/100)

```
T1 [0.5h]: pr-fallback gatekeeper 委托 → _verify_remote           ✅ 完成
  现状: git-workflow.sh L153-163 — raw git push origin + gh pr create, 无远端白名单
  修复: push 前独立白名单检查 (github.com/spz-ecosystem/* / atomgit.com/pjh3955/*)
       不白名单则 hard BLOCK (return 1)
  验证: bash -n OK, gatekeeper-cli.sh 语法验证通过

T2 [1.5h]: RED F8.1 绕过测试 (7 项, 全 PASS)                       ✅ 完成
  新增: TestF81RemoteGate class in test_eco_integration.py
  覆盖: _verify_remote 存在          ✅ PASS
        cmd_push 调用 _verify_remote ✅ PASS
        _ALLOWED_REMOTE_PATTERNS    ✅ PASS
        pr-fallback 白名单检查        ✅ PASS
        git-workflow push 委托        ✅ PASS
        RunCommand matcher           ✅ PASS
        current_remote in hint       ✅ PASS

T3 [1.0h]: AUDIT-RED F8.1 审计检查                                 ⏳ 待做
  (将在 Phase 4a gk_audit_state.py 中作为 AUDIT-RED 子项实现)
```

总测试: 245 passed, 0 failed ✅

### 修复记录
- pre_tool_hook.py: STATE_FILE 硬编码 → 优先读环境变量 (L32-35), 适配多 worktree 独立 state 文件

### 新增审计数据收集面 (F8.1)

```
AUDIT-F81-01: _verify_remote 函数存在         → gatekeeper-cli.sh  grep "_verify_remote()"
AUDIT-F81-02: cmd_push 入口调用 _verify_remote → gatekeeper-cli.sh  grep "cmd_push.*verify"
AUDIT-F81-03: _ALLOWED_REMOTE_PATTERNS 配置    → gatekeeper-cli.sh  grep "spz-ecosystem\|pjh3955"
AUDIT-F81-04: pr-fallback 白名单检查存在        → git-workflow.sh   grep "github.com/spz-ecosystem"
AUDIT-F81-05: git-workflow push 委托 gatekeeper → git-workflow.sh   grep "spz-gatekeeper-r-sequence"
AUDIT-F81-06: RunCommand 在 PreToolUse matcher  → .trae/settings.json  grep "RunCommand"
AUDIT-F81-07: current_remote 在 advance hint   → gatekeeper-cli.sh  grep "current_remote"
AUDIT-F81-08: pre_tool_hook 读 STATE_FILE env  → pre_tool_hook.py  grep "os.environ.*STATE_FILE"
```

实现位置: Phase 4a 的 `gk_audit_state.py` check_red() 子模块 (T3)

### 新增红队打击面 (F8.1)

```
覆盖 4 条 bypass 路径, 7 项测试:

Path A:  gatekeeper-cli.sh push confirm → 白名单内远端
  测试: 无 (集成测试, 调真实 git remote)
  攻击面: 白名单配置篡改 → 审计 AUDIT-F81-03 检测

Path B:  gatekeeper-cli.sh push confirm → 白名单外远端
  测试: TestF81RemoteGate (代码存在性验证)
  攻击面: _verify_remote 逻辑绕过后门 → 审计 AUDIT-F81-02 检测
  
Path C:  git-workflow pr-fallback → 白名单外远端
  测试: TestF81RemoteGate::test_pr_fallback_remote_check
  攻击面: pr-fallback 绕过白名单 → 审计 AUDIT-F81-04 检测

Path D:  直接 git push via RunCommand
  测试: TestF81RemoteGate::test_settings_json_has_runcommand
  攻击面: IDE hook 不拦截 RunCommand → 审计 AUDIT-F81-06 检测

Path E:  pre_tool_hook 硬编码 state 文件 (旁路)
  测试: 无专门测试 (245 full suite 覆盖)
  攻击面: 多 worktree 场景下读错 state 文件 → 审计 AUDIT-F81-08 检测

防范: agent 自行 `git remote set-url origin <恶意URL>` 后推送
  门禁: _verify_remote 每次 push 都读实时 remote URL
  绕过: set-url 后 push 会被 _verify_remote 拦截
```

### git-workflow 增强总结

```
git-workflow.sh (v2.0.0 → v2.0.1)
  push:     + gatekeeper CLI 委托 (L101-112)     # F8.1 远端白名单
  pr-fallback: + 独立白名单检查 (L160-178)        # F8.1 hard BLOCK
  (security_management / release_management 仍为 stub, 待 Phase 4 后增强)
```

### 执行风险与缓解

| 风险 | 缓解 |
|------|------|
| F8.1: IDE hook 不拦截 RunCommand + push 无远端验证 | Phase 4 前置: cmd_push 实现 _verify_remote() (A 方案) |
| cmd_audit 17 项检查耦合 state 直接读写 | 先写 37 项测试再动 bash 代码, 每批提取后跑回归 |
| AUDIT-HINT 依赖 advance hint JSON 结构 | 结构已稳定, 无变更预期 |
| AUDIT-RED 依赖 RED 引擎 8 阶段 | 已测试验证, 28→17 发现 |
| bash 函数删除后 CI 测试需更新 | Phase 4 完成后统一替换 cmd_audit 调用点 |

```
R6 计划 (202-plan L2516-2621):
  T23: v4 格式 6 类专项测试         ⏳ 未开始 (需新建 v4_format_test.cc)
  T24: gen-fixture v4 模式扩展      ⏳ 未开始 (main.cc + wasm_main.cc)
  T25: WASM 生产标志优化            ⏳ 未开始 (CMakeLists.txt +2 行)
  T26: WASM 构建验证               ⏳ 未开始 (需 Emscripten 环境)
  T27: 版本号 bump + CHANGELOG      ⏳ 未开始 (4 文件)
  T28: 删除旧文件 tlv.h/tlv.cc     ⏳ 未开始 (需确认无引用)

R6 阻塞项:
  1. T23 需要 v4 header 格式实现 (R1-R5 的 spz.cc 修改)
     → 当前 spz.cc 已有 v4 header 解析 (R1-R5 已完成)
     → 但 v4_format_test.cc 未创建
     → 可以开始

  2. T24 需要 gen-fixture v4 模式
     → main.cc 的 BuildFixtureBlob() 需要 ZSTD 压缩路径
     → 依赖 R1 的 zstd 集成 (已完成)

  3. T26 需要 Emscripten 环境
     → CI 的 wasm job 已配置 (Emscripten SDK)
     → 但本地验证需要 emcmake/emmake

  4. T28 需要确认 tlv.h/tlv.cc 无引用
     → gitnexus context 可验证

当前可以开始: T23, T24, T25, T27
需要环境: T26 (Emscripten), T28 (gitnexus 验证)

R6 CI 教训应用:
  → Windows CI 测试排除: 已知 5 个 CLI 测试卡死
  → CTest timeout: 用 --timeout 而非 TIMEOUT 属性
  → ci.yml shell: 显式指定 bash
```
  远端同步: origin/master 8f35eef + origin/batch8 1bbd99d ✅
```

## 回滚

```
每个 Phase 独立回滚: git checkout batch8-phaseN-before
紧急回滚: git checkout batch8-phase0a-before (恢复所有 bash 内嵌)
全局回滚: git checkout main (恢复原始 gatekeeper-cli.sh)
```

## F8: worktree 结构性漏洞 (2026-06-27 修正)

### 问题
batch8 worktree `origin` remote 指向 **skill 仓库** (atomgit.com/pjh3955/spz-gatekeeper-r-sequence.git)，而非 **gatekeeper 仓库**。推送时必须手动指定远端，否则默认推送错位。

```
gatekeeper 主仓库:
  origin → github.com/spz-ecosystem/spz_gatekeeper.git
  atomgit → atomgit.com/pjh3955/spz_gatekeeper.git

batch8 worktree (修正前):
  origin → atomgit.com/pjh3955/spz-gatekeeper-r-sequence.git  ← 错位
```

### 修复
- 新增 `atomgit` remote → atomgit.com/pjh3955/spz_gatekeeper.git
- 新增 `github` remote → github.com/spz-ecosystem/spz_gatekeeper.git
- 保留 `origin` 指向 skill 仓库 (用于 skill 备份推送)
- `.gitignore` 追加: `.gatekeeper.state`, `.gatekeeper.state.lock`, `.vk.json`, `__test_*.sh`

### 教训
worktree 独立 clone 后 remote 不会继承自源仓库。以后 worktree 创建后第一件事: `git remote -v` 确认远端正确。
