import Fastify, { type FastifyInstance } from "fastify";
import type { Transport } from "./transport/index.js";
import { serifFor, type AppEvent } from "./serif.js";
import type { Usage } from "./usage.js";

export type ServerDeps = {
  transport: Transport;
  synth: (text: string) => Promise<Buffer>;
  getUsage: () => Promise<Usage>;
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

  return app;
}
