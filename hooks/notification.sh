#!/usr/bin/env bash
# Claude Code Notification hook: 入力待ち等の通知 → surprised（注目アニメ）
# 失敗してもClaudeを止めない（常に exit 0）。
DAEMON_URL="${ZUNDA_DAEMON_URL:-http://127.0.0.1:4920}"
cat >/dev/null 2>&1 || true
curl -s -m 3 -X POST "$DAEMON_URL/event" \
  -H 'Content-Type: application/json' \
  -d '{"type":"attention"}' >/dev/null 2>&1 || true
exit 0
