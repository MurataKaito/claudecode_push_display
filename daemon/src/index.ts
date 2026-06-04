import { loadConfig } from "./config.js";
import { createServer } from "./server.js";
import { WiFiTransport } from "./transport/wifi.js";
import { synth } from "./voicevox.js";

const cfg = loadConfig();
const transport = new WiFiTransport(cfg.m5Url);
const app = createServer({
  transport,
  synth: (text) => synth(text, { baseUrl: cfg.voicevoxUrl, speakerId: cfg.speakerId }),
});

app
  .listen({ port: cfg.port, host: "0.0.0.0" })
  .then(() =>
    console.log(`zundamon daemon: :${cfg.port}  M5=${cfg.m5Url}  VOICEVOX=${cfg.voicevoxUrl}`),
  )
  .catch((e) => {
    console.error(e);
    process.exit(1);
  });
