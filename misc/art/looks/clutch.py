"""Clutch on tankjr: the orange armour tinted a glossy dark red with the
model's own shading kept, the steel trim left bright, shadows in the
creases, worn edges and scratches, the emblems covered by steel plates with three stencilled bars (the plates
are mirrored on the model, so no lettering), the gory feet turned to
dark steel, the bare head cased in steel as a sensor unit, and its eyes
lit amber. Team versions keep the
team's paint with the same plates, feet, head and eyes."""
import math
from skins import Sheet, Look, LabelMap, region, inside
from tremorart import Fbm, Noise, clamp

# what each texel of tankjr.tga is, 128 x 128, run-length coded rows:
# p paint, s steel, g gore, k shadow
MAP = LabelMap((
    "sk6sp3k16s4p7s2k2sksk18s15k2s6p5s2k25sp4s2k3",
    "k5p6k12s2p19k9s25k6p7s2k23s2p6sk3",
    "k3p8k11p23ks32k3sp8s4k19p11sk3",
    "sp10sk9p24s33k2p9s3p2k17sp13k3",
    "p12k8p26ks31k2p2s5p2s4psk14sp16k2",
    "p12k7p28ks30k2s15k12sp19k",
    "p12k6p29sks29k2s16k10p22",
    "p13k4sp30k2s28k3p4s10pk8p24",
    "p13k3sp32k2s27k7ps9p2k6sp24",
    "p14ksp33sk3s14k2s10k6ps12k5sp9s6p9",
    "p50sk5s12ks10k5ps13k5p9s8p8",
    "p51k3p3s22k5ps14k4p10s6p9",
    "p25s2p33sk2s16k4ps15k4p25",
    "sp24s2p36sk10s4k4s17pk3p25",
    "p65k16s18pk3p25",
    "p66k14s19p2k2p25",
    "p9sp23k10s4p12s2p4spk13s19p2k2p25",
    "p4sp3sk2p21k21p7sp3s3pk12s19pk3p17s2p3s2p",
    "s6p3k2p9s2p10k2s2k17p11s4pk11s19k4p16ks8",
    "s7p2skp9s2p9k2s4k16p12s5k11sp2s15k4p16s9",
    "s7p2sksp8sp8sk3s5k15p12s6k14s13k5p16s4p3s2",
    "s7p3skp16k4s6k15p11s8k14ps8pk7sp16s4p3s",
    "s3p6sk2p15k4s7k15p10s10k13p2s5psk7s2p16s6p2",
    "s2p7k2p16sk2s8k15p9s9p2sk12p3s3k9s3p17ks3p3",
    "sp27k2s9k7s3k4p8s11p2k15s4k7s5p22s",
    "sp5skp20k3s2k4s3k6s4k3p7s13p2sk12s8pk3s5p23",
    "p6kp21k10s3k4s6k2p4s17p2sk10s19p14s4p4",
    "p28k3s5p2ks12k2p2s19p3sk7s21p13s6p3",
    "p26sgk2s6p2sks11k2ps20p4k5s22kp13s2p2s2p3",
    "p7s2p16s3k2ps5p3sk2s9k2s21p3k5s23kp13sp3s2p2s",
    "p7s2p13s14p4k2s9k2s21psk4s27p9s4p7s",
    "p21s15p3k3s9k2s21k4s28kp6s8p6s",
    "p3kp16ks15p2sk2s32k3s31p5s10ps2p3",
    "p20ks14p3sks21p2s10ks23k2s9p4s14ps",
    "p20s15p3k2s19ps2ps33k4s8p4s15p",
    "p12sp7s15p2sk2s12ks6k3s4ks23gs3k6s10p3s13p2",
    "p11s3p5s3p2s11p2k3s11k2s4k11s15k2s4g3sk6s12ps10k6",
    "p12s3p4s2p4sps8psk2s13ks4k10s14k4s3g5sk5s22k7",
    "s3p10sp5s3p5ks8k3s24k3s15k3s3g6sk5s15k14",
    "k9s2p9s2p4ks5psk4s26k2s15k3s2g8k5s10ks2k16",
    "k11sp13ks7k4s15ks11k2s15k3s2g9k5s9k19",
    "k12s2p9s4p2s2k4s8p2s6k2s10k3s14k5s2g8k4s11k18",
    "k14sp8s4p3s2ks9p4s5k2s10k3s12k7s3g3s5k2s13k17",
    "k15sp6s6ps14p2s6k2s6ks4k3s10k8s27k10s2k4",
    "k16s34k4s5ks5ks11k9s16k2s8k9s4kps",
    "k17s32k5s30k3s15k4s6k9s4p3",
    "k20s28k6s7k2s22k3s15k4s4k10s3p4",
    "k21s26k7s7k3s21k4s14k5s2k7s2k2s3p4",
    "k22s24k8s8k2s20k5s15k12s4kps2p4",
    "k23s22k9s8k2s20k5s15k12sps3ps2p4",
    "k25s4k2s13k10s2gs5k3s20k5s15k10sp3s4p5",
    "k54sg3s4k3s22k3s15k9sp4ks3p5",
    "k55g4s2gk3s23k2s16k7s2p3skps2p5",
    "k55g3s3gk3s23k3s15k5ps3p3k2ps2p5",
    "p2s2k18sk32s7k3s23k3s16k3p2s4p2ks3p6",
    "ps5k15s4k9s3k17s9k2s23k4s15k3gps4p2ks3p6",
    "s7k14s16k17s3k4s2k3s21k6s14k3sp2s3p2k2sp7",
    "s11k11s14k19sk12s15k11s14k2sp2s3p2ks4p5",
    "s12k11s12k34s13g4k9s12gk3p2s3p3ks4gp3",
    "s12k3sk7s5ks4k36s13g4k10s13k2p3s2p5ks2p4",
    "s12k2s3k5s5p2sk39s17k8sp3s11k3ps2p7ksp4",
    "s12ks5k3s6p6s2k34s16k7sp6s10k3s2p8sksp3",
    "s11k2s4k4ps6p10s2k17s9k3s16k5s2p8s8gsk2s2p4s4k3s3",
    "s11k2s4k3p3s4p12s3k15s10k2s12g2sk5sp10s2gs5g2k3s2ps5k7",
    "s16k3p5sp14s4k3s6k4s26k6sp11s2gs5g2k5s5k8",
    "s16k2p20s39k10sp12s9k6s6k7",
    "ps4p2s9kp20s36k13sp13s8k5s9k6",
    "s4p2k2s7kp21s32k15s2p14sk3s2k5s12k5",
    "ks2p3ks7kp22s19k2s10k15s2p15k10s14k4",
    "ks3p2s7k2p22s18k4s9k8s8p15sk9s16k2s",
    "s11k4p22s18k4s9k5s3p21s4p2k5s20",
    "s10k6p21s19k3s9k4p13s2p3s2p4ks5psk3s21",
    "s9k7p22s17k4s9k4p11s10p3s8k2s22",
    "s7pk8p11s3p8s14k7s9k4p11s21k2s22",
    "k4s3k9p10s6p6s5k17s8k4p5skp4s20pk2s22",
    "k4s4k9p10s6k2s7k18s8k4p4s3p3ks20k4s21",
    "s11k23s9k18s7k4ps2ps22ks3k10s16",
    "s12k21s12k17gs4k5s3k2s6p2s7p3s3k15s15",
    "s12k20ps14k30s4p6s3k2sp3s3p2k11p6s10",
    "s12k6p4s4k5p7s10k29s3p11k5s2p4k9p7k2s8",
    "s12k5p23s9k13s2p9k3s3p12s2k3p6k8p7sk3s7",
    "s12k4p3sp22s9k10sp12ks3p16kp7k7p8sk4s6",
    "s12k4p2s3p23s8k9p13s2p25gk7p8s2k4s5",
    "s12k3sp3sp25s9k7p40sk6p9s2k7s2",
    "s12k3p31s10k5p40sk5sp9s2k8s",
    "s13ksp33s9k4p40sk4sp9s3k8s",
    "s14p35s8k4p40sk4p10s3k8s",
    "s14p36s6k4p41sk3p11s3k8s",
    "s13p37s2pk7p41sk2p13s3k7s",
    "s8p2s3p21sp12sk2sp3sk5p41sksp14s2k7s",
    "s3p3sp4s3p16s5p7s2k5sp7skp43kp18k6s",
    "s3p7s3k2p14s5p7k8p3s3p66k6s",
    "s4p4s5k2p15s4p2s6k6p3s5p65k6s",
    "s5p3s5k2p14s14k4p3s6kp64k6s",
    "s6p2s5k3p6sk2s19k2sp3s6kp3s10k2s7p42sk5s",
    "s6p2s4k13s22p3s6kp2k16s4p5sp36s3k3s",
    "s6p2s4k12s24p2ks6p2k16p8s3p39k2s",
    "s5p3s4p2k10s24p3sk3sp3k3p5s2p3k3p8s2p41ks",
    "s4p4s6pk8s25p10k3p6s2p4k2p7s3p42s",
    "s4k4s9k5s25p9sk4sp5s2p4k2sp5s4p43",
    "s4k3ps10k4s24p4s3k8sp4s4p3k2s10p43",
    "s5kps12k2s24p4k12s9p3k2s9p44",
    "s42p7k12s9p3ks10p44",
    "ps38p10kspk9s8p4s7p2sp45",
    "ps37p11s4k8s8p4s6p49",
    "ps29p4s4p12s4k7s8p5s6p45s2p",
    "s30p5s3p15sk7s8p5s7p6s2p35s4",
    "s30p5s3p16k8s10ps10p3s3p34s5",
    "s31p4s3p16k18s18p32s2k2s2",
    "s32p2s4p16k21s15p31s2k4s",
    "s32p2s5p12s3k22s14p25s4k8s",
    "s31p3s5p11s3pk23s13p24s5k8s",
    "s31p4s3p12s2p2k23s13p24s4k9s",
    "s27ps4p4s2p12s2p2k24s12p24s3k10s",
    "s27ps5p21k25s11p23s4k10s",
    "s34p19sk14s2k10s10p15s3p3k2s4k10s",
    "s10p5s19p7s9p3k14s4k9s10p13s8k17",
    "s3p2s4p7s4p3s11p7s10p2k10s8k10s9p12s8k9s3k6",
    "p6s2p9s2p4s11p8s13k7s9k10s9p12s8k7s6k5",
    "p27s6p10s16k2s9k11s9p11s8k7s6k6",
    "p27s6p11s24k14s7p9s11k4s10k5",
    "p8s2p16s6p13s22k15s4p11s7ks5kg3s10k4",
    "p7s5p15s2p17s22k15s2k4p3s9k6s5g2s11k3",
    "p7s6p3s8p26s18k22ps9k8s18k2",
    "p7s6p2s10p25s18k22ps9k9s12ks4k2",
    "p8s4p3s9p26k2s15k23s11k9s10k3s4k",
    "p14s6p23k2sp4k2s15k23s11k10s10k2s5",
    "p6s2p5s6p23k5s2k3s14k24ps11k12g2s6k2s2g2",
))

