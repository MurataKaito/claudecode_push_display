export type Config = {
  port: number;
  voicevoxUrl: string;
  speakerId: number;
  m5Url: string;
  usageLimit: number;
  thresholds: number[];
  pollIntervalSec: number;
  approveTimeoutSec: number;
};

export function loadConfig(env: Record<string, string | undefined> = process.env): Config {
  return {
    port: Number(env.ZUNDA_PORT ?? 4920),
    voicevoxUrl: env.ZUNDA_VOICEVOX_URL ?? "http://127.0.0.1:50021",
    speakerId: Number(env.ZUNDA_SPEAKER_ID ?? 3),
    m5Url: env.ZUNDA_M5_URL ?? "http://stackchan.local",
    usageLimit: Number(env.ZUNDA_USAGE_LIMIT ?? 100_000_000),
    thresholds: (env.ZUNDA_THRESHOLDS ?? "50,80,95")
      .split(",")
      .map((s) => Number(s.trim()))
      .filter((n) => !Number.isNaN(n)),
    pollIntervalSec: Number(env.ZUNDA_POLL_SEC ?? 60),
    approveTimeoutSec: Number(env.ZUNDA_APPROVE_TIMEOUT_SEC ?? 30),
  };
}
