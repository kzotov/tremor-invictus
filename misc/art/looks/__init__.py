"""The champions' looks on models the player has (see skins.Look), one
module each, each with a look():

  models/players/mynx/*_nyx[_red|_blue].skin, nyx/head_*.skin, nyx_*.tga
                       Nyx on mynx (looks/nyx.py): blue curls painted over
                       the hair, a violet-black suit with glowing seams, a
                       veil over the lower face, a crescent on the shoulder
  models/players/visor/*_deathknight*.skin, bones/head_deathknight.skin, ...
                       Death Knight on visor's body with bones' skull
                       (looks/deathknight.py): blackened plate with embers in
                       its cracks, a furnace grate on the shoulder, a charred
                       skull with fire in the sockets
  models/players/crash/*_athena*.skin, ...
                       Athena on crash (looks/athena.py): white and silver
                       armour with blue lights, a navy undersuit over the
                       skin and leathers
  models/players/bitterman/*_bj*.skin, ...
                       B.J. on bitterman (looks/bj.py): a human skin over the
                       hide, a blond buzz cut over the tentacles, a dark vest
                       and fatigues
  models/players/biker/*_eisen*.skin, ...
                       Eisen on biker (looks/eisen.py): a grey beard, brass
                       goggles on the brow, a leather apron, work trousers, a
                       steel left arm
  models/players/lucy/*_galena*.skin, uriel/head_galena.skin, ...
                       Galena on lucy's "angel" plate with uriel's hood
                       (looks/galena.py): cold iron with teal seams, dark
                       robes, a charcoal hood with a teal trim, a pale face
                       with teal eyes
  models/players/razor/*_scalebearer*.skin, visor/head_scalebearer.skin, ...
                       Scalebearer on razor's "id" armour with visor's helm
                       (looks/scalebearer.py): gold-rimmed red scales, bronze
                       scales over the arms, a gold shield, a red bronze helm
  models/players/major/*_raine*.skin, ...
                       Raine on major (looks/raine.py): a slate survivor's
                       suit with blue lights, the hair darkened
  models/players/uriel/*_beastmaster*.skin, ...
                       Beastmaster on uriel (looks/beastmaster.py): no wings,
                       a bronze hide with blue war paint, a dark pelt hood,
                       amber eyes
  models/players/orbb/*_unumrey*.skin, ...
                       Unumrey on orbb (looks/unumrey.py): bruised violet
                       flesh, blackened metal, an eye of our own with a
                       glowing violet ring
  models/players/sorlag/*_sorlag*.skin, ...
                       Sorlag (looks/sorlag.py): an olive-green hide with
                       glowing acid glands
  models/players/doom/*_doomslayer*.skin, ...
                       Doom Slayer (looks/doom.py): deep green armour and
                       helmet, a dark grey undersuit, a gold visor
  models/players/tankjr/*_clutch*.skin, ...
                       Clutch on tankjr (looks/clutch.py): orange-brown
                       armour, steel plates over the emblems, steel feet, a
                       steel-cased head with cyan eyes
  models/players/ranger/*_ranger*.skin, ...
                       Ranger (looks/ranger.py): olive-drab vest, pads and
                       helmet over a dark undersuit, a brass buckle
  models/players/ranger/*_shellshock*.skin, ...
                       Wrack on ranger's armoured set (looks/wrack.py):
                       red-brown plate, dark leather, bone skulls on the
                       thigh plates
  models/players/anarki/*_anarki*.skin, ...
                       Anarki (looks/anarki.py): tanned skin, a near-black
                       mohawk, brown leather, rusted scrap, brass goggles
"""
import importlib

MODULES = ("nyx", "deathknight", "athena", "bj", "eisen", "galena", "scalebearer", "raine", "beastmaster",
           "unumrey", "sorlag", "doom", "clutch", "ranger", "wrack", "anarki")


def ALL():
    """(module, look) for every champion's look."""
    out = []
    for name in MODULES:
        module = importlib.import_module("looks." + name)
        out.append((module, module.look()))
    return out
