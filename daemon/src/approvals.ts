export type Decision = "allow" | "deny";
export type ApprovalStatus = Decision | "pending" | "timeout" | "unknown";

type Record = { decision: Decision | null; expiresAt: number };

export class Approvals {
  private map = new Map<string, Record>();

  create(id: string, ttlMs: number, now: number): void {
    this.map.set(id, { decision: null, expiresAt: now + ttlMs });
  }

  resolve(id: string, decision: Decision): boolean {
    const r = this.map.get(id);
    if (!r) return false;
    r.decision = decision;
    return true;
  }

  get(id: string, now: number): ApprovalStatus {
    const r = this.map.get(id);
    if (!r) return "unknown";
    if (r.decision) return r.decision;
    if (now > r.expiresAt) return "timeout";
    return "pending";
  }
}
