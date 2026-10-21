// tremor-invictus player colours (original shaders, no game data)

// flat, unlit body in the entity colour
tremor/bright
{
	nopicmip
	{
		map $whiteimage
		rgbGen entity
	}
}

// the model's own look with the entity colour multiplied over it, doubled
// so pale and mid tones take the colour strongly
tremor/sport
{
	nopicmip
	{
		map $whiteimage
		blendFunc GL_DST_COLOR GL_SRC_COLOR
		rgbGen entity
		depthFunc equal
	}
}

// plain multiply, for weapons, grenades and corpses
tremor/tint
{
	nopicmip
	{
		map $whiteimage
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen entity
		depthFunc equal
	}
}
