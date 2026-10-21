// tremor-invictus ability effects (original shaders, no game data)
//
// The images are made at build time by misc/art/make-abilities.py. The
// splash pictures, medals and ability icons need no shaders of their own:
// they are plain images with alpha, drawn in 2D.

// the hook's line: links along the beam, one every 64 units
tremor/fx/hookline
{
	nopicmip
	cull none
	{
		map gfx/tremor/fx/hookline.tga
		blendFunc add
		rgbGen identity
		tcMod scale 4 1
	}
}

// a soft ball of light in the entity colour: fireballs, acid, the plasma trail
tremor/fx/glowball
{
	nopicmip
	cull none
	{
		clampmap gfx/tremor/fx/glowball.tga
		blendFunc add
		rgbGen entity
	}
}

// the dire orb: a dark core in a red corona, turning slowly
tremor/fx/orb
{
	nopicmip
	cull none
	{
		clampmap gfx/tremor/fx/orb.tga
		blendFunc blend
		rgbGen identity
		tcMod rotate 40
	}
}

// the barrier: hexagonal cells sliding over the player
tremor/fx/barrier
{
	nopicmip
	{
		map gfx/tremor/fx/barrier.tga
		blendFunc add
		tcGen environment
		tcMod scroll 0.05 0.1
		rgbGen wave sin 0.8 0.2 0 0.5
	}
}

// a ring of light on the floor, in the polygon's vertex colour
tremor/fx/ring
{
	nopicmip
	cull none
	{
		clampmap gfx/tremor/fx/ring.tga
		blendFunc add
		rgbGen vertex
	}
}
