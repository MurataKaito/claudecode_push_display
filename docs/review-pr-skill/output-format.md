# review-pr skill 出力フォーマットと投稿

subagent が返す出力契約・重大度モデルと、親がそれを PR に投稿する仕組みをまとめる。判断の根拠は [`decisions.md`](decisions.md)。

## 出力契約（構造化 JSON）

subagent は **JSON 1 つだけ**を返す。親はそれをパースして投稿する。

```json
{
  "topLevelComment": "Markdown 文字列",
  "inlineComments": [
    { "path": "daemon/src/poller.ts", "line": 42, "side": "RIGHT", "body": "Markdown 文字列" }
  ],
  "summary": { "criticalCount": 0, "importantCount": 1, "minorCount": 2, "suggestionCount": 1 }
}
```

- `topLevelComment`: PR 全体への要約・影響範囲・リスク・観点・指摘テーブル。
- `inlineComments`: 行を特定できる指摘。`line` は **ファイル内の行番号**（diff ハンクヘッダ `@@ -a,b +c,d @@` の `+` 側）。diff 出力自体の行カウントではない。
- `summary`: 重大度別件数。「主要な指摘」テーブルの行数と一致させる。

構造化 JSON にするのは、親が決定的にパースして gh CLI へ流し込めるようにするため。自然文だと投稿の自動化が壊れやすい。

## 重大度モデル（4 段階）

「何をすべきか」起点のテキストラベル。絵文字は使わない。

| ラベル | 意味 | 対応 JSON キー |
|---|---|---|
| 必須 | バグ・脆弱性・明らかに動かない・データ損失。マージ前に必ず直す | `criticalCount` |
| 推奨 | 設計上の問題・明確な改善余地・テスト不足。強く推奨 | `importantCount` |
| 任意 | スタイル・軽微な改善・命名の一貫性。余裕があれば | `minorCount` |
| 提案 | アイディア・強い推奨ではない代替案 | `suggestionCount` |

表示ラベルと JSON キーを分離している理由は [`decisions.md`](decisions.md) を参照。

## 投稿の安全設計

本文には Markdown（バッククォート、`$`、引用符など）が含まれる。シェルで安全に渡すための工夫。

### トップレベルコメント

```bash
gh pr comment <num> --repo <owner>/<repo> --body-file - <<'EOF'
（本文をそのまま）
EOF
```

- `--body-file -` で **標準入力**から本文を読ませる（一時ファイル不要）。
- ヒアドキュメントの開始タグを **シングルクォート**（`<<'EOF'`）にすることで、本文中の `$変数` や `` `コマンド` `` がシェルに評価されない。リテラルとして安全に渡る。
- 本文に `EOF` が出現する場合に備え、別センチネル（例: `__CLAUDE_REVIEW_END__`）も使える。

### インラインコメント

```bash
gh api -X POST repos/<owner>/<repo>/pulls/<num>/comments --input - <<'EOF'
{ "body": "...", "commit_id": "<headRefOid>", "path": "<path>", "line": <line>, "side": "RIGHT" }
EOF
```

- `--input -` で標準入力から JSON を読ませる。
- JSON 文字列値は正しくエスケープ（改行 `\n`、`"` は `\"`、`\` は `\\`）。subagent の応答から値を取り出して新しい JSON に埋める際は、**JSON エスケープを維持したまま**埋める。
- `side` は通常 `RIGHT`（追加・変更行）、削除行への指摘なら `LEFT`。
