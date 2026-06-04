import { describe, it, expect, vi } from "vitest";
import { createServer } from "./server.js";
import type { Notification, Transport } from "./transport/index.js";

describe("createServer /event", () => {
  it("done を受けると synth→transport.notify を呼び 200 を返す", async () => {
    const notified: Notification[] = [];
    const transport: Transport = {
      notify: async (n) => {
        notified.push(n);
      },
    };
    const synth = vi.fn(async (_text: string) => Buffer.from("WAVDATA"));

    const app = createServer({ transport, synth });
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
