import type { Transport, Notification } from "./index.js";

export class WiFiTransport implements Transport {
  constructor(
    private m5Url: string,
    private fetchImpl: typeof fetch = fetch,
  ) {}

  async notify(n: Notification): Promise<void> {
    const q = new URLSearchParams({ expr: n.expr, text: n.text });
    const res = await this.fetchImpl(`${this.m5Url}/notify?${q.toString()}`, {
      method: "POST",
      headers: { "Content-Type": "audio/wav" },
      // Buffer<ArrayBufferLike> は fetch の BodyInit に直接代入できない(@types/node v22)。
      // 具体的な ArrayBuffer 裏付けの Uint8Array に変換して渡す。
      body: Uint8Array.from(n.wav),
    });
    if (!res.ok) throw new Error(`notify failed: ${res.status}`);
  }

  async heartbeat(daemonPort: number): Promise<void> {
    await this.fetchImpl(`${this.m5Url}/heartbeat?port=${daemonPort}`, { method: "POST" });
  }
}
