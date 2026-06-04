# claudecode_push_display 設計書（v1 / 2026-06-05）

Claude Code の状態を、机上の **M5Stack Core2** のキャラ「**Clawd**」が**画面アニメ＋音声**で知らせるデスクトップ相棒。
ProtoPedia #8507「トークン残量お知らせずんだもん」をベースに、キャラをClawd（ClaudePixのドット絵）化し、
Claude Code のライフサイクル全体に連動させた。**声はVOICEVOXずんだもん（顔Clawd×声ずんだは意図的なハイブリッド）**。

- リポジトリ: `claudecode_push_display`／ブランチ `feature/zundamon-push-display`
- 関連: 原案 `2026-06-04-claudecode-push-display-design.md`、状況/手順 `2026-06-04-clawd-display-and-status.md`、各 `plans/*`

---

## 1. 目的・コンセプト
- ターミナルに張り付かなくても、**視界の端でClawdが動く**ことでClaudeの状態に気づける。
- 通知はポップアップでなく **机の上の物理的な存在** として返る。
- 主役は「タスク完了通知」。加えて作業中表示・使用率表示・残量警告・**操作承認（タップ/スワイプ）**。

## 2. 全体アーキテクチャ
```
┌────────────────────────── Mac ──────────────────────────┐
│  Claude Code                                             │
│   ├ UserPromptSubmit ─┐                                  │
│   ├ Stop ─────────────┤ hooks(curl)                      │
│   ├ Notification ─────┤                                  │
│   └ PreToolUse(Bash) ─┘                                  │
│                       ▼                                  │
│   ~/.claude/projects/*.jsonl ─ccusage─► [母艦デーモン:4920]│
│   VOICEVOX (Docker :50021) ◄─synth─────┤  ├ /event       │
│      └ ずんだWAV ──────────────────────┤  ├ /usage       │
│                                        │  ├ /approve*     │
│                                        │  └ poller(残量)  │
└────────────────────────────────────────┼──WiFi HTTP──────┘
                            (IP直指定/将来mDNS)│
                                             ▼
┌──────────────── M5Stack Core2 (自作ファーム) ───────────────┐
│ WiFi + AsyncWebServer(:80)                                  │
│ 受信: /notify(WAV) /base /approve /heartbeat /clip /state    │
│ M5Canvasでフリッカーレス描画（Clawdドット絵アニメ）           │
│ タッチ: ダブルタップ=使用率 / 承認画面 tap=許可 swipe=拒否     │
│ スピーカー: VOICEVOX WAV 再生                                │
└──────────────────────────────────────────────────────────────┘
```

## 3. コンポーネントと責務
| | 技術 | 責務 |
|---|---|---|
| 母艦デーモン | Node/TS + Fastify | フック受け口・ccusage集計・閾値ポーリング・VOICEVOX/say合成・M5へ送出・承認管理 |
| フック | bash + curl | Stop=完了 / UserPromptSubmit=作業中 / Notification=注目 / PreToolUse(Bash)=承認 |
| M5ファーム | PlatformIO/M5Unified | 表示(状態マシン/アニメ)・タッチ・スピーカー発話・HTTPサーバ |
| 変換ツール | Node(convert.mjs)+PIL(render.py) | ClaudePix HTML → フレームPNG |
| VOICEVOX | Docker(:50021) | ずんだもん音声合成（speaker=3）。未起動時はmacOS `say` |

## 4. 状態マシン（M5表示）
3層構造：**モーダル画面** ＞ **オーバーレイ(一時)** ＞ **ベース(持続)**。

### ベース（持続・数秒ごとプール内ローテ）
| 状態 | プール | セット契機 |
|---|---|---|
| idle | breathe / blink / look | 既定。Stop(done)後にidleへ |
| working | coding / think | UserPromptSubmit |

### オーバーレイ（数秒だけ割込→ベースへ戻る）
| 状態 | クリップ | 契機 |
|---|---|---|
| done(happy) | bounce/sway/djmix/bouncedj/swaydj から**ランダム** | Stop |
| worried | sleep | 残量ポーラ閾値跨ぎ |
| surprised | surprise | Notification |
| wink | wink | 承認をタップ(許可)した時 |

