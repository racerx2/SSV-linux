#!/usr/bin/env python3
# not upstream (SSV Linux port): repairs the off-hue DXT1 blocks in the dark areas of the cockpit textures (design ssv-texfix, Rev 4)
# usage: linux/ssv_texfix.py [-n] [-j N] [texture ...]   (-n: report only; -j: files in parallel; textures: file names to limit the run to)
import glob, os, struct, sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MESHES = ['VC.msh', 'MidDeck.msh', 'panel*.msh']  # in Meshes/SSV/OV
DARK = 72    # a texel whose brightest channel is below this is dark
LIMIT = 6    # a speck: dark, green further than this below its neighbourhood's (purple, a 565 artifact)
GREY = 6     # and red-blue balance within this of its neighbourhood's (red or blue detail is never a speck)
CELL = 8     # median cells; the neighbourhood is the 3 x 3 cells around a texel's cell
MINN = 32    # dark texels a neighbourhood needs for its median to count
PASSES = 32  # a level is repaired again until a pass changes nothing or repeats an earlier state
REFINE = 4   # least-squares refinements of the endpoints
BDARK = 12   # block rule (Rev 4), a tinted block: at least this many of its 16 texels dark,
BFLAT = 4    # its texels' a and b each within this of each other (no colour detail inside),
BDEV = 5     # its mean a or b further than this from the median of the dark texels of its 8 neighbour blocks,
BMAX = 24    # neither further than this (strong colour is real),
BNEAR = 6    # the neighbours flat: their a and b each within this (10th to 90th percentile),
BMINN = 64   # and at least this many dark texels in the neighbours
MAX565 = np.array([31, 63, 31, 31, 63, 31])


def textures():  # the DXT1 textures of the cockpit meshes (hull textures OV-*.dds excluded), the skipped ones
    tdir = os.path.join(ROOT, 'Textures')
    index = {}
    for d, _, fs in os.walk(tdir):
        for f in fs: index.setdefault(os.path.relpath(os.path.join(d, f), tdir).replace(os.sep, '/').lower(), os.path.join(d, f))
    names = []
    for pat in MESHES:
        for m in sorted(glob.glob(os.path.join(ROOT, 'Meshes', 'SSV', 'OV', pat))):
            lines = open(m, 'rb').read().decode('latin-1').splitlines()
            for i, l in enumerate(lines):
                t = l.split()
                if len(t) == 2 and t[0] == 'TEXTURES':
                    names += [x.split()[0] for x in lines[i + 1:i + 1 + int(t[1])] if x.split()]
    files, skipped = [], []
    for n in dict.fromkeys(names):
        p = index.get(n.replace('\\', '/').lower())
        if p is None: skipped.append(n + ' (missing)'); continue
        if os.path.basename(p).lower().startswith('ov-'): skipped.append(n + ' (hull)'); continue
        fc = open(p, 'rb').read(128)[84:88]
        if fc != b'DXT1': skipped.append(n + ' (%s)' % fc.decode('latin-1')); continue
        if p not in files: files.append(p)
    return sorted(files, key=str.lower), skipped


def unpack565(c):
    c = np.asarray(c).astype(np.uint32)
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.float64)


def pack565(rgb):
    r = np.clip(np.rint(rgb[..., 0] * 31 / 255), 0, 31).astype(np.int64)
    g = np.clip(np.rint(rgb[..., 1] * 63 / 255), 0, 63).astype(np.int64)
    b = np.clip(np.rint(rgb[..., 2] * 31 / 255), 0, 31).astype(np.int64)
    return (r << 11) | (g << 5) | b


def split565(c):  # 565 values (..., 2) -> channels (..., 6)
    c = np.asarray(c, np.int64)
    return np.stack([c[..., 0] >> 11, (c[..., 0] >> 5) & 63, c[..., 0] & 31, c[..., 1] >> 11, (c[..., 1] >> 5) & 63, c[..., 1] & 31], -1)


def join565(k):  # channels (..., 6) -> 565 values (..., 2)
    return np.stack([(k[..., 0] << 11) | (k[..., 1] << 5) | k[..., 2], (k[..., 3] << 11) | (k[..., 4] << 5) | k[..., 5]], -1)


def palette(c0, c1):  # (..., 4, 3); four-colour mode when c0 > c1, else three colours and black (index 3, transparent)
    c0, c1 = np.asarray(c0), np.asarray(c1)
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


