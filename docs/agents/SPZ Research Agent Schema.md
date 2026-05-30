# SPZ Research Agent Schema

## 概述

本文档定义SPZ研究代理体系的统一预算状态机和工具权限schema，用于规范研究代理的行为、资源管理和权限控制。

## 预算状态机

### 全局状态机定义

```json
{
  "global_state_machine": {
    "version": "1.0.0",
    "states": {
      "INIT": {
        "description": "初始化状态，代理启动后的初始状态",
        "next_states": ["PLANNING", "IDLE"],
        "entry_actions": ["load_config", "validate_permissions"],
        "exit_actions": ["save_state"]
      },
      "IDLE": {
        "description": "空闲状态，等待任务分配",
        "next_states": ["PLANNING", "SEARCHING", "ANALYZING"],
        "entry_actions": ["check_budget", "log_status"],
        "exit_actions": ["update_metrics"]
      },
      "PLANNING": {
        "description": "规划状态，分解任务和分配资源",
        "next_states": ["EXECUTING", "PAUSED", "FAILED"],
        "entry_actions": ["estimate_budget", "identify_dependencies"],
        "exit_actions": ["create_task_plan"]
      },
      "SEARCHING": {
        "description": "检索状态，搜索文献或代码",
        "next_states": ["SCREENING", "NO_RESULTS", "PAUSED"],
        "entry_actions": ["prepare_search_queries", "select_databases"],
        "exit_actions": ["log_search_results"]
      },
      "SCREENING": {
        "description": "筛选状态，评估检索结果",
        "next_states": ["READING", "SEARCHING", "PAUSED"],
        "entry_actions": ["apply_filters", "score_relevance"],
        "exit_actions": ["update_result_list"]
      },
      "READING": {
        "description": "阅读状态，深入分析文献或代码",
        "next_states": ["SYNTHESIZING", "SCREENING", "PAUSED"],
        "entry_actions": ["extract_key_information", "take_notes"],
        "exit_actions": ["update_knowledge_base"]
      },
      "ANALYZING": {
        "description": "分析状态，技术深度分析",
        "next_states": ["VALIDATING", "REPORTING", "PAUSED"],
        "entry_actions": ["select_analysis_method", "gather_evidence"],
        "exit_actions": ["document_findings"]
      },
      "VALIDATING": {
        "description": "验证状态，验证分析结果",
        "next_states": ["REPORTING", "ANALYZING", "FAILED"],
        "entry_actions": ["design_validation", "execute_tests"],
        "exit_actions": ["record_validation_results"]
      },
      "SYNTHESIZING": {
        "description": "综合状态，整合多个来源的信息",
        "next_states": ["REPORTING", "READING", "REVISION"],
        "entry_actions": ["identify_patterns", "resolve_conflicts"],
        "exit_actions": ["create_synthesis"]
      },
      "REPORTING": {
        "description": "报告状态，生成研究报告",
        "next_states": ["COMPLETED", "REVISION", "IDLE"],
        "entry_actions": ["compile_findings", "format_report"],
        "exit_actions": ["submit_report", "update_metrics"]
      },
      "REVISION": {
        "description": "修订状态，根据反馈修改报告",
        "next_states": ["REPORTING", "SYNTHESIZING", "ANALYZING"],
        "entry_actions": ["analyze_feedback", "identify_changes"],
        "exit_actions": ["update_report"]
      },
      "PAUSED": {
        "description": "暂停状态，等待外部输入或资源",
        "next_states": ["PLANNING", "SEARCHING", "ANALYZING", "IDLE"],
        "entry_actions": ["save_state", "notify_stakeholders"],
        "exit_actions": ["restore_state"]
      },
      "FAILED": {
        "description": "失败状态，任务执行失败",
        "next_states": ["PLANNING", "IDLE"],
        "entry_actions": ["analyze_failure", "log_error"],
        "exit_actions": ["cleanup_resources"]
      },
      "COMPLETED": {
        "description": "完成状态，任务成功完成",
        "next_states": ["IDLE"],
        "entry_actions": ["finalize_report", "archive_results"],
        "exit_actions": ["update_metrics", "cleanup_resources"]
      }
    },
    "transitions": {
      "INIT->PLANNING": {
        "conditions": ["task_assigned", "budget_available"],
        "actions": ["initialize_task"]
      },
      "PLANNING->EXECUTING": {
        "conditions": ["plan_approved", "resources_allocated"],
        "actions": ["start_execution"]
      },
      "EXECUTING->SYNTHESIZING": {
        "conditions": ["subtasks_completed", "evidence_gathered"],
        "actions": ["compile_results"]
      },
      "SYNTHESIZING->REPORTING": {
        "conditions": ["synthesis_complete", "quality_check_passed"],
        "actions": ["generate_report"]
      },
      "REPORTING->COMPLETED": {
        "conditions": ["report_approved", "no_revisions_needed"],
        "actions": ["finalize_task"]
      }
    }
  }
}
```

### 预算控制Schema

