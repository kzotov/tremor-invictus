/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_drawtools.c -- helper functions called by cg_draw, cg_scoreboard, cg_info, etc
#include "cg_local.h"

/*
================
CG_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
int		cg_hudAnchor = ANCHOR_CENTER;

void CG_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	// whole-screen fills and backdrops stretch to the real edges
	if ( *x <= 0 && *x + *w >= 640 ) {
		float	full = cgs.glconfig.vidWidth / 640.0f;

		*x *= full;
		*w *= full;
		*y *= cgs.screenYScale;
		*h *= cgs.screenYScale;
		return;
	}

	// the rest keeps its shape and sits by its anchor: the 4:3 area's
	// middle, or the screen's left or right edge
	*x = *x * cgs.screenXScale + ( cg_hudAnchor == ANCHOR_LEFT ? 0
		: cg_hudAnchor == ANCHOR_RIGHT ? 2 * cgs.screenXBias : cgs.screenXBias );
	*y *= cgs.screenYScale;
	*w *= cgs.screenXScale;
	*h *= cgs.screenYScale;
}

/*
================
CG_FillRect

Coordinates are 640*480 virtual values
=================
*/
/*
================
Layout debug

cg_layoutDebug 1 outlines every box the 2D pass draws (pictures green,
text yellow, fills red); cg_layoutDump 1 writes one frame's boxes to
layout.txt, marking any that leave the 640x480 screen
================
*/
#define MAX_LAYOUT_BOXES	1024

typedef struct {
	char	kind;		// 'p' picture, 's' string, 'f' fill
	float	x, y, w, h;
	char	text[24];
} layoutBox_t;

static layoutBox_t	layoutBoxes[MAX_LAYOUT_BOXES];
static int			numLayoutBoxes;

static void CG_LayoutRecord( char kind, float x, float y, float w, float h, const char *text ) {
	layoutBox_t	*b;

	if ( !cg_layoutDebug.integer && !cg_layoutDump.integer ) {
		return;
	}
	if ( numLayoutBoxes >= MAX_LAYOUT_BOXES ) {
		return;
	}
	b = &layoutBoxes[numLayoutBoxes++];
	b->kind = kind;
	b->x = x;
	b->y = y;
	b->w = w;
	b->h = h;
	Q_strncpyz( b->text, text ? text : "", sizeof( b->text ) );
}

static void CG_LayoutLine( float x, float y, float w, float h, const float *color ) {
	CG_AdjustFrom640( &x, &y, &w, &h );
	trap_R_SetColor( color );
	trap_R_DrawStretchPic( x, y, w < 1 ? 1 : w, h < 1 ? 1 : h, 0, 0, 0, 0, cgs.media.whiteShader );
}

/*
================
CG_LayoutFrameEnd

After the 2D pass: outlines, the dump, and a fresh list for the next frame
================
*/
void CG_LayoutFrameEnd( void ) {
	static const vec4_t	colors[3] = { { 0, 1, 0, 0.8f }, { 1, 1, 0, 0.8f }, { 1, 0, 0, 0.8f } };
	layoutBox_t	*b;
	const float	*c;
	int			i;

	if ( cg_layoutDebug.integer ) {
		for ( i = 0, b = layoutBoxes ; i < numLayoutBoxes ; i++, b++ ) {
			c = colors[b->kind == 'p' ? 0 : b->kind == 's' ? 1 : 2];
			CG_LayoutLine( b->x, b->y, b->w, 0.5f, c );
			CG_LayoutLine( b->x, b->y + b->h, b->w, 0.5f, c );
			CG_LayoutLine( b->x, b->y, 0.5f, b->h, c );
			CG_LayoutLine( b->x + b->w, b->y, 0.5f, b->h, c );
		}
		trap_R_SetColor( NULL );
	}

	if ( cg_layoutDump.integer ) {
		fileHandle_t	f;
		char			*line;

		trap_FS_FOpenFile( "layout.txt", &f, FS_WRITE );
		if ( f ) {
			for ( i = 0, b = layoutBoxes ; i < numLayoutBoxes ; i++, b++ ) {
				line = va( "%c %7.1f %7.1f %7.1f %7.1f%s %s\n", b->kind, b->x, b->y, b->w, b->h,
					b->x < 0 || b->y < 0 || b->x + b->w > 640 || b->y + b->h > 480 ? " OUTSIDE" : "", b->text );
				trap_FS_Write( line, strlen( line ), f );
			}
			trap_FS_FCloseFile( f );
			CG_Printf( "layout: %i boxes written to layout.txt\n", numLayoutBoxes );
		}
		trap_Cvar_Set( "cg_layoutDump", "0" );
	}
	numLayoutBoxes = 0;
}

