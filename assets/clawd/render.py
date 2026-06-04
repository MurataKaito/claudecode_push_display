# フレームJSON -> 200x200 PNG連番（黒地, 体=#CD7F6A, 目/空=黒）。holdに応じて固定dtでサンプリング。
import json, sys, os, bisect
from PIL import Image, ImageDraw
inp, outdir = sys.argv[1], sys.argv[2]
dt = int(sys.argv[3]) if len(sys.argv) > 3 else 180
data = json.load(open(inp)); frames = data['frames']
total = sum(f['hold'] for f in frames)
cum, t = [], 0
for f in frames:
    t += f['hold']; cum.append(t)
def grid_at(ms):
    i = bisect.bisect_right(cum, ms % total)
    return frames[min(i, len(frames)-1)]['grid']
N = max(1, min(48, round(total / dt)))
step = total / N
CELL = 10; SIZE = 20*CELL
BODY=(205,127,106); BG=(15,15,15); EDGE=(120,70,58)
os.makedirs(outdir, exist_ok=True)
for fn in os.listdir(outdir):
    if fn.startswith('clawd') and fn.endswith('.png'): os.remove(os.path.join(outdir, fn))
for i in range(N):
    g = grid_at(i*step)
    im = Image.new('RGB', (SIZE, SIZE), BG); d = ImageDraw.Draw(im)
    for r in range(20):
        for c in range(20):
            if g[r][c] == 1:
                x0, y0 = c*CELL, r*CELL
                d.rectangle([x0, y0, x0+CELL-1, y0+CELL-1], fill=BODY, outline=EDGE)
    im.save(os.path.join(outdir, f'clawd{i}.png'))
print(f'wrote {N} frames (total={total}ms, dt~{step:.0f}ms) -> {outdir}')
