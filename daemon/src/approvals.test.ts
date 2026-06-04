import { describe, it, expect } from "vitest";
import { Approvals } from "./approvals.js";

describe("Approvals", () => {
  it("作成直後は pending", () => {
    const a = new Approvals();
    a.create("id1", 1000, 0);
    expect(a.get("id1", 100)).toBe("pending");
  });

  it("resolve で allow/deny", () => {
    const a = new Approvals();
    a.create("id1", 1000, 0);
    expect(a.resolve("id1", "allow")).toBe(true);
    expect(a.get("id1", 100)).toBe("allow");
  });

  it("期限超過で timeout", () => {
    const a = new Approvals();
    a.create("id1", 1000, 0);
    expect(a.get("id1", 2000)).toBe("timeout");
  });

  it("未知idは unknown", () => {
    const a = new Approvals();
    expect(a.get("nope", 0)).toBe("unknown");
    expect(a.resolve("nope", "allow")).toBe(false);
  });
});
