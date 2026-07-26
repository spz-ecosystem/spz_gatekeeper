#!/usr/bin/env bash
# Quick CI status check — no Python, just curl + jq (or sed fallback).
# Usage: bash scripts/ci-status.sh [TOKEN] [COUNT]
#   TOKEN  GitHub token (or set GITHUB_TOKEN env var)
#   COUNT  Number of runs to show (default: 5)

set -euo pipefail

TOKEN="${1:-${GITHUB_TOKEN:-}}"
COUNT="${2:-5}"
REPO="spz-ecosystem/spz_gatekeeper"

if [ -z "${TOKEN}" ]; then
  echo "Usage: $0 <GITHUB_TOKEN> [COUNT]"
  echo "  Or set GITHUB_TOKEN env var"
  exit 1
fi

# Fetch recent runs
RESPONSE=$(curl -sf \
  -H "Authorization: Bearer ${TOKEN}" \
  -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  "https://api.github.com/repos/${REPO}/actions/runs?per_page=${COUNT}" 2>&1)

if [ $? -ne 0 ]; then
  echo "Failed to fetch CI status"
  echo "${RESPONSE}"
  exit 1
fi

# Parse with python3 (always available in CI and WSL), much lighter than a full script
python3 -c "
import json, sys
data = json.loads(sys.stdin.read())
runs = data.get('workflow_runs', [])
if not runs:
    print('No CI runs found')
    sys.exit(0)
fmt = '{:<12} {:<8} {:<8} {:<30} {}'
print(fmt.format('RUN_ID', 'STATUS', 'RESULT', 'BRANCH', 'TITLE'))
print('-' * 90)
for r in runs:
    sid = str(r['id'])[-8:]
    status = r['status'][:7]
    result = (r.get('conclusion') or '...')[:7]
    branch = r.get('head_branch', '?')[:28]
    title = r.get('display_title', '?')[:40]
    icon = '✓' if result == 'success' else '✗' if result == 'failure' else '…'
    print(fmt.format(sid, status, f'{icon} {result}', branch, title))
" <<< "${RESPONSE}"
