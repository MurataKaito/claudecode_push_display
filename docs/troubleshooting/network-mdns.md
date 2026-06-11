# ネットワークのつまづき記録（mDNS・M5 への到達性）

Mac（daemon）から M5 への通知が届かない・不安定になる問題の記録。表示やタッチが正しくても、そもそも通知が届かなければキャラは動かない。根拠は git のコミットと開発セッションの会話記録（2026-06-04）。

## 1. テザリング環境で mDNS（stackchan.local）が引けなくなる

- **問題**: 固定のローカル WiFi では動くのに、iPhone テザリングに切り替えると `stackchan.local` の名前解決が断続的に失敗し、daemon から M5 に通知が届かなくなる。実測では `stackchan.local` 宛の curl が 3 回中 3 回 HTTP 000（到達失敗）、IP 直指定に変えると 5 回中 5 回成功という明確な差が出た。
- **原因**: ESP32 の mDNS 実装は強くなく、特にテザリングのようなアドホックなネットワークでは応答が不安定になりやすい。加えて ESP32 は 2.4GHz 帯のみ対応で、iPhone テザリングは「互換性を優先」を ON にしないと 2.4GHz が有効にならない。
- **対処**: 段階的に 3 つ。

  1. **通信の堅牢化**: `/approve` への通知を JSON ボディから**クエリパラメータ方式**に変更（M5 側のボディ受信処理の方が壊れやすかったため）。コミット `e739a20`（`daemon/src/transport/wifi.ts`）

     ```ts
     // 変更前: POST + JSON body
     // 変更後:
     const q = new URLSearchParams({ id, title, detail });
     await this.fetchImpl(`${this.m5Url}/approve?${q}`, { method: "POST" });
     ```

  2. **IP 直指定の運用ガイド**: mDNS に頼らず M5 の IP を直接指定して daemon を起動する手順を README に明記。コミット `cc5d8db`

     ```bash
     ZUNDA_M5_URL=http://<M5のIP> npm run dev
     ```

  3. **（未実装・TODO）**: M5 からの heartbeat による動的 IP 学習。mDNS なしでも daemon が M5 の現在 IP を追従できるようにする構想。
- **発見・教訓**:
  - mDNS は「あると便利」程度に位置づけ、**信頼できる前提にしない**。IP 直指定というフォールバックを最初から運用に組み込む。
  - 通信の形式選択（ボディ vs クエリ）も堅牢性に効く。組込み側の受信実装が弱い場合、シンプルな形式（クエリパラメータ）に寄せる。
  - ネットワーク起因の問題は「実装は正しいのに動かない」ように見える。環境（テザリング / 2.4GHz / mDNS）を疑う切り分け順を持っておく。

## 関連

- キャラ表示のつまづき: [`clawd-display.md`](clawd-display.md)
- タッチ操作のつまづき: [`touch-gesture.md`](touch-gesture.md)
- ハード・環境のつまづき: [`hardware-environment.md`](hardware-environment.md)
- セットアップ時のネットワーク注意: ルート `README.md` のセットアップ節
