"""Slash in her own colours: an olive-gold corset with boning, orange
shorts with stitched hems, olive legs, dark knee pads, bracers and
shoulder pad, tan skate boots, pale skin, dark hair and green glowing
skates; the colours are tinted over the model's own shading, with
shadows in the creases and worn edges. Team versions paint the shorts
in the team's colour."""
from skins import Sheet, Look, LabelMap, region, inside
from tremorart import Fbm, Noise, clamp

# what each texel of slash.tga is, 128 x 128, run-length coded rows:
# c corset, p shorts, l legs, b boots, d pads, s skin, k shadow (left alone)
MAP = (
    "c7k40p18k23b4k36",
    "c7k25p5k6p18k22b10k35",
    "c7k17p36k16b18k9c4k21",
    "c8k15p37k16b18k8c6k20",
    "c8k15p37k11b2k2b20k7c7k19",
    "c8k15p38k9b25k7c8k14c3k",
    "c9k14p38k9b25k7c9k11cs2c3",
    "c9k14p38k9b26c17k3c3s7ck",
    "c9k14p39k8b26c15s15ck",
    "c9k14p39k8b22k3bc13s16ck2",
    "c9k14p39k8b21k4bc12s17ck2",
    "c10k13p39k9b19k5bc8s20ck3",
    "c6s5c7k5p40k8b20k5c7s20ck4",
    "c6s13c3kp40k8b23k2c6s21ck4",
    "c6s17p40k9b23k2c3s23k5",
    "c6s17p40k14b19kc2s23ck5",
    "c6s17p40k14b5k3b11c2s24ck5",
    "c6s17p41k10b8k4b10c2s24ck5",
    "c6s17p17s2p22k9b9k4b10c3s23ck5",
    "c7s16p14s7p20k9b9k4b10c4s23ck4",
    "c11s12p13s9p19k8b10k3b11c4s23c2k3",
    "c12s11p13s10p18k8b9k4b11c4s23c5",
    "c12s11p13s10p18k8b9k4b11c3s19c3s3c4",
    "c7s3cs12p13s9p19k8b8k4b12c3s18c4s7",
    "c7s15cp6s2p5s8p21k7b7k5b12c2s19c4s7",
    "c6s16cp6s3p4s7p19l8k2l8k5l2k3l7cs19c3s8",
    "c6s16cp6s3p4s7p19l18k5l2k5l3k2cs19c3s8",
    "c6s16cp5s3p6s5p20l13k9l3k10cs20c2s8",
    "c6s16cp5s2p8s3p21l12k9l5k9s21c3s7",
    "c6s16cp39l13k8l6k7ls15c2s4c3s7",
    "c6s16cp39l14k6l7k7ls6c3s5c5sc5s6",
    "c6s16cp39l15k2l11k6s5c22s5",
    "c6s16ckp38l28k5l2s3c28",
    "c6s16ckp38l28k4l3c31",
    "c6s16ckp37l30k3l3c31",
    "c6s15ck2l67k3l3c20kc10",
    "c6s12c4k2l68k2l3c19k6c6",
    "c6s8c8k2l73c20k5c6",
    "c7s4c11k2l73c22k3c6",
    "c22k2l73c31",
    "c22k2l73c31",
    "c22k2l73c31",
    "c22k2l73c31",
    "c22k3l72c31",
    "c22k3l72c31",
    "c22k4l71c31",
    "c22k4l71c31",
    "c22k4l71c31",
    "c22k4l68k2lc31",
    "c22k4l68k3c31",
    "c22k4l68k3c31",
    "c22k4l68k3c31",
    "c21k5l68k4c30",
    "c21k5l8k5l55k4c30",
    "c21k5l7k7l55k3c30",
    "c20k7l6k8l54k3c30",
    "c20k7l6k8l54k3c30",
    "c19k8l6k9l54k2c30",
    "c19k9l4k10l54k3c29",
    "c19k9l4k11l54k2c29",
    "c19k9l4k11l54k3c28",
    "c19k10l3k11l54ck2c28",
    "c19k25l6k3l44c31",
    "c18k27l3k8l20k2l19c31",
    "c17k44l12k8l16c31",
    "c16k47l9k13l12c29k2",
    "c15k81c5kc23k3",
    "c14k83c2k4c21k4",
    "c13k90c19k6",
    "c13k91c16k8",
    "c7k4c7k87c14k9",
    "dk8d13k18d6k61s11k10",
    "k8d15k16d8k59s13k9",
    "k6d19k14d8k55s18k8",
    "k5d21k13d8k33dk14d3k3s20k6s",
    "k4d24k11d8k32d3k12d5k2s21k5s",
    "k3d26k10d8k31d5k10d7ks22k4s",
    "k2d9k2d17k9d8k30d7k9d7s24k2s2",
    "k2d8k12d9k8d8k13d4k12d9k7d8s28",
    "kd7k15d8k8d8k11d9k9d10k5d9s28",
    "d7k17d7k8d8k10d11k7d13k2d10s28",
    "d7k18d6k8d8k9d13k4d27s28",
    "d6k19d7k7d8k9d4k4d36s28",
    "d6k20d6k7d8k9d3k6d35s28",
    "d5k22d4k8d8k9d2k7d35s28",
    "d4k36d8k8d2k7d35s28",
    "d4k41d7k3d3k6d4k3d29s28",
    "d4k17d5k20d12k4d5k5d12k2d14s28",
    "d4k14d11k16d12k5d4k6d28s28",
    "d4k11d17k12d12k17d27sk2s25",
    "d5k7d22k9d14k17d26sk2s25",
    "d5k6d24k8d6k2d6k18d25sk2s25",
    "d5k5d26k7d5k4d6k4d8k5d25sk3s24",
    "d4k5d28k6d5k4d7kd11k4d25sk3s24",
    "d2k6d30k5d5k4d20k3d25sk3s24",
    "k7d32k4d6k2d22k2d25sk3s24",
    "k6d34k3d57sk3s24",
    "k5d36kd14k2d7s3d18kd13sk3s24",
    "k4d51k3d6s5d17kd13sk3s24",
    "k4d42kd6k4d6s6d31k4s23k",
    "k3d42k12d6s6d31sk4s22k",
    "k2d43k12d7s4d32s2k3s22k",
    "k2d43k11d44s2k3s22k",
    "kd44k11d44s3k2s23",
    "kd44k12d43s3k2s22k",
    "k2d43k12d18kd24s3k2s22k",
    "k2d43k13d16k3d23s3k2s23",
    "k2d43k14d15k3d23s27k",
    "k2d43k15d14k3d23s27k",
    "k2d43k16d12k4d23s27k",
    "k2d43k16d11k5d23s27k",
    "k2d43k17dk3d5k6d23s27k",
    "k2d42k23d2k8d23s27k",
    "k2d42k34d22s27k",
    "k3d41k34d22s27k",
    "k3d40k22d4k9d22s27k",
    "dk3d38k5d2k11d15k2d23s27k",
    "dk4d37k5d3kd49s27k",
    "dk5d35k6d53s27k",
    "dk5d34k9d51s27k",
    "dk6d32k10d51s26k2",
    "k8d30k10d52s26k2",
    "k9d29k10d52s26k2",
    "k10d27k11d52s26k2",
    "k11d25k12d52s26k2",
    "k12d22k14d52s26k2",
    "k12d19k17d52s6k2s18k2",
    "d5k4d4kd14k6d4k4d5kd30k2d20s6k2s18k2",
)
MAP = LabelMap(MAP)

