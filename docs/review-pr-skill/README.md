# review-pr skill ドキュメント

このディレクトリは、本リポジトリに追加した PR レビュー自動化の仕組み（`/review-pr` skill ＋ `pr-reviewer` subagent）について、**設計・実装・導入までの記録**を用途別にまとめたもの。

## 何のための仕組みか

GitHub の Pull Request を Claude が**客観的にコードレビュー**し、結果を **PR コメント（トップレベル＋インライン）として自動投稿**する。人間レビュアーが読む「下地」を機械的に用意することが目的。

レビュー本体は親エージェント（skill 実行者）ではなく、**コンテキストを分離した `pr-reviewer` subagent** が担当する。これにより「自分が書いたコードだから正しい」というバイアスを排除する。

## ファイル構成（用途別）

設計（どう作られているか・なぜそうしたか）:

| ファイル | 用途 |
|---|---|
| [`architecture.md`](architecture.md) | 目的・2役構成・責務分界・入力設計・親の不変条件 |
| [`decisions.md`](decisions.md) | 設計判断の根拠（なぜ subagent に委譲、なぜ絵文字なし 等） |
| [`output-format.md`](output-format.md) | 出力 JSON 契約・重大度モデル・投稿の安全設計 |
| [`error-handling.md`](error-handling.md) | エラーハンドリング方針 |

実装・導入（何をしたか）:

| ファイル | 用途 |
|---|---|
| [`migration.md`](migration.md) | 別プロジェクトからの移植・環境調査・カスタマイズ（PR #2） |
| [`review-run.md`](review-run.md) | 追加した skill で PR #1 を実レビューした記録 |
| [`changelog.md`](changelog.md) | PR 別の変更履歴・成果物・学び・残課題 |

実体（運用される定義ファイル）は本ディレクトリではなく以下にある:

- `.claude/skills/review-pr/SKILL.md` … 親エージェントの手順書
- `.claude/agents/pr-reviewer.md` … レビュー担当 subagent の定義（system prompt）

## 最小限の使い方

```
/review-pr            # 現在ブランチに対応する open PR を自動検出
/review-pr 1          # PR 番号を指定
/review-pr <PR URL>   # URL でも可
```

前提:

- `gh` CLI が `github.com`（このリポジトリの remote）で認証済みであること（`gh auth status` で確認）
- レビュー対象 PR の head ブランチをローカルにチェックアウトしていること（subagent がローカルファイルも参照するため）

詳細な手順・投稿コマンド・エラーハンドリングは `.claude/skills/review-pr/SKILL.md` を参照。

## 関連

- 元になった skill は別プロジェクト（社内 GitHub の Lambda/CDK 系リポジトリ）で運用していたものを、本リポジトリ向けに移植・調整した。経緯は [`migration.md`](migration.md)。
- このリポジトリの他の設計ドキュメント: `docs/superpowers/specs/`, `docs/superpowers/plans/`