```json
{
  "budget_schema": {
    "version": "1.0.0",
    "resource_types": {
      "tokens": {
        "description": "API调用token消耗",
        "unit": "tokens",
        "measurement": "累计消耗"
      },
      "rounds": {
        "description": "代理交互轮次",
        "unit": "rounds",
        "measurement": "累计轮次"
      },
      "time": {
        "description": "执行时间",
        "unit": "minutes",
        "measurement": "累计时间"
      },
      "operations": {
        "description": "工具调用次数",
        "unit": "calls",
        "measurement": "累计调用"
      }
    },
    "budget_allocation": {
      "coordinator": {
        "tokens": 100000,
        "rounds": 20,
        "time": 30,
        "operations": 50
      },
      "technical_analyst": {
        "tokens": 50000,
        "rounds": 10,
        "time": 15,
        "operations": 30
      },
      "literature_reviewer": {
        "tokens": 40000,
        "rounds": 15,
        "time": 20,
        "operations": 25
      }
    },
    "thresholds": {
      "soft_limit": {
        "percentage": 80,
        "actions": ["compress_context", "prioritize_tasks", "notify_user"]
      },
      "hard_limit": {
        "percentage": 100,
        "actions": ["stop_execution", "generate_partial_report", "request_budget_increase"]
      }
    },
    "monitoring": {
      "check_interval": "per_round",
      "reporting_interval": "per_task",
      "alert_threshold": 90
    }
  }
}
```

## 工具权限Schema

### 权限分级定义

```json
{
  "permission_tiers": {
    "P0": {
      "name": "ReadOnly",
      "description": "只读操作，无副作用",
      "risk_level": "low",
      "approval_required": false,
      "tools": [
        "search_file",
        "search_content",
        "read_file",
        "web_fetch",
        "list_dir"
      ],
      "constraints": {
        "max_file_size": "10MB",
        "allowed_paths": ["*"],
        "blocked_paths": ["/etc/shadow", "~/.ssh/*"]
      }
    },
    "P1": {
      "name": "SafeWrite",
      "description": "本地安全写操作",
      "risk_level": "medium",
      "approval_required": false,
      "tools": [
        "todo_write",
        "write_to_file",
        "replace_in_file"
      ],
      "constraints": {
        "max_file_size": "5MB",
        "allowed_paths": ["~/.codebuddy/*", "project/*"],
        "blocked_paths": ["~/.ssh/*", "/etc/*"],
        "backup_required": true
      }
    },
    "P2": {
      "name": "ExternalAccess",
      "description": "外部接入操作",
      "risk_level": "high",
      "approval_required": true,
      "tools": [
        "execute_command",
        "task",
        "web_fetch"
      ],
      "constraints": {
        "timeout_seconds": 300,
        "allowed_commands": ["git", "cmake", "make", "python", "node"],
        "blocked_commands": ["rm -rf", "sudo", "chmod 777"],
        "network_access": true
      }
    },
    "P3": {
      "name": "Destructive",
      "description": "凭证/破坏性操作",
      "risk_level": "critical",
      "approval_required": true,
      "tools": [],
      "constraints": {
        "requires_human_approval": true,
        "requires_justification": true,
        "requires_backup": true
      }
    }
  }
}
```

### 代理权限矩阵

```json
{
  "agent_permissions": {
    "spz-research-coordinator": {
      "tier": "P2",
      "allowed_tools": [
        "search_file",
        "search_content",
        "read_file",
        "execute_command",
        "todo_write",
        "task"
      ],
      "forbidden_tools": [],
      "requires_approval": ["execute_command"],
      "special_permissions": [
        "can_delegate_tasks",
        "can_manage_budget",
        "can_generate_reports"
      ]
    },
    "spz-technical-analyst": {
      "tier": "P1",
      "allowed_tools": [
        "search_file",
        "search_content",
        "read_file",
        "execute_command",
        "todo_write"
      ],
      "forbidden_tools": ["task"],
      "requires_approval": ["execute_command"],
      "special_permissions": [
        "can_analyze_code",
        "can_validate_technical",
        "can_generate_technical_reports"
      ]
    },
    "spz-literature-reviewer": {
      "tier": "P0",
      "allowed_tools": [
        "search_file",
        "search_content",
        "read_file",
        "web_fetch",
        "todo_write"
      ],
      "forbidden_tools": ["execute_command", "task"],
      "requires_approval": [],
      "special_permissions": [
        "can_search_literature",
        "can_generate_reviews",
        "can_manage_references"
      ]
    }
  }
}
```

## 状态转换规则

### 转换条件定义

