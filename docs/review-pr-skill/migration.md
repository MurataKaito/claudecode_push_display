# review-pr skill の移植・カスタマイズ

別プロジェクト由来の skill を本リポジトリへ移植し、環境に合わせて調整した記録（PR #2 / merge commit `544cee1`）。

## 出発点

- 別プロジェクト（社内 GitHub 上の Lambda/CDK 系リポジトリ）で運用していた `review-pr` skill と `pr-reviewer` agent の定義を、本リポジトリ向けに移植・調整して追加したい、という依頼。
- 元定義の前提:
  - GitHub ホストが社内 GitHub
  - `infrastructure/functions/<name>/`（Lambda）、`docs/<name>.md`（設計書）という構成
  - 仲間 skill `/self-review`（push 前自己チェック）と `/create-pr`（PR 作成＋自動チェーン）の存在
  - PBI 番号（例 `KSFDXPJ-486`）など社内チケット文化
  - 重大度・カテゴリを絵文字で表現

## 環境調査

導入先の実態を確認し、差し替え点を洗い出した。

| 項目 | 実態 |
|---|---|
| remote | `github.com/MurataKaito/claudecode_push_display`（社内 GitHub ではない） |
| gh 認証 | `github.com` アカウントは有効。社内 GitHub アカウントは失効（本件には無関係） |
| `.claude/` | 当初は未存在。`/self-review`・`/create-pr` もこの環境には無い |
| 構成 | `firmware/`（ESP32 C++ / PlatformIO）、`daemon/`（TypeScript / Node / vitest）、`assets/clawd/`（render.py・convert.mjs・HTML）、`docs/superpowers/`（specs・plans）、`hooks/` |

結論: 元定義のプロジェクト固有参照は**全面的に差し替えが必要**。

## 追加したファイル

- `.claude/skills/review-pr/SKILL.md`
- `.claude/agents/pr-reviewer.md`

## 行ったカスタマイズ

- **参照の差し替え**: Lambda(`infrastructure/functions/`)・設計書(`docs/<name>.md`)を、本リポジトリ構成（`firmware`/`daemon`/`assets/clawd`/`docs/superpowers`）へ。
- **ホスト**: 社内 GitHub の記述・URL 例を `github.com`（`MurataKaito/claudecode_push_display`）へ。
- **スタンドアロン化**: このリポジトリに無い `/self-review`・`/create-pr` との連携（役割分担表・自動チェーン）を外し、手動起動の単独 skill として整理。
- **チケット例の一般化**: PBI 番号など社内チケット参照例を、issue/PR/チケット番号・設計書ファイル名という一般表現に。
- **例パスの差し替え**: JSON 例の `infrastructure/functions/example/index.py` を `daemon/src/poller.ts` などに。

## 作業中に入った 2 つのフィードバック

1. **「絵文字はいらない」**: コミット前に指示が入ったため、`pr-reviewer.md` の絵文字（重大度の赤丸・橙丸等、カテゴリ絵文字、見出し絵文字）を**全廃しテキスト化**してからコミットした。`review-pr` SKILL.md 側は元から絵文字なし。
2. **重大度ラベルの変更**: 「重大/重要/軽微/提案」を「**必須/推奨/任意/提案**」（what-to-do 起点）へ変更。選択肢を提示して確定。JSON キー（`criticalCount` 等）は内部識別子として維持し、表示ラベルのみ差し替え。

作業順序として、絵文字除去・ラベル確定を**コミット前に**完了することで、後からの手戻り再編集を避けた。

## マージ

`gh pr create`（PR #2）→ `gh pr merge --squash --delete-branch`。main に `544cee1` として入った。

## 関連

- 絵文字なし・ラベル変更の設計上の理由: [`decisions.md`](decisions.md)
- 実際に使ってみた記録: [`review-run.md`](review-run.md)
- PR 別の変更履歴: [`changelog.md`](changelog.md)
