import { execFile } from "node:child_process";
import { promisify } from "node:util";

const execFileP = promisify(execFile);

export type ActiveBlock = { totalTokens: number; endTime: string };
export type Runner = (cmd: string, args: string[]) => Promise<string>;

const defaultRunner: Runner = async (cmd, args) => {
  const { stdout } = await execFileP(cmd, args, { maxBuffer: 16 * 1024 * 1024 });
  return stdout;
};

export async function getActiveBlock(runner: Runner = defaultRunner): Promise<ActiveBlock | null> {
  try {
    const out = await runner("npx", ["-y", "ccusage@latest", "blocks", "--active", "--json"]);
    const data = JSON.parse(out);
    const block = data?.blocks?.[0];
    if (!block || typeof block.totalTokens !== "number" || typeof block.endTime !== "string") {
      return null;
    }
    return { totalTokens: block.totalTokens, endTime: block.endTime };
  } catch {
    return null;
  }
}
