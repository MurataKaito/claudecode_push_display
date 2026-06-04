#!/usr/bin/env bash
# Claude Code PreToolUse hook: ロボット上で許可/拒否を仰ぐ。
# daemon不達・タイムアウト時は何も出力せず exit 0（=通常の許可フロー/ask）。
DAEMON_URL="${ZUNDA_DAEMON_URL:-http://127.0.0.1:4920}"

input=$(cat)
tool=$(printf '%s' "$input" | python3 -c "import sys,json;print(json.load(sys.stdin).get('tool_name',''))" 2>/dev/null || echo "")
cmd=$(printf '%s' "$input" | python3 -c "import sys,json;d=json.load(sys.stdin);print((d.get('tool_input') or {}).get('command',''))" 2>/dev/null || echo "")

payload=$(python3 -c "import json,sys;print(json.dumps({'tool':sys.argv[1],'command':sys.argv[2]}))" "$tool" "$cmd" 2>/dev/null) || exit 0
resp=$(curl -s -m 5 -X POST "$DAEMON_URL/approve" -H 'Content-Type: application/json' -d "$payload") || exit 0
id=$(printf '%s' "$resp" | python3 -c "import sys,json;print(json.load(sys.stdin).get('id',''))" 2>/dev/null || echo "")
[ -z "$id" ] && exit 0

for _ in $(seq 1 60); do
  d=$(curl -s -m 3 "$DAEMON_URL/approve_result/$id" | python3 -c "import sys,json;print(json.load(sys.stdin).get('decision',''))" 2>/dev/null || echo "")
  case "$d" in
    allow)
      printf '%s\n' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow","permissionDecisionReason":"stackchan tap"}}'
      exit 0;;
    deny)
      printf '%s\n' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"stackchan swipe"}}'
      exit 0;;
    timeout|unknown)
      exit 0;;
  esac
  sleep 0.5
done
exit 0
