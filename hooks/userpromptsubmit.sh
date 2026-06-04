#!/usr/bin/env bash
# Claude Code UserPromptSubmit hook: Claudeが動き出した → working（作業中アニメ）
# 失敗してもClaudeを止めない（常に exit 0）。
DAEMON_URL="${ZUNDA_DAEMON_URL:-http://127.0.0.1:4920}"
cat >/dev/null 2>&1 || true
curl -s -m 3 -X POST "$DAEMON_URL/event" \
  -H 'Content-Type: application/json' \
  -d '{"type":"working"}' >/dev/null 2>&1 || true
exit 0
