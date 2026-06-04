# claudecode_push_display — トークン残量お知らせずんだもん

Claude Code の状態を、机の上の **M5Stack Core2** のずんだもんが「物理的な相棒」として知らせる。
（ProtoPedia #8507「トークン残量お知らせずんだもん🫛」にインスパイア）

## Phase 1（実装済み・実機確認済み）
Claude Code のタスク完了を、ずんだもんが **表情＋音声**（M5スピーカー発話）で通知する。

- 母艦デーモン(Mac)が Claude Code の `Stop` フックから完了イベントを受信
- VOICEVOX でずんだもんボイスを生成（未起動時は macOS `say` に自動フォールバック）
- WiFi HTTP（mDNS: `stackchan.local`）で M5 に送信 → 表情変化＋スピーカー再生

> Phase 2以降（別計画）: 画面2回タッチで使用率表示 / 残量警告 / 許可リクエストのタップ承認 / 立ち絵 / USBトランスポート

## 構成
```
daemon/    … Mac常駐デーモン (Node/TS): フック受け口・VOICEVOX/say・M5通知
hooks/     … Claude Code フック (Stop)
firmware/  … M5Stack Core2 ファーム (PlatformIO/M5Unified)
assets/    … ずんだもん立ち絵（任意）
docs/superpowers/  … 設計(spec)・実装計画(plan)
```

## セットアップ
1. **daemon**: `cd daemon && npm install && npm run dev`（既定 `:4920`）
2. **firmware**: `firmware/include/secrets.h` にWiFi情報を記入し
   `cd firmware && pio run -e core2 -t upload`
3. **Stopフック**: `~/.claude/settings.json` の `hooks.Stop` に
   `bash <repo>/hooks/stop.sh` を登録（新規セッションで有効）
4. **ネットワーク**: Mac と M5 を同一ネットワークに（自宅WiFi or iPhoneテザリング）
   - ⚠️ ESP32 は **2.4GHz のみ**。iPhoneテザリングは「互換性を優先」をON
5. （任意）**VOICEVOX** を起動（`http://127.0.0.1:50021`）すると、声がずんだもんボイスに切替

## テスト
- daemon: `cd daemon && npm test` / `npm run build`
- hook: `bash hooks/stop.test.sh`
- firmware(ロジック): `cd firmware && pio test -e native`

## クレジット
- 音声: **VOICEVOX:ずんだもん**
- キャラクター: 東北ずん子・ずんだもんプロジェクト（キャラクター利用の手引きに従う）
