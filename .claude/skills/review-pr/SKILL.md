---
name: review-pr
description: PR の URL または番号、または現在ブランチに対応する open PR を対象に、`pr-reviewer` subagent を起動してコードレビューを実施し、結果をトップレベルコメント + インラインコメントとして PR に自動投稿する。
allowed-tools: Bash Read Glob Grep Agent
---

# review-pr

GitHub PR を対象にコードレビューを実施し、結果を PR にコメントとして投稿する skill。

レビュー本体は **`pr-reviewer` subagent**（`.claude/agents/pr-reviewer.md`）に委譲する。
親エージェント（このskillを実行している Claude）の実装コンテキストを引き継がせず、客観的な目でレビューさせるための設計。

## このプロジェクトでの位置づけ

このリポジトリ（`claudecode_push_display`）は以下の構成:

- `firmware/` — PlatformIO 製の ESP32 / M5Stack ファーム（C++）。`firmware/data/` は LittleFS に焼く画像アセット
- `daemon/` — PC 側の TypeScript / Node デーモン（`daemon/src/*.ts`、テストは `*.test.ts` / vitest）
- `assets/clawd/` — Clawd 顔アニメの生成・変換（`render.py` / `convert.mjs` / HTML プリセット）
- `docs/superpowers/specs/*.md`, `docs/superpowers/plans/*.md` — 設計書（spec / plan）

`/review-pr` は **これらいずれの変更に対しても** 使える汎用 PR レビュー skill であり、特定言語・特定モジュールに依存しない。

> NOTE: 元になった別プロジェクトには `/self-review`（push 前の自己チェック）と `/create-pr`（PR 作成 + 自動チェーン）という skill があったが、**このリポジトリには存在しない**。したがって `/review-pr` は **手動起動のスタンドアロン skill** として運用する（自動チェーンや push 時の二重実行といった連携は考慮不要）。

## 使用方法

```
/review-pr               # 現在ブランチに対応する open PR を自動検出
/review-pr <PR番号>      # 例: /review-pr 1
/review-pr <PR URL>      # 例: /review-pr https://github.com/MurataKaito/claudecode_push_display/pull/1
```

## 設計上の重要原則

**親エージェントの実装コンテキストをレビューに持ち込まない**こと。
これを担保するため、レビュー本体は **必ず `pr-reviewer` subagent に委譲する**。
親エージェントが「どんなコードを書いたか」「何を意図したか」を知っていても、subagent は知らない状態でレビューする。

**レビュー結果はユーザー確認なしで PR に自動投稿する**。
本 skill の目的は「Claude のレビュー結果を PR に残し、人間がそれを確認する」フローの実現であり、投稿前の承認ステップは設けない。

親エージェントの責務:
- PR 情報の取得（gh CLI）
- subagent への入力プロンプト構築
- subagent からの結果（JSON）受け取り
- gh CLI でのコメント投稿
- CLI への結果報告

subagent の責務:
- diff と関連コードの分析
- 客観的なレビュー本文の生成
- 構造化された JSON での結果返却

## 手順

### 1. 対象 PR の特定

引数が指定されている場合:
- 数値 → そのまま PR 番号として使用
- URL → 末尾の数値を抽出

引数なしの場合:

```bash
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
PR_NUMBER=$(gh pr list --head "$CURRENT_BRANCH" --state open --json number --jq '.[0].number')
```

PR が見つからない場合は **エラーで終了**：

> 「現在ブランチ `<branch>` に対応する open PR が見つかりません。先に GitHub 上で PR を作成するか、`/review-pr <PR番号>` で対象を指定してください。」

### 2. PR 情報の取得

```bash
gh pr view <num> --json number,title,body,author,baseRefName,headRefName,files,additions,deletions,url,headRefOid,headRepository,headRepositoryOwner
```

> 注意: `baseRepository` / `headRepository` というフィールドは `gh pr view --json` には**存在しない**（指定するとエラー）。リポジトリの owner/name は別途、以下で確実に取得する:
>
> ```bash
> REPO=$(gh repo view --json nameWithOwner --jq .nameWithOwner)   # 例: MurataKaito/claudecode_push_display
> ```
>
> （`headRepositoryOwner.login` + `headRepository.name` からも組み立てられるが、fork PR でなければ `gh repo view` が最も確実）

主要情報を整理:
- PR タイトル、本文
- 作成者
- ベースブランチ、ヘッドブランチ
- 変更ファイル一覧（path, additions, deletions）
- ヘッドコミット SHA（インラインコメント投稿時に必要）
- リポジトリ owner/name（上記 `REPO`）

### 3. Diff の取得

```bash
gh pr diff <num>
```

