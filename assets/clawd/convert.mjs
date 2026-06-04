// ClaudePixのアニメHTML → フレームJSON（{name, frames:[{hold, grid20x20}]}）
import vm from 'node:vm';
import fs from 'node:fs';
const [, , htmlPath, enginePath] = process.argv;
const engineSrc = fs.readFileSync(enginePath, 'utf8');
const html = fs.readFileSync(htmlPath, 'utf8');
const m = html.match(/<script>\s*([\s\S]*?)<\/script>/);
if (!m) { console.error('no inline script'); process.exit(1); }
const fakeEl = () => ({ style: {}, innerHTML: '', appendChild() {} });
const sandbox = {
  console, performance: { now: () => 0 },
  requestAnimationFrame: () => 0, cancelAnimationFrame: () => {},
  window: { addEventListener() {} },
  document: { getElementById: () => fakeEl(), createElement: () => fakeEl() },
};
sandbox.globalThis = sandbox;
vm.createContext(sandbox);
vm.runInContext(engineSrc, sandbox);
try { vm.runInContext(m[1], sandbox); } catch (e) {}
const PE = sandbox.window.PixelEngine, P = sandbox.window.PRESET;
if (!P) { console.error('no PRESET'); process.exit(1); }
const frames = P.frames.map(f => ({ hold: f.hold, grid: f.frame || PE.CREATURE }));
process.stdout.write(JSON.stringify({ name: P.name, frames }));
