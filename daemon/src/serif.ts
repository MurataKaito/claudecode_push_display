export type Expr = "normal" | "happy" | "worried" | "surprised";
export type AppEvent = { type: "done"; summary?: string };
export type Serif = { expr: Expr; text: string };

export function serifFor(event: AppEvent): Serif {
  switch (event.type) {
    case "done":
      return { expr: "happy", text: "おしごと、おわったのだ！" };
    default:
      return { expr: "normal", text: "なのだ" };
  }
}
