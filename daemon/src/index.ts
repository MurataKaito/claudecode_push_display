import { loadConfig } from "./config.js";
import { createServer } from "./server.js";
import { WiFiTransport } from "./transport/wifi.js";
import { synth } from "./voicevox.js";
import { saySynth } from "./sayfallback.js";
import { getActiveBlock } from "./ccusage.js";
import { computeUsage } from "./usage.js";
import { startPoller } from "./poller.js";
import { warnSerif } from "./serif.js";

const cfg = loadConfig();
const transport = new WiFiTransport(cfg.m5Url);

async function synthFn(text: string): Promise<Buffer> {
  try {
    return await synth(text, { baseUrl: cfg.voicevoxUrl, speakerId: cfg.speakerId });
  } catch (e) {
    console.warn(`VOICEVOX不可(${(e as Error).message}) → say にフォールバック`);
    return await saySynth(text);
  }
}

const app = createServer({
  transport,
  synth: synthFn,
  getUsage: async () => computeUsage(await getActiveBlock(), cfg.usageLimit, Date.now()),
});

app
  .listen({ port: cfg.port, host: "0.0.0.0" })
  .then(() => {
    console.log(`zundamon daemon: :${cfg.port}  M5=${cfg.m5Url}  VOICEVOX=${cfg.voicevoxUrl}`);
    const beat = () => transport.heartbeat(cfg.port).catch(() => {});
    beat();
    setInterval(beat, 15000);

    startPoller(
      {
        getActiveBlock,
        usageLimit: cfg.usageLimit,
        thresholds: cfg.thresholds,
        now: () => Date.now(),
        onWarn: async (threshold) => {
          const serif = warnSerif(threshold);
          const wav = await synthFn(serif.text);
          await transport.notify({ expr: serif.expr, text: serif.text, wav });
        },
      },
      cfg.pollIntervalSec * 1000,
    );
  })
  .catch((e) => {
    console.error(e);
    process.exit(1);
  });