### モーダル（全画面・他を奪う）
| 画面 | 契機 | 操作 |
|---|---|---|
| USAGE(使用率ゲージ) | 待機中の**タップ（単/ダブル）** ※重いアニメで2連打を取りこぼすため単発でも可 | 約5秒で復帰 |
| APPROVE(CLAUDE Bash OK?) | PreToolUse(Bash) | **タップ=許可 / スワイプ=拒否**、無操作30秒で復帰 |

## 5. データフロー（主要機能）
- **完了通知**: Stop→hook→daemon`/event{done}`→VOICEVOX合成→M5`/notify`(WAV)→ダンス＋発話→`/base{idle}`。
- **作業中**: UserPromptSubmit→`/event{working}`→`/base{working}`＋**応答後に非同期でずんだ声「おしごと、するのだ！」**。
- **使用率表示**: M5を**タップ（シングル/ダブルどちらでも）**→daemon`/usage`をGET→ccusage集計%→ゲージ表示。
- **残量警告**: daemonポーラ(60s)→ccusage→閾値(50/80/95%)跨ぎ→`/notify{worried}`＋発話。
- **タップ承認**: PreToolUse→`/approve{tool,cmd}`→daemonがid発行＋M5`/approve`push→M5承認画面→tap/swipe→M5が`/approve_result`返却→フックがポーリングしallow/deny出力。**OK時は wink＋ずんだ声「オッケーなのだ」（応答後に非同期）**。無操作はask。
- **注目**: Notification→`/event{attention}`→`/notify{surprised}`＝**無音（surprise顔のみ。入力待ちの“呼び出し”合図）**。

## 6. インターフェイス仕様
### 母艦デーモン HTTP（:4920）
| メソッド/パス | 用途 |
|---|---|
| POST `/event` `{type: done\|working\|idle\|attention}` | done/attention=声つき, working/idle=声なしsetBase |
| GET `/usage` | `{percent,used,limit,resetAt,resetMin}` |
| POST `/approve` `{tool,command}` | 承認要求→`{id}` |
| GET `/approve_result/:id` | `{decision: allow\|deny\|pending\|timeout\|unknown}` |
| POST `/approve_result/:id` `{decision}` | M5からの結果確定 |

### M5 HTTP（:80）
| メソッド/パス | 用途 |
|---|---|
| POST `/notify?expr=&text=` (body=WAV) | 一時状態＋発話 |
| POST `/base?s=working\|idle` | ベース状態切替 |
| POST `/approve?id=&title=&detail=` | 承認画面表示 |
| POST `/heartbeat?port=` | daemonアドレス学習(remoteIP+port) |
| POST `/clip?p=&ms=` | デバッグ:任意クリップ強制表示 |
| GET `/state` | デバッグ:png/active/各クリップ枚数 等 |

### Claude Code フック（`~/.claude/settings.json`）
Stop→`stop.sh`(/event done) / UserPromptSubmit→`userpromptsubmit.sh`(/event working) /
Notification→`notification.sh`(/event attention) / PreToolUse(matcher:Bash)→`pretooluse.sh`(/approve)。
※daemon停止中はフックは素通り（通常動作）。

## 7. Clawd 表示パイプライン
- ソース: ClaudePix（claudepix.vercel.app）の20×20ドット絵13種（`app.js`のMANIFEST、公開配信）。`assets/clawd/html/`（gitignore）。
- 2形式: エンジン型`window.PRESET`(0空/1体/2目) と 自己完結型`window.FRAMES`+多色`PAL`。
- `convert.mjs`(Node/vm)→`{frames:[{hold,grid}],pal}`／`render.py`(PIL)→`<prefix>0..N.png`（200×200 RGB・黒地#0f0f0f・dt180ms・最大16枚・inset影風の枠）。
- firmware: 起動時に13prefixのフレーム数を数え、状態→プールから選択。`(frame/2)%枚数` で `(60,0)` に描画。LittleFS(3.5MB)に`uploadfs`。
- 差し替え: `assets/clawd/README.md` の手順（HTML→convert→render→uploadfs）。

