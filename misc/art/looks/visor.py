"""Visor in his own colours: the steel and the old plates painted orange,
the hide turned to pale skin, the dark joints kept dark gunmetal, and the
visor painted as an orange face mask with a dark slit. Team versions paint
the armour in the team's colour."""
import re
from skins import Sheet, Look
from tremorart import Fbm, clamp

# what each texel of visor.tga is, 128 x 128, run-length coded rows:
# o armour, s skin, d joints, k shadow (left alone)
MAP = (
    "sk31o5s7k13s3k26o2k6o3k7s10k3s7k4",
    "k31o6s8k11s5k25o2k6o3k4s27",
    "k5s3k6s5k10o8s10k8s6k15s5k4o3k6o3k3s28",
    "k4s5k4s7k8o9s11k5s8k14s8o5k7o2s16k2s13",
    "k4s6k2s8kdo3k2o10s11k4s10k12s9o5k7o2s16k3s12",
    "k3s18o16s12k2s11k12s9o5k7o2s16k4s11",
    "k3s18o16s25k12s9o5k7o2s16k4s11",
    "k4s11ks4o17s26k11s9o5k6o3sk3s13k3s7k2s2",
    "k7o6ko23s26k11s9o6k5o4k2o28",
    "k5o6ko25s4ks22k10s9o6k4o4k4o27",
    "k4o7ko25s2k4s21k10s9oko5k2o5k8o23",
    "k4o33sk5s6k2s14k8s10k3o9s2k8s7o16",
    "k3o34k6s5k4s15k5s11k3o8s2k7s10o15",
    "k3o34k7s3k5s16k3s12k3o8s2k6s11o15",
    "k3o34k8sk6s31k4o7sk3s15o15",
    "k2o35sk7sk5s32k4o7s2ks16o15",
    "k2o35s2k6s2k3s32k6o6s19o15",
    "k2o35s3k5s37k12s19o15",
    "k2o35s4k4s37k12s19o15",
    "k2d3o32s5k3s37k10o2s19o15",
    "kd5o31s6k3s36k9o3s19o15",
    "d7o30s7ks37k7o5s19o15",
    "d7o30s45k6o6s19o15",
    "d6o31s45k5o7s19o15",
    "d3o8d2o24s44k5o8ks18o15",
    "d2o9d3o23s44k3o10ks18o15",
    "d2o10d2o23s44k2o11ks18o15",
    "d2o35s44k2o11ks18o15",
    "d2o35s44kso11ks18o15",
    "do36s43k2so11s19o15",
    "o37s43k3o11s19o15",
    "o37s42k5o10s19o15",
    "o37s42k5o10s19o15",
    "o37s42k5o10s19o15",
    "o37s42k5o10s19o15",
    "o37s42k5o10s19o15",
    "o37s41k5o12s11k2s4o16",
    "o37s42k2s2o45",
    "o37s46o45",
    "o37s4k3s39o45",
    "o37s2k5s39o45",
    "o37k8s38o45",
    "o37k8s38o19k2o24",
    "o37s5k4s37o8s11k4s4o18",
    "o37ks5k3s34k2so7sk3s9k2s6o17",
    "o34k4s6ks34k17s15o17",
    "o33k5s41k17s15o17",
    "o34ko2s43k16s15o17",
    "o37s43k15s16o17",
    "ko36s43k15s16o16k",
    "ko34kds43k7o2k5s16ko16k",
    "ko33k4s45o7k3s16k2o17",
    "k2o24k2o6k7s4ks37o7k3s16k3o15k",
    "k2o23k3o5k9s2k3s36o7k2s17k3o13k3",
    "k3o21k5o3k10s2k4s34o8k2s16k4o12k4",
    "k3o20k19o2k5s28ko12k2s15k4o13k4",
    "k3o19k28s4k2s2k2s16k2o12k2s14k3s2o14k3",
    "k2o20k56o12k3s12k2s4o15k2",
    "k2o12k2o6k56o12k3s11k2s5o15k2",
    "k3o8k6o6k19o3k33o12k4s9k2s6o16k",
    "k17o6k18o5k32o12k5s7k2s7o17",
    "k17do7k15o6k24o4k5o10k7s5k2s7o18",
    "o2k6o2k7o4d2o4k6o14k22o6k23o2k2o26",
    "o4d3o5d3o6d2o24k12o2k7o8k26o26",
    "o13d2s32k4s4k4s3k6o8k19o2k5o26",
    "o15s11ks35ok4o9k16o6k3o27",
    "o15s10k3s34o2k2o11k4o3k3o41",
    "o15s11k2s13k4s12ks4o66",
    "o15s25k9s7k3s3o66",
    "o15s25k9s8ks4o66",
    "o15s26k3s18o66",
    "o15s47o66",
    "do14s47o66",
    "d2o13s47o66",
    "d2o13s47o66",
    "d2o13s47o66",
    "d2o13s7k2s38o28do37",
    "kdo13s5k4s38o16k2o9d3o4d2o30",
    "ko13ds4k4s39ko5k2o20d2o3d5o28",
    "k2o11d2s4k2s41k2o3k4o24dk5o27",
    "k2o11d2s47k8o14k2o10k6o26",
    "k2od2o9ds47k7o15k2o12dk4do24",
    "k2od2o10sk2s44k5o34k3do22k",
    "k2od2o10sk2s43k5o36dk2o22k",
    "k3d2o10k3s43k4o38dk2o22",
    "k3o12k3s43k4o14k2o23dk2o21",
    "k3o12sk2s44k2o15k3o24k2o17k2o",
    "k3o12sk2s44o17k2o26k4o12k4d",
    "k3o12k3s44o37d3o4dk4o11k5o",
    "k3o12k3s6ks37o38d3o3dk4o11k5o",
    "k4o10k5s4k3s15ks6k2s12o38d4od2k3o13k4o",
    "k5o3k17s15k3s19o39d6k2o14k4o",
    "k25s16k3s18o29k2o8d5o18k3o",
    "k25s17k3sk2s14o24k9o7d3o20k3",
    "k26s5k8s2k10s11o24k10o29k3",
    "k51s11o25k12o27k2",
    "k18s2k31s11o26k12o27k",
    "k17s4k29s12o27k13o26",
    "o2k14s6k12s3k13s12o27k14o25",
    "o4k11s8k10s5k11s12o29k13o25",
    "o6k7o11k9o7k9o44k9o26",
    "o24k8o10k8o44k7o27",
    "o24k7o12k7o41d2ok6o28",
    "o24k7o12k6o41d4k6o28",
    "o25k5o12k6do41d3ok5o29",
    "o42k5do43do2k5o29",
    "k2o39k5do47k4o30",
    "k3o38k5o48k4o30",
    "k4o36k5o49k3o31",
    "k3o37k4o50k3o31",
    "ko40k2o50k5o30",
    "o93k7o28",
    "o93k10o3k2o20",
    "ko92k17o18",
    "k2o91k17o3k2o13",
    "k3o89k15o21",
    "k3o88k15o22",
    "k3o86k17o22",
    "k3o38k4o43k18o22",
    "k3o37k5o38ko5k17o22",
    "k3o36k7o35k4o7k5do5k3o22",
    "k4o5k3o3k3o10k5o5k8o9k3o20k6o20k2o22",
    "k47o8k3o5k3o2k2o5k9o44",
    "k47o8k4o3k9o3k10o44",
    "k48o6k18ok12o43",
    "k49o3d2k31o43",
    "k50od2k33d4o2d6o3d3o24",
    "k28o3k18od3k19o2k31o23",
)
GRID = ["".join(c * int(n or 1) for c, n in re.findall(r"([a-z])(\d*)", row)) for row in MAP]
N = len(GRID)

