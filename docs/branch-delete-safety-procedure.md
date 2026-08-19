# 分支删除安全审查流程

## 概述

本文档定义了分支删除的安全审查流程，确保分支删除操作的安全性和可追溯性。

## 安全检查项目

### 1. 保护分支检查
**目标**：防止误删重要分支
**检查内容**：
- main/master 分支
- develop 分支
- release 分支
- production 分支

**处理方式**：保护分支禁止删除，除非使用强制删除（`git branch -D`）

### 2. 当前分支检查
**目标**：防止删除当前工作分支
**检查内容**：确认要删除的分支不是当前检出的分支

**处理方式**：如果是当前分支，提示先切换到其他分支

### 3. 合并状态检查
**目标**：确保分支内容已合并到主分支
**检查内容**：
- 检查是否已合并到本地 main 分支
- 检查是否已合并到远端 origin/main 分支

**处理方式**：未合并的分支需要确认是否强制删除

### 4. 未提交更改检查
**目标**：防止丢失未提交的工作
**检查内容**：检查分支是否有未提交的更改

**处理方式**：有未提交更改的分支需要先提交或暂存更改

### 5. 最后提交时间检查
**目标**：识别长期未活动的分支
**检查内容**：检查分支最后提交时间

**处理方式**：超过30天未活动的分支会显示警告

### 6. PR关联检查
**目标**：确认分支是否已通过PR合并
**检查内容**：检查分支是否有对应的Pull Request

**处理方式**：有关联PR的分支可以更安全地删除

## 使用方法

### 1. 安全检查（推荐）
```bash
# 仅检查，不实际删除
.git-hooks/branch-delete-safety-check.sh --dry-run <分支名>

# 示例
.git-hooks/branch-delete-safety-check.sh --dry-run chore/gitignore-update
```

### 2. 安全删除
```bash
# 交互式删除（会询问确认）
.git-hooks/branch-delete-safety-check.sh <分支名>

# 强制删除（跳过确认）
.git-hooks/branch-delete-safety-check.sh --force <分支名>
```

### 3. 手动删除（不推荐）
```bash
# 删除本地分支
git branch -d <分支名>  # 安全删除
git branch -D <分支名>  # 强制删除

# 删除远端分支
git push origin --delete <分支名>
```

## 安全审查报告示例

```
==========================================
分支删除安全审查报告
==========================================
分支名称: chore/gitignore-update
审查时间: Sun May  3 13:58:36 CST 2026
------------------------------------------
✓ 保护分支检查: 通过
✓ 当前分支检查: 通过
✓ 合并状态检查: 通过
⚠ 未提交更改检查: 警告（有未提交更改）
ℹ 最后提交时间: 2026-05-02 22:06:10 +0800
ℹ PR关联检查: 未找到关联PR
------------------------------------------
✓ 所有安全检查通过
可以安全删除分支: chore/gitignore-update
==========================================
```

## 最佳实践

### 1. 删除前检查
- **必须**：运行安全检查脚本
- **必须**：确认分支已合并到主分支
- **建议**：检查是否有未提交的更改

### 2. 删除顺序
1. 先删除本地分支
2. 再删除远端分支
3. 确认删除操作成功

### 3. 记录删除
- 记录删除的分支名称
- 记录删除时间
- 记录删除原因（如：已合并、不再需要等）

### 4. 备份重要分支
对于重要分支，删除前可以：
```bash
# 创建备份标签
git tag backup/<分支名> <分支名>

# 或创建备份分支
git branch backup/<分支名> <分支名>
```

## 常见问题

### Q1: 如何删除保护分支？
A1: 保护分支默认禁止删除。如果确实需要删除，使用强制删除：
```bash
git branch -D <保护分支名>
```

### Q2: 如何删除未合并的分支？
A2: 未合并的分支需要强制删除：
```bash
git branch -D <分支名>
```
**注意**：强制删除会丢失分支上的所有未合并提交！

