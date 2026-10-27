"""The champions' looks on models the player has (see skins.Look), one
module each."""


def ALL():
    from . import nyx
    return [nyx.look()]