void CG_FillRect( float x, float y, float width, float height, const float *color ) {
	CG_LayoutRecord( 'f', x, y, width, height, NULL );
	trap_R_SetColor( color );

	CG_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cgs.media.whiteShader );

	trap_R_SetColor( NULL );
}

/*
================
CG_DrawSides

Coords are virtual 640x480
================
*/
void CG_DrawSides(float x, float y, float w, float h, float size) {
	CG_AdjustFrom640( &x, &y, &w, &h );
	size *= cgs.screenXScale;
	trap_R_DrawStretchPic( x, y, size, h, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_DrawStretchPic( x + w - size, y, size, h, 0, 0, 0, 0, cgs.media.whiteShader );
}

void CG_DrawTopBottom(float x, float y, float w, float h, float size) {
	CG_AdjustFrom640( &x, &y, &w, &h );
	size *= cgs.screenYScale;
	trap_R_DrawStretchPic( x, y, w, size, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_DrawStretchPic( x, y + h - size, w, size, 0, 0, 0, 0, cgs.media.whiteShader );
}
/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void CG_DrawRect( float x, float y, float width, float height, float size, const float *color ) {
	trap_R_SetColor( color );

  CG_DrawTopBottom(x, y, width, height, size);
  CG_DrawSides(x, y + size, width, height - size * 2, size);

	trap_R_SetColor( NULL );
}



/*
================
CG_DrawPic

Coordinates are 640*480 virtual values
=================
*/
/* The pack importer emits only anchors validated against installed pixels.
 * Missing metadata/unknown shaders retain the ordinary texture center. The
 * approved ability glyphs have their own anchors and are absent from this file. */
void CG_IconAnchor( qhandle_t shader, float *u, float *v ) {
	static struct { qhandle_t shader; float u, v; } anchors[128];
	static int count;
	static qboolean loaded;
	int i;

	*u = *v = 0.5f;
	if ( !loaded ) {
		fileHandle_t f;
		static char buf[16384];
		char *p, *token;
		int len;

		loaded = qtrue;
		len = trap_FS_FOpenFile( "gfx/tremor/icons/anchors.txt", &f, FS_READ );
		if ( f ) {
			if ( len > 0 && len < (int)sizeof( buf ) ) {
				trap_FS_Read( buf, len, f );
				buf[len] = 0;
			} else {
				buf[0] = 0;
			}
			trap_FS_FCloseFile( f );
			p = buf;
			while ( count < ARRAY_LEN( anchors ) ) {
				char path[MAX_QPATH];
				float au, av;
				qhandle_t handle;

				token = COM_Parse( &p );
				if ( !token[0] ) break;
				Q_strncpyz( path, token, sizeof( path ) );
				au = atof( COM_Parse( &p ) );
				av = atof( COM_Parse( &p ) );
				if ( Q_strncmp( path, "gfx/tremor/icons/", 17 ) || strstr( path, ".." )
					|| !( au > 0 && au < 1 && av > 0 && av < 1 ) ) continue;
				handle = trap_R_RegisterShaderNoMip( path );
				if ( !handle ) continue;
				anchors[count].shader = handle;
				anchors[count].u = au;
				anchors[count++].v = av;
			}
		}
	}
	for ( i = 0 ; i < count ; i++ ) {
		if ( anchors[i].shader == shader ) {
			*u = anchors[i].u;
			*v = anchors[i].v;
			return;
		}
	}
}

void CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	float u, v;
	CG_LayoutRecord( 'p', x, y, width, height, NULL );
	CG_IconAnchor( hShader, &u, &v );
	x += ( 0.5f - u ) * width;
	y += ( 0.5f - v ) * height;
	CG_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}

/*
================
CG_LevelshotAspect

The shape (width over height) of a map's levelshot picture. The classic
ones are square textures holding a 4:3 picture; others (1920 x 1080...)
are their own shape. A map without one shows the missing-map picture, 4:3
in a square texture too. Remembers the last few maps asked for
================
*/
float CG_LevelshotAspect( const char *map ) {
	static struct {
		char	map[MAX_QPATH];
		float	aspect;
	}				known[8];
	static int		next;
	static byte		head[4096];
	static const char	*exts[] = { "tga", "jpg", "png" };
	fileHandle_t	f;
	int				i, len, w, h;
	float			aspect = 4.0f / 3.0f;

	for ( i = 0 ; i < 8 ; i++ ) {
		if ( known[i].map[0] && !Q_stricmp( known[i].map, map ) ) {
			return known[i].aspect;
		}
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		len = trap_FS_FOpenFile( va( "levelshots/%s.%s", map, exts[i] ), &f, FS_READ );
		if ( !f ) {
			continue;
		}
		if ( len > (int)sizeof( head ) ) {
			len = sizeof( head );
		}
		trap_FS_Read( head, len, f );
		trap_FS_FCloseFile( f );
		if ( Q_ImageSize( head, len, &w, &h ) && w != h && w > 0 && h > 0 ) {
			aspect = (float)w / h;
		}
		break;
	}
	Q_strncpyz( known[next].map, map, sizeof( known[next].map ) );
	known[next].aspect = aspect;
	next = ( next + 1 ) % 8;
	return aspect;
}

/*
================
CG_DrawLevelshotPic

A map's levelshot in a box, or the missing-map picture when it has none,
cropped to the box in the picture's own shape instead of stretched
================
*/
void CG_DrawLevelshotPic( float x, float y, float width, float height, const char *map ) {
	qhandle_t	shot = trap_R_RegisterShaderNoMip( va( "levelshots/%s.tga", map ) );
	float		picture = CG_LevelshotAspect( map ), box, s0 = 0, s1 = 1, t0 = 0, t1 = 1;

	if ( !shot ) {
		shot = trap_R_RegisterShaderNoMip( "gfx/tremor/splash/nomap" );
	}
	CG_LayoutRecord( 'p', x, y, width, height, NULL );
	CG_AdjustFrom640( &x, &y, &width, &height );
	box = width / height;
	if ( box > picture ) {
		t0 = 0.5f - 0.5f * picture / box;
		t1 = 1.0f - t0;
	} else {
		s0 = 0.5f - 0.5f * box / picture;
		s1 = 1.0f - s0;
	}
	trap_R_DrawStretchPic( x, y, width, height, s0, t0, s1, t1, shot );
}

/*
================
CG_DrawLogo

The horizontal logo with its artwork's top left corner at x, y and the
artwork h high. The texture holds the artwork with transparent margins
(misc/art/import-brand-art.py fits the placeholder logo into the same box)
================
*/
void CG_DrawLogo( float x, float y, float h ) {
	qhandle_t	logo = trap_R_RegisterShaderNoMip( "gfx/tremor/logo" );
	float		th = h * 270 / 188.0f, tw = th * 1024 / 270.0f;

	if ( logo ) {
		CG_DrawPic( x - tw * 188 / 1024.0f, y - th * 46 / 270.0f, tw, th, logo );
	}
}



/*
===============
CG_DrawChar

Coordinates and size in 640*480 virtual screen size
===============
*/
/*
===============
CG_DrawGlyph

One glyph of a TrueType font (FIX-11) at a baseline, in virtual units:
xs and ys the font pixels to virtual units across and down
===============
*/
static void CG_DrawGlyph( const fontInfo_t *font, float x, float baseline, float xs, float ys, int ch ) {
	const glyphInfo_t	*g = &font->glyphs[ch & 255];
	float				ax, ay, aw, ah;

	if ( !g->glyph || !g->imageWidth ) {
		return;
	}
	ax = x;
	ay = baseline - g->top * ys;
	aw = g->imageWidth * xs;
	ah = g->imageHeight * ys;
	CG_AdjustFrom640( &ax, &ay, &aw, &ah );
	trap_R_DrawStretchPic( ax, ay, aw, ah, g->s, g->t, g->s2, g->t2, g->glyph );
}

static float CG_PropFontWidth( const fontInfo_t *font, const char *str, float height );
static void CG_DrawPropFont( const fontInfo_t *font, float x, float y, const char *str, float height, const float *color );

/*
===============
CG_FontReady
===============
*/
static qboolean CG_FontReady( const fontInfo_t *font ) {
	return font->glyphs['A'].glyph != 0;
}

const fontInfo_t	*cg_cellFont;		// the font for cells drawn now (NULL: cgs.fontMono)

void CG_DrawChar( int x, int y, int width, int height, int ch ) {
	int row, col;
	float frow, fcol;
	float size;
	float	ax, ay, aw, ah;
	const fontInfo_t	*font = cg_cellFont && CG_FontReady( cg_cellFont ) ? cg_cellFont : &cgs.fontMono;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	// Roboto Mono in the cell: capitals about three quarters of its height,
	// the advance squeezed to the cell's width
	// (control and symbol codes, and glyphs the font lacks, stay bitmap)
	if ( CG_FontReady( font ) && ( ( ch >= 32 && ch < 127 ) || ch >= 160 )
		&& font->glyphs[ch].imageWidth > 0 ) {
		float	ys = height * 1.05f / 48.0f;
		float	xs = (float)width / font->glyphs[ch].xSkip;

		if ( xs > ys ) {
			xs = ys;
		}
		CG_DrawGlyph( font, x + ( width - font->glyphs[ch].xSkip * xs ) * 0.5f,
			y + height * 0.84f, xs, ys, ch );
		return;
	}

	ax = x;
	ay = y;
	aw = width;
	ah = height;
	CG_AdjustFrom640( &ax, &ay, &aw, &ah );

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	trap_R_DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cgs.media.charsetShader );
}


