#!/usr/bin/env python3
# not upstream (SSV Linux port): repairs the off-hue DXT1 blocks in the dark areas of the cockpit textures (design ssv-texfix)
# usage: linux/ssv_texfix.py [-n]   (-n: report only, write nothing)
import os, struct, sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = [os.path.join(ROOT, 'Textures', 'SSV', 'OV', 'stsvc0%d.dds' % i) for i in (1, 2, 3)]
DARK = 72    # a texel whose brightest channel is below this is dark
LIMIT = 6    # a dark grey texel whose green is further than this below its neighbourhood's (purple, a 565 artifact) is moved to the neighbourhood's colour
GREY = 6     # grey: red and blue at most this far apart (a red or blue detail is never a speck)
CELL = 8     # median cells; the neighbourhood is the 3 x 3 cells around a texel's cell
MINN = 32    # dark texels a neighbourhood needs for its median to count
PASSES = 8   # a level is repaired again until a pass changes no block (the medians move as specks go)


def unpack565(c):
    c = c.astype(np.uint32)
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.float64)


def pack565(rgb):
    r = np.clip(np.rint(rgb[..., 0] * 31 / 255), 0, 31).astype(np.uint32)
    g = np.clip(np.rint(rgb[..., 1] * 63 / 255), 0, 63).astype(np.uint32)
    b = np.clip(np.rint(rgb[..., 2] * 31 / 255), 0, 31).astype(np.uint32)
    return (r << 11) | (g << 5) | b


def palette(c0, c1):  # (..., 4, 3); four-colour mode when c0 > c1, else three colours and black (index 3, transparent)
    p0, p1 = unpack565(c0), unpack565(c1)
    four = (c0 > c1)[..., None]
    p2 = np.where(four, (2 * p0 + p1) / 3, (p0 + p1) / 2)
    p3 = np.where(four, (p0 + 2 * p1) / 3, 0.0)
    return np.stack([p0, p1, p2, p3], -2)


def decode(raw, bw, bh):  # -> texels (bh*4, bw*4, 3), selectors (bh, bw, 16), c0, c1
    b = np.frombuffer(raw, np.uint8).reshape(bh, bw, 8)
    c0 = b[..., 0].astype(np.uint32) | (b[..., 1].astype(np.uint32) << 8)
    c1 = b[..., 2].astype(np.uint32) | (b[..., 3].astype(np.uint32) << 8)
    idx = b[..., 4].astype(np.uint32) | (b[..., 5].astype(np.uint32) << 8) | (b[..., 6].astype(np.uint32) << 16) | (b[..., 7].astype(np.uint32) << 24)
    sel = (idx[..., None] >> (2 * np.arange(16, dtype=np.uint32))) & 3
    pal = palette(c0, c1)
    tex = np.take_along_axis(pal, np.repeat(sel[..., None], 3, -1).astype(np.int64), 2)  # (bh, bw, 16, 3)
    img = tex.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 3)
    return img, sel, c0, c1


def chroma(img):
    return (img[..., 0] + img[..., 2]) / 2 - img[..., 1], img[..., 0] - img[..., 2]


def targets(img):  # the purple specks moved to their neighbourhood's chroma, luminance kept; which moved; the rule as a count for a block's new colours
    H, W = img.shape[:2]
    a, b = chroma(img)
    dark = img.max(-1) < DARK
    ch, cw = H // CELL, W // CELL
    amed = np.full((ch, cw), np.nan); bmed = np.full((ch, cw), np.nan)
    has = dark.reshape(ch, CELL, cw, CELL).any(axis=(1, 3))
    for cy, cx in zip(*np.nonzero(has)):
        y0, y1, x0, x1 = max(0, cy - 1) * CELL, min(ch, cy + 2) * CELL, max(0, cx - 1) * CELL, min(cw, cx + 2) * CELL
        m = dark[y0:y1, x0:x1]
        if m.sum() < MINN: continue
        amed[cy, cx] = np.median(a[y0:y1, x0:x1][m]); bmed[cy, cx] = np.median(b[y0:y1, x0:x1][m])
    am = np.repeat(np.repeat(amed, CELL, 0), CELL, 1); bm = np.repeat(np.repeat(bmed, CELL, 0), CELL, 1)
    with np.errstate(invalid='ignore'):
        move = dark & ~np.isnan(am) & (a - am > LIMIT) & (np.abs(b) <= GREY)
    t = img.copy()
    x = np.where(move, (a - am) / 1.5, 0.0)  # green up, red and blue down by half: a to the median, R + G + B kept
    y = np.where(move, b - bm, 0.0)          # red down, blue up by half: b to the median
    t[..., 0] -= x / 2 + y / 2
    t[..., 1] += x
    t[..., 2] -= x / 2 - y / 2

    def specks(y, x, cols):  # the block at texel (y, x) in the colours cols (16, 3): its texels the rule would move
        c = cols.reshape(4, 4, 3)
        ca, cb = chroma(c)
        s = np.s_[y:y + 4, x:x + 4]
        with np.errstate(invalid='ignore'):
            return int(((c.max(-1) < DARK) & ~np.isnan(am[s]) & (ca - am[s] > LIMIT) & (np.abs(cb) <= GREY)).sum())
    return np.clip(t, 0, 255), move, specks


def indices(P, X):  # nearest palette entry per texel, its total squared error
    d = ((X[:, None, :] - P[None, :, :]) ** 2).sum(-1)
    s = d.argmin(1)
    return s, d[np.arange(16), s].sum()