diff 全体を取得して、subagent に渡す入力に含める。

### 4. ローカルブランチの確認

subagent は `Read/Glob/Grep` でローカルファイルを参照するため、ローカルのチェックアウト状態が PR の head ブランチと一致している必要がある。

```bash
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
```

- `CURRENT_BRANCH` が PR の `headRefName` と **一致する場合** → そのまま続行
- **一致しない場合** → ユーザーに確認を取る:

> 「現在のローカルブランチ `<CURRENT_BRANCH>` は PR のブランチ `<headRefName>` と異なります。subagent がローカルファイルを参照する際に正しくないコードを読む可能性があります。`git checkout <headRefName>` で切り替えてから続行しますか？」

ユーザーが承認した場合は `git checkout <headRefName>` を実行してから次のステップへ進む。拒否した場合はそのまま続行するが、subagent プロンプトに以下の警告を追加する:

> 「注意: ローカルのチェックアウトブランチが PR の head ブランチと異なります。Read/Glob/Grep で確認するローカルファイルは PR の最新状態と一致しない可能性があります。diff の内容を最優先で判断してください。」

### 5. pr-reviewer subagent の起動（コアステップ）

`Agent` ツールを使って `pr-reviewer` subagent を起動する。

```
Agent ツールの呼び出し:
- subagent_type: pr-reviewer
- description: "PRレビュー実施"
- prompt: 下記参照
```

#### subagent に渡すプロンプトのテンプレート

**意図的に含めないもの**:

- **PR 作成者（author）**: バイアスを持ち込まないため
- **ヘッドコミット SHA**: 親が Step 8（インラインコメント投稿）で使うもの。subagent は知らなくてよい
- **変更ファイル一覧と行数**: diff から自分で読み取れる
- **親エージェントの実装意図や経緯**: 客観性を損なうため

**意図的に含めるもの**:

- **PR 本文（body）**: レビュアーが通常参照する公開情報であり、変更の目的を理解するために必要。親エージェントの内部的な実装経緯とは異なる

````
以下の GitHub PR を客観的にレビューしてください。

## リポジトリ情報

- リポジトリ: <owner>/<repo>
- PR番号: #<number>
- タイトル: <title>
- ヘッドブランチ: <headRefName>
- ベースブランチ: <baseRefName>
- PR URL: <url>

## PR 本文

```
<PR の body>
```

## Diff

```diff
<gh pr diff の出力をそのまま貼る>
```

## あなたの役割

あなたは独立したレビュアーです。私（呼び出し元エージェント）はこのコードを書いた本人かもしれませんが、あなたはそのコンテキストを引き継いでいません。第三者の目線で評価してください。

必要に応じて Read/Glob/Grep ツールでリポジトリ内の関連ファイル（変更後の状態）を確認してかまいません。このリポジトリの主な構成は次のとおりです:

- `firmware/` … ESP32 / M5Stack ファーム（C++ / PlatformIO）。`firmware/data/` は LittleFS 画像アセット
- `daemon/` … PC 側 TypeScript / Node デーモン（`daemon/src/*.ts`、テストは `daemon/src/*.test.ts` / vitest）
- `assets/clawd/` … Clawd 顔アニメ生成（`render.py` / `convert.mjs` / HTML プリセット + `creature-engine.js`）
- `docs/superpowers/specs/*.md`, `docs/superpowers/plans/*.md` … 設計書

## 出力

JSON 1つだけを返してください（前置きや説明は不要）:

```json
{
  "topLevelComment": "[Markdown 文字列]",
  "inlineComments": [
    { "path": "...", "line": 42, "side": "RIGHT", "body": "..." }
  ],
  "summary": {
    "criticalCount": 0,
    "importantCount": 0,
    "minorCount": 0,
    "suggestionCount": 0
  }
}
```

フォーマット仕様の詳細は `.claude/agents/pr-reviewer.md` を参照してください（あなた自身の system prompt 内に既に含まれています）。
````

### 6. subagent 結果の受け取りと検証

subagent からの応答テキストを JSON として解釈し、パースする。

応答がコードフェンス（` ```json ... ``` `）で囲まれている場合は、フェンスを除去してから JSON 部分のみを抽出すること。LLM は指示に反してコードフェンスを付与する場合がある。

検証ポイント:
- JSON 形式として valid か（コードフェンス除去後）
- 必須フィールドが揃っているか
- inlineComments の各要素に path/line/body が揃っているか

不正な形式の場合はエラー報告して終了（gh CLI 投稿はしない）。

### 7. トップレベルコメントの投稿