# boxes in a 768 px view: the plates over the emblems
PLATES = [(448, 470, 562, 575), (70, 80, 170, 170)]
HEAD = [region([(0, 575), (335, 575), (335, 768), (0, 768)], 768, 768)]
FEET = [region(p, 768, 768) for p in (
    [(305, 215), (395, 215), (395, 355), (305, 355)],
    [(495, 205), (550, 205), (550, 275), (495, 275)],
    [(300, 700), (410, 700), (410, 768), (300, 768)],
    [(660, 690), (768, 690), (768, 768), (660, 768)],
    [(580, 340), (640, 340), (640, 410), (580, 410)],
)]
EYES = [(85 / 768, 680 / 768, 16 / 768), (128 / 768, 680 / 768, 16 / 768)]
PAINT = (0.3, 0.03, 0.03)
LIGHTS = {"": (1.0, 0.75, 0.2), "_red": (1.0, 0.35, 0.2), "_blue": (0.3, 0.7, 1.0)}


def plate(u, v):
    """None off the plates, else whether (u, v) is on a stencilled bar."""
    for x0, y0, x1, y1 in PLATES:
        x, y = (u * 768 - x0) / (x1 - x0), (v * 768 - y0) / (y1 - y0)
        if 0 <= x <= 1 and 0 <= y <= 1:
            return 0.22 < y < 0.78 and any(abs(x - c) < 0.07 for c in (0.3, 0.5, 0.7))
    return None


