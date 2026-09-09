"""Preview exported native DC6 frames with their real offsets and palette.

This is an asset contact sheet, not an in-game screenshot. Supports joined cells
and compares the current effects against an earlier exported sprite bank.
"""
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import argparse, json, re, struct
from PIL import Image, ImageChops, ImageDraw, ImageFont
from native_loot_catalog_data import PROFILES

ROOT = Path(__file__).resolve().parents[1]
PALETTE = [tuple(map(int, m)) for m in re.findall(r'\{(\d+),(\d+),(\d+)\}',
    (ROOT/'src/dx12/native_loot_palette.h').read_text())]
assert len(PALETTE) == 256
WIDTH, HEIGHT, ANCHOR = 384, 640, (192, 560)

def layer(path, frame):
    b = path.read_bytes()
    count = struct.unpack_from('<I', b, 20)[0]
    assert count % 24 == 0
    parts = count//24
    pixels = Image.new('RGB', (WIDTH, HEIGHT))
    pix = pixels.load()
    for part in range(parts):
        start = struct.unpack_from('<I', b, 24+(frame*parts+part)*4)[0]
        _, w, h, ox, oy, _, _, length = struct.unpack_from('<IIIiiIII', b, start)
        x = row = 0
        at = start+32
        while at < start+32+length:
            token = b[at]; at += 1
            if token == 128:
                assert x == w
                x = 0; row += 1
                continue
            n = token & 127
            if not token & 128:
                for i in range(n):
                    px, py = ANCHOR[0]+ox+x+i, ANCHOR[1]+oy-row
                    assert 0 <= px < WIDTH and 0 <= py < HEIGHT, (path, px, py)
                    pix[px, py] = PALETTE[b[at+i]]
                at += n
            x += n
        assert row == h
    return pixels

def effect(bank, name, frame=6):
    image = Image.new('RGB', (WIDTH, HEIGHT), (15, 18, 23))
    for prefix in ('bloom-', ''):
        path = bank/(prefix+name+'.dc6')
        if path.exists():
            image = ImageChops.add(image, layer(path, frame))
    return image

def font(size):
    return ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', size)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('bank', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    selected = [('SacredBase','Sacred base'),('ArcaneShard','Arcane shard'),
        ('GemBlue','Perfect gem'),('Unique','Unique'),('SacredUnique','Sacred unique'),
        ('RuneHigh','High common rune'),('RuneEnchanted','Enchanted rune'),
        ('RuneGreat','Great rune'),('RuneXis','Xis'),('Ultimate','Top reward')]
    cellw = WIDTH*2 if args.baseline else WIDTH
    cellh = HEIGHT+50
    columns = 5
    sheet = Image.new('RGB',(columns*cellw,2*cellh+52),(15,18,23))
    d = ImageDraw.Draw(sheet)
    d.text((16,12),'Native loot artwork | previous / bigger + sparkles' if args.baseline else 'Native loot artwork',font=font(22),fill=(240,227,194))
    for i,(name,label) in enumerate(selected):
        x,y = (i%columns)*cellw,52+(i//columns)*cellh
        if args.baseline:
            sheet.paste(effect(args.baseline,name),(x,y+40))
            sheet.paste(effect(args.bank,name),(x+WIDTH,y+40))
        else:
            sheet.paste(effect(args.bank,name),(x,y+40))
        d.text((x+10,y+8),label,font=font(20),fill=(225,225,225))
    sheet.save(args.output)
    # Size/count inventory includes only unique runtime shapes, matching sharing
    # in NativeLoot::initialize rather than counting duplicate profile aliases.
    seen=set(); size=frames=0
    for name,rank,colour,style in PROFILES[1:]:
        key=rank,colour,style
        if key in seen: continue
        seen.add(key)
        for prefix in ('','bloom-'):
            path=args.bank/(prefix+name+'.dc6')
            if not path.exists(): continue
            size+=path.stat().st_size
            frames+=struct.unpack_from('<I',path.read_bytes(),20)[0]
    for path in args.bank.glob('landing-*.dc6'):
        size+=path.stat().st_size
        frames+=struct.unpack_from('<I',path.read_bytes(),20)[0]
    # Six instants show the landing burst expand and fade, followed by the
    # smaller repeating ground pulse. No live game screenshots are fabricated.
    pulseSheet=Image.new('RGB',(WIDTH*6,HEIGHT+44),(15,18,23))
    pd=ImageDraw.Draw(pulseSheet)
    for i,(age,frame) in enumerate([(0,0),(165,5),(330,10),(495,15),(726,22),(990,None)]):
        picture=effect(args.bank,'SacredUnique',(age//50)%24)
        path=args.bank/'landing-3-1.dc6'
        if frame is not None and path.exists(): picture=ImageChops.add(picture,layer(path,frame))
        pulseSheet.paste(picture,(i*WIDTH,40))
        pd.text((i*WIDTH+12,10),f'{age} ms',font=font(18),fill=(225,225,225))
    pulseSheet.save(args.output.with_stem(args.output.stem+'-pulse'))
    print(json.dumps(dict(preview=str(args.output),resident_distinct_frames=frames,
        resident_encoded_bytes=size,visual_combinations=len(seen))))

if __name__ == '__main__': main()
