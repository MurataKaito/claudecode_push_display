# トークン残量お知らせずんだもん（claudecode_push_display）設計

- 作成日: 2026-06-04
- 元ネタ: ProtoPedia #8507「トークン残量お知らせずんだもん🫛」(YS_beaver) のクローン／発展
- ステータス: 設計確定（実装計画はこの後 writing-plans で作成）

---

## 1. 目的・コンセプト

机の上の **M5Stack Core2** に **ずんだもん** を表示し、Claude Code の状態を「物理的な相棒」として知らせる。
ターミナルに張り付かなくても、**視界の端でずんだもんが反応する**ことで「終わったな」とすぐ気づける。
通知が画面のポップアップではなく、机の上の存在として返ってくることが価値。

### 一番の役割
**タスク完了通知**（声＋画面リアクション）。これが主役。

### スコープに含む機能（4つ）
1. **タスク完了通知**（主役）: Claude Code が応答／作業を終えたら、ずんだもんが表情変化＋発話で知らせる。
2. **トークン残量の警告**: 5時間ウィンドウの使用率%が閾値を跨いだら、ずんだもんが声で警告。
3. **使用率のオンデマンド表示**: 画面を **2回タッチ**すると現在の使用率%をゲージ表示。
4. **許可リクエストの承認**: Claude Code の許可要求をM5に表示し、**タップ=許可 / スワイプ=拒否**で応答（PreToolUse フック）。

### スコープ外（将来）
- Codex / Antigravity の監視（今回は **Claude Code のみ**）
- サーボによる物理可動（**Core2 単体**。「動き」は画面アニメで表現）
- 「Claude版キャラ」への差し替え（v1完成後の追加。立ち絵を差し替えられる構成にしておく）

---

## 2. 確定事項（意思決定ログ）

| 項目 | 決定 |
|---|---|
| 監視対象 | Claude Code のみ |
| 通知トリガー | タスク完了 ＋ トークン残量低下 ＋ 画面2回タッチで使用率表示 ＋ 許可承認 |
| 残量の基準 | **5時間ウィンドウの使用率(%)**（`ccusage` で `~/.claude` ログを集計） |
| M5側 | **新規PlatformIOファーム**（Core2単体、サーボ無し） |
| 声 | **VOICEVOX ずんだもん**（speaker=3 ノーマル） |
| 音声の出し方 | **案B: WAVをM5へ送り Core2スピーカーで発話**（M5不達時はPCスピーカーへフォールバック） |
| 画面キャラ | **公式ずんだもん立ち絵**（表情差分）。立ち絵を差し替え可能な構成 |
| 通信 | **WiFi HTTP ＋ mDNS（stackchan.local）主軸 / USBシリアルをフォールバック**（通信層を抽象化） |

### クレジット表記（必須）
- 音声: `VOICEVOX:ずんだもん`
- キャラクター: 「東北ずん子・ずんだもん」キャラクター利用の手引きに従い、所定のクレジットを README とアプリ内に記載。

---

## 3. システム構成

```
┌──────────────────────── Mac (PC) ────────────────────────┐
│  Claude Code                                              │
│   ├─ Stop フック ─────────────┐                           │
│   └─ PreToolUse フック ───────┤ (curl)                    │
│                               ▼                           │
│   ~/.claude/projects/*.jsonl ─┼──► [ 母艦デーモン (Node) ] │
│        ▲                      │      ├─ HTTP server       │
│        └── npx ccusage ───────┘      ├─ 使用率ポーラ       │
│                                      ├─ 閾値判定           │
│   VOICEVOX (localhost:50021) ◀───────┤ WAV生成            │
│                                      └─ M5クライアント      │
│                                          │ ▲ WiFi HTTP     │
└──────────────────────────────────────────┼─┼──────────────┘
                       mDNS: stackchan.local│ │ heartbeat(daemon address)
                                            ▼ │
┌──────────────── M5Stack Core2 (自作ファーム) ──────────────┐
│  WiFi + mDNS + HTTPサーバ                                  │
│  画面: ずんだもん立ち絵/表情 ・ テキスト ・ 使用率ゲージ        │
│  タッチ: タップ / ダブルタップ / スワイプ                    │
│  スピーカー: VOICEVOX WAV 再生                              │
└────────────────────────────────────────────────────────────┘
```

### コンポーネントと責務

