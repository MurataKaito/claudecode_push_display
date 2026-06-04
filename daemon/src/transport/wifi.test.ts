import { describe, it, expect, vi } from "vitest";
import { WiFiTransport } from "./wifi.js";

describe("WiFiTransport", () => {
  it("M5 /notify に expr/text クエリ＋WAVボディでPOSTする", async () => {
    let seenUrl = "";
    let seenBody: Uint8Array | undefined;
    let seenContentType = "";
    const fetchImpl = vi.fn(async (url: string, init?: RequestInit) => {
      seenUrl = url;
      seenBody = init?.body as Uint8Array;
      seenContentType = (init?.headers as Record<string, string>)["Content-Type"];
      return new Response('{"ok":true}', { status: 200 });
    }) as unknown as typeof fetch;

    const t = new WiFiTransport("http://stackchan.local", fetchImpl);
    await t.notify({ expr: "happy", text: "やったのだ", wav: Buffer.from([1, 2, 3]) });

    expect(seenUrl).toContain("http://stackchan.local/notify?");
    expect(seenUrl).toContain("expr=happy");
    expect(decodeURIComponent(seenUrl)).toContain("text=やったのだ");
    expect(seenContentType).toBe("audio/wav");
    expect(Array.from(seenBody!)).toEqual([1, 2, 3]);
  });

  it("非2xxなら例外", async () => {
    const fetchImpl = vi.fn(async () => new Response("", { status: 503 })) as unknown as typeof fetch;
    const t = new WiFiTransport("http://stackchan.local", fetchImpl);
    await expect(
      t.notify({ expr: "normal", text: "x", wav: Buffer.from([0]) }),
    ).rejects.toThrow(/notify failed/);
  });
});
