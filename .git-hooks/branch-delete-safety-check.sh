#!/bin/bash
# Branch Delete Safety Check Script
# 分支删除安全审查流程

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# 检查是否在git仓库中
check_git_repo() {
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        log_error "当前目录不是git仓库"
        exit 1
    fi
}

# 检查分支是否存在
check_branch_exists() {
    local branch=$1
    if ! git show-ref --verify --quiet "refs/heads/$branch"; then
        log_error "分支 '$branch' 不存在"
        exit 1
    fi
}

# 检查是否为保护分支
check_protected_branch() {
    local branch=$1
    local protected_branches=("main" "master" "develop" "release" "production")
    
    for protected in "${protected_branches[@]}"; do
        if [ "$branch" = "$protected" ]; then
            log_error "不能删除保护分支: $branch"
            log_info "保护分支包括: ${protected_branches[*]}"
            exit 1
        fi
    done
}

# 检查是否为当前分支
check_current_branch() {
    local branch=$1
    local current_branch=$(git branch --show-current)
    
    if [ "$branch" = "$current_branch" ]; then
        log_error "不能删除当前分支: $branch"
        log_info "请先切换到其他分支: git checkout <other-branch>"
        exit 1
    fi
}

# 检查分支是否已合并到main
check_branch_merged() {
    local branch=$1
    
    log_info "检查分支 '$branch' 是否已合并到main..."
    
    # 检查本地main分支
    if git show-ref --verify --quiet "refs/heads/main"; then
        if git merge-base --is-ancestor "$branch" main; then
            log_success "分支 '$branch' 已合并到本地main分支"
            return 0
        fi
    fi
    
    # 检查远端main分支
    if git show-ref --verify --quiet "refs/remotes/origin/main"; then
        if git merge-base --is-ancestor "$branch" origin/main; then
            log_success "分支 '$branch' 已合并到远端main分支"
            return 0
        fi
    fi
    
    log_warn "分支 '$branch' 可能未合并到main分支"
    return 1
}

# 检查分支是否有未提交的更改
check_uncommitted_changes() {
    local branch=$1
    
    log_info "检查分支 '$branch' 是否有未提交的更改..."
    
    # 切换到目标分支检查状态
    local current_branch=$(git branch --show-current)
    git checkout "$branch" --quiet
    
    if [ -n "$(git status --porcelain)" ]; then
        log_warn "分支 '$branch' 有未提交的更改:"
        git status --short
        git checkout "$current_branch" --quiet
        return 1
    fi
    
    git checkout "$current_branch" --quiet
    log_success "分支 '$branch' 没有未提交的更改"
    return 0
}

# 检查分支最后提交时间
check_last_commit_time() {
    local branch=$1
    
    log_info "检查分支 '$branch' 最后提交时间..."
    
    local last_commit=$(git log -1 --format="%ci" "$branch")
    local last_commit_hash=$(git log -1 --format="%h" "$branch")
    local last_commit_msg=$(git log -1 --format="%s" "$branch")
    
    log_info "最后提交: $last_commit_hash - $last_commit_msg"
    log_info "提交时间: $last_commit"
    
    # 检查是否超过30天
    local thirty_days_ago=$(date -d "30 days ago" +%s 2>/dev/null || date -v-30d +%s 2>/dev/null)
    local commit_date=$(date -d "$last_commit" +%s 2>/dev/null || date -jf "%Y-%m-%d %H:%M:%S %z" "$last_commit" +%s 2>/dev/null)
    
    if [ "$commit_date" -lt "$thirty_days_ago" ]; then
        log_warn "分支 '$branch' 最后提交超过30天"
    fi
}

# 检查分支是否有对应的PR
check_pull_request() {
    local branch=$1
    
    log_info "检查分支 '$branch' 是否有对应的Pull Request..."
    
    # 尝试使用gh CLI检查PR
    if command -v gh &> /dev/null; then
        local pr_info=$(gh pr list --head "$branch" --state all --json number,title,state 2>/dev/null)
        if [ -n "$pr_info" ] && [ "$pr_info" != "[]" ]; then
            log_info "找到关联的PR:"
            echo "$pr_info" | jq -r '.[] | "  #\(.number) - \(.title) (\(.state))"'
            return 0
        fi
    fi
    
    log_info "未找到关联的PR（可能需要手动检查）"
    return 1
}