// set while drawing chat: ^4 and ^1 draw as the palette blue and red
qboolean	cg_drawTeamPalette;

/*
==================
CG_DrawStringExt

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void CG_DrawStringExt( int x, int y, const char *string, const float *setColor, 
		qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars ) {
	vec4_t		color;
	const char	*s;
	int			xx;
	int			cnt;

	if (maxChars <= 0)
		maxChars = 32767; // do them all!

	{
		int		len = CG_DrawStrlen( string );

		CG_LayoutRecord( 's', x, y, ( len < maxChars ? len : maxChars ) * charWidth, charHeight, string );
	}

	// draw the drop shadow
	if (shadow) {
		color[0] = color[1] = color[2] = 0;
		color[3] = setColor[3];
		trap_R_SetColor( color );
		s = string;
		xx = x;
		cnt = 0;
		while ( *s && cnt < maxChars) {
			if ( Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			CG_DrawChar( xx + 2, y + 2, charWidth, charHeight, *s );
			cnt++;
			xx += charWidth;
			s++;
		}
	}

	// draw the colored text
	s = string;
	xx = x;
	cnt = 0;
	trap_R_SetColor( setColor );
	while ( *s && cnt < maxChars) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
				if ( cg_drawTeamPalette && ColorIndex( *( s + 1 ) ) == ColorIndex( COLOR_BLUE ) ) {
					// hud.md palette blue #0099FF for team names in chat
					color[0] = 0; color[1] = 0.6f; color[2] = 1;
				} else if ( cg_drawTeamPalette && ColorIndex( *( s + 1 ) ) == ColorIndex( COLOR_RED ) ) {
					// and the palette red #FF3838
					color[0] = 1; color[1] = 0.22f; color[2] = 0.22f;
				}
				color[3] = setColor[3];
				trap_R_SetColor( color );
			}
			s += 2;
			continue;
		}
		CG_DrawChar( xx, y, charWidth, charHeight, *s );
		xx += charWidth;
		cnt++;
		s++;
	}
	trap_R_SetColor( NULL );
}

void CG_DrawBigString( int x, int y, const char *s, float alpha ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	CG_DrawStringExt( x, y, s, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
}

void CG_DrawBigStringColor( int x, int y, const char *s, vec4_t color ) {
	CG_DrawStringExt( x, y, s, color, qtrue, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
}

void CG_DrawSmallString( int x, int y, const char *s, float alpha ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	CG_DrawStringExt( x, y, s, color, qfalse, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

void CG_DrawSmallStringColor( int x, int y, const char *s, vec4_t color ) {
	CG_DrawStringExt( x, y, s, color, qtrue, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

/*
=================
CG_DrawStrlen

Returns character count, skiping color escape codes
=================
*/
int CG_DrawStrlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
=============
CG_TileClearBox

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
static void CG_TileClearBox( int x, int y, int w, int h, qhandle_t hShader ) {
	float	s1, t1, s2, t2;

	s1 = x/64.0;
	t1 = y/64.0;
	s2 = (x+w)/64.0;
	t2 = (y+h)/64.0;
	trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader );
}