`gh pr comment` の `--body-file -` で **標準入力（stdin）から本文を読ませる**。
ヒアドキュメント `<<'EOF'` を使うことで、変数展開もコマンド実行もされず、本文を**リテラルとして安全に渡せる**。一時ファイル不要。

```bash
gh pr comment <num> --repo <owner>/<repo> --body-file - <<'EOF'
[result.topLevelComment の内容をそのまま貼る]
EOF
```

**重要**: ヒアドキュメントの開始タグは `<<'EOF'`（**シングルクォートで囲む**）にすること。
シングルクォートが無いと、本文中の `$variable` や `` `command` `` がシェルに評価されてしまう。

`EOF` という文字列が本文中に出現する可能性は極めて低いが、念のため別のセンチネル（例: `__CLAUDE_REVIEW_END__`）を使ってもよい。

### 8. インラインコメントの投稿

`result.inlineComments` の各要素について、個別に投稿。
`gh api` の `--input -` で **標準入力から JSON を読ませる**。
ヒアドキュメント `<<'EOF'` を使うことで、JSON 内の Markdown 本文（バッククォートや絵文字を含む）を**リテラルとして安全に渡せる**。一時ファイル不要。

```bash
gh api -X POST repos/<owner>/<repo>/pulls/<num>/comments --input - <<'EOF'
{
  "body": "[インラインコメント本文。\n は JSON として valid な改行表現]",
  "commit_id": "<headRefOid>",
  "path": "<path>",
  "line": <line>,
  "side": "<side: 通常は RIGHT。削除行への指摘なら LEFT>"
}
EOF
```

**重要**:

- ヒアドキュメントは `<<'EOF'`（**シングルクォート付き**）。シェル展開を無効化する
- JSON 内の文字列値は **正しくエスケープ** されている必要がある:
    - 改行 → `\n`
    - ダブルクォート → `\"`
    - バックスラッシュ → `\\`
- subagent の応答から `body` 等の値を取り出す際、**JSON 文字列としてのエスケープを維持したまま** 新しい JSON に埋め込むこと。概念的にパース（解釈）した場合は改行やクォートが生の文字に戻るため、新しい JSON に埋め込む際に再エスケープが必要になる
- 各インラインコメントごとにこのコマンドを1回ずつ実行する

### 9. 結果の CLI 出力（簡潔に）

投稿完了後、CLI に簡潔な確認のみ出す:

```
Claude PR Review 完了
- トップレベルコメント: 1件投稿
- インラインコメント: <inlineCommentsの要素数>件投稿
- 重大度内訳: 必須 <criticalCount> / 推奨 <importantCount> / 任意 <minorCount> / 提案 <suggestionCount>
- PR: <PR URL>
```

レビュー本文は CLI には出さない（PR コメントを見ればよい）。

## エラーハンドリング

### 対象 PR が見つからない
ステップ1の通り、エラーメッセージを出して終了。

### gh CLI の認証失敗
`gh auth status` で認証状態を確認するように案内。このリポジトリの remote は `github.com`（`MurataKaito/claudecode_push_display`）なので、`github.com` アカウントが有効である必要がある。

### subagent の応答が JSON として valid でない
- リトライは1回まで
- 2回目も失敗した場合は、subagent の応答そのままを CLI に出力してエラー終了

### subagent が「指摘ゼロ」を返した
- `inlineComments` が空配列、`summary` が全て 0
- トップレベルコメントは投稿する（要約・影響範囲・リスク・観点はあるはず）
- インラインコメント投稿はスキップ
- 「特に問題なし」のメッセージとして扱う

### インラインコメント投稿が失敗した場合
- 該当 line が diff に存在しない可能性 → スキップして次へ
- 全体を失敗扱いにせず、できる限り投稿を完了する
- 失敗した件数を CLI に報告

### 大規模 PR（diff が context window に収まらない）
- subagent への入力時点で diff が大きすぎる場合、変更ファイルを優先順位付けして必要なものだけ含める
- トップレベルコメントに「変更が大きいため一部ファイルは概観のみ」と明記するよう subagent に指示

### gh API レート制限
- 一定時間待ってリトライ
- それでも失敗するならエラーメッセージを出してユーザーに通知

## 注意事項

### subagent からの結果は加工しない

- subagent が返した topLevelComment / inlineComments の内容を親エージェントが書き換えない
- 「もっと優しい表現にしよう」「この指摘は不要では」のような介入をしない
- そのまま gh CLI で投稿する
- 親エージェントの仕事は「投稿する」だけで、「レビューする」ではない

### コメント投稿者

- 投稿は `gh` CLI の現ユーザーとして行われる（個人名）
- コメント本文の末尾に「`/review-pr` skill により 自動生成されました。」と明記されているので、人間レビュアーが混同することはない
