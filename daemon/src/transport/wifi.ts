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
      body: n.wav,
    });
    if (!res.ok) throw new Error(`notify failed: ${res.status}`);
  }
}