SHORTS = {"": (0.9, 0.42, 0.08), "_red": (0.75, 0.1, 0.07), "_blue": (0.12, 0.28, 0.75)}
COLOURS = {"c": (0.55, 0.47, 0.12), "l": (0.36, 0.35, 0.12), "b": (0.66, 0.46, 0.22),
           "d": (0.1, 0.1, 0.1), "s": (0.86, 0.82, 0.8)}
ALPHA = {"c": 0.5, "p": 0.45, "l": 0.5, "b": 0.45, "d": 0.45, "s": 0.35}
# the grade under the paint: the same colours, as a x2 multiply
TINT = {"c": (0.75, 0.62, 0.22), "l": (0.55, 0.53, 0.25), "b": (0.85, 0.6, 0.32),
        "d": (0.25, 0.25, 0.26), "s": (0.62, 0.6, 0.6)}
TEAM_TINT = {"": (1.0, 0.55, 0.18), "_red": (0.95, 0.2, 0.15), "_blue": (0.25, 0.4, 0.95)}
# slash_h.tga, 768 px view: the hair, clear of the face and the goggles
HAIR = [region(p, 768, 768) for p in (
    [(100, 160), (768, 160), (768, 600), (230, 600), (230, 380), (100, 380)],
    [(0, 230), (100, 230), (100, 380), (0, 380)],
)]
HAIR_COLOUR = (0.07, 0.05, 0.05)