def encode(X):  # 16 target colours -> (c0, c1, selectors), four-colour mode, or one colour
    m = X.mean(0)
    best = None
    cands = []
    C = np.cov((X - m).T)
    if np.allclose(C, 0):
        cands.append((m, m))
    else:
        v = np.linalg.eigh(C)[1][:, -1]
        t = (X - m) @ v
        cands.append((m + v * t.max(), m + v * t.min()))
    for k in range(2):
        e0, e1 = cands[-1]
        c0, c1 = int(pack565(e0)), int(pack565(e1))
        if c0 < c1: c0, c1 = c1, c0
        P = palette(np.array(c0), np.array(c1))
        if c0 == c1:
            s, err = np.zeros(16, int), ((X - P[0]) ** 2).sum()
        else:
            s, err = indices(P, X)
        if best is None or err < best[3]: best = (c0, c1, s, err)
        if c0 == c1: break
        w = np.array([1.0, 0.0, 2 / 3, 1 / 3])[s]  # least squares on the endpoints for these selectors
        A = np.stack([w, 1 - w], 1)
        sol = np.linalg.lstsq(A, X, rcond=None)[0]
        cands.append((sol[0], sol[1]))
    return best


def block_bytes(c0, c1, s):
    idx = 0
    for i in range(16): idx |= int(s[i]) << (2 * i)
    return struct.pack('<HHI', c0, c1, idx)


def fix_level(raw, w, h, report):
    if w < 4 * CELL or h < 4 * CELL or w % CELL or h % CELL: return raw
    first = raw
    for k in range(PASSES):
        part = []
        new = fix_pass(raw, w, h, part)
        if new == raw: break
        raw = new
        report.append('pass %d: %s' % (k + 1, part[0]))
    else:
        report.append('not settled after %d passes' % PASSES)
    if raw == first: report.append('%dx%d: nothing to repair' % (w, h))
    return raw


def fix_pass(raw, w, h, report):
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
    img, sel, c0, c1 = decode(raw, bw, bh)
    tgt, move, specks = targets(img)
    out = bytearray(raw)
    mb = move.reshape(bh, 4, bw, 4).any(axis=(1, 3))
    punch = (c0 <= c1) & (sel == 3).any(-1)
    nblk = ntex = kept = 0
    maxd = np.zeros(3)
    for by, bx in zip(*np.nonzero(mb)):
        if punch[by, bx]: kept += 1; continue
        X = tgt[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4].reshape(16, 3)
        old = img[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4].reshape(16, 3)
        e0, e1, s, err = encode(X)
        new = palette(np.array(e0), np.array(e1))[s]
        if err >= ((X - old) ** 2).sum() or specks (by * 4, bx * 4, new) >= specks (by * 4, bx * 4, old): kept += 1; continue  # the old bytes are as close, or the new ones keep as many specks
        o = (by * bw + bx) * 8
        out[o:o + 8] = block_bytes(e0, e1, s)
        nblk += 1; ntex += int(move[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4].sum())
        maxd = np.maximum(maxd, np.abs(X - old).max(0))
    report.append('%dx%d: %d blocks encoded again (%d texels moved), %d kept, largest change R %.0f G %.0f B %.0f' % (w, h, nblk, ntex, kept, *maxd))
    return bytes(out)


def offhue(raw, w, h):  # dark texels the rule would move (purple specks), as a share of dark texels
    img = decode(raw, max(1, (w + 3) // 4), max(1, (h + 3) // 4))[0]
    t, move, _ = targets(img)
    dark = img.max(-1) < DARK
    return move.sum() / max(1, dark.sum())


def main():
    dry = '-n' in sys.argv[1:]
    for path in FILES:
        data = open(path, 'rb').read()
        if data[:4] != b'DDS ' or data[84:88] != b'DXT1': sys.exit('%s: not a DXT1 DDS' % path)
        flags, h, w = struct.unpack('<3I', data[8:20])
        mips = struct.unpack('<I', data[28:32])[0]
        caps2 = struct.unpack('<I', data[112:116])[0]
        if caps2: sys.exit('%s: cube or volume texture' % path)
        n = mips if (flags & 0x20000) and mips > 0 else 1
        out, o, report, before, after = bytearray(data[:128]), 128, [], [], []
        lw, lh = w, h
        for _ in range(n):
            size = max(1, (lw + 3) // 4) * max(1, (lh + 3) // 4) * 8
            raw = data[o:o + size]
            if len(raw) < size: sys.exit('%s: short file' % path)
            fixed = fix_level(raw, lw, lh, report)
            if lw >= 4 * CELL and lh >= 4 * CELL and not lw % CELL and not lh % CELL:
                before.append(offhue(raw, lw, lh)); after.append(offhue(fixed, lw, lh))
            out += fixed
            o += size
            lw, lh = max(1, lw // 2), max(1, lh // 2)
        out += data[o:]
        print(os.path.relpath(path, ROOT) + ':', '; '.join(report), '| off-hue dark texels %s -> %s' % (
            ', '.join('%.2f %%' % (100 * v) for v in before), ', '.join('%.2f %%' % (100 * v) for v in after)))
        if not dry and bytes(out) != data:
            with open(path, 'wb') as f: f.write(out)
    print('report only, nothing written' if dry else 'ok')


if __name__ == '__main__':
    main()