SLIT = (0, 412, 140, 428)                        # across the mask, 768 px view
ARMOUR = {"": (0.86, 0.4, 0.1), "_red": (0.78, 0.12, 0.08), "_blue": (0.14, 0.3, 0.78)}
SKIN = (0.8, 0.62, 0.5)
JOINTS = (0.12, 0.12, 0.13)


def label(u, v):
    return GRID[min(N - 1, int(v * N))][min(N - 1, int(u * N))]


def detail_fn(variant):
    n = Fbm(2117, 4, 6)

    def f(u, v):
        x, y = u * 768, v * 768
        if SLIT[0] <= x <= SLIT[2] and SLIT[1] <= y <= SLIT[3]:
            return [0.03, 0.03, 0.03, 1]
        k = 0.85 + 0.3 * n.at(u * 8, v * 8)
        c = label(u, v)
        if c == "o":
            return [clamp(z * k) for z in ARMOUR[variant]] + [0.72]
        if c == "s":
            return [clamp(z * k) for z in SKIN] + [0.8]
        if c == "d":
            return [z * k for z in JOINTS] + [0.6]
        return [0, 0, 0, 0]
    return f


def look():
    return Look("visor", "visor", {
        "body": Sheet({"": "visor", "_red": "visor", "_blue": "visor"},
                      detail={v: detail_fn(v) for v in ARMOUR}, size=512),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body"},
        "head": {"h_head": "body"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
