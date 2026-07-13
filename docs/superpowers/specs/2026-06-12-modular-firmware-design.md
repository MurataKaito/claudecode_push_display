# モジュール化ファームウェア設計 (2026-06-12)

## 目的

スタックチャン・アタマのファームウェアを「機能 = モジュール」の構造に再編し、新機能をディレクトリ追加と登録1行で組み込めるようにする。将来的にはこの規約を「任意のM5Stackプロジェクトにモジュール化アーキテクチャを導入・モジュール追加できる汎用skill」に育てる。本specはその第一段階であるリファレンス実装（このリポジトリのファームウェアのモジュール化）を対象とする。

## 背景

現状の `firmware/src/` は main.cpp がモード管理（IDLE/USAGE/APPROVE）とタイマー群を直接持ち、net.cpp がHTTPエンドポイントを `app_state.h` のグローバル変数経由で main に繋ぐ密結合構成。機能を1つ足すたびに main.cpp の状態機械・net.cpp のルート・app_state.h のグローバルの3箇所へ同時に手を入れる必要があり、機能間の干渉（画面の取り合い）も都度アドホックに解決している。

## 方針（採用案）

検討した3案のうち「モジュールインターフェース + 静的レジストリ」を採用する。

- 採用: ビルド時選択。モジュールはレジストリ（配列）に登録された分だけバイナリに含まれる。再現性が高く、ESP32のメモリ事情とも相性が良い
- 不採用: 実行時ON/OFF（NVS等の状態管理が増え「自分の環境だと動かない」を生みやすい）
- 不採用: FreeRTOSタスク分離（M5GFXがスレッドセーフでなく、コミュニティ向け規約として重すぎる）

## ディレクトリ構成

```
firmware/src/
  core/
    module.h             # Moduleインターフェース + Services定義
    app.h / app.cpp      # appSetup()/appLoop()。今のmain.cppの後継
    net_core.h / net_core.cpp  # WiFi/mDNS/HTTPサーバ起動 + 基盤エンドポイント(/heartbeat /state /base /clip)
  modules/
    usage/usage.h .cpp   # タップ→使用率表示（既存機能の移植）
    approve/approve.h .cpp  # 承認フロー（同上）
    notify/notify.h .cpp # 通知+WAV再生（同上）
  module_registry.h / .cpp  # 有効モジュール一覧。1モジュール=1行。ここから消せばビルドに含まれない
  main.cpp               # appSetup()/appLoop()を呼ぶだけの数行
  display.h / .cpp       # 既存のまま（モジュールから直接includeして使う描画ヘルパ）
  audio.h / .cpp         # 同上
firmware/lib/
  gesture/               # 既存のまま
  wavparse/              # 既存のまま
  screen_arbiter/        # 新規。画面所有権の調停（Arduino非依存の純ロジック）
firmware/test/
  test_gesture/          # 既存のまま
  test_wavparse/         # 既存のまま
  test_screen_arbiter/   # 新規
```

`app_state.h` と `net.h/.cpp` は解体・廃止する。グローバル共有状態のうち core に残るのは `daemonBase`（heartbeatで学習するdaemonのURL）と `baseState`（idle/working）だけ。各機能の pending 状態（承認待ち・通知待ち等）は各モジュールが自分で持つ。

## モジュールインターフェース

```cpp
// core/module.h
struct Services;

struct Module {
  virtual ~Module() = default;
  virtual const char* name() = 0;
  virtual void setup(Services& s) {}        // 起動時1回。HTTPルート登録・初期化
  virtual void loop(uint32_t now) {}        // 毎ループ呼ばれる
  virtual bool onGesture(Gesture g, uint32_t now) { return false; }  // trueで消費（後続に回さない）
  virtual void draw(uint32_t now) {}        // 画面を所有している間、毎ループ呼ばれる
  virtual void onScreenLost() {}            // 横取り・タイムアウトで画面を失ったとき
  virtual void appendState(String& json) {} // GET /state に診断フィールドを追記
};
```

- `draw(now)` は毎tick呼ばれる。静止画面のモジュール（usage/approve）は内部のdirtyフラグで初回のみ描画する。アニメするモジュール（notifyの顔オーバーレイ等）は毎tick描く
- 描画・音声は既存の `display.h` / `audio.h` の自由関数をモジュールが直接includeして使う。ラッパは作らない（ボイラープレート削減と既存コード温存のため）