### Q3: 如何删除远端分支？
A3: 删除远端分支的命令：
```bash
git push origin --delete <分支名>
```

### Q4: 删除分支后如何恢复？
A4: 如果分支已推送到远端，可以从远端恢复：
```bash
git checkout -b <分支名> origin/<分支名>
```
如果分支只有本地，且已删除，恢复较困难，建议定期备份重要分支。

## 自动化集成

### 1. Git Hook 集成
可以将安全检查脚本集成到 Git Hook 中：
```bash
# 在 .git/hooks/pre-delete 中调用安全检查
.git-hooks/branch-delete-safety-check.sh "$@"
```

### 2. CI/CD 集成
在 CI/CD 流程中添加分支清理步骤：
```yaml
# GitHub Actions 示例
- name: Clean up merged branches
  run: |
    git fetch --prune
    git branch -r --merged main | grep -v main | xargs -I {} git push origin --delete {}
```

### 3. 定期清理
设置定期任务清理已合并的分支：
```bash
# 每周清理一次已合并的分支
0 0 * * 0 /path/to/cleanup-merged-branches.sh
```

## 相关工具

### 1. safe-branch-delete.sh
简单的保护分支检查脚本，防止删除 main/master 分支。

### 2. branch-delete-safety-check.sh
完整的安全审查脚本，包含所有检查项目。

### 3. Git 命令
- `git branch -d`：安全删除（已合并才允许）
- `git branch -D`：强制删除
- `git push origin --delete`：删除远端分支

## Worktree隔离管理

### 隔离策略（强制）
- **主worktree保护**：仅用于main分支，禁止在此创建其他分支
- **隔离要求**：所有新分支必须在隔离worktree中工作
- **锁定机制**：使用`git worktree lock`保护主worktree

### Worktree创建规范
```bash
# 标准创建流程（强制）
git worktree add /root/.config/superpowers/worktrees/<repo>/<branch> <branch>

# 命名规范
路径：/root/.config/superpowers/worktrees/<仓库名>/<分支名>
示例：/root/.config/superpowers/worktrees/spz_gatekeeper_project/feature-new-feature
```

### Worktree清理流程
```bash
# 删除分支时自动清理worktree
1. 检查worktree是否被占用
2. 检查worktree是否有未提交更改
3. 清理worktree：git worktree remove <path>
4. 验证worktree已清理
```

### 主Worktree保护机制
```bash
# 锁定主worktree
git worktree lock --reason '主分支保护' .

# 配置主worktree标识
git config --local worktree.mainWorktree true

# 验证保护状态
git worktree list
```

## Git Workflow Umbrella Skill 集成

### 迁移说明
本流程已集成到`git-workflow` umbrella skill中，作为分支管理模块。

### 新的使用方式
```bash
# 分支删除（推荐）
git-workflow branch delete <分支名>

# 分支清理
git-workflow branch cleanup

# Worktree清理
git-workflow worktree cleanup
```

### 兼容性说明
- **原有脚本**：`.git-hooks/branch-delete-safety-check.sh` 仍然可用
- **推荐使用**：`git-workflow branch delete` 作为统一入口
- **功能完整**：所有原有安全检查在git-workflow中完整保留

### 相关文件
- **git-workflow skill**: `~/.codebuddy/skills/git-workflow/SKILL.md`
- **配置文件**: `~/.codebuddy/skills/git-workflow/git-workflow.yaml`
- **脚本文件**: `~/.codebuddy/skills/git-workflow/scripts/git-workflow.sh`

# 验证保护状态
git worktree list
```

## 更新记录

- **2026-05-03**：初始版本，创建完整的分支删除安全审查流程
- 添加了安全检查脚本和详细文档
- 定义了最佳实践和常见问题解答
- **2026-05-04**：添加worktree隔离管理
- 强制要求新分支使用隔离worktree
- 添加主worktree保护机制
- 更新安全检查脚本，添加worktree检查

---

*本文档基于 Git 最佳实践和项目安全需求制定，旨在确保分支删除操作的安全性和可追溯性。*