| コンポーネント | 役割 | 依存 |
|---|---|---|
| **母艦デーモン** (Node/TS, 常駐) | 司令塔。フック受け口・使用率算出・閾値判定・VOICEVOX呼出し・M5への通知/承認依頼 | ccusage, VOICEVOX, M5 |
| **Stop フック** (sh) | タスク完了時に daemon `/event` を叩くだけ（即時・非ブロッキング） | daemon |
| **PreToolUse フック** (sh) | 許可要求を daemon `/approve` に送り、**結果が返るまで待って** allow/deny を返す | daemon |
| **M5ファーム** (PlatformIO/C++) | 表示・タッチ・発話。daemon の push を受け、タッチ時は daemon に問い合わせ | daemon |
| **assets** | 公式ずんだもん立ち絵（表情差分）をM5用に変換した画像 | ― |

通信層は `Transport` インターフェイス（`notify` / `requestApproval` / `getUsage`）として抽象化。`WiFiTransport` を主実装、`SerialTransport` をフォールバック。

---

## 4. データフロー（4機能）

### ① タスク完了通知（主役）
```
Claude完了 → Stopフック(stdin読取) → daemon POST /event {type:"done", ...}
  → daemon: セリフ生成（例「終わったのだ！」）→ VOICEVOXでWAV化
  → daemon → M5 POST /notify (?expr=happy&text=...) body=WAV
  → M5: 笑顔表示＋軽いアニメ＋WAV再生 → 数秒後 IDLEへ
```

### ② トークン残量の警告
```
daemon ポーラ(既定60秒): npx ccusage blocks --active --json
  → 5時間枠の used トークンと limit から 使用率% を算出
  → 閾値(既定: 使用50% / 80% / 95%)を新たに跨いだら、その閾値につき1回だけ
  → セリフ生成（例「もう8割使ったのだ、気をつけるのだ！」）→ WAV化
  → M5 POST /notify (?expr=worried) body=WAV
  ※ ウィンドウがリセット(resetAt経過)したら閾値到達フラグをクリア
```
- `limit`（分母）は設定値。既定は `ccusage` が示す直近ブロック最大値(`max`相当)か、ユーザ設定値。サブスクは正確なトークン上限を公開しないため、**使用率は設定された分母に対する目安**である旨をREADMEに明記。

### ③ 画面2回タッチで使用率表示
```
M5 ダブルタップ検知(IDLE時) → M5 → daemon GET /usage
  → daemon: {percent, used, limit, resetAt} を返す
  → M5: 円ゲージ＋数値＋リセットまでの時間を数秒(またはタップで)表示 → IDLEへ
  ※ daemon未到達なら「？」表示
```

### ④ 許可リクエストの承認（TAP OK / SWIPE NG）
```
Claudeがツール実行前 → PreToolUseフック(stdinでtool情報受取)
  → daemon POST /approve {id, tool, command, cwd}  （daemonは即 id を返す）
  → daemon → M5 POST /approve {id, title:"CLAUDE BASH OK?", detail:cmd}（M5即ACK）
  → M5: 承認画面表示。タップ=allow / スワイプ=deny
  → M5 → daemon POST /approve_result/{id} {decision}
  → フックは daemon GET /approve_result/{id} をポーリング → decision取得
  → フック出力: permissionDecision = allow | deny
  ※ 無操作タイムアウト(既定30秒)時: フックは "ask" を出力（通常の許可プロンプトにフォールバック）、M5はIDLEへ
  ※ 対象ツールは matcher で限定（既定: Bash）。Read等まで承認すると煩雑なため。
```

---

## 5. M5Stack Core2 ファーム設計

- **PlatformIO** / framework=arduino / board=`m5stack-core2`
- ライブラリ: **M5Unified**（描画・タッチ・スピーカー）, `WiFi`, `ESPmDNS`, `WebServer`, `HTTPClient`, `ArduinoJson`

### 画面ステート
| State | 内容 | 遷移 |
|---|---|---|
| IDLE | ずんだもん待機（まばたき等の軽アニメ） | ダブルタップ→USAGE / push受信→NOTIFY,APPROVE |
| NOTIFY | 表情(happy/worried)＋テキスト枠＋WAV再生 | 再生後 数秒で IDLE |
| USAGE | 使用率ゲージ＋数値＋resetまで | 数秒 or タップで IDLE |
| APPROVE | プロンプト枠＋「TAP OK / SWIPE NG」 | タップ→allow / スワイプ→deny / timeout→IDLE |

