import { loadConfig } from "./config.js";
import { createServer } from "./server.js";
import { WiFiTransport } from "./transport/wifi.js";
import { synth } from "./voicevox.js";
import { saySynth } from "./sayfallback.js";

const cfg = loadConfig();
const transport = new WiFiTransport(cfg.m5Url);

// VOICEVOXを試し、ダメなら say にフォールバック。
async function synthFn(text: string): Promise<Buffer> {
  try {
    return await synth(text, { baseUrl: cfg.voicevoxUrl, speakerId: cfg.speakerId });
  } catch (e) {
    console.warn(`VOICEVOX不可(${(e as Error).message}) → say にフォールバック`);
    return await saySynth(text);
  }
}

const app = createServer({ transport, synth: synthFn });

app
  .listen({ port: cfg.port, host: "0.0.0.0" })
  .then(() =>
    console.log(`zundamon daemon: :${cfg.port}  M5=${cfg.m5Url}  VOICEVOX=${cfg.voicevoxUrl}`),
  )
  .catch((e) => {
    console.error(e);
    process.exit(1);
  });
