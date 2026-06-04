export type Expr = "normal" | "happy" | "worried" | "surprised" | "working" | "wink";
export type AppEvent = { type: "done" | "working" | "idle" | "attention"; summary?: string };
export type Serif = { expr: Expr; text: string };

export function serifFor(event: AppEvent): Serif {
  switch (event.type) {
    case "done":
      return { expr: "happy", text: "おしごと、おわったのだ！" };
    case "attention":
      return { expr: "surprised", text: "よばれたのだ？" };
    default:
      return { expr: "normal", text: "なのだ" };
  }
}

export function warnSerif(threshold: number): Serif {
  return { expr: "worried", text: `もう ${threshold}％ つかったのだ、きをつけるのだ！` };
}