/*
==============
CG_TileClear

Clear around a sized down screen
==============
*/
void CG_TileClear( void ) {
	int		top, bottom, left, right;
	int		w, h;

	w = cgs.glconfig.vidWidth;
	h = cgs.glconfig.vidHeight;

	if ( cg.refdef.x == 0 && cg.refdef.y == 0 && 
		cg.refdef.width == w && cg.refdef.height == h ) {
		return;		// full screen rendering
	}

	top = cg.refdef.y;
	bottom = top + cg.refdef.height-1;
	left = cg.refdef.x;
	right = left + cg.refdef.width-1;

	// clear above view screen
	CG_TileClearBox( 0, 0, w, top, cgs.media.backTileShader );

	// clear below view screen
	CG_TileClearBox( 0, bottom, w, h - bottom, cgs.media.backTileShader );

	// clear left of view screen
	CG_TileClearBox( 0, top, left, bottom - top + 1, cgs.media.backTileShader );

	// clear right of view screen
	CG_TileClearBox( right, top, w - right, bottom - top + 1, cgs.media.backTileShader );
}



/*
================
CG_FadeColor
================
*/
float *CG_FadeColor( int startMsec, int totalMsec ) {
	static vec4_t		color;
	int			t;

	if ( startMsec == 0 ) {
		return NULL;
	}

	t = cg.time - startMsec;

	if ( t >= totalMsec ) {
		return NULL;
	}

	// fade out
	if ( totalMsec - t < FADE_TIME ) {
		color[3] = ( totalMsec - t ) * 1.0/FADE_TIME;
	} else {
		color[3] = 1.0;
	}
	color[0] = color[1] = color[2] = 1;

	return color;
}