### タッチ判定（M5.Touch）
- **タップ**: 短押し・移動なし
- **ダブルタップ**: 約400ms以内に2回タップ（IDLEで使用率要求）
- **スワイプ**: 一定距離以上の移動（APPROVEで拒否）
- ステートマシンで曖昧さを排除。チャタリング対策のデバウンス。

### 音声再生
- VOICEVOX WAV（既定 24kHz/16bit/mono）を `/notify` の body で受信。
- 44バイトWAVヘッダを除去し PCM を **PSRAM** にバッファ → `M5.Speaker.playRaw(int16*, len, sampleRate)`。
- 長尺はチャンク再生。再生中の多重通知はキュー or 上書き（既定: 直近優先で上書き）。

### 立ち絵アセット
- 公式立ち絵を表情別（normal / happy / worried / surprised）に用意し、320×240へリサイズ。
- 形式は JPG もしくは RGB565 バイナリ。**LittleFS** に格納（差し替え容易）。M5Unified で描画。
- 「Claude版」差し替えを見据え、アセットはパス参照（ハードコードしない）。

### ネットワーク
- WiFi接続 → `MDNS.begin("stackchan")` で `stackchan.local`。HTTPサーバ port 80。
- **daemonアドレス学習**: 受信した daemon リクエストの送信元IP、および daemon からの定期 heartbeat を記録。タッチ起点の `GET /usage`・`POST /approve_result` に使用。起動直後でアドレス未知ならheartbeat受信まで「未接続」表示。

### M5側エンドポイント
| メソッド/パス | 用途 |
|---|---|
| `POST /notify?expr=&text=` (body: WAV) | 通知表示＋発話 |
| `POST /approve` (JSON: id,title,detail) | 承認画面表示（即ACK） |
| `POST /heartbeat` (JSON: addr) | daemonアドレス更新＋死活 |

---

## 6. 母艦デーモン設計（Node.js v22 / TypeScript）

- HTTPサーバ（Fastify もしくは native http）port 既定 `4920`。
- 設定: `daemon/config.json`（port, voicevoxUrl, speakerId=3, thresholds, usageLimit, m5Host=stackchan.local, pollIntervalSec, approveTimeoutSec, approveToolMatcher）。

### エンドポイント
| メソッド/パス | 呼び出し元 | 動作 |
|---|---|---|
| `POST /event` | Stopフック | セリフ生成→VOICEVOX→M5 /notify |
| `POST /approve` | PreToolUseフック | M5 /approve に転送、pendingに登録、id返却 |
| `GET /approve_result/:id` | PreToolUseフック | {decision: allow\|deny\|pending\|timeout} |
| `POST /approve_result/:id` | M5 | ユーザのタップ/スワイプ結果を確定 |
| `GET /usage` | M5 | 現在の使用率 {percent,used,limit,resetAt} |

### モジュール
- `ccusage.ts`: `npx ccusage blocks --active --json` を実行・パースし、usedトークン・resetAtを取得。失敗時は null。
- `usage.ts`: percent = used/limit*100 を算出。limit解決ロジック（設定値 or ccusage baseline）。
- `poller.ts`: `pollIntervalSec` 毎に使用率取得→閾値跨ぎ判定（前回値と比較、ウィンドウ毎に発火済みフラグ）。
- `voicevox.ts`: `POST /audio_query?text=&speaker=3` → `POST /synthesis?speaker=3` → WAV。よく使うセリフはキャッシュ。
- `serif.ts`: イベント種別→ずんだもんセリフ文言（「〜のだ」調）。
- `m5client.ts` / `transport/`: `Transport` IF（`notify`/`requestApproval`/`getUsage`）。`WiFiTransport`(fetch to stackchan.local) と `SerialTransport`(USB) を切替。
- `approvals.ts`: pending承認の管理（id, 期限, 結果）。

---

## 7. フック設計（Claude Code settings.json）

### Stop フック（hooks/stop.sh）
- stdin の JSON（session情報等）を読み、`curl -s -X POST localhost:4920/event -d '{"type":"done",...}'`。即 exit 0（ブロッキングしない）。

### PreToolUse フック（hooks/pretooluse.sh）
- matcher: 既定 `Bash`（設定で拡張可）。
- stdin から tool_name / tool_input を取得 → `POST /approve` で id 取得。
- `GET /approve_result/:id` を短間隔ポーリング（最大 approveTimeoutSec）。
- 結果に応じ標準出力へ:
  ```json
  {"hookSpecificOutput":{"hookEventName":"PreToolUse",
    "permissionDecision":"allow|deny",
    "permissionDecisionReason":"approved on stackchan (tap/swipe)"}}
  ```
