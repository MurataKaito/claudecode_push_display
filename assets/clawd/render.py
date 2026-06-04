# {frames,pal} -> <prefix>0..N.png (200px, 黒地#0f0f0f, パレット多色対応)
import json, sys, os, re, bisect
from PIL import Image, ImageDraw
BG = (15, 15, 15)
def col(s):
    s = (s or '').strip().lower()
    if s in ('transparent', ''): return None
    if s.startswith('#'):
        h = s[1:]
        if len(h) == 3: h = ''.join(c*2 for c in h)
        return (int(h[0:2],16), int(h[2:4],16), int(h[4:6],16))
    return {'black':(0,0,0),'white':(255,255,255)}.get(s)
inp, outdir, prefix = sys.argv[1], sys.argv[2], sys.argv[3]
dt = int(sys.argv[4]) if len(sys.argv) > 4 else 180
data = json.load(open(inp)); frames = data['frames']
pal = [col(c) for c in data['pal']]
total = sum(f['hold'] for f in frames); cum=[]; t=0
for f in frames: t += f['hold']; cum.append(t)
def grid_at(ms):
    i = bisect.bisect_right(cum, ms % total); return frames[min(i, len(frames)-1)]['grid']
N = max(1, min(16, round(total / dt))); step = total / N
CELL, SIZE = 10, 200
os.makedirs(outdir, exist_ok=True)
for fn in os.listdir(outdir):
    if re.fullmatch(re.escape(prefix)+r'\d+\.png', fn): os.remove(os.path.join(outdir, fn))
for i in range(N):
    g = grid_at(i*step)
    im = Image.new('RGB',(SIZE,SIZE),BG); d=ImageDraw.Draw(im)
    for r in range(20):
        for c in range(20):
            v = g[r][c]
            rgb = pal[v] if 0 <= v < len(pal) else None
            if rgb is None: continue
            x0,y0 = c*CELL, r*CELL
            # 本家の inset影 を再現：セル色を~30%暗くした1pxの内枠（白系は枠なし）
            edge = rgb if max(rgb) > 235 else tuple(int(x*0.7) for x in rgb)
            d.rectangle([x0,y0,x0+CELL-1,y0+CELL-1], fill=rgb, outline=edge)
    im.save(os.path.join(outdir, f'{prefix}{i}.png'), optimize=True)  # RGB（LittleFSは3.5MBで余裕）
print(f'{prefix:10s}: {N} frames (total={total}ms)')
