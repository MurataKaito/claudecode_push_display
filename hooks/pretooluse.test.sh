#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'kill "${SRV_PID:-0}" 2>/dev/null || true; rm -rf "$TMP"' EXIT

python3 - 14921 <<'PY' &
import sys, json, http.server
port = int(sys.argv[1])
class H(http.server.BaseHTTPRequestHandler):
    def _send(self, obj):
        self.send_response(200); self.send_header("Content-Type","application/json"); self.end_headers()
        self.wfile.write(json.dumps(obj).encode())
    def do_POST(self):
        n=int(self.headers.get("Content-Length",0)); self.rfile.read(n)
        if self.path == "/approve": self._send({"id":"t1"})
        else: self._send({"ok":True})
    def do_GET(self):
        self._send({"decision":"allow"})
    def log_message(self,*a): pass
http.server.HTTPServer(("127.0.0.1",port),H).serve_forever()
PY
SRV_PID=$!
sleep 0.5

out=$(echo '{"tool_name":"Bash","tool_input":{"command":"ls"}}' | ZUNDA_DAEMON_URL="http://127.0.0.1:14921" bash "$HERE/pretooluse.sh")
echo "$out" | grep -q '"permissionDecision":"allow"' || { echo "FAIL: allowが出ない: $out"; exit 1; }
echo "PASS"
