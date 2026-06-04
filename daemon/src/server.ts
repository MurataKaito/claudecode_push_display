import Fastify, { type FastifyInstance } from "fastify";
import { randomUUID } from "node:crypto";
import type { Transport } from "./transport/index.js";
import { serifFor, type AppEvent } from "./serif.js";
import type { Usage } from "./usage.js";
import type { Approvals, Decision } from "./approvals.js";

export type ServerDeps = {
  transport: Transport;
  synth: (text: string) => Promise<Buffer>;
  getUsage: () => Promise<Usage>;
  approvals: Approvals;
  approveTtlMs: number;
  now: () => number;
};

export function createServer(deps: ServerDeps): FastifyInstance {
  const app = Fastify({ logger: false });

  app.post("/event", async (req, reply) => {
    const event = req.body as AppEvent;
    // ベース状態（声なし）
    if (event.type === "working") {
      await deps.transport.setBase("working");
      reply.send({ ok: true });
      // 応答後に非同期で「作業開始」発話（プロンプト送信をブロックしない）
      (async () => {
        try {
          const wav = await deps.synth("おしごと、するのだ！");
          await deps.transport.notify({ expr: "working", text: "おしごとちゅうなのだ", wav });
        } catch {}
      })();
      return;
    }
    if (event.type === "idle") {
      await deps.transport.setBase("idle");
      reply.send({ ok: true });
      return;
    }
    if (event.type === "attention") {
      // 入力待ち：surprise顔だけ（声なし）。ダミーボディでアニメのみ出す（playWavは失敗→無音）
      await deps.transport.notify({ expr: "surprised", text: "よばれてるのだ？", wav: Buffer.from("MUTE") });
      reply.send({ ok: true });
      return;
    }
    // 声つきイベント（done=完了）
    const serif = serifFor(event);
    const wav = await deps.synth(serif.text);
    await deps.transport.notify({ expr: serif.expr, text: serif.text, wav });
    if (event.type === "done") await deps.transport.setBase("idle"); // 完了後はidleへ
    reply.send({ ok: true });
  });

  app.get("/usage", async (_req, reply) => {
    reply.send(await deps.getUsage());
  });

  app.post("/approve", async (req, reply) => {
    const { tool, command } = (req.body ?? {}) as { tool?: string; command?: string };
    const id = randomUUID();
    deps.approvals.create(id, deps.approveTtlMs, deps.now());
    const title = `CLAUDE ${tool ?? "TOOL"} OK?`;
    await deps.transport.requestApproval(id, title, command ?? "");
    reply.send({ id });
  });

  app.get("/approve_result/:id", async (req, reply) => {
    const { id } = req.params as { id: string };
    reply.send({ decision: deps.approvals.get(id, deps.now()) });
  });

  app.post("/approve_result/:id", async (req, reply) => {
    const { id } = req.params as { id: string };
    const { decision } = (req.body ?? {}) as { decision?: Decision };
    const ok = decision ? deps.approvals.resolve(id, decision) : false;
    reply.send({ ok });
    // 承認OK時は wink＋声「オッケーなのだ」（応答後に非同期）
    if (decision === "allow") {
      (async () => {
        try {
          const wav = await deps.synth("オッケーなのだ");
          await deps.transport.notify({ expr: "wink", text: "オッケーなのだ", wav });
        } catch {}
      })();
    }
  });

  return app;
}
