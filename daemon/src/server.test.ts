import { describe, it, expect, vi } from "vitest";
import { createServer } from "./server.js";
import type { Notification, Transport } from "./transport/index.js";
import type { Usage } from "./usage.js";

describe("createServer /event", () => {
  it("done を受けると synth→transport.notify を呼び 200 を返す", async () => {
    const notified: Notification[] = [];
    const transport: Transport = {
      notify: async (n) => {
        notified.push(n);
      },
      heartbeat: async () => {},
    };
    const synth = vi.fn(async (_text: string) => Buffer.from("WAVDATA"));
    const getUsage = async (): Promise<Usage> => ({
      percent: 42,
      used: 1,
      limit: 2,
      resetAt: null,
      resetMin: 0,
    });

    const app = createServer({ transport, synth, getUsage });
    const res = await app.inject({
      method: "POST",
      url: "/event",
      payload: { type: "done" },
    });

    expect(res.statusCode).toBe(200);
    expect(res.json()).toEqual({ ok: true });
    expect(synth).toHaveBeenCalledWith("おしごと、おわったのだ！");
    expect(notified).toHaveLength(1);
    expect(notified[0].expr).toBe("happy");
    expect(notified[0].text).toBe("おしごと、おわったのだ！");
    expect(notified[0].wav.toString()).toBe("WAVDATA");
    await app.close();
  });
});

describe("createServer /usage", () => {
  it("getUsage の結果を返す", async () => {
    const transport: Transport = { notify: async () => {}, heartbeat: async () => {} };
    const synth = async () => Buffer.from("x");
    const usage: Usage = { percent: 73, used: 7, limit: 10, resetAt: "Z", resetMin: 12 };
    const app = createServer({ transport, synth, getUsage: async () => usage });
    const res = await app.inject({ method: "GET", url: "/usage" });
    expect(res.statusCode).toBe(200);
    expect(res.json()).toEqual(usage);
    await app.close();
  });
});
