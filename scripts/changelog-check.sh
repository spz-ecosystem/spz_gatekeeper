#!/usr/bin/env bash
# changelog-check.sh — 发布门禁：tag 区间内每个 commit 都必须在 CHANGELOG.md 有记录。
#
# 移植自 spz2glb（计划 PRE.16 + PRE.18）。背景：PR→tag 模式下 tag 是 main 的冻结快照，
# tag 之后合入的 PR 永远进不了该版本 CHANGELOG（spz2glb v2.0.4 即因此重打 tag）。
# 故打 tag 时自动校验完整性，缺条目即拦截发布。
#
# 用法:
#   scripts/changelog-check.sh <cur-tag>              # prev 自动取 describe 最近 tag
#   scripts/changelog-check.sh <prev-tag> <cur-tag>
# 退出码: 0 全部有记录 / 1 存在缺失 / 2 用法或环境错误
#
# 匹配规则（按序，PRE.18 的"自记录豁免"必须【先于】主匹配逻辑）:
#   1. merge commit            → 跳过（squash 常规路径下不存在；直接推送的 merge 不单独记录）
#   2. `docs:` / `docs(scope):` → 自记录豁免（记录他人 commit 的文档提交，天然不可能记录自己
#                                 ⇒ 否则无限回归：#25 记录 #23/#24，但 #25 自己也需要被记录）
#   3. subject 含 `(#N)`        → 要求 CHANGELOG 出现 `(#N)`（squash 合并的常规路径）
#   4. 无 PR 编号               → 回退关键词匹配：取 subject 去类型前缀后的首个词，要求出现在
#                                 CHANGELOG（直接 commit 路径）
set -euo pipefail

CHANGELOG="${CHANGELOG:-CHANGELOG.md}"

usage() {
  echo "usage: $0 <cur-tag> | $0 <prev-tag> <cur-tag>" >&2
  exit 2
}

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  usage
fi

if [ $# -eq 2 ]; then
  PREV_TAG="$1"
  CUR_TAG="$2"
else
  CUR_TAG="$1"
  # prev = 相对 cur 的最近可达 tag（不含 cur 自身）
  PREV_TAG="$(git describe --tags --abbrev=0 --exclude="${CUR_TAG}" "${CUR_TAG}^" 2>/dev/null || true)"
  if [ -z "${PREV_TAG}" ]; then
    echo "changelog-check: 无法自动推断 ${CUR_TAG} 的前一 tag；请显式传 <prev-tag> <cur-tag>" >&2
    exit 2
  fi
fi

[ -f "${CHANGELOG}" ] || { echo "changelog-check: 找不到 ${CHANGELOG}" >&2; exit 2; }
git rev-parse -q --verify "${PREV_TAG}^{commit}" >/dev/null \
  || { echo "changelog-check: 未知 prev tag/rev: ${PREV_TAG}" >&2; exit 2; }
git rev-parse -q --verify "${CUR_TAG}^{commit}" >/dev/null \
  || { echo "changelog-check: 未知 cur tag/rev: ${CUR_TAG}" >&2; exit 2; }

doc="$(cat "${CHANGELOG}")"

missing=()
missing_n=0
checked=0
skip_docs=0
skip_merge=0

while IFS= read -r sha; do
  subject="$(git log -1 --pretty=%s "${sha}")"
  parents="$(git log -1 --pretty=%P "${sha}")"
  parent_n="$(printf '%s' "${parents}" | wc -w | tr -d ' ')"

  if [ "${parent_n}" -gt 1 ]; then
    skip_merge=$((skip_merge + 1))
    continue
  fi

  # ⚠️ PRE.18：自记录豁免必须置于主匹配逻辑之前
  case "${subject}" in
    docs:*|docs\(*\):*)
      skip_docs=$((skip_docs + 1))
      continue
      ;;
  esac

  checked=$((checked + 1))

  pr="$(printf '%s' "${subject}" | grep -oE '\(#[0-9]+\)' | head -1 | tr -d '()' || true)"
  if [ -n "${pr}" ]; then
    if printf '%s' "${doc}" | grep -qF "(${pr})"; then
      continue
    fi
    missing+=("${sha:0:9}  ${subject}  [需 CHANGELOG 含 (${pr})]")
    missing_n=$((missing_n + 1))
    continue
  fi

  key="$(printf '%s' "${subject}" | sed -E 's/^[a-zA-Z]+(\([^)]*\))?!?:[[:space:]]*//' | awk '{print $1}')"
  if [ -n "${key}" ] && printf '%s' "${doc}" | grep -qF "${key}"; then
    continue
  fi
  missing+=("${sha:0:9}  ${subject}  [无 PR 编号，且关键词 '${key}' 未见于 ${CHANGELOG}]")
  missing_n=$((missing_n + 1))
done < <(git rev-list --no-merges "${PREV_TAG}..${CUR_TAG}")

if [ "${missing_n}" -gt 0 ]; then
  echo "changelog-check FAILED: ${CUR_TAG} 区间(${PREV_TAG}..${CUR_TAG}) 有 ${missing_n} 个 commit 未记入 ${CHANGELOG}" >&2
  printf '  %s\n' "${missing[@]}" >&2
  exit 1
fi

echo "changelog-check PASSED: ${PREV_TAG}..${CUR_TAG} 共 ${checked} 个需记录 commit 全部覆盖（跳过 ${skip_docs} 个 docs 自记录 / ${skip_merge} 个 merge）"