def medians(a, b, dark):  # per cell: medians of a and b over the dark texels of its 3 x 3 cells (nan: none in the cell or fewer than MINN)
    ch, cw = dark.shape[0] // CELL, dark.shape[1] // CELL
    has = dark.reshape(ch, CELL, cw, CELL).any(axis=(1, 3))
    out = []
    for v in (a, b):
        p = np.full((ch + 2, cw + 2, CELL * CELL), np.nan)
        p[1:-1, 1:-1] = np.where(dark, v, np.nan).reshape(ch, CELL, cw, CELL).transpose(0, 2, 1, 3).reshape(ch, cw, CELL * CELL)
        m = np.full((ch, cw), np.nan)
        step = max(1, 4096 // cw)
        for r0 in range(0, ch, step):
            r1 = min(ch, r0 + step)
            win = np.sort(np.concatenate([p[r0 + dy:r1 + dy, dx:dx + cw] for dy in range(3) for dx in range(3)], -1), -1)
            n = (~np.isnan(win)).sum(-1)
            lo = np.take_along_axis(win, (np.maximum(n - 1, 0) // 2)[..., None], -1)[..., 0]
            hi = np.take_along_axis(win, (n // 2)[..., None], -1)[..., 0]
            m[r0:r1] = np.where(has[r0:r1] & (n >= MINN), (lo + hi) / 2, np.nan)
        out.append(m)
    return out


def speck(img, am, bm):  # the rule: dark, green short against the neighbourhood, red-blue balance like it
    a, b = chroma(img)
    with np.errstate(invalid='ignore'):
        return (img.max(-1) < DARK) & (a - am > LIMIT) & (np.abs(b - bm) <= GREY)


def blockview(v):  # (bh*4, bw*4, ...) -> (bh, bw, 16, ...)
    bh, bw = v.shape[0] // 4, v.shape[1] // 4
    return v.reshape(bh, 4, bw, 4, *v.shape[2:]).swapaxes(1, 2).reshape(bh, bw, 16, *v.shape[2:])


def tinted(X, amb, bmb):  # blocks (..., 16, 3) against their neighbours' medians: the block rule
    a, b = chroma(X)
    dev = np.maximum(np.abs(a.mean(-1) - amb), np.abs(b.mean(-1) - bmb))
    with np.errstate(invalid='ignore'):
        return ((X.max(-1) < DARK).sum(-1) >= BDARK) & (np.ptp(a, -1) <= BFLAT) & (np.ptp(b, -1) <= BFLAT) & (dev > BDEV) & (dev <= BMAX)


def tints(img):  # per block: tinted, the medians of a and b of its neighbours' dark texels (nan: neighbours not flat)
    X = blockview(img)
    a, b = chroma(X)
    dk = X.max(-1) < DARK
    amb = np.full(dk.shape[:2], np.nan); bmb = amb.copy()
    ys, xs = np.nonzero((dk.sum(-1) >= BDARK) & (np.ptp(a, -1) <= BFLAT) & (np.ptp(b, -1) <= BFLAT))
    pad = [np.pad(np.where(dk, v, np.nan), ((1, 1), (1, 1), (0, 0)), constant_values=np.nan) for v in (a, b)]
    nb = [(dy, dx) for dy in range(3) for dx in range(3) if (dy, dx) != (1, 1)]
    for i in range(0, len(ys), 65536):
        y, x = ys[i:i + 65536], xs[i:i + 65536]
        ok, med = True, []
        for p in pad:
            win = np.sort(np.concatenate([p[y + dy, x + dx] for dy, dx in nb], -1), -1)
            n = (~np.isnan(win)).sum(-1)
            at = lambda k: np.take_along_axis(win, np.maximum(k, 0)[:, None], -1)[:, 0]
            med.append((at((n - 1) // 2) + at(n // 2)) / 2)
            ok = ok & (n >= BMINN) & (at(np.rint(0.9 * (n - 1)).astype(np.int64)) - at(np.rint(0.1 * (n - 1)).astype(np.int64)) <= BNEAR)
        amb[y, x] = np.where(ok, med[0], np.nan); bmb[y, x] = np.where(ok, med[1], np.nan)
    return tinted(X, amb, bmb), amb, bmb


def targets(img):  # the specks and the tinted blocks moved to their neighbourhood's chroma, luminance kept; the specks; the medians per texel; tinted blocks; their medians
    a, b = chroma(img)
    amed, bmed = medians(a, b, img.max(-1) < DARK)
    am = np.repeat(np.repeat(amed, CELL, 0), CELL, 1); bm = np.repeat(np.repeat(bmed, CELL, 0), CELL, 1)
    move = speck(img, am, bm)
    tb, amb, bmb = tints(img)
    tt = np.repeat(np.repeat(tb, 4, 0), 4, 1)
    at = np.where(tt, np.repeat(np.repeat(amb, 4, 0), 4, 1), am); bt = np.where(tt, np.repeat(np.repeat(bmb, 4, 0), 4, 1), bm)
    mv = move | tt
    t = img.copy()
    x = np.where(mv, (a - at) / 1.5, 0.0)  # green up, red and blue down by half: a to the median, R + G + B kept
    y = np.where(mv, b - bt, 0.0)          # red down, blue up by half: b to the median
    t[..., 0] -= x / 2 + y / 2
    t[..., 1] += x
    t[..., 2] -= x / 2 - y / 2
    return np.clip(t, 0, 255), move, am, bm, tb, amb, bmb


def blocksum(v, bw, bh):
    return v.reshape(bh, 4, bw, 4).sum(axis=(1, 3))


def evaluate(raw, w, h):  # per block: specks, squared error to its target, tinted
    bw, bh = (w + 3) // 4, (h + 3) // 4
    img = decode(raw, bw, bh)[0]
    tgt, move, _, _, tb, _, _ = targets(img)
    return blocksum(move.astype(np.int64), bw, bh), blocksum(((img - tgt) ** 2).sum(-1), bw, bh), tb


def fit(X, c):  # endpoint pairs (n, 2) for colours X (16, 3) -> pairs (c0 > c1, or one colour), selectors, squared errors
    c = -np.sort(-np.asarray(c, np.int64), 1)
    P = palette(c[:, 0], c[:, 1])
    one = c[:, 0] == c[:, 1]
    P[one, 3] = P[one, 0]
    d = ((X[None, :, None, :] - P[:, None, :, :]) ** 2).sum(-1)
    s = d.argmin(-1)
    err = np.take_along_axis(d, s[..., None], -1)[..., 0].sum(-1)
    s[one] = 0
    return c, s, err


W4 = np.array([1.0, 0.0, 2 / 3, 1 / 3])


def encode(X, old):  # 16 target colours, the old endpoints -> c0, c1, selectors, squared error (four colours, or one)
    m = X.mean(0)
    starts = [(pack565(m), pack565(m)), old, (pack565(X.max(0)), pack565(X.min(0)))]
    C = np.cov((X - m).T)
    if not np.allclose(C, 0):
        v = np.linalg.eigh(C)[1][:, -1]
        t = (X - m) @ v
        starts.append((pack565(m + v * t.max()), pack565(m + v * t.min())))
    c, s, err = fit(X, starts)
    cands = [(err[i], tuple(c[i]), s[i]) for i in range(len(c))]
    for i in range(len(c)):  # least squares on the endpoints for the selectors, again
        ci, si = c[i], s[i]
        for _ in range(REFINE):
            if ci[0] == ci[1] or np.unique(si).size < 2: break
            w = W4[si]
            e = np.linalg.lstsq(np.stack([w, 1 - w], 1), X, rcond=None)[0]
            cn, sn, en = fit(X, [(pack565(e[0]), pack565(e[1]))])
            cands.append((en[0], tuple(cn[0]), sn[0]))
            if np.array_equal(sn[0], si): break
            ci, si = cn[0], sn[0]
    best = min(cands, key=lambda k: (k[0], k[1]))
    err, cur, s = best[0], np.array(best[1]), best[2]
    step = np.vstack([np.eye(6, dtype=np.int64), -np.eye(6, dtype=np.int64)])
    for _ in range(256):  # greedy: one 565 step on one endpoint channel while the error falls
        k = np.clip(split565(cur)[None, :] + step, 0, MAX565)
        cn, sn, en = fit(X, join565(k))
        i = int(en.argmin())
        if en[i] >= err: break
        err, cur, s = en[i], cn[i], sn[i]
    return int(cur[0]), int(cur[1]), s, err


def block_bytes(c0, c1, s):
    idx = 0
    for i in range(16): idx |= int(s[i]) << (2 * i)
    return struct.pack('<HHI', c0, c1, idx)


def punched(raw, w, h):  # per block: three colours and transparent texels
    _, sel, c0, c1 = decode(raw, (w + 3) // 4, (h + 3) // 4)
    return (c0 <= c1) & (sel == 3).any(-1)


def fix_pass(raw, w, h, cache):  # speck and tinted blocks encoded again for their targets where that is closer and leaves fewer specks, or not tinted
    bw, bh = (w + 3) // 4, (h + 3) // 4
    img, sel, c0, c1 = decode(raw, bw, bh)
    tgt, move, am, bm, tb, amb, bmb = targets(img)
    out = bytearray(raw)
    punch = (c0 <= c1) & (sel == 3).any(-1)  # three colours and transparent texels: kept
    for by, bx in zip(*np.nonzero((blocksum(move.astype(np.int64), bw, bh) > 0) | tb)):
        if punch[by, bx]: continue
        q = np.s_[by * 4:by * 4 + 4, bx * 4:bx * 4 + 4]
        o = (by * bw + bx) * 8
        X = tgt[q].reshape(16, 3)
        key = (bytes(raw[o:o + 8]), X.tobytes(), am[q].tobytes(), bm[q].tobytes(), np.array([amb[by, bx], bmb[by, bx]]).tobytes())
        if key not in cache:
            old = img[q].reshape(16, 3)
            e0, e1, s, err = encode(X, (int(c0[by, bx]), int(c1[by, bx])))
            new = palette(e0, e1)[s]
            sn, so = int(speck(new.reshape(4, 4, 3), am[q], bm[q]).sum()), int(move[q].sum())
            tn, to = bool(tinted(new[None], amb[by, bx], bmb[by, bx])[0]), bool(tb[by, bx])
            better = err < ((X - old) ** 2).sum() and sn <= so and (tn, sn) < (to, so)
            cache[key] = block_bytes(e0, e1, s) if better else None
        if cache[key] is not None: out[o:o + 8] = cache[key]
    return bytes(out)


def blocks(raw, w, h):
    return np.frombuffer(raw, np.uint8).reshape((h + 3) // 4, (w + 3) // 4, 8)


def mixture(cyc, w, h):  # per block the state of the cycle not tinted, with the fewest specks, then the lowest error
    ev = [evaluate(s, w, h) for s in cyc]
    bl = [blocks(s, w, h) for s in cyc]
    out = bl[0].copy()
    for by, bx in zip(*np.nonzero((np.stack(bl) != bl[0][None]).any(-1).any(0))):
        i = min(range(len(cyc)), key=lambda i: (ev[i][2][by, bx], ev[i][0][by, bx], ev[i][1][by, bx], bl[i][by, bx].tobytes()))
        out[by, bx] = bl[i][by, bx]
    tot = [(e[2].sum(), e[0].sum(), e[1].sum(), s) for e, s in zip(ev, cyc)]
    return out.tobytes(), min(tot)[3]


def fix_level(raw, w, h, cache):  # -> the repaired level, passes, note
    states, seen, note = [raw], {raw: 0}, ''
    for k in range(1, PASSES + 1):
        new = fix_pass(raw, w, h, cache)
        if new == raw: return raw, k, note
        if new in seen:  # a cycle: the per-block best if it is one of the states or stays, else the best state
            cyc = states[seen[new]:]
            mix, whole = mixture(cyc, w, h)
            stays = mix in seen or fix_pass(mix, w, h, cache) == mix
            return (mix if stays else whole), k, 'cycle of %d states, %s' % (len(cyc), 'best per block' if stays else 'best state')
        seen[new] = len(states)
        states.append(new)
        raw = new
    return raw, PASSES, 'not settled after %d passes' % PASSES


def fix_file(path):  # -> report line, the repaired file (None: unchanged)
    data = open(path, 'rb').read()
    name = os.path.relpath(path, os.path.join(ROOT, 'Textures'))
    if len(data) < 128 or data[:4] != b'DDS ' or data[84:88] != b'DXT1': return '%s: not a DXT1 DDS, skipped' % name, None
    flags, h, w = struct.unpack('<3I', data[8:20])
    mips = struct.unpack('<I', data[28:32])[0]
    if struct.unpack('<I', data[112:116])[0]: return '%s: cube or volume texture, skipped' % name, None
    n = mips if (flags & 0x20000) and mips > 0 else 1
    out, o, cache = bytearray(data[:128]), 128, {}
    before = after = nblk = passes = tbef = taft = nbr = 0
    left = np.zeros(4, np.int64)  # speck blocks left (punch-through, other), tinted blocks left (punch-through, other)
    maxd, maxb, notes, lw, lh, off = np.zeros(3), np.zeros(3), [], w, h, None
    for _ in range(n):
        bw, bh = (lw + 3) // 4, (lh + 3) // 4
        raw = data[o:o + bw * bh * 8]
        if len(raw) < bw * bh * 8: return '%s: short file, skipped' % name, None
        fixed = raw
        if lw >= 4 * CELL and lh >= 4 * CELL and not lw % CELL and not lh % CELL:
            sb, _, tbb = evaluate(raw, lw, lh)
            fixed, k, note = fix_level(raw, lw, lh, cache)
            sa, _, tba = evaluate(fixed, lw, lh) if fixed != raw else (sb, None, tbb)
            before += int(sb.sum()); after += int(sa.sum()); passes = max(passes, k)
            tbef += int(tbb.sum()); taft += int(tba.sum())
            pu = punched(fixed, lw, lh)
            left += [int((pu & (sa > 0)).sum()), int((~pu & (sa > 0)).sum()), int((pu & tba).sum()), int((~pu & tba).sum())]
            if note: notes.append('%dx%d: %s' % (lw, lh, note))
            if fixed != raw:
                d = (blocks(fixed, lw, lh) != blocks(raw, lw, lh)).any(-1)
                nblk += int(d.sum()); nbr += int((d & tbb).sum())
                dd = np.abs(blockview(decode(fixed, bw, bh)[0]) - blockview(decode(raw, bw, bh)[0]))
                maxd = np.maximum(maxd, dd.reshape(-1, 3).max(0))
                if (d & tbb).any(): maxb = np.maximum(maxb, dd[d & tbb].reshape(-1, 3).max(0))
            if off is None:
                dk = int((decode(raw, bw, bh)[0].max(-1) < DARK).sum())
                off = (100 * sb.sum() / max(1, dk), 100 * sa.sum() / max(1, dk), int((sa > 0).sum()))
        out += fixed
        o += bw * bh * 8
        lw, lh = max(1, lw // 2), max(1, lh // 2)
    out += data[o:]
    kept = 'kept: speck blocks %d punch-through %d not closer, tinted blocks %d punch-through %d not closer' % tuple(left)
    if bytes(out) == data:
        return '%s: unchanged, %d specks, %d tinted blocks; %s' % (name, before, tbef, kept) + ('; ' + '; '.join(notes) if notes else ''), None
    line = '%s: specks %d -> %d (top level %.2f %% -> %.2f %% of dark texels, %d speck blocks left), tinted blocks %d -> %d, %d blocks changed (%d by the block rule), %d passes, largest change R %.0f G %.0f B %.0f (block rule R %.0f G %.0f B %.0f); %s' % (
        name, before, after, off[0], off[1], off[2], tbef, taft, nblk, nbr, passes, *maxd, *maxb, kept)
    return line + ('; ' + '; '.join(notes) if notes else ''), bytes(out)


def main():
    args = sys.argv[1:]
    dry = '-n' in args
    jobs = int(args[args.index('-j') + 1]) if '-j' in args else 1
    only = {a.lower() for a in args if a.lower().endswith('.dds')}
    files, skipped = textures()
    if only: files = [f for f in files if os.path.basename(f).lower() in only]
    print('%d textures: %s' % (len(files), ' '.join(os.path.basename(f) for f in files)))
    if skipped: print('skipped: ' + ', '.join(skipped))
    if jobs > 1:
        from multiprocessing import Pool
        pool = Pool(jobs)
        res = pool.imap(fix_file, files)
    else:
        res = map(fix_file, files)
    written = 0
    for path, (line, out) in zip(files, res):
        print(line, flush=True)
        if out is not None and not dry:
            with open(path, 'wb') as f: f.write(out)
            written += 1
    print('report only, nothing written' if dry else 'ok, %d files written' % written)


if __name__ == '__main__':
    main()
