# clawd（ベース顔）/ dance（演出）モジュール化設計 (2026-07-02)

## 目的

モジュール化ファームウェア（[2026-06-12-modular-firmware-design.md](2026-06-12-modular-firmware-design.md)）の続きとして、コアに直書きされている「ベース顔（idle/working）の描画」を独立モジュール `clawd` に切り出し、あわせて「新機能モジュールの追加」のリファレンスとして発火型のダンス演出モジュール `dance` を追加する。

これにより、コアからClawd固有ロジックが消え、レジストリから外せば背景顔ごと差し替え・除去できる（部分組み込みの完成）。同時に `dance` が「これみたいな機能を1つ足す」お手本になる。

## 背景

前回のモジュール化で usage / approve / notify は `modules/` に移ったが、**ベース顔だけはコアに残っている**。現状:

- アニメ描画エンジン（`display.cpp` の `tickFace()` / `selectClip()` / PNG描画 / `displayForceClip()` / `displayDebug()`）は共有ヘルパで、notify / approve / usage も直接使っている
- **ベース顔（idle/working）は `app.cpp` が直接描画**している。`arbiter.owner() < 0` のとき `tickFace(working ? "working" : "normal", …)` を呼ぶ else 節がそれ（`app.cpp` 82-89行）
- `POST /base`（状態切替）・`POST /clip`（強制クリップ）・`/state` の `dbg` キーはコア（net_core）が持っている

つまりClawdは「共有エンジン＋コア直書きのベース顔」で、まだ独立モジュールになっていない。ここを切り出すと、コアは「Moduleインターフェース＋画面調停＋WiFi/HTTP基盤」だけになり、どの機能モジュールにも依存しない状態になる。

## 方針（採用案）

画面調停 `ScreenArbiter` の grant 条件は「owner不在 / 自分の再request / 新priority >= 現priority」。この性質を使い、**clawdを優先度0の常駐（背景）オーナーとして実装する**。コアや調停ロジックには手を入れない。

検討した2案:

- **採用: 優先度0の常駐オーナー方式（コア無改造）**。clawdが `loop()` で毎ループ `requestScreen(0, …)` して背景オーナーであり続ける。上位モジュール（notify=50 / approve=100）が来れば自動的に負けて `onScreenLost()`、相手が release すれば次ループで再取得する。`app.cpp` は else 節を削るだけ
- 不採用: コアに「背景モジュール」概念を追加（`owner<0` のとき指定モジュールの `drawBackground()` を呼ぶ）。1ループの空白が原理的に無い利点はあるが、コア／インターフェースに手が入り「clawdも普通のモジュールと同じ作法」という一貫性が崩れる

採用案の唯一のトレードオフは、上位モジュールが release した次の1ループ（約10ms）だけ誰も描画しない瞬間が理論上ありうる点。実害はスプライトに直前フレームが残るだけで体感ゼロ。詳細は「1ループ空白の扱い」を参照。

## 2モジュールに分ける理由

「ベース顔（常時表示）」と「ダンス演出（発火して一定時間で戻る）」はライフサイクルが別物なので、`docs/firmware-modules.md` の「1機能1ディレクトリ」に沿って分離する。

- `clawd`: 常駐（優先度0）。設計としては approve/notify とは逆の「一番下に居続ける」役
- `dance`: 発火型（notify と同じ pending → loop → requestScreen パターン）。既存3モジュールと同じ作法なので、モジュール追加手順の実証になる

## ディレクトリ構成（差分のみ）

```
firmware/src/
  core/
    app.cpp              # else節（ベース顔描画）を削除。他は無変更
    net_core.cpp         # /clip ルートと /state の dbg 追記を削除（clawdへ移設）。/base は残す
  modules/
    clawd/clawd.h .cpp   # 新規。ベース顔（背景常駐オーナー）
    dance/dance.h .cpp   # 新規。発火型ダンス演出
  module_registry.cpp    # clawd と dance を追加
  display.h / .cpp       # 無変更（共有ヘルパのまま）
```

`display.cpp` のアニメエンジンは移動しない。clawdへ移すと notify/approve/usage がclawdに依存してしまうため、「共有は core/display 経由」という現行の原則を維持する。`displayBegin()` / `audioBegin()` も引き続きコア（`appSetup()`）が初期化する（共有サブシステム初期化はコアの責務）。

## clawd モジュール

責務: ベース顔（idle/working）の常時表示。コア `app.cpp` の else 節をそのまま引き継ぐ。

```cpp
// src/modules/clawd/clawd.h
#pragma once
#include "core/module.h"

// 背景（優先度0）に常駐し、baseStateに応じたベース顔を描く。/clip を所有。
class ClawdModule : public Module {
 public:
  const char* name() override { return "clawd"; }
  void setup(Services& s) override;         // /clip 登録
  void loop(uint32_t now) override;         // requestScreen(0,...) で背景常駐
  void draw(uint32_t now) override;         // baseStateに応じ tickFace(...)
  void appendState(String& json) override;  // dbg（displayDebug()）を出力

 private:
  Services* svc_ = nullptr;
};
```

