# 設計図 — claudecode_push_display（2026-06-08）

Claude Code の状態を、机上の **M5Stack Core2**（キャラ「Clawd」）が **画面アニメ＋ずんだ声** で知らせるデスクトップ相棒の図面集。
文章仕様は [`2026-06-05-system-design.md`](./2026-06-05-system-design.md) を参照。本書はそれを **図** にしたもの（Mermaid。GitHub / VS Code でそのまま描画）。

---

## 1. システム構成図

母艦（Mac）が頭脳と声、M5 が顔と口・耳。両者は **WiFi HTTP** でつながる。声の合成は Mac 側（VOICEVOX）、再生は常に M5 のスピーカー。

```mermaid
graph TB
  subgraph MAC["💻 Mac（母艦）"]
    CC["Claude Code"]
    HK["hooks (curl)<br/>UserPromptSubmit / Stop<br/>Notification / PreToolUse(Bash)"]
    DA["母艦デーモン :4920<br/>Fastify"]
    VV["VOICEVOX :50021<br/>Docker / speaker=3 ずんだもん"]
    CU["ccusage<br/>~/.claude/**.jsonl 集計"]
    CC --> HK
    HK -->|"/event ・ /approve"| DA
    DA <-->|"synth ⇄ ずんだWAV"| VV
    DA -->|"使用率"| CU
  end

  subgraph DEV["🤖 M5Stack Core2（自作ファーム / PlatformIO・M5Unified）"]
    NET["AsyncWebServer :80"]
    DISP["M5Canvas 描画<br/>Clawd ドット絵アニメ"]
    SPK["Speaker<br/>ずんだWAV 再生 🔊"]
    TCH["Touch<br/>tap=許可 / swipe=拒否"]
    NET --> DISP
    NET --> SPK
    TCH --> NET
  end

  DA ==>|"WiFi HTTP ▶ 送信<br/>/notify(WAV) ・ /base ・ /approve"| NET
  NET -.->|"◀ 受信 /heartbeat ・ /approve_result ・ /usage"| DA
```

---

## 2. 状態マシン（M5 表示）

### 2.1 3層構造（優先度）

上の層が下を覆う。**モーダル ＞ オーバーレイ ＞ ベース**。

```mermaid
graph TB
  M["① モーダル（全画面・最優先）<br/>USAGE 使用率ゲージ / APPROVE 承認画面"]
  O["② オーバーレイ（数秒だけ割込→ベースへ復帰）<br/>done / worried / surprised / wink"]
  B["③ ベース（常に下地・数秒ごとプール内ローテ）<br/>idle ⇄ working"]
  M -->|"無ければ"| O
  O -->|"無ければ"| B
```

### 2.2 状態遷移

`🔊` = 声あり ／ 無印 = 無音。オーバーレイ（done/worried/surprised/wink）は時間経過で **その時のベース（idle か working）** に戻る（図では idle に集約）。

```mermaid
stateDiagram-v2
  [*] --> idle
  note right of idle
    🔊=声あり ／ 無印=無音
    idle: breathe/blink/look
    working: coding/think
  end note

  idle --> working: プロンプト送信🔊
  working --> done: 完了 Stop🔊
  idle --> done: 完了 Stop🔊
  done --> idle: 約4秒（ダンス後）

  idle --> USAGE: タップ
  working --> USAGE: タップ
  USAGE --> idle: 約5秒

  idle --> APPROVE: PreToolUse Bash
  working --> APPROVE: PreToolUse Bash
  APPROVE --> wink: タップ=許可🔊
  APPROVE --> worried: スワイプ=拒否
  wink --> idle: 約1.8秒

  idle --> surprised: 入力待ち（無音）
  surprised --> idle: 約4秒

  idle --> worried: 残量警告🔊
  working --> worried: 残量警告🔊
  worried --> idle: 約4秒
```

---

## 3. シーケンス図（主要フロー）

### 3.1 タスク完了通知（主役）

`Stop` フック → daemon が声を合成 → M5 がダンス＋発話 → 待機へ。

```mermaid
sequenceDiagram
  autonumber
  participant CC as Claude Code
  participant HK as hook stop.sh
  participant DA as daemon :4920
  participant VV as VOICEVOX
  participant M5 as M5 Core2
  CC->>HK: Stop（タスク完了）
  HK->>DA: POST /event {type:done}
  DA->>VV: synth「おしごと、おわったのだ！」
  VV-->>DA: ずんだ WAV
  DA->>M5: POST /notify (WAV, happy)
  M5->>M5: ダンス＋発話 🔊
  DA->>M5: POST /base {idle}
  Note over M5: 約4秒後に idle へ
```

### 3.2 タップ承認（PreToolUse Bash）

承認画面を出し、tap=許可 / swipe=拒否。許可時のみ wink＋声。フックは結果をポーリングで取得。

```mermaid
sequenceDiagram
  autonumber
  participant CC as Claude Code
  participant HK as hook pretooluse.sh
  participant DA as daemon :4920
  participant M5 as M5 Core2
  participant US as あなた
  CC->>HK: PreToolUse Bash
  HK->>DA: POST /approve {tool, command}
  DA->>DA: id 発行
  DA->>M5: POST /approve（承認画面）
  DA-->>HK: {id}
  M5->>US: 「CLAUDE Bash OK?」表示
  US->>M5: タップ=許可 ／ スワイプ=拒否
  M5->>DA: POST /approve_result/:id {decision}
  alt 許可 allow
    DA->>M5: POST /notify (wink, WAV)「オッケーなのだ」🔊
  end
  loop フックがポーリング
    HK->>DA: GET /approve_result/:id
    DA-->>HK: allow / deny / pending
  end
  HK-->>CC: allow→実行 ／ deny→中止
```

### 3.3 使用率表示（画面タップ）

M5 をタップ → daemon に問い合わせ → ゲージ表示。

```mermaid
sequenceDiagram
  autonumber
  participant US as あなた
  participant M5 as M5 Core2
  participant DA as daemon :4920
  participant CU as ccusage
  US->>M5: 画面をタップ
  M5->>DA: GET /usage
  DA->>CU: 使用量を集計
  CU-->>DA: percent / resetMin
  DA-->>M5: {percent, resetMin}
  M5->>M5: 使用率ゲージ表示
  Note over M5: 約5秒で待機へ
```

### 3.4 残量警告（自動ポーリング）

daemon が 60 秒ごとに使用率を見張り、閾値を跨いだ瞬間だけ警告。

```mermaid
sequenceDiagram
  autonumber
  participant DA as daemon ポーラ 60s
  participant CU as ccusage
  participant VV as VOICEVOX
  participant M5 as M5 Core2
  loop 60秒ごと
    DA->>CU: 使用率を取得
    CU-->>DA: percent
    alt 閾値 50/80/95% を跨いだ
      DA->>VV: synth「もう○○% つかったのだ」
      VV-->>DA: ずんだ WAV
      DA->>M5: POST /notify (worried, WAV) 🔊
    else 跨いでいない
      DA->>DA: 何もしない
    end
  end
```

---

## 4. 凡例

| 記号 | 意味 |
|---|---|
| `==>`（実線・太） | Mac → M5 への送信（/notify, /base, /approve） |
| `-.->`（点線） | M5 → Mac への送信（/heartbeat, /approve_result, /usage） |
| `🔊` | 声あり（VOICEVOX ずんだもん。M5 スピーカーで再生） |
| 無印 | 無音（アニメ／画面のみ） |

**音マップ**：喋るのは **working / done / worried / wink** の4つ。**idle / 入力待ち(surprised)** は無音（机上でうるさくない）。
