# claudecode_push_display — トークン残量お知らせずんだもん

Claude Code の状態を、机の上の **M5Stack Core2** のずんだもんが「物理的な相棒」として知らせる。
（ProtoPedia #8507「トークン残量お知らせずんだもん🫛」にインスパイア）

## 実装済み機能（実機確認済み）

**Phase 1 — タスク完了通知**
Claude完了時に、ずんだもんが表情＋音声（M5スピーカー発話）で知らせる。

**Phase 2 — 使用率/警告/承認**
- **使用率表示**: 画面を2回タッチ → 5時間枠の使用率%をゲージ表示
- **残量警告**: 使用率が閾値(既定 50/80/95%)を跨ぐと、ずんだもんが声で警告
- **タップ承認**: Claudeのツール実行前にM5へ「CLAUDE BASH OK?」表示 → **タップ=許可 / スワイプ=拒否**（PreToolUseフック）

> 声は VOICEVOX:ずんだもん（未起動時は macOS `say` に自動フォールバック）。
> 顔は公式ずんだもん立ち絵（`firmware/data/` にJPGを置けば本物の顔に。無ければ仮の丸顔）。

## 構成
```
daemon/    … Mac常駐デーモン (Node/TS): フック受け口・ccusage集計・VOICEVOX/say・M5通知・承認管理
hooks/     … Claude Code フック (Stop=完了通知, PreToolUse=承認)
firmware/  … M5Stack Core2 ファーム (PlatformIO/M5Unified)
assets/    … ずんだもん立ち絵
docs/superpowers/ … 設計(spec)・実装計画(plan)
```

## セットアップ
1. **daemon**: `cd daemon && npm install && npm run dev`（既定 `:4920`）
2. **firmware**: `firmware/include/secrets.h` にWiFi情報を記入し
   `cd firmware && pio run -e core2 -t upload`
3. **フック登録**（`~/.claude/settings.json`）:
   - `Stop` → `bash <repo>/hooks/stop.sh`（完了通知）
   - `PreToolUse`（matcher: `Bash`）→ `bash <repo>/hooks/pretooluse.sh`（承認）
   - ※新規セッションで有効。daemon未起動時はフックは何もしない（通常フロー）
4. **ネットワーク**: Mac と M5 を同一ネットワークに
   - ⚠️ ESP32 は **2.4GHz のみ**。iPhoneテザリングは「互換性を優先」をON
   - ⚠️ **テザリングでは mDNS(`stackchan.local`) が不安定**なことがある。その場合は
     M5のIPを直接指定して daemon を起動： `ZUNDA_M5_URL=http://<M5のIP> npm run dev`
     （M5のIPは起動直後にdaemonの `GET /usage` 等で確認、または `dns-sd -q stackchan.local`）
5. （任意）**VOICEVOX** を起動（`http://127.0.0.1:50021`）すると声がずんだもんボイスに

## 設定（環境変数）
| 変数 | 既定 | 説明 |
|---|---|---|
| `ZUNDA_PORT` | 4920 | daemonポート |
| `ZUNDA_M5_URL` | http://stackchan.local | M5のURL（テザリング時はIP直指定推奨） |
| `ZUNDA_VOICEVOX_URL` | http://127.0.0.1:50021 | VOICEVOXエンジン |
| `ZUNDA_SPEAKER_ID` | 3 | VOICEVOX話者（3=ずんだもんノーマル） |
| `ZUNDA_USAGE_LIMIT` | 100000000 | 使用率の分母トークン数（プランに合わせ調整） |
| `ZUNDA_THRESHOLDS` | 50,80,95 | 警告閾値(%) |
| `ZUNDA_POLL_SEC` | 60 | 使用率ポーリング間隔 |
| `ZUNDA_APPROVE_TIMEOUT_SEC` | 30 | 承認待ちタイムアウト |

## テスト
- daemon: `cd daemon && npm test` / `npm run build`
- hooks: `bash hooks/stop.test.sh` / `bash hooks/pretooluse.test.sh`
- firmware(ロジック): `cd firmware && pio test -e native`（WAVパーサ・ジェスチャ判定）

## 診断
M5の状態確認: `curl http://<M5のIP>/state` → `{daemonBase, recv, shown, touch, ...}`

## クレジット
- 音声: **VOICEVOX:ずんだもん**
- キャラクター: 東北ずん子・ずんだもんプロジェクト（キャラクター利用の手引きに従う）
