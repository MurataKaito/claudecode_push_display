# Clawd表示 ＆ プロジェクト現状 まとめ（2026-06-04）

原案 `2026-06-04-claudecode-push-display-design.md` から実装が進み、画面キャラを
**Claude Codeマスコット「Clawd」風（ClaudePixのドット絵）**に差し替え、状態別アニメ化した。
このファイルは「今どこまで出来てるか／どう作ってあるか／次に何をするか」の索引。

---

## 1. これは何
Claude Code の状態を、机上の **M5Stack Core2** のキャラが声＋画面で知らせるデバイス。
- 母艦デーモン(Mac, Node/TS) ＋ Claude Codeフック(Stop/PreToolUse) ＋ M5ファーム(PlatformIO)
- ブランチ `feature/zundamon-push-display`（全コミット済み）

## 2. 機能と状態（すべて実機確認済み）
| 機能 | 内容 | 状態 |
|---|---|---|
| P1 タスク完了通知 | Stopフック→daemon→VOICEVOX/say→M5が発話＋アニメ | ✅ |
| M3 使用率表示 | 画面2回タッチ→ccusage→ゲージ | ✅ |
| M4 残量警告 | daemonが閾値(50/80/95%)監視→声＋sleep顔 | ✅ |
| M5 タップ承認 | PreToolUse→M5「CLAUDE Bash OK?」→タップ=許可/スワイプ=拒否 | ✅ |
| Clawd表示 | ClaudePixのドット絵を状態別アニメ（完了はランダムダンス） | ✅ |

## 3. Clawd 表示パイプライン（重要）
画面キャラは **ClaudePix（https://claudepix.vercel.app/）の20×20ドット絵アニメ**を移植して表示。
M5はHTMLを描画できないので「フレーム抽出→PNG連番→ネイティブ描画」する。

### ソース
- ClaudePixの `app.js` 内 `MANIFEST` に13アニメのファイル名。各 `animations/<file>` で公開配信。
- 取得済み: `assets/clawd/html/*.html`（13個）＋ `creature-engine.js`。**gitignore**（再配布しない／各自取得）。

### 2つのフレーム形式
- **エンジン型**: `creature-engine.js` の `window.PRESET.frames`（`{hold, frame|null}`、値 0空/1体/2目、色 体=#CD7F6A・目/背景=#0f0f0f）。
- **自己完結型(DJ系)**: `window.FRAMES`（`{hold, frame}`）＋ 多色パレット `window.PAL`。値はパレットindex。

### 変換ツール（`assets/clawd/`）
- `convert.mjs`（Node, vm）: HTMLの inline script を実行し、両形式から `{frames:[{hold,grid}], pal:[css色]}` を出力。ブラウザ不要。
- `render.py`（PIL）: holdに応じ **固定dt=180msでサンプリング**、`<prefix>0..N.png`（**200×200・黒地#0f0f0f・パレット多色・最大24枚**）を `firmware/data/` に生成。
- 例: `node convert.mjs html/idle_look_around.html html/creature-engine.js > a.json && python3 render.py a.json ../../firmware/data idle`

### 状態→クリップ割り当て（firmware）
| 状態(expr) | prefix | 元アニメ |
|---|---|---|
| 待機 normal | `idle` | idle_look_around |
| 完了 happy | ランダム: `djmix`/`sway`/`bouncedj`/`swaydj` | dance_* |
| 警告 worried | `sleep` | expression_sleep |
| 驚き surprised | `bounce` | dance_bounce（現状トリガ無し） |

### firmware描画（`firmware/src/display.cpp`）
- `M5Canvas`(320×240, PSRAM)でフリッカーレス。背景 `#0f0f0f`（PNGと同色＝四角の縁が出ない）。
- 起動時 `countFrames(prefix)` で各クリップ枚数を数え `g_pngMode=(idle>0)`。
- `tickFace(expr,text)`: 90msごと、exprが変わったら `selectClip`（happiはランダム）＋frame=0。
- `renderPngFrame`: `idx=(frame/2)%枚数` → `<prefix>idx.png` を **(60,0)** に描画（200幅を水平中央・高さフィット）。
- PNGが無ければコード描画の自作クリーチャーにフォールバック。
- `GET /state` の `dbg` で `png/idle/sleep/bounce/active/happy枚数` を確認可。

## 4. アニメの追加・差し替え手順
1. `assets/clawd/html/<name>.html` を用意（ClaudePixから or 自作）
2. `node convert.mjs html/<name>.html html/creature-engine.js > /tmp/a.json`
3. `python3 render.py /tmp/a.json firmware/data <prefix>`（prefixは状態名 idle/sleep/bounce/djmix…）
4. `cd firmware && pio run -e core2 -t uploadfs`（FS書き込み）＋ 必要なら `-t upload`
5. 状態追加時は `display.cpp` の `selectClip`/`countFrames` に prefix を足す

## 5. 運用メモ／ハマりどころ
- **USB**: M5は **USB-CデータケーブルでMac本体に直挿し**（CH9102, `/dev/cu.usbserial-*`）。充電専用ケーブル/ドック経由はNG。
- **WiFi**: ESP32は**2.4GHzのみ**。iPhoneテザリングは「**互換性を優先**」ON。
- **mDNS不安定**: テザリングでは `stackchan.local` が時々引けない → daemonは **`ZUNDA_M5_URL=http://<M5のIP>`** でIP直指定（今回 172.20.10.2、Mac 172.20.10.5）。
- **daemon起動**: `cd daemon && ZUNDA_M5_URL=http://172.20.10.2 ZUNDA_APPROVE_TIMEOUT_SEC=30 npm run dev`
- **声**: VOICEVOX未起動時は macOS `say` に自動フォールバック（speaker=3 ずんだもん）。
- **フック**: `~/.claude/settings.json` に Stop と PreToolUse(Bash) 登録済み（gitignore外＝手動／`!`で登録）。daemon停止中はフックは素通り。
- **LittleFS**: `board_build.filesystem = littlefs`。`firmware/data/` を `uploadfs`。素材PNGはgitignore。

## 6. 設定（環境変数, daemon）
`ZUNDA_PORT`(4920) / `ZUNDA_M5_URL`(http://stackchan.local) / `ZUNDA_VOICEVOX_URL`(:50021) /
`ZUNDA_SPEAKER_ID`(3) / `ZUNDA_USAGE_LIMIT`(1e8) / `ZUNDA_THRESHOLDS`(50,80,95) /
`ZUNDA_POLL_SEC`(60) / `ZUNDA_APPROVE_TIMEOUT_SEC`(30)

## 7. クレジット／権利
- 音声: **VOICEVOX:ずんだもん**
- 画面キャラ: ClaudePix（claudepix.vercel.app）のドット絵を移植。明示ライセンス無し＝**個人デバイス用途・各自判断**。Clawd は Anthropic の Claude Code マスコット。素材はリポジトリにコミットしない。

## 8. 次の一手（TODO）
- [ ] daemon常駐の自動起動（launchd）＋ M5 IP自動解決（mDNS再試行/設定UI）
- [ ] 驚き(surprised)状態のトリガ（例: 特定イベント）→ bounce
- [ ] アニメ速度/dtの調整（今180ms固定）と per-frame hold の忠実再現
- [ ] 立ち絵JPG等の他キャラ対応（prefix切替で既に差し替え可能）
- [ ] ブランチ仕上げ（mainマージ / PR）
- [ ] VOICEVOX導入手順の自動化（任意）
