import { describe, it, expect, vi } from "vitest";
import { createServer } from "./server.js";
import type { Notification, Transport } from "./transport/index.js";
import type { Usage } from "./usage.js";
import { Approvals } from "./approvals.js";

const ZERO_USAGE: Usage = { percent: 0, used: 0, limit: 1, resetAt: null, resetMin: 0 };

function mockTransport(over: Partial<Transport> = {}): Transport {
  return {
    notify: async () => {},
    heartbeat: async () => {},
    requestApproval: async () => {},
    setBase: async () => {},
    ...over,
  };
}

describe("createServer /event", () => {
  it("done を受けると synth→notify→setBase(idle) を呼び 200 を返す", async () => {
    const notified: Notification[] = [];
    const bases: string[] = [];
    const transport = mockTransport({
      notify: async (n) => {
        notified.push(n);
      },
      setBase: async (s) => {
        bases.push(s);
      },
    });
    const synth = vi.fn(async (_text: string) => Buffer.from("WAVDATA"));

    const app = createServer({
      transport,
      synth,
      getUsage: async () => ZERO_USAGE,
      approvals: new Approvals(),
      approveTtlMs: 30000,
      now: () => 0,
    });
    const res = await app.inject({ method: "POST", url: "/event", payload: { type: "done" } });

    expect(res.statusCode).toBe(200);
    expect(synth).toHaveBeenCalledWith("おしごと、おわったのだ！");
    expect(notified[0].expr).toBe("happy");
    expect(bases).toEqual(["idle"]);
    await app.close();
  });

  it("working を受けると setBase(working) のみ（声なし）", async () => {
    const bases: string[] = [];
    const synth = vi.fn(async () => Buffer.from("x"));
    const transport = mockTransport({ setBase: async (s) => { bases.push(s); } });
    const app = createServer({
      transport,
      synth,
      getUsage: async () => ZERO_USAGE,
      approvals: new Approvals(),
      approveTtlMs: 30000,
      now: () => 0,
    });
    const res = await app.inject({ method: "POST", url: "/event", payload: { type: "working" } });
    expect(res.statusCode).toBe(200);
    expect(bases).toEqual(["working"]);
    expect(synth).not.toHaveBeenCalled();
    await app.close();
  });
});

describe("createServer /usage", () => {
  it("getUsage の結果を返す", async () => {
    const usage: Usage = { percent: 73, used: 7, limit: 10, resetAt: "Z", resetMin: 12 };
    const app = createServer({
      transport: mockTransport(),
      synth: async () => Buffer.from("x"),
      getUsage: async () => usage,
      approvals: new Approvals(),
      approveTtlMs: 30000,
      now: () => 0,
    });
    const res = await app.inject({ method: "GET", url: "/usage" });
    expect(res.statusCode).toBe(200);
    expect(res.json()).toEqual(usage);
    await app.close();
  });
});

describe("createServer approval flow", () => {
  it("POST /approve→M5要求→POST result→GET resultでallow", async () => {
    let approved: { id: string; title: string; detail: string } | null = null;
    const transport = mockTransport({
      requestApproval: async (id, title, detail) => {
        approved = { id, title, detail };
      },
    });
    const approvals = new Approvals();
    const app = createServer({
      transport,
      synth: async () => Buffer.from("x"),
      getUsage: async () => ZERO_USAGE,
      approvals,
      approveTtlMs: 30000,
      now: () => 0,
    });

    const r1 = await app.inject({
      method: "POST",
      url: "/approve",
      payload: { tool: "Bash", command: "ls -la" },
    });
    expect(r1.statusCode).toBe(200);
    const id = r1.json().id as string;
    expect(id).toBeTruthy();
    expect(approved!.title).toContain("Bash");

    const rp = await app.inject({ method: "GET", url: `/approve_result/${id}` });
    expect(rp.json()).toEqual({ decision: "pending" });

    await app.inject({ method: "POST", url: `/approve_result/${id}`, payload: { decision: "allow" } });
    const r2 = await app.inject({ method: "GET", url: `/approve_result/${id}` });
    expect(r2.json()).toEqual({ decision: "allow" });
    await app.close();
  });
});
