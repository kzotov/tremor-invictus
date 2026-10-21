// tremor-invictus menus (original shaders, no game data)

// the logo, rendered from misc/logo at build time on black and added over
// the menu background
gfx/tremor/logo
{
	nopicmip
	nomipmaps
	{
		map gfx/tremor/logo.png
		blendFunc add
		rgbGen vertex
	}
}