/*
================
CG_TeamColor
================
*/
/* Player identity is relative while playing/following. A free camera keeps
 * actual red/blue ownership. Objective/flag code still uses actual teams. */
int CG_IdentityTeam( int client, int team ) {
	int view, mine;
	if ( !cg_identityColors.integer || !cg.snap ) {
		return team;
	}
	view = cg.snap->ps.clientNum;
	mine = cg.snap->ps.persistant[PERS_TEAM];
	if ( team == TEAM_SPECTATOR ) {
		return TEAM_SPECTATOR;
	}
	if ( mine == TEAM_SPECTATOR ) {
		if ( cg.snap->ps.pm_flags & PMF_FOLLOW ) {
			if ( view >= 0 && view < MAX_CLIENTS ) {
				mine = cgs.clientinfo[view].team;
			}
		} else {
			/* Duel has no team IDs: keep the first active client blue in
			 * free view, independent of which direction the camera looks. */
			if ( cgs.gametype == GT_TOURNAMENT && client >= 0 && client < MAX_CLIENTS ) {
				int i;
				for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
					if ( cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team != TEAM_SPECTATOR ) {
						return client == i ? TEAM_BLUE : TEAM_RED;
					}
				}
			}
			return team;
		}
	}
	if ( cgs.gametype >= GT_TEAM ) {
		return team == TEAM_RED || team == TEAM_BLUE
			? ( team == mine ? TEAM_BLUE : TEAM_RED ) : team;
	}
	return client >= 0 && client < MAX_CLIENTS ? ( client == view ? TEAM_BLUE : TEAM_RED ) : TEAM_FREE;
}

void CG_IdentityColor( int client, int team, vec4_t out ) {
	int side = CG_IdentityTeam( client, team );
	VectorSet( out, 1, 1, 1 );
	out[3] = 1;
	if ( side == TEAM_RED || side == TEAM_BLUE ) {
		CG_TeamPalette( side, out );
	} else if ( side == TEAM_SPECTATOR ) {
		VectorSet( out, 0.65f, 0.65f, 0.65f );
	}
}

float *CG_TeamColor( int team ) {
	static vec4_t	red = {1, 0.2f, 0.2f, 1};
	static vec4_t	blue = {0.2f, 0.2f, 1, 1};
	static vec4_t	other = {1, 1, 1, 1};
	static vec4_t	spectator = {0.7f, 0.7f, 0.7f, 1};

	switch ( team ) {
	case TEAM_RED:
		return red;
	case TEAM_BLUE:
		return blue;
	case TEAM_SPECTATOR:
		return spectator;
	default:
		return other;
	}
}



/*
=================
CG_HealthPalette

The HUD's colour for a low (level 0), middling (1) or healthy (2) value:
red and yellow, or with cg_colorBlind 1 or 2 (deuteranopia, protanopia)
blue and yellow, with 3 (tritanopia) red and cyan; healthy is white
=================
*/
void CG_HealthPalette( int level, vec4_t out ) {
	static const float	palettes[4][2][3] = {
		{ { 1, 0.2f, 0.2f }, { 1, 0.85f, 0.2f } },
		{ { 0.25f, 0.55f, 1 }, { 1, 0.8f, 0.1f } },
		{ { 0.25f, 0.55f, 1 }, { 1, 0.8f, 0.1f } },
		{ { 1, 0.15f, 0.2f }, { 0.2f, 0.9f, 0.9f } }
	};
	int		p = cg_colorBlind.integer >= 0 && cg_colorBlind.integer <= 3 ? cg_colorBlind.integer : 0;

	out[3] = 1.0f;
	if ( level >= 2 ) {
		VectorSet( out, 1, 1, 1 );
		return;
	}
	VectorCopy( palettes[p][level], out );
}

/*
=================
CG_GetColorForHealth
=================
*/
void CG_GetColorForHealth( int health, int armor, vec4_t hcolor ) {
	int		count;
	int		max;

	// calculate the total points of damage that can
	// be sustained at the current health / armor level
	if ( health <= 0 ) {
		VectorClear( hcolor );	// black
		hcolor[3] = 1;
		return;
	}
	count = armor;
	max = health * ARMOR_PROTECTION / ( 1.0 - ARMOR_PROTECTION );
	if ( max < count ) {
		count = max;
	}
	health += count;

	// colour-blind palettes in three steps
	if ( cg_colorBlind.integer > 0 ) {
		CG_HealthPalette( health < 30 ? 0 : health < 66 ? 1 : 2, hcolor );
		return;
	}

	// set the color based on health
	hcolor[0] = 1.0;
	hcolor[3] = 1.0;
	if ( health >= 100 ) {
		hcolor[2] = 1.0;
	} else if ( health < 66 ) {
		hcolor[2] = 0;
	} else {
		hcolor[2] = ( health - 66 ) / 33.0;
	}

	if ( health > 60 ) {
		hcolor[1] = 1.0;
	} else if ( health < 30 ) {
		hcolor[1] = 0;
	} else {
		hcolor[1] = ( health - 30 ) / 30.0;
	}
}

