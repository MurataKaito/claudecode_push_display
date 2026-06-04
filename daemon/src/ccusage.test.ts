import { describe, it, expect } from "vitest";
import { getActiveBlock } from "./ccusage.js";

const FIXTURE = JSON.stringify({
  blocks: [
    {
      id: "2026-06-04T01:00:00.000Z",
      startTime: "2026-06-04T01:00:00.000Z",
      endTime: "2026-06-04T06:00:00.000Z",
      isActive: true,
      totalTokens: 82818924,
      tokenCounts: { inputTokens: 21041, outputTokens: 354628 },
    },
  ],
});

describe("getActiveBlock", () => {
  it("active blockのtotalTokensとendTimeを返す", async () => {
    const block = await getActiveBlock(async () => FIXTURE);
    expect(block).toEqual({ totalTokens: 82818924, endTime: "2026-06-04T06:00:00.000Z" });
  });

  it("runnerが落ちたらnull", async () => {
    const block = await getActiveBlock(async () => {
      throw new Error("boom");
    });
    expect(block).toBeNull();
  });

  it("blocksが空ならnull", async () => {
    const block = await getActiveBlock(async () => JSON.stringify({ blocks: [] }));
    expect(block).toBeNull();
  });
});