### Services

core が所有するオブジェクトへの参照と、共有状態へのアクセサだけを持つ薄い構造体。

```cpp
struct Services {
  AsyncWebServer* http;  // setup()内でのルート登録用
  int selfId;            // レジストリ上の自分のID（画面調停で使う）

  // 画面を要求。grantならtrue。別モジュールを横取りした場合は相手のonScreenLost()が呼ばれる
  bool requestScreen(int priority, uint32_t timeoutMs, uint32_t now);
  void releaseScreen();

  const String& daemonBase() const;  // 例 "http://172.20.10.5:4920"。未学習なら空文字
  int baseState() const;             // 0=idle / 1=working
};
```

ScreenArbiter を直接モジュールに見せず requestScreen/releaseScreen メソッドで包むのは、横取り発生時に被横取りモジュールへの `onScreenLost()` 通知（レジストリの知識が必要）を core 側で完結させるため。

## 画面調停（ScreenArbiter）

今 main.cpp の `mode` 変数とタイマー群（usageUntil/approveDeadline/notifyUntil）がアドホックにやっていることの一般化。Arduino非依存の純C++クラスとして `lib/screen_arbiter/` に置き、native環境でユニットテストする。

```cpp
class ScreenArbiter {
 public:
  // 画面を要求。grantされたらtrue。先住owner横取り時はtrueを返しつつ被横取りidを*preemptedに格納
  bool request(int id, int priority, uint32_t timeoutMs, uint32_t now, int* preempted);
  void release(int id);       // owner以外からの呼び出しは無視
  int tick(uint32_t now);     // 期限切れしたowner idを返す（なければ-1）。返したら所有解除
  int owner() const;          // 現owner id。いなければ-1
};
```

### 調停ルール

- grant条件: owner不在、同一ownerによる再request（自身の優先度変更・期限更新を含む。preempted通知なし）、または `新priority >= 現ownerのpriority`（同優先度は後勝ち）
- priority割当: approve=100、notify=50、usage=50。操作後のフィードバック顔（「オッケーなのだ」1.8秒、「しゅとくしっぱいなのだ」2.5秒等）は50（現行どおり通知やタップで上書き可能にするため、approveもフィードバック時は50に自己降格する）
- owner不在のとき core がアイドル顔を描く: `tickFace(baseStateに応じたexpr, text)`（働き中なら working/「おしごとちゅうなのだ」、待機なら normal/「まってるのだ」）

この割当は現行の挙動をそのまま再現する：

| 現行挙動 | 新ルールでの実現 |
|---|---|
| 承認は最優先で画面を奪い、通知は承認中に割り込めない | approve(100) > notify(50)。通知はgrant拒否され、pendingのまま次ループで再試行 |
| 通知は使用率表示に割り込める | notify(50) >= usage(50) で後勝ち |
| 通知オーバーレイ中のタップで使用率を出せる | usage(50) >= notify(50) で後勝ち |
| 新しい承認が承認画面を置き換える | approve(100) >= approve(100) で後勝ち |
| 承認30秒・使用率5秒・通知4秒で自動的にアイドルへ | timeoutMsで表現。tick()が期限切れを返す |

### ジェスチャルーティング

core が毎ループ `M5.Touch` からジェスチャを検出し（`lib/gesture` は既存のまま使用）、以下の順で `onGesture()` を回す。最初に true を返したモジュールで止める。

1. 画面のowner（いれば）
2. レジストリ順（owner除く）

approve は画面所有中すべてのジェスチャを消費する（スワイプ=deny、他=allow）。notify は消費しない（falseを返す）ので、オーバーレイ中のタップはレジストリ順で usage に届く。usage は TAP/DOUBLETAP のとき画面を要求し、grantされたら使用率取得を開始する。

## データフロー

HTTP受信（ESPAsyncWebServerのasync TCPタスク）→ モジュール内の pending フラグ → モジュールの `loop()` が拾って画面要求、という現行パターンを踏襲する。pending構造体の `ready` フラグは volatile bool のまま維持し、asyncタスク側は書き込み完了後に最後に ready を立てる（現行と同じ作法）。

```
daemon/PC                core(net_core)            module                  core(app loop)
   |  POST /notify 等        |                        |                        |
   |----------------------->| moduleがsetupで登録した |                        |
   |                        | ハンドラが直接呼ばれる   |                        |
   |                        |----------------------->| pendingに記録, ready=true
   |                        |                        |<--- loop(now)で検出 ---|
   |                        |                        | screen.request(...)    |
   |                        |                        | grant→draw()で表示     |
```

