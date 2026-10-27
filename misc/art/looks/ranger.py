"""Ranger in his own colours: olive-drab plate over a dark undersuit. The
orange trousers and the bare arms go dark, the yellow vest is dulled to
olive drab, the brown helmet, pads and boots are painted to match the vest, and the belt
buckle stays brass. Team versions keep the team's paint on the vest and
trousers with the dark sleeves and the olive helmet."""
from skins import Sheet, Look, region, inside
from tremorart import Fbm

# regions in a 768 px view of ranger.tga (the same layout as wrack.tga)
VEST = [region(p, 768, 768) for p in (
    [(0, 0), (225, 0), (225, 330), (0, 330)],
    [(540, 0), (768, 0), (768, 300), (540, 300)],
)]
ARMS = [region([(220, 0), (530, 0), (530, 300), (220, 300)], 768, 768)]
TROUSERS = [region([(480, 470), (768, 470), (768, 768), (480, 768)], 768, 768)]
BUCKLE = [region([(540, 570), (600, 570), (600, 665), (540, 665)], 768, 768)]
HELMET = [region([(0, 460), (335, 460), (335, 768), (0, 768)], 768, 768)]


def body_grade(u, v):
    if inside(VEST, u, v):
        return [0.5, 0.52, 0.42, 1]
    return [0.5, 0.5, 0.48, 1]


def undersuit(n, u, v, a):
    k = 0.8 + 0.4 * n.at(u * 8, v * 8)
    return [0.1 * k, 0.1 * k, 0.09 * k, a]


def body_detail_fn(team):
    n = Fbm(2111, 4, 6)

    def f(u, v):
        if inside(BUCKLE, u, v):
            return [0, 0, 0, 0]
        if inside(ARMS, u, v):
            return undersuit(n, u, v, 0.8)
        if not team and inside(TROUSERS, u, v):
            return undersuit(n, u, v, 0.85)
        if inside(HELMET, u, v):
            return [0.3, 0.31, 0.2, 0.55]
        if not team and inside(VEST, u, v):
            return [0.3, 0.31, 0.2, 0.35]
        if not team:
            return [0.27, 0.28, 0.19, 0.4]        # pads, shins and boots in drab
        return [0, 0, 0, 0]
    return f


def look():
    return Look("ranger", "ranger", {
        "body": Sheet({"": "ranger", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(False), "_red": body_detail_fn(True),
                              "_blue": body_detail_fn(True)}, size=256),
        "face": Sheet({"": "ranger_h", "_red": "ranger_h", "_blue": "ranger_h"}, size=64),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_larm": "body", "u_rarm": "body"},
        "head": {"h_helmet": "body", "h_face": "face"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
