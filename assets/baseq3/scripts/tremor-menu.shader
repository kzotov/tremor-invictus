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

// the dropped console: generated gunmetal with the heartbeat along its foot
// (misc/art/make-console.py), the game a little visible through it
tremor/console
{
	nopicmip
	nomipmaps
	{
		clampmap gfx/tremor/console.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen identity
		alphaGen const 0.94
	}
}
