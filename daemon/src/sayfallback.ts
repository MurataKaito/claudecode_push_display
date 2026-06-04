import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { readFile, unlink } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";

const execFileP = promisify(execFile);

// VOICEVOXが使えない時のフォールバック。macOSの say + afconvert で
// 24kHz/16bit/mono の WAV(Buffer) を生成する（M5の再生設定に一致）。
export async function saySynth(text: string): Promise<Buffer> {
  const base = join(tmpdir(), `zunda-say-${process.pid}-${Date.now()}`);
  const aiff = `${base}.aiff`;
  const wav = `${base}.wav`;
  try {
    try {
      await execFileP("say", ["-v", "Kyoko", "-o", aiff, text]);
    } catch {
      await execFileP("say", ["-o", aiff, text]); // Kyoko未導入なら既定ボイス
    }
    await execFileP("afconvert", ["-f", "WAVE", "-d", "LEI16@24000", "-c", "1", aiff, wav]);
    return await readFile(wav);
  } finally {
    await unlink(aiff).catch(() => {});
    await unlink(wav).catch(() => {});
  }
}