def body_grade_fn(variant):
    n = Fbm(2120, 4, 6)

    def f(u, v):
        c, k = MAP.label(u, v), MAP.shade(u, v, 0.5)
        k *= 0.88 + 0.24 * n.at(u * 5, v * 5)
        if c == "k":
            return [0.5 * k, 0.5 * k, 0.5 * k, 1]
        tint = TEAM_TINT[variant] if c == "p" else TINT[c]
        return [clamp(z * k) for z in tint] + [1]
    return f


def body_detail_fn(variant):
    n, fine = Fbm(2116, 4, 6), Noise(2122, 256, 256)

    def f(u, v):
        c = MAP.label(u, v)
        if c == "k":
            return [0, 0, 0, 0]
        x, y, e = u * 512, v * 512, MAP.edge(u, v)
        k = 0.85 + 0.3 * n.at(u * 8, v * 8)
        colour = SHORTS[variant] if c == "p" else COLOURS[c]
        if c == "c" and (x % 22) < 1.6:
            return [0.12, 0.1, 0.03, 0.7]                    # the corset's boning
        if c == "p" and 1.2 < e < 2.2 and (x + y) % 9 < 4.5:
            return [0.95, 0.85, 0.6, 0.7]                    # stitched hem
        if c in "pbd" and e < 1.2 and fine.at(x, y) > 0.55:
            return [0.8, 0.75, 0.65, 0.5]                    # worn edge
        if c == "d" and n.at(u * 20, v * 20) > 0.62:
            return [0.35, 0.35, 0.36, 0.5]                   # a scuff on the pads
        return [clamp(z * k) for z in colour] + [ALPHA[c]]
    return f


def head_detail_fn():
    n = Fbm(2118, 4, 6)

    def f(u, v):
        if inside(HAIR, u, v):
            k = 0.8 + 0.4 * n.at(u * 30, v * 6)
            return [z * k for z in HAIR_COLOUR] + [0.8]
        return [0, 0, 0, 0]
    return f


def skate_grade(u, v):
    return [0.2, 0.9, 0.35, 1]                   # the blue glow turned green


def look():
    return Look("slash", "slash", {
        "body": Sheet({"": "slash", "_red": "slash", "_blue": "slash"},
                      grade={v: body_grade_fn(v) for v in SHORTS},
                      detail={v: body_detail_fn(v) for v in SHORTS}, size=512),
        "head": Sheet({"": "slash_h", "_red": "slash_h", "_blue": "slash_h"}, detail=head_detail_fn(), size=256),
        "skate": Sheet({"": "slashskate", "_red": "slashskate", "_blue": "slashskate"}, grade=skate_grade, size=128),
    }, {"lower": {"l_legs": "body", "l_skatel": "skate", "l_skater": "skate"},
        "upper": {"u_torso": "body"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