- タイムアウト時は `permissionDecision":"ask"`（通常プロンプトへフォールバック）。
- daemon 不達時も `ask` を返し、Claudeを止めない。

### settings.json への登録例（雛形）
```json
{
  "hooks": {
    "Stop": [{"hooks":[{"type":"command","command":"bash <repo>/hooks/stop.sh"}]}],
    "PreToolUse": [{"matcher":"Bash","hooks":[{"type":"command","command":"bash <repo>/hooks/pretooluse.sh"}]}]
  }
}
```

---

## 8. エラー処理・エッジケース

| 事象 | 挙動 |
|---|---|
| M5 不達（通知） | 数回リトライ後、**PCスピーカー(afplay)で発話**してフォールバック（通知を落とさない） |
| M5 不達（承認） | フックは `ask` を返し通常プロンプトへ（Claudeが固まらない） |
| VOICEVOX 未起動 | 健康チェックで検知 → **macOS `say`** で代替発話＋起動ヒントをログ |
| ccusage 失敗 / データ無し | 使用率は「不明」。誤警告は出さない |
| 承認タイムアウト | `ask` フォールバック。M5はIDLEへ復帰 |
| テザリングでIP変動 | mDNS（stackchan.local）＋heartbeatで再発見 |
| 承認の多重発生 | daemonでキュー化。M5は1件ずつ処理 |
| 通知の多重発生 | M5側は直近優先で上書き（再生中断→新規） |

---

## 9. テスト戦略

- **デーモン（vitest）**: ccusageパース、閾値跨ぎ判定、セリフ生成、VOICEVOXクライアント（モック）、Transport（モックM5）。`/event`・`/approve` フローの統合テスト（モックM5 HTTPサーバ）。
- **フック**: モックdaemon（ローカルstub）に対し allow / deny / timeout の出力JSONを検証。
- **M5ファーム**: タッチ・ジェスチャ判定をホスト側テスト可能な関数に切り出して単体テスト。実機は手動チェックリスト。
- **E2E（手動）**: 実Claude Code → ロボットが反応するまでを通し確認。プロジェクトの test-driven-development / verification-before-completion スキルは daemon のHTTP挙動に適用、ファームは手動検証で補完。

---

## 10. ディレクトリ構成

```
claudecode_push_display/
├── daemon/                  # Mac側 母艦デーモン (Node/TS)
│   ├── src/{server,ccusage,usage,poller,voicevox,serif,m5client,approvals,config}.ts
│   ├── src/transport/{index,wifi,serial}.ts
│   ├── test/
│   └── package.json
├── hooks/                   # Claude Code フック
│   ├── stop.sh
│   └── pretooluse.sh
├── firmware/                # M5Stack Core2 (PlatformIO)
│   ├── platformio.ini
│   └── src/{main,display,touch,audio,net}.cpp + include/
├── assets/zundamon/         # 立ち絵(表情差分) + 変換スクリプト
├── docs/superpowers/specs/
└── README.md                # セットアップ手順・クレジット表記
```

---

## 11. マイルストーン（writing-plans で詳細化）

- **M0 足場**: daemon/firmware/hooks のスケルトン、設定、ビルド確認。
- **M1 疎通**: WiFi＋mDNS、heartbeat、`/notify` で「こんにちはなのだ」表示までの往復。
- **M2 タスク完了通知（主役）**: Stopフック→daemon→VOICEVOX→M5 発話＋表情。
- **M3 使用率表示**: ccusage→`/usage`、M5ダブルタップでゲージ。
- **M4 残量警告**: ポーラ＋閾値判定＋発話。
- **M5 承認**: PreToolUseフック＋`/approve`＋M5タップ/スワイプ。
- **M6 仕上げ**: フォールバック（PC発話 / say / ask）、USBトランスポート、READMEとクレジット、手動E2E。

---

## 12. 前提・必要セットアップ

- VOICEVOX（無料）をMacにインストールし、エンジン（`localhost:50021`）を起動。
- `npx ccusage` が使用可能（node v22 確認済み）。
- M5Stack Core2 実機 ＋ PlatformIO 環境（既存 `M5Core2-claude-meetup-lt` の資産を流用可）。
- MacとM5を同一ネットワーク（自宅WiFi または iPhoneパーソナルホットスポット）に接続。
