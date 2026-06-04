#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'kill "${SRV_PID:-0}" 2>/dev/null || true; rm -rf "$TMP"' EXIT

# 受信ボディを $TMP/body に書くスタブサーバ (port 14920)
python3 - "$TMP/body" 14920 <<'PY' &
import sys, http.server
out, port = sys.argv[1], int(sys.argv[2])
class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        open(out, "wb").write(self.rfile.read(n))
        self.send_response(200); self.end_headers(); self.wfile.write(b"{}")
    def log_message(self, *a): pass
http.server.HTTPServer(("127.0.0.1", port), H).serve_forever()
PY
SRV_PID=$!
sleep 0.5

echo '{"hook_event_name":"Stop"}' | ZUNDA_DAEMON_URL="http://127.0.0.1:14920" bash "$HERE/stop.sh"
sleep 0.3

grep -q '"type":"done"' "$TMP/body" || { echo "FAIL: body にdoneイベントが無い"; cat "$TMP/body"; exit 1; }
echo "PASS"