/*
=================
CG_ColorForHealth
=================
*/
void CG_ColorForHealth( vec4_t hcolor ) {

	CG_GetColorForHealth( cg.snap->ps.stats[STAT_HEALTH], 
		cg.snap->ps.stats[STAT_ARMOR], hcolor );
}


/*
=================
UI_DrawProportionalString2
=================
*/
static int	propMap[128][3] = {
{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},

{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},

{0, 0, PROP_SPACE_WIDTH},		// SPACE
{11, 122, 7},	// !
{154, 181, 14},	// "
{55, 122, 17},	// #
{79, 122, 18},	// $
{101, 122, 23},	// %
{153, 122, 18},	// &
{9, 93, 7},		// '
{207, 122, 8},	// (
{230, 122, 9},	// )
{177, 122, 18},	// *
{30, 152, 18},	// +
{85, 181, 7},	// ,
{34, 93, 11},	// -
{110, 181, 6},	// .
{130, 152, 14},	// /

{22, 64, 17},	// 0
{41, 64, 12},	// 1
{58, 64, 17},	// 2
{78, 64, 18},	// 3
{98, 64, 19},	// 4
{120, 64, 18},	// 5
{141, 64, 18},	// 6
{204, 64, 16},	// 7
{162, 64, 17},	// 8
{182, 64, 18},	// 9
{59, 181, 7},	// :
{35,181, 7},	// ;
{203, 152, 14},	// <
{56, 93, 14},	// =
{228, 152, 14},	// >
{177, 181, 18},	// ?

{28, 122, 22},	// @
{5, 4, 18},		// A
{27, 4, 18},	// B
{48, 4, 18},	// C
{69, 4, 17},	// D
{90, 4, 13},	// E
{106, 4, 13},	// F
{121, 4, 18},	// G
{143, 4, 17},	// H
{164, 4, 8},	// I
{175, 4, 16},	// J
{195, 4, 18},	// K
{216, 4, 12},	// L
{230, 4, 23},	// M
{6, 34, 18},	// N
{27, 34, 18},	// O

{48, 34, 18},	// P
{68, 34, 18},	// Q
{90, 34, 17},	// R
{110, 34, 18},	// S
{130, 34, 14},	// T
{146, 34, 18},	// U
{166, 34, 19},	// V
{185, 34, 29},	// W
{215, 34, 18},	// X
{234, 34, 18},	// Y
{5, 64, 14},	// Z
{60, 152, 7},	// [
{106, 151, 13},	// '\'
{83, 152, 7},	// ]
{128, 122, 17},	// ^
{4, 152, 21},	// _

{134, 181, 5},	// '
{5, 4, 18},		// A
{27, 4, 18},	// B
{48, 4, 18},	// C
{69, 4, 17},	// D
{90, 4, 13},	// E
{106, 4, 13},	// F
{121, 4, 18},	// G
{143, 4, 17},	// H
{164, 4, 8},	// I
{175, 4, 16},	// J
{195, 4, 18},	// K
{216, 4, 12},	// L
{230, 4, 23},	// M
{6, 34, 18},	// N
{27, 34, 18},	// O

{48, 34, 18},	// P
{68, 34, 18},	// Q
{90, 34, 17},	// R
{110, 34, 18},	// S
{130, 34, 14},	// T
{146, 34, 18},	// U
{166, 34, 19},	// V
{185, 34, 29},	// W
{215, 34, 18},	// X
{234, 34, 18},	// Y
{5, 64, 14},	// Z
{153, 152, 13},	// {
{11, 181, 5},	// |
{180, 152, 13},	// }
{79, 93, 17},	// ~
{0, 0, -1}		// DEL
};

static int propMapB[26][3] = {
{11, 12, 33},
{49, 12, 31},
{85, 12, 31},
{120, 12, 30},
{156, 12, 21},
{183, 12, 21},
{207, 12, 32},

{13, 55, 30},
{49, 55, 13},
{66, 55, 29},
{101, 55, 31},
{135, 55, 21},
{158, 55, 40},
{204, 55, 32},

{12, 97, 31},
{48, 97, 31},
{82, 97, 30},
{118, 97, 30},
{153, 97, 30},
{185, 97, 25},
{213, 97, 30},

{11, 139, 32},
{42, 139, 51},
{93, 139, 32},
{126, 139, 31},
{158, 139, 25},
};

