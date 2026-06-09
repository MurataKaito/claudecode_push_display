// ClaudePixのアニメHTML → {frames:[{hold,grid20x20(palette idx)}], pal:[css color]}
// 2形式対応: (A) creature-engine の window.PRESET / (B) 自己完結 window.FRAMES+PAL
import vm from 'node:vm';
import fs from 'node:fs';
const [, , htmlPath, enginePath] = process.argv;
const engineSrc = enginePath && fs.existsSync(enginePath) ? fs.readFileSync(enginePath, 'utf8') : '';
const html = fs.readFileSync(htmlPath, 'utf8');
const m = html.match(/<script>\s*([\s\S]*?)<\/script>/);
if (!m) { console.error('no inline script'); process.exit(1); }
const fakeEl = () => ({ style: {}, innerHTML: '', appendChild() {} });
const noop = () => 0;
const sandbox = {
  console, performance: { now: () => 0 },
  requestAnimationFrame: noop, cancelAnimationFrame: () => {},
  setTimeout: noop, clearTimeout: () => {}, setInterval: noop, clearInterval: () => {},
  window: { addEventListener() {} },
  document: { getElementById: () => fakeEl(), createElement: () => fakeEl() },
};
sandbox.globalThis = sandbox;
vm.createContext(sandbox);
if (engineSrc) { try { vm.runInContext(engineSrc, sandbox); } catch (e) {} }
try { vm.runInContext(m[1], sandbox); } catch (e) {}
const W = sandbox.window;
let frames, pal;
if (W.PRESET) {
  const PE = W.PixelEngine;
  frames = W.PRESET.frames.map(f => ({ hold: f.hold || 120, grid: f.frame || PE.CREATURE }));
  pal = ['transparent', '#CD7F6A', '#0f0f0f'];
} else if (W.FRAMES) {
  frames = W.FRAMES.map(f => ({ hold: f.hold || 120, grid: f.frame }));
  pal = W.PAL || ['transparent', '#CD7F6A', '#111111'];
} else { console.error('no PRESET/FRAMES'); process.exit(1); }
process.stdout.write(JSON.stringify({ frames, pal }));
