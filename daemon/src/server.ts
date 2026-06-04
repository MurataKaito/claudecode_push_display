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
    const serif = serifFor(event);
    const wav = await deps.synth(serif.text);
    await deps.transport.notify({ expr: serif.expr, text: serif.text, wav });
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
  });

  return app;
}