#define PROPB_GAP_WIDTH		4
#define PROPB_SPACE_WIDTH	12
#define PROPB_HEIGHT		36

/*
=================
UI_DrawBannerString
=================
*/
static void UI_DrawBannerString2( int x, int y, const char* str, vec4_t color )
{
	const char* s;
	unsigned char	ch;
	float	ax;
	float	ay;
	float	aw;
	float	ah;
	float	frow;
	float	fcol;
	float	fwidth;
	float	fheight;

	if ( CG_FontReady( &cgs.fontBanner ) ) {
		CG_DrawPropFont( &cgs.fontBanner, x, y, str, PROPB_HEIGHT, color );
		return;
	}

	// draw the colored text
	trap_R_SetColor( color );
	
	ax = x * cgs.screenXScale + ( cg_hudAnchor == ANCHOR_LEFT ? 0
		: cg_hudAnchor == ANCHOR_RIGHT ? 2 * cgs.screenXBias : cgs.screenXBias );
	ay = y * cgs.screenYScale;

	s = str;
	while ( *s )
	{
		ch = *s & 127;
		if ( ch == ' ' ) {
			ax += ((float)PROPB_SPACE_WIDTH + (float)PROPB_GAP_WIDTH)* cgs.screenXScale;
		}
		else if ( ch >= 'A' && ch <= 'Z' ) {
			ch -= 'A';
			fcol = (float)propMapB[ch][0] / 256.0f;
			frow = (float)propMapB[ch][1] / 256.0f;
			fwidth = (float)propMapB[ch][2] / 256.0f;
			fheight = (float)PROPB_HEIGHT / 256.0f;
			aw = (float)propMapB[ch][2] * cgs.screenXScale;
			ah = (float)PROPB_HEIGHT * cgs.screenYScale;
			trap_R_DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol+fwidth, frow+fheight, cgs.media.charsetPropB );
			ax += (aw + (float)PROPB_GAP_WIDTH * cgs.screenXScale);
		}
		s++;
	}

	trap_R_SetColor( NULL );
}

void UI_DrawBannerString( int x, int y, const char* str, int style, vec4_t color ) {
	const char *	s;
	int				ch;
	int				width;
	vec4_t			drawcolor;

	// find the width of the drawn text
	s = str;
	width = 0;
	if ( CG_FontReady( &cgs.fontBanner ) ) {
		width = CG_PropFontWidth( &cgs.fontBanner, str, PROPB_HEIGHT ) + PROPB_GAP_WIDTH;
		s = "";
	}
	while ( *s ) {
		ch = *s;
		if ( ch == ' ' ) {
			width += PROPB_SPACE_WIDTH;
		}
		else if ( ch >= 'A' && ch <= 'Z' ) {
			width += propMapB[ch - 'A'][2] + PROPB_GAP_WIDTH;
		}
		s++;
	}
	width -= PROPB_GAP_WIDTH;

	switch( style & UI_FORMATMASK ) {
		case UI_CENTER:
			x -= width / 2;
			break;

		case UI_RIGHT:
			x -= width;
			break;

		case UI_LEFT:
		default:
			break;
	}

	if ( style & UI_DROPSHADOW ) {
		drawcolor[0] = drawcolor[1] = drawcolor[2] = 0;
		drawcolor[3] = color[3];
		UI_DrawBannerString2( x+2, y+2, str, drawcolor );
	}

	UI_DrawBannerString2( x, y, str, color );
}


/*
=================
CG_PropFontWidth / CG_DrawPropFont

The proportional strings in Roboto (FIX-11): a line height of height
virtual units, the capitals about three quarters of it
=================
*/
static float CG_PropFontWidth( const fontInfo_t *font, const char *str, float height ) {
	float	scale = height * 1.0f / 48.0f, w = 0;

	for ( ; *str ; str++ ) {
		if ( Q_IsColorString( str ) ) {
			str++;
			continue;
		}
		w += font->glyphs[*str & 255].xSkip * scale;
	}
	return w;
}

static void CG_DrawPropFont( const fontInfo_t *font, float x, float y, const char *str, float height, const float *color ) {
	float	scale = height * 1.0f / 48.0f;

	trap_R_SetColor( color );
	for ( ; *str ; str++ ) {
		if ( Q_IsColorString( str ) ) {
			str++;
			continue;
		}
		CG_DrawGlyph( font, x, y + height * 0.82f, scale, scale, *str );
		x += font->glyphs[*str & 255].xSkip * scale;
	}
	trap_R_SetColor( NULL );
}

