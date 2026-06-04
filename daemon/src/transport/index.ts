import type { Expr } from "../serif.js";

export type Notification = { expr: Expr; text: string; wav: Buffer };

export interface Transport {
  notify(n: Notification): Promise<void>;
}