- `loop()`: `svc_->requestScreen(0, kRenewMs, now)` を毎ループ呼ぶ。自分がownerなら期限が更新され、空きなら取得、上位がownerなら false が返るだけ（何もしない）。`kRenewMs` は 1000ms 程度（毎ループ更新されるので実質切れない）
- `draw()`: 現行 app.cpp と同一。`bool working = svc_->baseState() == 1; tickFace(working ? "working" : "normal", working ? "おしごとちゅうなのだ" : "まってるのだ");`
- `onScreenLost()`: 何もしない（次ループの `loop()` で再取得を試みる）
- `appendState()`: `,"dbg":"..."`（`displayDebug()` の出力）を追記。net_core から移設

`/base` と `baseState` はコア（net_core）に残す。「Claudeが作業中か」はシステム共通シグナルで、すでに `Services::baseState()` として公開されている。clawdはそれを読むだけにする（clawd固有状態にすると Services から見えなくなり、将来他モジュールが参照できない）。

## dance モジュール

責務: `POST /dance` で発火し、一定時間ダンスアニメをフルスクリーン再生して自動で戻る。既存モジュールの pending → loop → requestScreen パターンに従う。

```cpp
// src/modules/dance/dance.h
#pragma once
#include "core/module.h"

// POST /dance?clip=&ms= を受けてダンス演出を再生。priority 40。
class DanceModule : public Module {
 public:
  const char* name() override { return "dance"; }
  void setup(Services& s) override;
  void loop(uint32_t now) override;
  void draw(uint32_t now) override;

 private:
  struct Pending {
    volatile bool ready = false;  // asyncタスク→loop()受け渡し。最後に立てる
    String clip;                  // 空なら "done" プールからランダム
    uint32_t ms = 0;
  };
  Pending pending_;
  Services* svc_ = nullptr;
  String clip_;
  bool active_ = false;
};
```

- ルート `POST /dance?clip=&ms=`: `clip` 省略時は `tickFace("done", …)` に任せて done プール（bounce/sway/djmix/bouncedj/swaydj）からランダム再生。`ms` 省略時は既定 6000ms。ハンドラは pending に積んで `ready=true`（描画・音声はしない）
- `loop()`: `pending_.ready` を見たら `requestScreen(40, pending_.ms, now)`。grant されたら `active_=true`、`clip` 指定時は `displayForceClip(clip, ms)` を呼ぶ。拒否時は pending 保持で次ループ再試行（notifyと同じ）
- `draw()`: `tickFace("done", "おどるのだ")` を毎tick。`clip` 指定時は `displayForceClip` が張った強制クリップを `tickFace` が優先表示する（display.cpp の既存挙動）
- timeout（ms）満了 → arbiter が `onScreenLost()` を呼ぶ → clawd が次ループで背景を取り戻し idle/working に自然復帰。dance 側は `onScreenLost()` で `active_=false` に戻す

### priority とトリガの決定

- **priority = 40**（notify=50 / approve=100 の下）。通知と承認は演出より重要なので邪魔しない。承認中・通知中に `/dance` が来たら pending のまま待ち、上位が退いてから再生される
- **トリガは `POST /dance` のみ**。ジェスチャ（TAP/DOUBLETAP）は現状 usage が消費しており衝突するため、MVPではHTTP発火に限定する。ジェスチャ割当は必要になった時点で別途検討する

## registry

並び順 = ジェスチャのフォールバック順（前面owner → 先頭から）。clawdは常駐背景なので最後尾、danceは演出として approve の次あたりに置く。

```cpp
Module* const MODULES[] = {
    &approveModule,   // 100（割り込み禁止）
    &danceModule,     //  40（演出）
    &notifyModule,    //  50
    &usageModule,     //  タップ/ダブルタップ
    &clawdModule,     //   0（背景常駐オーナー）
};
```

clawd の `onGesture()` は既定の false（消費しない）ので、背景オーナー中のタップはレジストリ順で usage に届く（現行どおり）。dance の `onGesture()` も false（演出中のタップは特に消費しない）。

## app.cpp の差分

else 節（ベース顔描画）を削除するだけ。

```cpp
// 変更前
int own = arbiter.owner();
if (own >= 0) {
  MODULES[own]->draw(now);
} else {
  bool working = netBaseState() == 1;
  tickFace(working ? "working" : "normal",
           working ? "おしごとちゅうなのだ" : "まってるのだ");
}

// 変更後
int own = arbiter.owner();
if (own >= 0) MODULES[own]->draw(now);
```

これに伴い app.cpp から `tickFace` / `netBaseState` の描画用途の参照が消える（`displayBegin()` のための display.h include は残す）。