## エンドポイントの所属

| エンドポイント | 所属 | 備考 |
|---|---|---|
| POST /heartbeat | core (net_core) | daemonBase学習。全モジュールの基盤 |
| GET /state | core (net_core) | core分のフィールド + 各moduleのappendState()を連結。既存のJSONキー（daemonBase, recv, shown, touch, g, ready, lastId, dbg）は名前・型とも維持する |
| POST /base | core (net_core) | baseState切替。アイドル顔の表示はcoreの責務のため |
| POST /clip | core (net_core) | displayデバッグ用 |
| POST /notify | notifyモジュール | WAVボディ受信含む |
| POST /approve | approveモジュール | |
| GET /usage への問い合わせ（client側） | usageモジュール | daemonへのHTTPClient発行 |
| POST /approve_result/:id（client側） | approveモジュール | 同上 |

/state の診断カウンタの所有: `touch`/`g`（タッチ・ジェスチャ）は core、`recv`/`shown`/`ready`/`lastId` は approve モジュールが持ち appendState で出力する。

## モジュール追加手順（この規約が skill の核になる）

1. `src/modules/<name>/<name>.h` と `<name>.cpp` を作り、`Module` を実装する
2. `module_registry.cpp` に include・インスタンス・配列への1行を足す
3. HTTPで起動する機能なら `setup()` で `s.http.on(...)` を登録し、pendingフラグ→`loop()`で処理
4. 画面が要るなら priority を決めて `s.screen.request(s.selfId, prio, timeoutMs, now, &preempted)`
5. `pio run -e core2` でビルド、`pio test -e native` でテストを確認

レジストリから1行消せばその機能はバイナリから消える（部分組み込み）。

## エラー処理

- daemonBase未学習時のHTTP発行: 各モジュールが現行同様に自前でガード（usageは「まだつながってないのだ」表示）
- HTTP失敗・タイムアウト: 現行同様モジュール内で処理（usageは「しゅとくしっぱいなのだ」表示、4秒タイムアウト設定維持）
- 画面要求の拒否: モジュールはpendingを保持したまま次ループで再試行（notifyの現行挙動と同じ）
- モジュールsetup失敗: ログ出力のみ。他モジュールの起動は止めない

## テスト計画

- native（`pio test -e native`）:
  - 既存: test_gesture / test_wavparse は無変更でパスすること
  - 新規 test_screen_arbiter: grant/deny、同優先度の後勝ち、高優先度の横取り（preempted通知）、タイムアウト、owner以外のrelease無視、owner不在時の挙動
- ビルド: `pio run -e core2` が通ること
- 実機の手動確認（リグレッションチェックリスト）:
  - [ ] 起動→WiFi接続→アイドル顔アニメ
  - [ ] daemonからの通知（顔+テキスト+WAV再生、4秒で復帰）
  - [ ] /base working/idle でベース顔が切り替わる
  - [ ] 承認要求→画面静止表示→タップでallow/スワイプでdeny→フィードバック顔→daemonへPOST
  - [ ] 承認30秒放置でアイドル復帰
  - [ ] アイドル中タップで使用率表示、5秒で復帰
  - [ ] daemon未接続時タップで「まだつながってないのだ」
  - [ ] GET /state が従来キーを返す

## 互換性

純リファクタとする。HTTPエンドポイントの仕様（パス・パラメータ・レスポンス）、daemon連携、UX（表示内容・タイムアウト秒数・優先順位）は一切変えない。daemon側・hooks側のコードには触れない。

## スコープ外

- 実行時ON/OFF機構
- 新機能モジュールの追加（アーキテクチャ実証は既存3機能の移植で行う）
- 汎用skill（M5Stackプロジェクト向けモジュール化skill）の作成 — 本実装で規約を実証した後、別specとして設計する
- daemon / hooks / assets の変更

## 完了条件

1. `pio run -e core2` がビルド成功
2. `pio test -e native` が全パス（既存 + screen_arbiter）
3. 実機リグレッションチェックリスト全項目パス
4. main.cpp が数行になり、app_state.h / net.cpp が消え、既存3機能が modules/ 配下に移動している
5. README または docs にモジュール追加手順が記載されている
