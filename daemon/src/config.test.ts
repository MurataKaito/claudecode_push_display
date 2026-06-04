import { describe, it, expect } from "vitest";
import { loadConfig } from "./config.js";

describe("loadConfig", () => {
  it("空envなら既定値を返す", () => {
    const c = loadConfig({});
    expect(c).toEqual({
      port: 4920,
      voicevoxUrl: "http://127.0.0.1:50021",
      speakerId: 3,
      m5Url: "http://stackchan.local",
      usageLimit: 100_000_000,
      thresholds: [50, 80, 95],
      pollIntervalSec: 60,
    });
  });

  it("env で上書きできる", () => {
    const c = loadConfig({
      ZUNDA_PORT: "5000",
      ZUNDA_VOICEVOX_URL: "http://localhost:60000",
      ZUNDA_SPEAKER_ID: "1",
      ZUNDA_M5_URL: "http://192.168.0.5",
    });
    expect(c.port).toBe(5000);
    expect(c.voicevoxUrl).toBe("http://localhost:60000");
    expect(c.speakerId).toBe(1);
    expect(c.m5Url).toBe("http://192.168.0.5");
    expect(loadConfig({ ZUNDA_USAGE_LIMIT: "5000000" }).usageLimit).toBe(5_000_000);
    expect(loadConfig({ ZUNDA_THRESHOLDS: "30,70" }).thresholds).toEqual([30, 70]);
    expect(loadConfig({ ZUNDA_POLL_SEC: "10" }).pollIntervalSec).toBe(10);
  });
});