# 检查分支是否有关联的worktree
check_worktree_association() {
    local branch=$1
    
    log_info "检查分支 '$branch' 是否有关联的worktree..."
    
    # 查找关联的worktree
    local worktree_info=$(git worktree list | grep "$branch")
    
    if [ -n "$worktree_info" ]; then
        local worktree_path=$(echo "$worktree_info" | awk '{print $1}')
        local worktree_commit=$(echo "$worktree_info" | awk '{print $2}')
        local worktree_branch=$(echo "$worktree_info" | awk '{print $3}')
        
        log_warn "分支 '$branch' 有关联的worktree:"
        echo "  路径: $worktree_path"
        echo "  提交: $worktree_commit"
        echo "  分支: $worktree_branch"
        
        # 检查worktree是否被其他进程占用
        if command -v fuser &> /dev/null; then
            if fuser -v "$worktree_path" &> /dev/null; then
                log_error "Worktree 被其他进程占用:"
                fuser -v "$worktree_path"
                return 1
            fi
        fi
        
        # 检查worktree是否有未提交的更改
        if [ -d "$worktree_path" ]; then
            local worktree_status=$(git -C "$worktree_path" status --porcelain 2>/dev/null)
            if [ -n "$worktree_status" ]; then
                log_warn "Worktree 有未提交的更改:"
                git -C "$worktree_path" status --short
                return 1
            fi
        fi
        
        return 0
    fi
    
    log_info "分支 '$branch' 没有关联的worktree"
    return 0
}

# 检查是否为主worktree
check_main_worktree() {
    local branch=$1
    
    log_info "检查分支 '$branch' 是否在主worktree中..."
    
    # 获取当前worktree路径
    local current_worktree=$(git rev-parse --show-toplevel)
    local main_worktree=$(git worktree list | head -1 | awk '{print $1}')
    
    if [ "$current_worktree" = "$main_worktree" ]; then
        log_warn "当前在主worktree中操作"
        log_info "主worktree路径: $main_worktree"
        
        # 检查是否尝试删除主分支
        local current_branch=$(git branch --show-current)
        if [ "$branch" = "$current_branch" ]; then
            log_error "不能在主worktree中删除当前分支: $branch"
            log_info "请先切换到其他worktree或分支"
            return 1
        fi
        
        return 0
    fi
    
    log_info "当前在隔离worktree中操作"
    return 0
}

# 生成删除报告
generate_delete_report() {
    local branch=$1
    local force=$2
    
    echo ""
    echo "=========================================="
    echo "分支删除安全审查报告"
    echo "=========================================="
    echo "分支名称: $branch"
    echo "审查时间: $(date)"
    echo "------------------------------------------"
    
    # 检查结果汇总
    local all_checks_passed=true
    
    # 1. 检查是否为保护分支
    if check_protected_branch "$branch" 2>/dev/null; then
        echo "✓ 保护分支检查: 通过"
    else
        echo "✗ 保护分支检查: 失败"
        all_checks_passed=false
    fi
    
    # 2. 检查是否为当前分支
    if check_current_branch "$branch" 2>/dev/null; then
        echo "✓ 当前分支检查: 通过"
    else
        echo "✗ 当前分支检查: 失败"
        all_checks_passed=false
    fi
    
    # 3. 检查是否已合并
    if check_branch_merged "$branch"; then
        echo "✓ 合并状态检查: 通过"
    else
        echo "⚠ 合并状态检查: 警告（可能未合并）"
    fi
    
    # 4. 检查未提交更改
    if check_uncommitted_changes "$branch"; then
        echo "✓ 未提交更改检查: 通过"
    else
        echo "⚠ 未提交更改检查: 警告（有未提交更改）"
    fi
    
    # 5. 检查最后提交时间
    check_last_commit_time "$branch"
    
    # 6. 检查关联PR
    if check_pull_request "$branch"; then
        echo "✓ PR关联检查: 有关联PR"
    else
        echo "ℹ PR关联检查: 未找到关联PR"
    fi
    
    # 7. 检查worktree关联
    if check_worktree_association "$branch" 2>/dev/null; then
        echo "✓ Worktree关联检查: 通过"
    else
        echo "⚠ Worktree关联检查: 警告（有关联worktree）"
    fi
    
    # 8. 检查是否为主worktree
    if check_main_worktree "$branch" 2>/dev/null; then
        echo "✓ 主Worktree检查: 通过"
    else
        echo "✗ 主Worktree检查: 失败"
        all_checks_passed=false
    fi
    
    echo "------------------------------------------"
    
    if [ "$all_checks_passed" = true ]; then
        echo "✓ 所有安全检查通过"
        echo "可以安全删除分支: $branch"
    else
        echo "✗ 部分安全检查失败"
        echo "请手动确认后使用强制删除: git branch -D $branch"
    fi
    
    echo "=========================================="
}

