"""Eisen on sarge's body, painted from scratch: a red quilted work vest
over a yellow work shirt and a white undershirt, a brown leather belt,
khaki work trousers with leather chaps over the thighs, grey boots,
yellow work gloves, and ginger hair and a full beard. All of it is our own
opaque paint except the face, which keeps the model's features under a
skin wash; shadows gather where one material meets another, the vest is
quilted in puffed diamonds, the shirt and trousers fold, the chaps and
vest are stitched, and the leather and boots are scuffed. The shoulder pad is hidden. Team versions paint the vest in
the team's colour."""
import math
from skins import Sheet, Look, LabelMap
from tremorart import Fbm, Noise, clamp, smooth, mix

# what each texel of sarge's band.tga is, 128 x 128, run-length coded rows:
# h hair, b beard, f face, v vest, w undershirt, y shirt and sleeves, l gloves,
# s bare skin, g belt, c chaps, k trousers, o boots
MAP = (
    "v13b12g23b3k37c5k11c9k4v11",
    "v19b6g22o5k46o15k2v13",
    "v24g22o6k46o16v14",
    "v25g21o6k47o15v14",
    "v25g21o7k46o15v14",
    "v3y2v21g20o7k47o14v14",
    "w2y3v21g20o7k47o14v14",
    "w2y3v21g20o7k48o13v14",
    "w2y3v21g20o7k48o14v13",
    "w2y3v21g20o7k48o14v13",
    "w2y3v21g20o7k48o14v13",
    "w2y3v21g20o7k48o14v13",
    "w2y3v21g20o7k48o13v14",
    "w2y3v21g19o8k47v28",
    "w2y3v21g18ko8k46v29",
    "w2y3v22g18o8k46v29",
    "w2y3v22g18o8k45v30",
    "w2y3v22g18o8k45v30",
    "w2y3v22g18o8k44v31",
    "w2y3v23g6s11o7k45v31",
    "w2y3v24s17o6k44v32",
    "w2y3v24s17o6k8ok35v32",
    "w2y3v24s17o6k7o2k11ok23v32",
    "w2y3v24s16lo6k6o3k11o2k22v32",
    "w2y3v25s13l3o6k6o8k6o2ko3k17v33",
    "w2y3v25s11l6o5k5o14ko7k16v33",
    "w2y3v25s9l9o3k5o24k15v33",
    "w2y3v25s7l12ok5o26k14v33",
    "w2y3v26s5l13sk4o28k12v34",
    "w2y3v26s2l15s3o32k11v34",
    "w2y3v26sl11g5s11os12y10o8k4v34",
    "w2y3v25g5l3g10s24y11s2o6ks3v33",
    "w2y3v24g19s24y11s5o2s6v32",
    "w2y3v24g18s24y11s14v32",
    "w2y3v23g19s24y11s14v32",
    "w2y3v23g19s24y11s14v32",
    "w2y3v23g19s24y10s14v33",
    "w2y3v23g19s23y11s12v35",
    "w2y3v22g19y2s22y11s12v35",
    "w2y3v22g19y2s22y10s13v35",
    "w2y3v22g19y2s21y11s13v35",
    "w2y3v23g17y3s21y11s13v35",
    "w2y3v41ys22y10s14v35",
    "w2y3v40y2s21y11s14v35",
    "w2y3v40y2sy2s18y11s14v35",
    "w2y3v19sv20y5s18y11s14v35",
    "w2y3v18s3v19y5s18y11s14v35",
    "w2y3v18s4v18y5s19y10s15v34",
    "w2y3v18s6v16y5s19y9s16v34",
    "w2y3v17s8v15y6s18y9s16v34",
    "w2y3v17s10v12y8s17y8s22v29",
    "w2y3v16s13v5s6y8s18y5s25l2v25",
    "w2y3v16s25y4s22y3s26l4v23",
    "w2y3v16s28ys22y3s26l4v23",
    "g9v12s52ys27l5v22",
    "g11v9ks80l5v22",
    "g12v8ks80l7v20",
    "g13v7ks80l7v20",
    "g14v6ks80l7v20",
    "g15v5ks80l7v20",
    "g16v5s55ys3ys20l7v20",
    "g13v9s55y3s21l7v20",
    "vg7v14s56ys22l8v19",
    "v2gv20s78l8v19",
    "v23s78l8v19",
    "v24s77l8v19",
    "v24s78l3s3lv19",
    "v24s78ls5o4v16",
    "v25s82o13v8",
    "ov24s80h2o21",
    "v26s69h12o21",
    "v26s68h13o21",
    "v26s68h13o21",
    "v27s67h13o21",
    "v28s67h12o21",
    "v28s67h12o21",
    "v27s30o2s36h12o21",
    "v27s29o4s35h12o21",
    "v27s29o5s34h13o19v",
    "v27s28o15s25h13o19v",
    "v27s28o17s23h13o8g11v",
    "v27s28o19s21h11g2o2g17v",
    "v27s28o22s17h12g21v",
    "v27s27o31s5g4h7g26v",
    "v27s27o32c5g32k4v",
    "v28s25o32c7g26k9v",
    "v28s25o31c9g2c3gc17k12",
    "kv26s26o31c11g4c17k12",
    "kv26s26o31c8g7c17k12",
    "kv26s26o31c9g6c17k12",
    "k2v25s25o31c11g5c17k12",
    "k2v25s25o31c12g2c19k12",
    "k2v25s25o31c33k12",
    "k3v22h2s25o31c33k12",
    "k3hv17h8s23o31c32k13",
    "k2h10v5h35o31c32k13",
    "k2h62g13o5c33k13",
    "k2h62g14o2c35k13",
    "k2fh61g15c36k13",
    "k2f5h57g16c35k13",
    "k2f24h38g16c35k13",
    "kf27h36g16c35k13",
    "kf27h36g16c35k13",
    "kf27h36g16c35k13",
    "kf27h36g16c35k13",
    "kf27h36g16c35k13",
    "f28h36g17c34k13",
    "f29h34g18c34k13",
    "f30h33g18c34k12f",
    "f29h34g18c34k12f",
    "f29h34g18c34k12f",
    "f29h34k16g2c34k12f",
    "f29h34k18c34k12f",
    "f29h34k18c34k12f",
    "f29h34k18c34k12f",
    "fb31h30k19c34k12f",
    "b32h30k18c34k13f",
    "b32h30k18c34k13b",
    "b32h2b4h2b7h15k18c35k12b",
    "b48h14k17c4k5c27k12b",
    "b50h11k18c3k5c6k3c19k12b",
    "b52h9k26c6k5c17k13",
    "b53h8k25c7k6c16k13",
    "b54h7k25c7k8c14k13",
    "b57h3k26c7k10c12k13",
    "b37g3b18h2k24c9k11c11k13",
    "v2b33g7b16k25c10k11c11k12v",
    "v3b23g21b10k26c10k11c10k9v5",
)
MAP = LabelMap(MAP)

