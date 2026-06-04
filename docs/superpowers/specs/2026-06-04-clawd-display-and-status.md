# Clawd表示 ＆ プロジェクト現状 まとめ（2026-06-04 / 更新 2026-06-05）

原案 `2026-06-04-claudecode-push-display-design.md` から実装が進み、画面キャラを
**Claude Codeマスコット「Clawd」風（ClaudePixのドット絵）**に差し替え、**13クリップを状態プールで使い分ける**ようにした。
このファイルは「今どこまで出来てるか／どう作ってあるか／次に何をするか」の索引。

---

## 1. これは何
Claude Code の状態を、机上の **M5Stack Core2** のキャラが声＋画面で知らせるデバイス。
- 母艦デーモン(Mac, Node/TS) ＋ Claude Codeフック(Stop/PreToolUse) ＋ M5ファーム(PlatformIO)
- ブランチ `feature/zundamon-push-display`（全コミット済み）

## 2. 機能と状態
| 機能 | 内容 | 状態 |
|---|---|---|
| P1 タスク完了通知 | Stopフック→daemon→VOICEVOX/say→M5が発話＋アニメ | ✅ |
| M3 使用率表示 | 画面2回タッチ→ccusage→ゲージ | ✅ |
| M4 残量警告 | daemonが閾値(50/80/95%)監視→声＋sleep顔 | ✅ |
| M5 タップ承認 | PreToolUse→M5「CLAUDE Bash OK?」→タップ=許可/スワイプ=拒否 | ✅ |
| Clawd表示(13クリップ) | ClaudePixの13アニメを状態プールで表示（実機ショーケース確認済み） | ✅ |
| Phase B: 実イベント連携 | working状態＋UserPromptSubmit/Notification連携 | ⏳ 未 |

## 3. Clawd 表示パイプライン
画面キャラは **ClaudePix（https://claudepix.vercel.app/）の20×20ドット絵アニメ13個**を移植。
M5はHTMLを描画できないので「フレーム抽出→PNG連番→ネイティブ描画」する。

### ソース取得
- ClaudePix `app.js` の `MANIFEST` に13ファイル名。各 `animations/<file>` で公開配信。`assets/clawd/html/` に取得済み（engine含む、**gitignore**）。
- 2形式: エンジン型(`window.PRESET.frames`, 値0空/1体/2目) と 自己完結型DJ系(`window.FRAMES`＋多色`window.PAL`)。

### 変換ツール（`assets/clawd/`）
- `convert.mjs`（Node/vm）: 両形式から `{frames:[{hold,grid}], pal:[css色]}` を抽出（ブラウザ不要）。
- `render.py`（PIL）: holdを**固定dt=180msでサンプリング**、`<prefix>0..N.png`（**200×200・RGB・黒地#0f0f0f・最大16枚**、各ドットに inset影風の枠）を `firmware/data/` に生成。
- 生成例: `node convert.mjs html/work_coding.html html/creature-engine.js > a.json && python3 render.py a.json ../../firmware/data coding`

### 13クリップ → 状態プール（firmware）
| 状態(expr) | プール（ランダム選択） | 元アニメ prefix |
|---|---|---|
| 待機 idle/normal（数秒ごとローテ） | breathe / blink / look | idle_breathe / idle_blink / idle_look_around |
| 作業 working（ローテ） | coding / think | work_coding / work_think |
| 完了 happy/done（5種ランダム） | bounce / sway / djmix / bouncedj / swaydj | dance_* |
| 警告 worried | sleep | expression_sleep |
| 注目 surprised | surprise | expression_surprise |
| 承認OK wink | wink | expression_wink |

### firmware描画（`firmware/src/display.cpp`）
- `M5Canvas`(320×240, PSRAM)でフリッカーレス。背景 `#0f0f0f`（PNGと同色＝四角の縁が出ない）。
- 起動時に13 prefix のフレーム数を `countFrames` し `g_pngMode=(look>0)`。
- `tickFace(expr,text)`: 90msごと。exprが変わったら `selectClip`（プールからランダム）＋frame=0。**ベース状態(idle/working)は数秒ごとに再選択＝ローテ**。一時状態(done/worried/surprised/wink)は1回選んで `main` の `notifyUntil` で数秒後にベースへ戻る。
- `renderPngFrame`: `idx=(frame/2)%枚数` → `<prefix>idx.png` を **(60,0)** に描画（200幅を水平中央・高さフィット）。
- PNGが無ければコード描画フォールバック。
- デバッグ: `GET /state` の `dbg`（png/active/各クリップ枚数）、`POST /clip?p=<prefix>&ms=<ms>`（任意クリップ強制表示）。