# 主函数
main() {
    local branch=""
    local force=false
    local dry_run=false
    
    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -f|--force)
                force=true
                shift
                ;;
            -n|--dry-run)
                dry_run=true
                shift
                ;;
            -h|--help)
                echo "用法: $0 [选项] <分支名>"
                echo ""
                echo "选项:"
                echo "  -f, --force    强制删除（跳过确认）"
                echo "  -n, --dry-run  仅检查，不实际删除"
                echo "  -h, --help     显示帮助信息"
                echo ""
                echo "安全检查项目:"
                echo "  1. 保护分支检查（main, master, develop等）"
                echo "  2. 当前分支检查"
                echo "  3. 合并状态检查"
                echo "  4. 未提交更改检查"
                echo "  5. 最后提交时间检查"
                echo "  6. PR关联检查"
                echo "  7. Worktree关联检查"
                echo "  8. 主Worktree检查"
                exit 0
                ;;
            -*)
                log_error "未知选项: $1"
                exit 1
                ;;
            *)
                branch="$1"
                shift
                ;;
        esac
    done
    
    # 检查是否提供了分支名
    if [ -z "$branch" ]; then
        log_error "请提供要删除的分支名"
        echo "用法: $0 [选项] <分支名>"
        exit 1
    fi
    
    # 检查git仓库
    check_git_repo
    
    # 检查分支是否存在
    check_branch_exists "$branch"
    
    # 生成删除报告
    generate_delete_report "$branch" "$force"
    
    # 如果是dry-run模式，直接退出
    if [ "$dry_run" = true ]; then
        log_info "Dry-run模式，不实际删除分支"
        exit 0
    fi
    
    # 如果所有检查通过，询问确认
    if check_protected_branch "$branch" 2>/dev/null && \
       check_current_branch "$branch" 2>/dev/null; then
        
        if [ "$force" = false ]; then
            echo ""
            read -p "确认删除分支 '$branch'? (y/N): " confirm
            if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
                log_info "取消删除分支 '$branch'"
                exit 0
            fi
        fi
        
        # 执行删除
        log_info "正在删除分支 '$branch'..."
        if git branch -d "$branch"; then
            log_success "本地分支 '$branch' 已删除"
            
            # 尝试删除远端分支
            if git show-ref --verify --quiet "refs/remotes/origin/$branch"; then
                read -p "是否同时删除远端分支 'origin/$branch'? (y/N): " confirm_remote
                if [[ "$confirm_remote" =~ ^[Yy]$ ]]; then
                    if git push origin --delete "$branch"; then
                        log_success "远端分支 'origin/$branch' 已删除"
                    else
                        log_error "删除远端分支失败"
                    fi
                fi
            fi
        else
            log_error "删除分支失败"
            log_info "如果确定要删除，请使用: git branch -D $branch"
            exit 1
        fi
    else
        log_error "安全检查未通过，无法删除分支"
        log_info "如果确定要删除，请使用: git branch -D $branch"
        exit 1
    fi
}

# 执行主函数
main "$@"