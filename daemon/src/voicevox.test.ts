import { describe, it, expect, vi } from "vitest";
import { synth } from "./voicevox.js";

describe("synth", () => {
  it("audio_query→synthesis の順に呼び、WAVのBufferを返す", async () => {
    const calls: string[] = [];
    const fetchImpl = vi.fn(async (url: string, _init?: RequestInit) => {
      calls.push(url);
      if (url.includes("/audio_query")) {
        return new Response(JSON.stringify({ accent_phrases: [] }), {
          status: 200,
          headers: { "Content-Type": "application/json" },
        });
      }
      return new Response(new Uint8Array([0x52, 0x49, 0x46, 0x46]), { status: 200 });
    }) as unknown as typeof fetch;

    const wav = await synth("こんにちは", {
      baseUrl: "http://vv.test",
      speakerId: 3,
      fetchImpl,
    });

    expect(Buffer.isBuffer(wav)).toBe(true);
    expect(wav.subarray(0, 4).toString("latin1")).toBe("RIFF");
    expect(calls[0]).toContain("/audio_query?");
    expect(calls[0]).toContain("speaker=3");
    expect(calls[1]).toContain("/synthesis?speaker=3");
  });

  it("audio_query が失敗したら例外", async () => {
    const fetchImpl = vi.fn(async () => new Response("", { status: 500 })) as unknown as typeof fetch;
    await expect(
      synth("x", { baseUrl: "http://vv.test", speakerId: 3, fetchImpl }),
    ).rejects.toThrow(/audio_query/);
  });
});
