// tremor-invictus lineup and podium pillars (original shaders, no game data)
//
// The images are made at build time by misc/textures/make-pillar-textures.py.
// Dark metal with the light baked in, then the glow mask added on top in the
// polygon's vertex colour (logo red, or the team's colour), then a little of
// the mask again in white so the brightest lines run hot.

// one face of the hexagonal column: u across the face, v down from the top
// edge over 128 units
tremor/pillar/side
{
	nopicmip
	{
		clampmap gfx/tremor/pillar/side.tga
		rgbGen identity
	}
	{
		clampmap gfx/tremor/pillar/side_glow.tga
		blendFunc add
		rgbGen vertex
	}
	{
		clampmap gfx/tremor/pillar/side_glow.tga
		blendFunc add
		rgbGen const ( 0.15 0.15 0.15 )
	}
}

// the top face, mapped flat across 48 x 48 units centred on the column
tremor/pillar/cap
{
	nopicmip
	{
		clampmap gfx/tremor/pillar/cap.tga
		rgbGen identity
	}
	{
		clampmap gfx/tremor/pillar/cap_glow.tga
		blendFunc add
		rgbGen vertex
	}
	{
		clampmap gfx/tremor/pillar/cap_glow.tga
		blendFunc add
		rgbGen const ( 0.15 0.15 0.15 )
	}
}

// podium places, a 16 x 16 unit quad laid just in front of the face that
// looks at the camera
tremor/pillar/rank1
{
	nopicmip
	{
		clampmap gfx/tremor/pillar/rank1_glow.tga
		blendFunc add
		rgbGen vertex
	}
}

tremor/pillar/rank2
{
	nopicmip
	{
		clampmap gfx/tremor/pillar/rank2_glow.tga
		blendFunc add
		rgbGen vertex
	}
}

tremor/pillar/rank3
{
	nopicmip
	{
		clampmap gfx/tremor/pillar/rank3_glow.tga
		blendFunc add
		rgbGen vertex
	}
}
