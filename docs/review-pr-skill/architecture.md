# review-pr skill アーキテクチャ

`/review-pr` skill と `pr-reviewer` subagent が「どう組まれているか」（構造・データフロー・責務）をまとめる。判断の根拠（なぜそうしたか）は [`decisions.md`](decisions.md) を参照。

## 目的と非目的

### 目的

- GitHub PR を Claude が客観的にレビューし、結果を **PR コメントとして自動投稿**する。
- 人間レビュアーが確認する「下地」を機械的に整える。

### 非目的

- 人間レビューの置き換えではない。最終判断は人間。
- コードの自動修正はしない（指摘のみ）。
- CI ゲート用途は想定しない（手動起動のアシスト）。

## 2 役構成とデータフロー

役割を 2 つに明確分離する。

```
ユーザー
  │  /review-pr <PR番号>
  ▼
親エージェント（skill 実行者）= オーケストレーション専任
  - gh CLI で PR 情報・diff を取得
  - subagent への入力プロンプトを構築
  - subagent から JSON を受け取る
  - gh CLI で PR にコメント投稿
  - CLI へ結果報告
  │  プロンプト（リポジトリ情報・PR本文・diff）
  ▼
pr-reviewer subagent = レビュー分析専任（コンテキスト分離）
  - Read / Glob / Grep で関連コードを確認
  - 客観的にレビュー
  - 構造化 JSON を返す（編集・投稿はしない）
```

## 責務分界

| | 親エージェント | pr-reviewer subagent |
|---|---|---|
| PR 情報・diff 取得 | 担当（gh CLI） | しない |
| レビュー分析 | しない | 担当（Read/Glob/Grep） |
| レビュー本文生成 | しない（加工も禁止） | 担当 |
| コメント投稿 | 担当（gh CLI） | しない |
| 使えるツール | Bash, Read, Glob, Grep, Agent | Read, Glob, Grep のみ |

subagent に Bash/Edit/Write を与えないのは、「分析専門」であることを構造的に保証するため。副作用（投稿）は親だけが持つ。

## 入力設計（subagent に渡すもの／渡さないもの）

原則は「人間レビュアーが通常見る公開情報だけを渡す」。

### 渡す

- リポジトリ情報（owner/repo、PR 番号、タイトル、base/head ブランチ、URL）
- PR 本文（body）
- diff（`gh pr diff` の出力全体）
- リポジトリ構成の手がかり（どこに何があるか）

### あえて渡さない

| 渡さないもの | 理由（詳細は decisions.md） |
|---|---|
| PR 作成者（author） | 権威バイアスを避ける |
| ヘッドコミット SHA | 親が投稿時に使う情報。レビューに不要 |
| 変更ファイル一覧・行数 | diff から読み取れる |
| 親の実装意図・経緯 | 客観性を損なう核心 |

## 親が守るべき不変条件

- **subagent の結果を加工しない**。「優しい表現に」「この指摘は不要」といった介入をしない。親の仕事は「投稿する」ことであり「レビューする」ことではない。
- ローカルのチェックアウトブランチが PR の head と異なる場合、subagent が誤ったローカルファイルを読む可能性があるため、切り替えるか、diff 優先の警告を subagent プロンプトに添える。

## 関連

- 設計判断の根拠: [`decisions.md`](decisions.md)
- 出力フォーマットと投稿: [`output-format.md`](output-format.md)
- エラーハンドリング: [`error-handling.md`](error-handling.md)