### LittleFS
- パーティション **3.5MB**（容量余裕）。`board_build.filesystem = littlefs`。`firmware/data/` を `uploadfs`。素材PNGは**gitignore**。
- ※uploadfsは新ファイルを書くがファームが旧prefixを探すと不一致でフォールバックする。**FSとファームはセットで更新**。

## 4. アニメの追加・差し替え手順
1. `assets/clawd/html/<name>.html` を用意（ClaudePix or 自作）
2. `node assets/clawd/convert.mjs <html> assets/clawd/html/creature-engine.js > /tmp/a.json`
3. `python3 assets/clawd/render.py /tmp/a.json firmware/data <prefix>`
4. `cd firmware && pio run -e core2 -t uploadfs`（＋ prefixを増やすなら `display.cpp` の `PFX[]`/プールに追加して `-t upload`）
5. 確認: `POST /clip?p=<prefix>` で単体表示

## 5. 運用メモ／ハマりどころ
- **USB**: M5は **USB-CデータケーブルでMac本体に直挿し**（CH9102, `/dev/cu.usbserial-*`）。充電専用/ドック経由はNG。
- **WiFi**: ESP32は**2.4GHzのみ**。iPhoneテザリングは「**互換性を優先**」ON。
- **mDNS不安定**: テザリングでは `stackchan.local` が引けないことがある → daemonは **`ZUNDA_M5_URL=http://<M5のIP>`** でIP直指定（今回 172.20.10.2 / Mac 172.20.10.5）。
- **daemon起動**: `cd daemon && ZUNDA_M5_URL=http://172.20.10.2 ZUNDA_APPROVE_TIMEOUT_SEC=30 npm run dev`
- **声**: VOICEVOX未起動時は macOS `say` に自動フォールバック（speaker=3）。
- **フック**: `~/.claude/settings.json` に Stop / PreToolUse(Bash) 登録済み（`!`でpython追記）。daemon停止中は素通り。

## 6. 設定（環境変数, daemon）
`ZUNDA_PORT`(4920) / `ZUNDA_M5_URL`(http://stackchan.local) / `ZUNDA_VOICEVOX_URL`(:50021) /
`ZUNDA_SPEAKER_ID`(3) / `ZUNDA_USAGE_LIMIT`(1e8) / `ZUNDA_THRESHOLDS`(50,80,95) /
`ZUNDA_POLL_SEC`(60) / `ZUNDA_APPROVE_TIMEOUT_SEC`(30)

## 7. クレジット／権利
- 音声: **VOICEVOX:ずんだもん**
- 画面キャラ: **ClaudePix**（claudepix.vercel.app）のドット絵を移植。明示ライセンス無し＝**個人デバイス用途・各自判断**。Clawd は Anthropic の Claude Code マスコット。素材はリポジトリにコミットしない（gitignore）。

## 8. 次の一手（TODO）
- [ ] **Phase B（実イベント連携）**：
  - `UserPromptSubmit` フック → `POST /base?s=working`（作業中状態に。`/base` エンドポイント＋ `g_baseState` を新設）
  - `Stop` → done演出後に `/base?s=idle`
  - `Notification`（入力待ち）→ `/notify expr=surprised`
  - 承認タップ → firmwareで `wink`（ローカル）
  - `main.cpp` でベース(idle/working)＋一時overlayの2層に整理、settings.jsonにフック登録
- [ ] アニメ速度/dtの調整（今180ms）と per-frame hold の忠実化
- [ ] 驚き(surprised)の常用トリガ確定
- [ ] launchdでdaemon常駐＋M5 IP自動解決
- [ ] ブランチ仕上げ（mainマージ / PR）