## net_core.cpp の差分

- `POST /clip` ルート（40-45行）を削除 → clawd の `setup()` へ移設
- `/state` の `,"dbg":...` 追記（52行）を削除 → clawd の `appendState()` へ移設
- `POST /base`・`/heartbeat`・`/state` の本体は残す。`netBaseState()` は `Services::baseState()` の実体として引き続き提供

## 1ループ空白の扱い

上位モジュールが release してから clawd が背景を取り戻すまでの遷移を、`appLoop()` の処理順（ジェスチャ配布 → `tick()` 期限処理 → 全モジュール `loop()` → owner の `draw()`）で追う。

- **`tick()` 期限切れ / `onGesture` 内 release の場合**: owner が -1 になった後に全モジュールの `loop()` が回る。clawd の `loop()` が `requestScreen(0,…)` で背景を取得 → 同ループの `draw()` で clawd が描く。空白なし
- **モジュールが自分の `loop()` 内で release し、それが registry 上 clawd より後ろの場合**: clawd の `loop()` は既に実行済みなので、そのループは owner=-1 のまま `draw()` に到達し、誰も描かない（スプライトは直前フレームを保持）。次ループで clawd が取得して復帰

実害は約10msの1フレーム保持のみ。現行モジュール（notify/approve/usage）はいずれも `tick()` 期限切れか `onGesture` で退場するため、通常は空白すら発生しない。

## エラー処理

- `/dance` の `ms` 過大・過小: 常識的な範囲にクランプ（例 500〜30000ms）。範囲外は既定 6000ms 扱い
- 存在しない `clip` 指定: `displayForceClip` はフレーム数0のクリップを描画しないだけで落ちない（`renderPngFrame` が `g_activeFrames<=0` で return）。無害なので特別扱いしない
- clawd が背景取得に失敗し続けるケース: 上位 owner がいる間は正常。上位が退けば次ループで取得できる。恒久的に取れない状況は発生しない

## テスト計画

- native（`pio test -e native`）:
  - 既存 test_gesture / test_wavparse / test_screen_arbiter は無変更でパスすること
  - dance に純ロジック（ms のクランプ計算など）を切り出せる場合のみ `lib/` 化して最小テストを追加。描画・画面遷移は実機依存のため native テスト対象外
- ビルド: `pio run -e core2` が通ること
- 実機の手動確認（リグレッションチェックリスト）:
  - [ ] 起動 → WiFi接続 → アイドル顔アニメ（clawd 背景が描く）
  - [ ] `/base working` / `idle` でベース顔が切り替わる（clawd が baseState を読む）
  - [ ] `/clip?p=wink&ms=3000` で強制クリップが出て戻る（clawd 所有に移設後も動く）
  - [ ] daemon からの通知（顔+テキスト+WAV、4秒で clawd 背景に復帰）
  - [ ] 承認要求 → 静止表示 → タップ/スワイプ → フィードバック顔 → clawd 復帰
  - [ ] アイドル中タップで使用率表示、5秒で clawd 復帰
  - [ ] `POST /dance` でダンス再生 → 既定6秒で idle/working に復帰
  - [ ] `POST /dance?clip=djmix&ms=4000` で指定クリップが4秒再生されて戻る
  - [ ] 通知中に `/dance` → 通知が終わってからダンスが出る（priority 40 < 50）
  - [ ] 承認中に `/dance` → 承認が終わってからダンスが出る（priority 40 < 100）
  - [ ] GET /state が従来キー（dbg 含む）を返す（clawd の appendState へ移設後も同じ）

## 互換性

- 既存エンドポイント（`/base` `/clip` `/state` `/notify` `/approve` `/heartbeat`）のパス・パラメータ・レスポンスは変えない。`/clip` と `/state` の `dbg` は所有モジュールが変わるだけで外部仕様は同一
- ベース顔・通知・承認・使用率の UX（表示内容・タイムアウト秒数・優先順位）は不変
- daemon / hooks / assets には触れない
- 追加は `POST /dance` のみ（新規エンドポイント）

## スコープ外

- dance のジェスチャトリガ（HTTPのみで実装。将来検討）
- dance の音楽（WAV）再生
- 実行時 ON/OFF 機構
- clawd のベース顔ロジック自体の変更（クリップ選択・回転間隔は現行を移設するだけ）
- daemon / hooks / assets の変更

## 完了条件

1. `pio run -e core2` がビルド成功
2. `pio test -e native` が全パス（既存分）
3. `app.cpp` から else 節が消え、ベース顔が `modules/clawd/` に移り、`net_core.cpp` から `/clip` と `dbg` 追記が消えている
4. `modules/dance/` が追加され `POST /dance` が動く
5. 実機リグレッションチェックリスト全項目パス
6. `docs/firmware-modules.md` に dance を「追加例」として反映（任意だが推奨）