int UI_ProportionalStringWidth( const char* str ) {
	const char *	s;
	int				ch;
	int				charWidth;
	int				width;

	if ( CG_FontReady( &cgs.fontProp ) ) {
		return CG_PropFontWidth( &cgs.fontProp, str, PROP_HEIGHT );
	}

	s = str;
	width = 0;
	while ( *s ) {
		ch = *s & 127;
		charWidth = propMap[ch][2];
		if ( charWidth != -1 ) {
			width += charWidth;
			width += PROP_GAP_WIDTH;
		}
		s++;
	}

	width -= PROP_GAP_WIDTH;
	return width;
}

static void UI_DrawProportionalString2( int x, int y, const char* str, vec4_t color, float sizeScale, qhandle_t charset )
{
	const char* s;
	unsigned char	ch;
	float	ax;
	float	ay;
	float	aw;
	float	ah;
	float	frow;
	float	fcol;
	float	fwidth;
	float	fheight;

	if ( CG_FontReady( &cgs.fontProp ) ) {
		CG_DrawPropFont( &cgs.fontProp, x, y, str, PROP_HEIGHT * sizeScale, color );
		return;
	}

	// draw the colored text
	trap_R_SetColor( color );
	
	ax = x * cgs.screenXScale + ( cg_hudAnchor == ANCHOR_LEFT ? 0
		: cg_hudAnchor == ANCHOR_RIGHT ? 2 * cgs.screenXBias : cgs.screenXBias );
	ay = y * cgs.screenYScale;

	s = str;
	while ( *s )
	{
		ch = *s & 127;
		if ( ch == ' ' ) {
			aw = (float)PROP_SPACE_WIDTH * cgs.screenXScale * sizeScale;
		} else if ( propMap[ch][2] != -1 ) {
			fcol = (float)propMap[ch][0] / 256.0f;
			frow = (float)propMap[ch][1] / 256.0f;
			fwidth = (float)propMap[ch][2] / 256.0f;
			fheight = (float)PROP_HEIGHT / 256.0f;
			aw = (float)propMap[ch][2] * cgs.screenXScale * sizeScale;
			ah = (float)PROP_HEIGHT * cgs.screenYScale * sizeScale;
			trap_R_DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol+fwidth, frow+fheight, charset );
		} else {
			aw = 0;
		}

		ax += (aw + (float)PROP_GAP_WIDTH * cgs.screenXScale * sizeScale);
		s++;
	}

	trap_R_SetColor( NULL );
}

/*
=================
UI_ProportionalSizeScale
=================
*/
float UI_ProportionalSizeScale( int style ) {
	if(  style & UI_SMALLFONT ) {
		return 0.75;
	}

	return 1.00;
}


/*
=================
UI_DrawProportionalString
=================
*/
void UI_DrawProportionalString( int x, int y, const char* str, int style, vec4_t color ) {
	vec4_t	drawcolor;
	int		width;
	float	sizeScale;

	sizeScale = UI_ProportionalSizeScale( style );

	switch( style & UI_FORMATMASK ) {
		case UI_CENTER:
			width = UI_ProportionalStringWidth( str ) * sizeScale;
			x -= width / 2;
			break;

		case UI_RIGHT:
			width = UI_ProportionalStringWidth( str ) * sizeScale;
			x -= width;
			break;

		case UI_LEFT:
		default:
			break;
	}

	if ( style & UI_DROPSHADOW ) {
		drawcolor[0] = drawcolor[1] = drawcolor[2] = 0;
		drawcolor[3] = color[3];
		UI_DrawProportionalString2( x+2, y+2, str, drawcolor, sizeScale, cgs.media.charsetProp );
	}

	if ( style & UI_INVERSE ) {
		drawcolor[0] = color[0] * 0.8;
		drawcolor[1] = color[1] * 0.8;
		drawcolor[2] = color[2] * 0.8;
		drawcolor[3] = color[3];
		UI_DrawProportionalString2( x, y, str, drawcolor, sizeScale, cgs.media.charsetProp );
		return;
	}

	if ( style & UI_PULSE ) {
		drawcolor[0] = color[0] * 0.8;
		drawcolor[1] = color[1] * 0.8;
		drawcolor[2] = color[2] * 0.8;
		drawcolor[3] = color[3];
		UI_DrawProportionalString2( x, y, str, color, sizeScale, cgs.media.charsetProp );

		drawcolor[0] = color[0];
		drawcolor[1] = color[1];
		drawcolor[2] = color[2];
		drawcolor[3] = 0.5 + 0.5 * sin( cg.time / PULSE_DIVISOR );
		UI_DrawProportionalString2( x, y, str, drawcolor, sizeScale, cgs.media.charsetPropGlow );
		return;
	}

	UI_DrawProportionalString2( x, y, str, color, sizeScale, cgs.media.charsetProp );
}
