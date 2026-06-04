import type { Expr } from "../serif.js";

export type Notification = { expr: Expr; text: string; wav: Buffer };

export interface Transport {
  notify(n: Notification): Promise<void>;
  heartbeat(daemonPort: number): Promise<void>;
  requestApproval(id: string, title: string, detail: string): Promise<void>;
  setBase(state: string): Promise<void>; // "idle" | "working"
}