TINT = (0.44, 0.11, 0.1)                        # the orange paint to dark red
STEEL = (0.72, 0.72, 0.74)


def body_grade_fn(team):
    n = Fbm(2013, 4, 6)

    def f(u, v):
        c, k = MAP.label(u, v), MAP.shade(u, v, 0.5)
        k *= 0.88 + 0.24 * n.at(u * 5, v * 5)               # grime
        if c == "p" and not team:
            return [clamp(z * k) for z in TINT] + [1]
        if c == "s":
            return [0.56 * k, 0.56 * k, 0.6 * k, 1]          # the steel a little brighter and cooler
        return [0.5 * k, 0.5 * k, 0.5 * k, 1]
    return f


def detail_fn(team):
    n, fine, scratch = Fbm(2011, 4, 6), Noise(2015, 256, 256), Noise(2017, 64, 512)

    def f(u, v):
        p = plate(u, v)
        if p is not None:
            if p:
                return [0.85, 0.82, 0.7, 1.0]
            k = 0.3 * (0.8 + 0.4 * n.at(u * 8, v * 8))
            return [k, k, k * 1.05, 1.0]
        if inside(FEET, u, v):
            k = 0.2 * (0.75 + 0.5 * n.at(u * 8, v * 8))
            return [k, k, k * 1.05, 0.95]
        if inside(HEAD, u, v):
            k = 0.36 * (0.75 + 0.5 * n.at(u * 6, v * 6))
            return [k, k * 1.02, k * 1.08, 0.88]
        c = MAP.label(u, v)
        if c == "p":
            if MAP.edge(u, v) < 0.9 and fine.at(u * 400, v * 400) > 0.68:
                return list(STEEL) + [0.75]                  # paint worn off the edge
            if abs(scratch.at(u * 12, v * 90) - 0.5) < 0.006:
                return list(STEEL) + [0.45]                  # a scratch
            if not team:
                k = 0.85 + 0.3 * n.at(u * 6, v * 6)
                return [PAINT[0] * k, PAINT[1] * k, PAINT[2] * k, 0.45]
        return [0, 0, 0, 0]
    return f


def eyes_fn(colour):
    def f(u, v):
        k = 0.0
        for cu, cv, r in EYES:
            k = max(k, 1 - math.hypot(u - cu, v - cv) / r)
        k = clamp(k * 1.4)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def look():
    return Look("tankjr", "clutch", {
        "body": Sheet({"": "tankjr", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade_fn(False), "_red": body_grade_fn(True), "_blue": body_grade_fn(True)},
                      detail={"": detail_fn(False), "_red": detail_fn(True), "_blue": detail_fn(True)},
                      glow={v: eyes_fn(LIGHTS[v]) for v in LIGHTS}, size=512),
    }, {"lower": {s: "body" for s in ("l_rcalf", "l_rfoot", "l_lfoot", "l_hips", "l_rthigh", "l_lthigh",
                                      "l_rtoe", "l_ltoe", "l_rtoes", "l_ltoes", "l_lcalf")},
        "upper": {"u_abdomen": "body", "u_arms": "body", "u_torso": "body"},
        "head": {"h_head": "body"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
