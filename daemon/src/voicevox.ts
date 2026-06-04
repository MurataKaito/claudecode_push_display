export type SynthOptions = {
  baseUrl: string;
  speakerId: number;
  fetchImpl?: typeof fetch;
};

export async function synth(text: string, opts: SynthOptions): Promise<Buffer> {
  const f = opts.fetchImpl ?? fetch;
  const q = new URLSearchParams({ text, speaker: String(opts.speakerId) });
  const queryRes = await f(`${opts.baseUrl}/audio_query?${q.toString()}`, { method: "POST" });
  if (!queryRes.ok) throw new Error(`audio_query failed: ${queryRes.status}`);
  const query = await queryRes.json();
  const synthRes = await f(`${opts.baseUrl}/synthesis?speaker=${opts.speakerId}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(query),
  });
  if (!synthRes.ok) throw new Error(`synthesis failed: ${synthRes.status}`);
  return Buffer.from(await synthRes.arrayBuffer());
}
