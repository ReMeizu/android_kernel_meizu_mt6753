#!/usr/bin/env python3
# Minimal sdat2img / img2sdat for FULL block OTAs (transfer.list v4, new/zero/erase only).
import sys

BLOCK = 4096

def parse_rangeset(text):
    v = [int(x) for x in text.strip().split(',')]
    n = v[0]
    assert len(v) - 1 == n, "bad rangeset %r" % text
    p = v[1:]
    return list(zip(p[0::2], p[1::2]))  # [(start,end), ...]  end-exclusive

def load_tlist(path):
    lines = open(path).read().splitlines()
    version = int(lines[0])
    cmds = lines[4:] if version >= 2 else lines[1:]
    news = []          # ordered list of rangesets for 'new' commands
    maxblk = 0
    for line in cmds:
        if not line.strip():
            continue
        c, _, rest = line.partition(' ')
        if c == 'new':
            rs = parse_rangeset(rest)
            news.append(rs)
            maxblk = max(maxblk, max(e for _, e in rs))
        elif c in ('zero', 'erase'):
            rs = parse_rangeset(rest)
            maxblk = max(maxblk, max(e for _, e in rs))
    return news, maxblk

def sdat2img(tlist, newdat, outimg):
    news, maxblk = load_tlist(tlist)
    with open(newdat, 'rb') as nd, open(outimg, 'wb') as out:
        out.truncate(maxblk * BLOCK)          # sparse backing
        for rs in news:
            for s, e in rs:
                out.seek(s * BLOCK)
                out.write(nd.read((e - s) * BLOCK))
    print("sdat2img: wrote %s (%d blocks span)" % (outimg, maxblk))

def img2sdat(tlist, img, newdat):
    # Re-slice the (edited) image back into .dat using the SAME transfer.list.
    news, maxblk = load_tlist(tlist)
    with open(img, 'rb') as im, open(newdat, 'wb') as nd:
        for rs in news:
            for s, e in rs:
                im.seek(s * BLOCK)
                nd.write(im.read((e - s) * BLOCK))
    print("img2sdat: wrote %s" % newdat)

if __name__ == '__main__':
    mode = sys.argv[1]
    if mode == 'unpack':
        sdat2img(sys.argv[2], sys.argv[3], sys.argv[4])
    elif mode == 'pack':
        img2sdat(sys.argv[2], sys.argv[3], sys.argv[4])