```json
{
  "transition_conditions": {
    "budget_check": {
      "description": "检查预算是否充足",
      "condition": "current_usage < threshold",
      "actions": {
        "pass": ["continue_execution"],
        "fail": ["pause_execution", "notify_user"]
      }
    },
    "task_completion": {
      "description": "检查任务是否完成",
      "condition": "all_subtasks_completed AND quality_check_passed",
      "actions": {
        "pass": ["transition_to_next_state"],
        "fail": ["retry_or_revise"]
      }
    },
    "error_handling": {
      "description": "处理执行错误",
      "condition": "error_occurred",
      "actions": {
        "recoverable": ["retry_with_backoff"],
        "non_recoverable": ["transition_to_failed", "notify_user"]
      }
    },
    "timeout_handling": {
      "description": "处理超时情况",
      "condition": "execution_time > timeout_limit",
      "actions": {
        "soft_timeout": ["compress_and_continue"],
        "hard_timeout": ["stop_execution", "generate_partial_report"]
      }
    }
  }
}
```

### 转换动作定义

```json
{
  "transition_actions": {
    "save_state": {
      "description": "保存当前状态",
      "parameters": {
        "include": ["current_state", "progress", "budget_usage", "artifacts"],
        "format": "json",
        "location": "~/.codebuddy/state/"
      }
    },
    "restore_state": {
      "description": "恢复之前的状态",
      "parameters": {
        "source": "~/.codebuddy/state/",
        "validate": true
      }
    },
    "compress_context": {
      "description": "压缩上下文以节省token",
      "parameters": {
        "strategy": "summarize_and_prune",
        "preserve": ["key_findings", "critical_evidence", "task_progress"],
        "remove": ["intermediate_results", "redundant_information"]
      }
    },
    "notify_user": {
      "description": "通知用户当前状态",
      "parameters": {
        "channel": "console",
        "include": ["status", "budget_usage", "next_actions"],
        "urgency": "normal"
      }
    },
    "generate_report": {
      "description": "生成研究报告",
      "parameters": {
        "format": "markdown",
        "sections": ["executive_summary", "findings", "conclusions", "references"],
        "location": "project/docs/plans/"
      }
    }
  }
}
```

## 监控和报告

### 监控指标

```json
{
  "monitoring_metrics": {
    "performance": {
      "tokens_per_round": "平均每轮token消耗",
      "rounds_per_task": "平均每任务轮次",
      "time_per_task": "平均每任务时间",
      "success_rate": "任务成功率"
    },
    "quality": {
      "completeness": "研究完整性",
      "accuracy": "研究准确性",
      "relevance": "研究相关性",
      "consistency": "研究一致性"
    },
    "efficiency": {
      "budget_utilization": "预算利用率",
      "time_utilization": "时间利用率",
      "resource_efficiency": "资源使用效率"
    }
  }
}
```

### 报告格式

```json
{
  "report_format": {
    "status_report": {
      "frequency": "per_round",
      "sections": ["current_state", "progress", "budget_status", "next_actions"],
      "recipients": ["user", "coordinator"]
    },
    "progress_report": {
      "frequency": "per_task",
      "sections": ["task_summary", "findings", "challenges", "recommendations"],
      "recipients": ["user", "coordinator", "stakeholders"]
    },
    "final_report": {
      "frequency": "per_project",
      "sections": ["executive_summary", "methodology", "findings", "conclusions", "references"],
      "recipients": ["user", "stakeholders", "archive"]
    }
  }
}
```

## 安全和合规

### 安全约束

```json
{
  "security_constraints": {
    "data_protection": {
      "sensitive_data_handling": "不处理敏感个人信息",
      "data_retention": "研究数据保留30天",
      "data_encryption": "传输和存储加密"
    },
    "access_control": {
      "principle_of_least_privilege": "最小权限原则",
      "separation_of_duties": "职责分离",
      "audit_logging": "操作审计日志"
    },
    "compliance": {
      "ethical_guidelines": "遵守研究伦理",
      "citation_standards": "遵循引用规范",
      "intellectual_property": "尊重知识产权"
    }
  }
}
```

## 使用示例

### 研究任务执行流程

```json
{
  "example_workflow": {
    "task": "分析SPZ v4格式与门卫项目的兼容性",
    "steps": [
      {
        "step": 1,
        "agent": "spz-research-coordinator",
        "action": "分解任务",
        "state_transition": "INIT -> PLANNING"
      },
      {
        "step": 2,
        "agent": "spz-technical-analyst",
        "action": "分析SPZ v4格式",
        "state_transition": "IDLE -> ANALYZING"
      },
      {
        "step": 3,
        "agent": "spz-literature-reviewer",
        "action": "检索相关文献",
        "state_transition": "IDLE -> SEARCHING"
      },
      {
        "step": 4,
        "agent": "spz-research-coordinator",
        "action": "综合结果",
        "state_transition": "EXECUTING -> SYNTHESIZING"
      },
      {
        "step": 5,
        "agent": "spz-research-coordinator",
        "action": "生成报告",
        "state_transition": "SYNTHESIZING -> REPORTING"
      }
    ]
  }
}
```

## 版本历史

- **v1.0.0** (2026-05-06): 初始版本，定义基础状态机和权限schema
- **v1.1.0** (计划): 增加动态预算调整和自适应权限管理
- **v1.2.0** (计划): 增加多代理协作和冲突解决机制