## 8. 音声パイプライン
- daemonが **VOICEVOX(:50021, speaker=3 ずんだもん)** で `audio_query`→`synthesis`→WAV取得。失敗時 **macOS `say`+`afconvert`** で24kHz/16bit/mono WAVにフォールバック。
- WAVをM5 `/notify` ボディで送り、M5が `parseWav`→`M5.Speaker.playRaw` で**自分のスピーカーから発話**（声の出所は常にM5）。
- VOICEVOXは**Docker起動**: `docker run --rm -d -p 50021:50021 --name voicevox voicevox/voicevox_engine:cpu-latest`（イメージはキャッシュ済み）。停止 `docker stop voicevox`。

### 音マップ（どの状態で喋るか）
| 状態 | 声 | セリフ |
|---|---|---|
| 待機 idle | 🔇 無音 | — |
| 入力待ち attention | 🔇 無音 | （surprise顔のみ＝呼び出し合図） |
| 作業中 working | 🔊 | 「おしごと、するのだ！」 |
| 完了 done | 🔊 | 「おしごと、おわったのだ！」 |
| 残量警告 worried | 🔊 | 「もう○○％つかったのだ、きをつけるのだ！」 |
| 承認OK wink | 🔊 | 「オッケーなのだ」 |

→ 喋るのは working / done / worried / wink の4つ。待機・入力待ちは静か（机の上でうるさくない）。

## 9. 設定（環境変数, daemon）
`ZUNDA_PORT`(4920) / `ZUNDA_M5_URL`(http://stackchan.local ※テザリングはIP直指定) / `ZUNDA_VOICEVOX_URL`(http://127.0.0.1:50021) /
`ZUNDA_SPEAKER_ID`(3) / `ZUNDA_USAGE_LIMIT`(1e8) / `ZUNDA_THRESHOLDS`(50,80,95) / `ZUNDA_POLL_SEC`(60) / `ZUNDA_APPROVE_TIMEOUT_SEC`(30)

## 10. デプロイ／起動手順
1. firmware: `firmware/include/secrets.h` にWiFi記入 → `cd firmware && pio run -e core2 -t uploadfs && pio run -e core2 -t upload`
2. VOICEVOX: `docker run --rm -d -p 50021:50021 --name voicevox voicevox/voicevox_engine:cpu-latest`
3. daemon: `cd daemon && ZUNDA_M5_URL=http://<M5のIP> npm run dev`
4. フック登録（`~/.claude/settings.json`、`!`でpython追記）→ **新規セッション**で有効
5. Mac と M5 を同一WiFi（テザリングは「互換性を優先」=2.4GHz）

## 11. 制約・既知の課題
- **テザリングでmDNS不安定** → daemonはM5のIP直指定。M5再起動でIP変動の恐れ（→IP固定/mDNS再試行が将来課題）。
- **電源**: M5はUSB給電必須（Mac直挿し不要、充電器/モバイルバッテリーでも可）。電源断は内蔵バッテリーで数十分。
- **声はMac依存**: VOICEVOX/sayはMac側合成→M5へ送る。M5単体（Macなし）では待機アニメのみ、声・状態連動は出ない。
- **承認の摩擦**: PreToolUse(Bash)を有効化すると全Bashがタップ待ちでブロック。daemon停止=全機能オフのマスタースイッチ。承認だけ外したい時はPreToolUseフックのみ除去。
- アニメは固定dt(180ms)。per-frame holdの忠実化は未対応。

## 12. テスト
- daemon: `cd daemon && npm test`（vitest 33）/ `npm run build`
- hooks: `bash hooks/stop.test.sh` / `bash hooks/pretooluse.test.sh`
- firmware(ロジック): `cd firmware && pio test -e native`（WAVパーサ/ジェスチャ）
- 実機: `/clip` で全13クリップ確認、`/state` で内部状態確認

## 13. クレジット／権利
- 音声: **VOICEVOX:ずんだもん**
- 画面キャラ: ClaudePixのドット絵を移植（明示ライセンス無し＝個人用途・各自判断）。Clawd は Anthropic の Claude Code マスコット。素材PNG・ClaudePix HTML はリポジトリにコミットしない（gitignore）。
