# review-pr skill ドキュメント

このディレクトリは、本リポジトリに追加した PR レビュー自動化の仕組み（`/review-pr` skill ＋ `pr-reviewer` subagent）について、**設計・実装・導入までの記録**をまとめたものです。

## 何のための仕組みか

GitHub の Pull Request を Claude が**客観的にコードレビュー**し、結果を **PR コメント（トップレベル＋インライン）として自動投稿**する。人間レビュアーが読む「下地」を機械的に用意することが目的。

レビュー本体は親エージェント（skill 実行者）ではなく、**コンテキストを分離した `pr-reviewer` subagent** が担当する。これにより「自分が書いたコードだから正しい」というバイアスを排除する。

## ファイル構成（用途別）

| ファイル | 用途 | 読む人 |
|---|---|---|
| [`design.md`](design.md) | 設計・アーキテクチャ・設計判断の根拠 | 仕組みを理解／改修する人 |
| [`implementation-log.md`](implementation-log.md) | 導入から完成までの作業記録（ブランチ・PR・カスタマイズ・実レビュー・バグ修正） | 経緯を追う人／同様の移植をする人 |
| この `README.md` | 概要・索引・最小限の使い方 | 最初に読む人 |

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

- 元になった skill は別プロジェクト（社内 GitHub の Lambda/CDK 系リポジトリ）で運用していたものを、本リポジトリ向けに移植・調整した。経緯は [`implementation-log.md`](implementation-log.md) を参照。
- このリポジトリの他の設計ドキュメント: `docs/superpowers/specs/`, `docs/superpowers/plans/`
