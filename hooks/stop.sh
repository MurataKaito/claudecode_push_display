#!/usr/bin/env bash
# Claude Code Stop hook: notify the zundamon daemon that a task finished.
# 失敗してもClaudeを止めない（常に exit 0）。
DAEMON_URL="${ZUNDA_DAEMON_URL:-http://127.0.0.1:4920}"
# フックのstdin(JSON)は読み捨てる
cat >/dev/null 2>&1 || true
curl -s -m 3 -X POST "$DAEMON_URL/event" \
  -H 'Content-Type: application/json' \
  -d '{"type":"done"}' >/dev/null 2>&1 || true
exit 0
