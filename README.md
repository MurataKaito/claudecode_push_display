# claudecode_push_display — Claude Code の状態を知らせる机上の相棒

Claude Code の状態を、机の上の **M5Stack Core2** のキャラ「**Clawd**」が **画面アニメ＋音声** で「物理的な相棒」として知らせる。

> **キャラ構成**：顔は **Clawd**（Anthropic Claude Code マスコット。[ClaudePix](https://claudepix.vercel.app/) のドット絵アニメを移植）、声は **VOICEVOX:ずんだもん**。
> この「顔Clawd × 声ずんだ」のハイブリッドは **最初から狙った意図的な構成**（ミスマッチではなく、これがコンセプト）。

## 実装済み機能

| 機能 | きっかけ | 動作 |
|---|---|---|
| **タスク完了通知**（主役） | `Stop` | Clawdがダンス＋ずんだ声「おしごと、おわったのだ！」 |
| **作業中表示** | `UserPromptSubmit` | working アニメ＋ずんだ声「おしごと、するのだ！」 |
| **入力待ち** | `Notification` | surprise顔（**無音**） |
| **使用率表示** | 画面タップ（単／ダブル） | 5時間枠の使用率%をゲージ表示（約5秒） |
| **残量警告** | 閾値（既定 50/80/95%）跨ぎ | worried＋ずんだ声「もう○○%つかったのだ」 |
| **タップ承認** | `PreToolUse`(Bash) | 「CLAUDE Bash OK?」→ **タップ=許可 / スワイプ=拒否**。許可時は wink＋ずんだ声「オッケーなのだ」 |

**音マップ**：喋るのは **working / done / worried / wink** の4つ。**待機(idle) と 入力待ち(surprised)** は無音（机上でうるさくない）。
声の合成は Mac 側（VOICEVOX、未起動時は macOS `say` に自動フォールバック）、再生は常に **M5 のスピーカー**。

## アーキテクチャ

```
Mac: Claude Code ─hooks(curl)→ 母艦デーモン:4920 ─┬─ VOICEVOX:50021 (ずんだWAV)
                                                  └─ ccusage (使用率集計)
        │
        └─ WiFi HTTP ⇄ M5Stack Core2 (Clawdアニメ描画 / スピーカー発話 / タッチ)
```

詳細な構成図・状態マシン・シーケンス図 → [`docs/superpowers/specs/2026-06-08-architecture-diagrams.md`](docs/superpowers/specs/2026-06-08-architecture-diagrams.md)

## 構成
```
daemon/        … Mac常駐デーモン (Node/TS, Fastify :4920): フック受け口・ccusage集計・VOICEVOX/say合成・M5通知・承認管理
hooks/         … Claude Code フック (Stop=完了 / UserPromptSubmit=作業中 / Notification=入力待ち / PreToolUse(Bash)=承認)
firmware/      … M5Stack Core2 ファーム (PlatformIO/M5Unified): 状態マシン・Clawdドット絵アニメ・スピーカー発話・HTTPサーバ
assets/clawd/  … Clawdドット絵パイプライン (ClaudePix HTML → convert.mjs → render.py → LittleFS用PNG)
docs/superpowers/ … 設計書(specs)・設計図(diagrams)・実装計画(plans)
```

## セットアップ
1. **daemon**: `cd daemon && npm install && npm run dev`（既定 `:4920`）
2. **VOICEVOX（ずんだ声）**: Docker で起動（未起動でも `say` フォールバックで動作）
   ```
   docker run --rm -d -p 50021:50021 --name voicevox voicevox/voicevox_engine:cpu-latest
   ```
3. **firmware**（M5Stack Core2）:
   - `firmware/include/secrets.h` に WiFi 情報を記入
   - Clawdアニメを LittleFS へ書き込み: `cd firmware && pio run -e core2 -t uploadfs`
     （フレーム(PNG)の生成手順は [`assets/clawd/README.md`](assets/clawd/README.md)）
   - 本体ファームを書き込み: `pio run -e core2 -t upload`
4. **フック登録**（`~/.claude/settings.json`、`<repo>` は本リポジトリの絶対パス）:
   - `Stop` → `bash <repo>/hooks/stop.sh`（完了通知）
   - `UserPromptSubmit` → `bash <repo>/hooks/userpromptsubmit.sh`（作業中）
   - `Notification` → `bash <repo>/hooks/notification.sh`（入力待ち）
   - `PreToolUse`（matcher `Bash`）→ `bash <repo>/hooks/pretooluse.sh`（承認）
   - ※新規セッションで有効。daemon 未起動時はフックは素通り（通常フロー）
5. **ネットワーク**: Mac と M5 を同一ネットワークに
   - 注意: ESP32 は **2.4GHz のみ**。iPhoneテザリングは「互換性を優先」を ON
   - 注意: テザリングでは mDNS(`stackchan.local`) が不安定なことがある → M5 の IP を直接指定して daemon を起動：
     `ZUNDA_M5_URL=http://<M5のIP> npm run dev`

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
- daemon: `cd daemon && npm test`（vitest） / `npm run build`
- hooks: `bash hooks/stop.test.sh` / `bash hooks/pretooluse.test.sh`
- firmware（ロジック）: `cd firmware && pio test -e native`（WAVパーサ・ジェスチャ判定）

## 診断
- M5 状態: `curl http://<M5のIP>/state` → `{daemonBase, recv, shown, touch, g(最後のジェスチャ), ready, lastId, dbg}`
- 任意クリップ強制表示（13種の確認）: `curl -X POST "http://<M5のIP>/clip?p=djmix&ms=4000"`

## ドキュメント
- 設計書（アーキ/状態マシン/I/F/パイプライン/制約）: [`docs/superpowers/specs/2026-06-05-system-design.md`](docs/superpowers/specs/2026-06-05-system-design.md)
- 設計図（構成図/状態マシン/シーケンス図, PNG付き）: [`docs/superpowers/specs/2026-06-08-architecture-diagrams.md`](docs/superpowers/specs/2026-06-08-architecture-diagrams.md)

## クレジット
- 音声: **VOICEVOX:ずんだもん**
- 画面キャラ: **Clawd**（Anthropic Claude Code マスコット）。ドット絵は [ClaudePix](https://claudepix.vercel.app/) を移植
- ※素材PNG・ClaudePix HTML はリポジトリにコミットしない（`.gitignore`）。各自で取得すること
- 「顔Clawd × 声ずんだ」のハイブリッドは最初から狙った意図的な構成（これがコンセプト）
