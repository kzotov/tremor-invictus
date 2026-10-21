Map pools for the end-of-match vote and rotation, one file per mode:
mappools/<mode>.txt (ffa, duel, tdm, ctf, ca, sacrifice, roundduel,
instagib, unholytrinity), map names separated by spaces or new lines.
g_mapPool, when set, overrides the file. Without either, the vote offers
every map whose .arena lists the mode.
