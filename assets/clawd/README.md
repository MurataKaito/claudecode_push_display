# Clawd（ClaudePix）アニメ素材の取り込み

M5Stack Core2 は HTML を描画できないため、HTMLアニメの「フレーム（ドットの並び）」を
抽出してPNG化し、ネイティブ描画でアニメ表示します（見た目はピクセル単位で同じ）。

## 使い方
1. [ClaudePix](https://claudepix.vercel.app/) で各アニメを **copy code**
2. **`html/` フォルダ**に **1プリセット＝1ファイル**（`*.html`）で保存（13個）
3. 「置いた」と伝える
4. 各HTMLからフレームを抽出してPNG化 → `firmware/data/clawd0.png, …` →
   `pio run -e core2 -t uploadfs` で M5 に書き込み → 画面でループ再生
   （どのアニメをidle/完了/警告に割り当てるかは置いた後で相談）

## 注意
- ClaudePix は明示ライセンスが無いため、利用は各自の判断・責任で（個人デバイス用途）。
- 生成した `firmware/data/*.png` はリポジトリにはコミットしない（`.gitignore` 済み）。