VEST = {"": (0.62, 0.13, 0.1), "_red": (0.66, 0.08, 0.06), "_blue": (0.12, 0.24, 0.66)}
COLOURS = {
    "w": (0.86, 0.84, 0.78), "y": (0.86, 0.66, 0.14), "l": (0.88, 0.7, 0.16),
    "s": (0.8, 0.56, 0.42), "g": (0.3, 0.17, 0.08), "c": (0.46, 0.26, 0.12),
    "k": (0.52, 0.44, 0.22), "o": (0.44, 0.44, 0.42),
}
SKIN = (0.8, 0.56, 0.42)
GINGER = (0.8, 0.38, 0.13)


def quilt(x, y):
    """Light on a puffed diamond of the vest: 1 on its crown, low in the seams."""
    a, b = (x + y) / 26.0, (x - y) / 26.0
    fa, fb = a - math.floor(a), b - math.floor(b)
    seam = min(fa, 1 - fa, fb, 1 - fb)
    return smooth(0.0, 0.22, seam) * (0.85 + 0.3 * (0.5 - fa))   # lit from above


def detail_fn(variant):
    n, fine, strands = Fbm(2311, 4, 6), Noise(2313, 256, 256), Noise(2317, 128, 128)
    folds = Noise(2319, 32, 32)

    def f(u, v):
        x, y = u * 512, v * 512
        k = 0.82 + 0.36 * n.at(u * 6, v * 6)
        c, e = MAP.label(u, v), MAP.edge(u, v)
        if c == "f":
            return list(SKIN) + [0.35]
        k *= MAP.shade(u, v, 0.5, 3.5)                       # shadow where materials meet
        if c in "hb":
            s = strands.at(u * 60, v * 240) * 0.6 + strands.at(u * 140, v * 420 + 5) * 0.4
            hair = mix(tuple(z * 0.5 for z in GINGER), GINGER, smooth(0.3, 0.8, s))
            return [clamp(z * k) for z in hair] + [1]
        if c == "v":
            k *= 0.55 + 0.55 * quilt(x, y)
            if 1.0 < e < 1.8 and (x + 2 * y) % 7 < 3.5:
                return [0.85, 0.75, 0.6, 1]                  # stitched edge
            return [clamp(z * k) for z in VEST[variant]] + [1]
        if c in "ywk":
            fold = folds.at(u * 18, v * 5)                  # long folds down the cloth
            k *= 0.78 + 0.4 * smooth(0.2, 0.8, fold)
            if c == "k":
                k *= 0.92 + 0.16 * fine.at(x * 2, y * 2)     # twill
        elif c == "c":
            k *= (0.85 + 0.25 * fine.at(x, y)) * (0.6 if (x % 40) < 1.5 else 1)
            if 1.2 < e < 2.0 and (x + y) % 8 < 4:
                return [0.8, 0.66, 0.45, 1]                  # stitched hem
            if fine.at(x * 0.5 + 7, y * 0.5) > 0.7:
                k *= 1.25                                    # scuffed leather
        elif c == "o":
            k *= 0.85 + 0.3 * fine.at(x, y)
            if e < 1.5:
                k *= 0.55                                    # the sole and welt
            elif fine.at(x * 0.7, y * 0.7 + 9) > 0.72:
                k *= 1.3                                     # scuffs
        elif c == "g":
            k *= 0.8 + 0.4 * smooth(0.4, 0.6, fine.at(x * 0.5, y * 3))
            if 1.0 < e < 1.8 and x % 6 < 3:
                return [0.7, 0.55, 0.3, 1]                   # stitched belt edge
        elif c == "l":
            k *= 0.8 + 0.35 * folds.at(u * 40, v * 40)       # creased gloves
        return [clamp(z * k) for z in COLOURS[c]] + [1]
    return f


def look():
    return Look("sarge", "eisen", {
        "body": Sheet({"": "band", "_red": "band", "_blue": "band"},
                      detail={v: detail_fn(v) for v in VEST}, size=512),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_rshoulder": "tremor/nodraw"},
        "head": {"h_head": "body", "h_cigar": "tremor/nodraw"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
