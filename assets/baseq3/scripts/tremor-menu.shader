// tremor-invictus menus (original shaders, no game data)

// the logo: the branding pack's horizontal lockup, transparent around the
// artwork (misc/art/import-brand-art.py; without the pack, the working logo
// from misc/logo with its black made transparent). Blended by its alpha, so
// it keeps its dark bevels over light backgrounds; mipmapped, so it stays
// clean on the small loading-screen bar
gfx/tremor/logo
{
	nopicmip
	{
		clampmap gfx/tremor/logo.png
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}

// the dropped console: the branding pack's quiet dark metal with the emblem
// small at the bottom right (or misc/art/make-console.py's placeholder), the
// game a little visible through it; drawn keeping its 2:1 shape, its foot on
// the console's bottom edge (Con_DrawSolidConsole)
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
