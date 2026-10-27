"""The champions' looks on models the player has (see skins.Look), one
module each."""


def ALL():
    from . import nyx, deathknight
    return [nyx.look(), deathknight.look()]
