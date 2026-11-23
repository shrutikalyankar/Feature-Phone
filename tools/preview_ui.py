#!/usr/bin/env python3
"""Approximate PC preview of the BT815 coprocessor streams written by tests/ui_preview.c.
Fonts are only similar to the BT815 ROM fonts; line/corner geometry follows my reading of the
BT81x docs. Use it to check layout, not pixel accuracy."""
import struct, sys, math, pathlib
from PIL import Image, ImageDraw, ImageFont

W, H, S = 800, 480, 2   # supersample 2x
FONT_PX = {26: 20, 27: 25, 28: 29, 29: 38, 30: 49, 31: 63}   # approximate ROM font heights
def font(n):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",):
        try: return ImageFont.truetype(p, int(FONT_PX.get(n, 25) * 0.8 * S))
        except OSError: pass
    return ImageFont.load_default()

def render(path, out):
    data = pathlib.Path(path).read_bytes()
    words = struct.unpack("<%dI" % (len(data) // 4), data)
    img = Image.new("RGB", (W * S, H * S), (0, 0, 0)); d = ImageDraw.Draw(img)
    color = (255, 255, 255); lw = 16; ps = 16; prim = 0; strip = []; lines = []
    i = 0
    def v(x, y): return (x * S, y * S)
    def vert(w):
        x = (w >> 15) & 0x7FFF; y = w & 0x7FFF
        if x & 0x4000: x -= 0x8000
        if y & 0x4000: y -= 0x8000
        return x, y
    def thick(a, b, r):
        d.line([v(*a), v(*b)], fill=color, width=max(1, int(2 * r * S)))
        for p in (a, b): d.ellipse([v(p[0] - r, p[1] - r), v(p[0] + r, p[1] + r)], fill=color)
    pend = None
    while i < len(words):
        w = words[i]; i += 1
        if w == 0xFFFFFF0C:                          # CMD_TEXT x,y,font,options,string
            xy, fo = words[i], words[i + 1]; i += 2
            x = struct.unpack("<h", struct.pack("<H", xy & 0xFFFF))[0]; y = struct.unpack("<h", struct.pack("<H", xy >> 16))[0]
            fnt, opt = fo & 0xFFFF, fo >> 16
            raw = b""; 
            while True:
                chunk = struct.pack("<I", words[i]); i += 1; raw += chunk
                if b"\0" in chunk: break
            s = raw.split(b"\0")[0].decode()
            f = font(fnt); anchor = ("m" if opt & 0x200 else "l") + ("m" if opt & 0x400 else "a")
            d.text(v(x, y), s, font=f, fill=color, anchor=anchor)
        elif w >> 24 == 0xFF: continue                 # DLSTART/SWAP etc.
        elif w >> 30 == 1:                             # VERTEX2F
            x, y = vert(w)
            if prim == 9:
                if pend is None: pend = (x, y)
                else:
                    r = lw / 16
                    d.rounded_rectangle([v(pend[0] - r, pend[1] - r), v(x + r, y + r)], radius=r * S, fill=color); pend = None
            elif prim == 2: r = ps / 16; d.ellipse([v(x - r, y - r), v(x + r, y + r)], fill=color)
            elif prim == 4: strip.append((x, y)); 
            elif prim == 3:
                if pend is None: pend = (x, y)
                else: thick(pend, (x, y), lw / 16); pend = None
        else:
            op = w >> 24
            if op == 2: img.paste((w >> 16 & 255, w >> 8 & 255, w & 255), [0, 0, W * S, H * S])
            elif op == 4: color = (w >> 16 & 255, w >> 8 & 255, w & 255)
            elif op == 14: lw = w & 0xFFF
            elif op == 13: ps = w & 0x1FFF
            elif op == 31: prim = w & 15; strip = []; pend = None
            elif op == 33:
                if prim == 4:
                    for a, b in zip(strip, strip[1:]): thick(a, b, lw / 16)
                prim = 0; pend = None
    img.resize((W, H), Image.LANCZOS).save(out)

src = pathlib.Path(sys.argv[1]); dst = pathlib.Path(sys.argv[2]); dst.mkdir(exist_ok=True, parents=True)
for f in sorted(src.glob("*.bin")): render(f, dst / (f.stem + ".png"))
