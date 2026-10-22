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
// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

#include "cg_local.h"
#include "../client/keycodes.h"

#ifdef MISSIONPACK
#include "../ui/ui_shared.h"

// used for scoreboard
extern displayContextDef_t cgDC;
menuDef_t *menuScoreboard = NULL;
#else
int drawTeamOverlayModificationCount = -1;
#endif

int sortedTeamPlayers[TEAM_MAXOVERLAY];
int	numSortedTeamPlayers;

char systemChat[256];
char teamChat1[256];
char teamChat2[256];

/*
==============================================================================

HUD LAYOUTS (hud.md section 7)

A layout is data: hud/<cg_hudLayout>.hud lists elements as
	name  left|centre|right  top|bottom  x  y  [scale]
with offsets in design pixels (1080 per screen height). "modern" is the
default; "classic" keeps the earlier HUD (cg_hudStyle 1) and its places.
Elements the file leaves out keep the modern defaults below.

==============================================================================
*/

typedef struct {
	const char	*name;
	int			anchorX;	// ANCHOR_LEFT, ANCHOR_CENTER, ANCHOR_RIGHT
	qboolean	fromBottom;
	float		x, y;		// design pixels from the anchor
	float		scale;
} hudElement_t;

static const hudElement_t	hudDefaults[] = {
	{ "portrait",		ANCHOR_LEFT,	qtrue,	240, 80, 1 },
	{ "armor",			ANCHOR_LEFT,	qtrue,	209, 64, 1 },
	{ "health",			ANCHOR_LEFT,	qtrue,	236, 154, 1 },
	{ "follow",			ANCHOR_LEFT,	qtrue,	142, 250, 1 },
	{ "ability",		ANCHOR_CENTER,	qtrue,	0, 160, 1 },
	{ "abilitykey",		ANCHOR_CENTER,	qtrue,	0, 69, 1 },
	{ "abilityname",	ANCHOR_CENTER,	qtrue,	0, 43, 1 },
	{ "ammo",			ANCHOR_RIGHT,	qtrue,	103, 95, 1 },
	{ "weaponbar",		ANCHOR_RIGHT,	qtrue,	30, 215, 1 },
	{ "clock",			ANCHOR_CENTER,	qfalse,	0, 38, 1 },
	{ "clockline",		ANCHOR_CENTER,	qfalse,	0, 97, 1 },
	{ "killfeed",		ANCHOR_LEFT,	qfalse,	50, 150, 1 },
	{ "pickups",		ANCHOR_RIGHT,	qfalse,	50, 150, 1 },
	{ "medals",			ANCHOR_CENTER,	qfalse,	0, 392, 1 },		// the row's bottom edge
	{ "fragline",		ANCHOR_CENTER,	qfalse,	0, 412, 1 },
	{ "eventline",		ANCHOR_CENTER,	qfalse,	0, 200, 1 },
	{ "bannerword",		ANCHOR_CENTER,	qfalse,	0, 278, 1 },
	{ "chat",			ANCHOR_LEFT,	qtrue,	10, 308, 1 },
	{ "teamoverlay",	ANCHOR_LEFT,	qfalse,	10, 340, 1 },
	{ "passive",		ANCHOR_LEFT,	qtrue,	142, 110, 1 },
	{ "warmup",			ANCHOR_CENTER,	qfalse,	0, 249, 1 },
	{ "countdown",		ANCHOR_CENTER,	qfalse,	0, 260, 1 }
};

static hudElement_t	hudElements[ARRAY_LEN( hudDefaults )];
static char	hudLayoutLoaded[32];
static qboolean CG_BannerWordShowing( void );

#define DP	( 480.0f / 1080.0f )	// design pixels to the 640 x 480 frame

/*
=================
CG_HudLayout

Reads hud/<cg_hudLayout>.hud when the setting changes
=================
*/
static void CG_HudLayout( void ) {
	char			buf[4096], name[32], ax[16], ay[16], file[MAX_QPATH];
	char			*p, *line, *tok;
	fileHandle_t	f;
	int				len, i;
	float			x, y, scale;

	if ( hudLayoutLoaded[0] && !Q_strncmp( hudLayoutLoaded, cg_hudLayout.string, sizeof( hudLayoutLoaded ) ) ) {
		return;
	}
	Q_strncpyz( hudLayoutLoaded, cg_hudLayout.string, sizeof( hudLayoutLoaded ) );
	// every load starts from the defaults
	memcpy( hudElements, hudDefaults, sizeof( hudElements ) );
	if ( strlen( cg_hudLayout.string ) >= sizeof( hudLayoutLoaded ) ) {
		CG_Printf( S_COLOR_YELLOW "cg_hudLayout \"%s\" is too long; using modern\n", cg_hudLayout.string );
		return;
	}
	Com_sprintf( file, sizeof( file ), "hud/%s.hud", cg_hudLayout.string );
	len = trap_FS_FOpenFile( file, &f, FS_READ );
	if ( !f ) {
		if ( Q_stricmp( cg_hudLayout.string, "modern" ) ) {
			CG_Printf( S_COLOR_YELLOW "No %s; using modern\n", file );
		}
		return;
	}
	if ( len > (int)sizeof( buf ) - 1 ) {
		len = sizeof( buf ) - 1;
	}
	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );
	for ( line = buf ; line && *line ; line = p ) {
		p = strchr( line, '\n' );
		if ( p ) {
			*p++ = 0;
		}
		if ( line[0] == '/' || line[0] == '#' ) {
			continue;
		}
		// name anchorx anchory x y [scale]
		tok = COM_Parse( &line );
		if ( !tok[0] ) {
			continue;
		}
		Q_strncpyz( name, tok, sizeof( name ) );
		Q_strncpyz( ax, COM_Parse( &line ), sizeof( ax ) );
		Q_strncpyz( ay, COM_Parse( &line ), sizeof( ay ) );
		tok = COM_Parse( &line );
		if ( !tok[0] ) {
			continue;
		}
		x = atof( tok );
		tok = COM_Parse( &line );
		if ( !tok[0] ) {
			continue;
		}
		y = atof( tok );
		tok = COM_Parse( &line );
		scale = tok[0] ? atof( tok ) : 1;
		for ( i = 0 ; i < ARRAY_LEN( hudElements ) ; i++ ) {
			if ( !Q_stricmp( hudElements[i].name, name ) ) {
				hudElements[i].anchorX = !Q_stricmp( ax, "left" ) ? ANCHOR_LEFT : !Q_stricmp( ax, "right" ) ? ANCHOR_RIGHT : ANCHOR_CENTER;
				hudElements[i].fromBottom = !Q_stricmp( ay, "bottom" );
				hudElements[i].x = x;
				hudElements[i].y = y;
				hudElements[i].scale = scale > 0 ? scale : 1;
			}
		}
	}
}

/*
=================
CG_HudPlace

An element's point in the 640 x 480 frame; sets cg_hudAnchor to match.
x grows inward from a side anchor, y downward from the top or upward
from the bottom
=================
*/
static float CG_HudPlace( const char *name, float *x, float *y ) {
	int		i;

	if ( !hudLayoutLoaded[0] ) {
		CG_HudLayout();
	}

	for ( i = 0 ; i < ARRAY_LEN( hudElements ) ; i++ ) {
		hudElement_t	*e = &hudElements[i];

		if ( Q_stricmp( e->name, name ) ) {
			continue;
		}
		cg_hudAnchor = e->anchorX;
		*x = e->anchorX == ANCHOR_LEFT ? e->x * DP : e->anchorX == ANCHOR_RIGHT ? 640 - e->x * DP : 320 + e->x * DP;
		*y = e->fromBottom ? 480 - e->y * DP : e->y * DP;
		return e->scale;
	}
	cg_hudAnchor = ANCHOR_CENTER;
	*x = 320;
	*y = 240;
	return 1;
}

/*
=================
CG_HudModern

Whether the modern layout draws the HUD (cg_hudLayout other than classic)
=================
*/
qboolean CG_HudModern( void ) {
	return Q_stricmp( cg_hudLayout.string, "classic" ) != 0;
}


#ifdef MISSIONPACK

int CG_Text_Width(const char *text, float scale, int limit) {
  int count,len;
	float out;
	glyphInfo_t *glyph;
	float useScale;
	const char *s = text;
	fontInfo_t *font = &cgDC.Assets.textFont;
	if (scale <= cg_smallFont.value) {
		font = &cgDC.Assets.smallFont;
	} else if (scale > cg_bigFont.value) {
		font = &cgDC.Assets.bigFont;
	}
	useScale = scale * font->glyphScale;
  out = 0;
  if (text) {
    len = strlen(text);
		if (limit > 0 && len > limit) {
			len = limit;
		}
		count = 0;
		while (s && *s && count < len) {
			if ( Q_IsColorString(s) ) {
				s += 2;
				continue;
			} else {
				glyph = &font->glyphs[*s & 255];
				out += glyph->xSkip;
				s++;
				count++;
			}
    }
  }
  return out * useScale;
}

int CG_Text_Height(const char *text, float scale, int limit) {
  int len, count;
	float max;
	glyphInfo_t *glyph;
	float useScale;
	const char *s = text;
	fontInfo_t *font = &cgDC.Assets.textFont;
	if (scale <= cg_smallFont.value) {
		font = &cgDC.Assets.smallFont;
	} else if (scale > cg_bigFont.value) {
		font = &cgDC.Assets.bigFont;
	}
	useScale = scale * font->glyphScale;
  max = 0;
  if (text) {
    len = strlen(text);
		if (limit > 0 && len > limit) {
			len = limit;
		}
		count = 0;
		while (s && *s && count < len) {
			if ( Q_IsColorString(s) ) {
				s += 2;
				continue;
			} else {
				glyph = &font->glyphs[*s & 255];
	      if (max < glyph->height) {
		      max = glyph->height;
			  }
				s++;
				count++;
			}
    }
  }
  return max * useScale;
}

void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
  float w, h;
  w = width * scale;
  h = height * scale;
  CG_AdjustFrom640( &x, &y, &w, &h );
  trap_R_DrawStretchPic( x, y, w, h, s, t, s2, t2, hShader );
}

void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style) {
  int len, count;
	vec4_t newColor;
	glyphInfo_t *glyph;
	float useScale;
	fontInfo_t *font = &cgDC.Assets.textFont;
	if (scale <= cg_smallFont.value) {
		font = &cgDC.Assets.smallFont;
	} else if (scale > cg_bigFont.value) {
		font = &cgDC.Assets.bigFont;
	}
	useScale = scale * font->glyphScale;
  if (text) {
		const char *s = text;
		trap_R_SetColor( color );
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
    len = strlen(text);
		if (limit > 0 && len > limit) {
			len = limit;
		}
		count = 0;
		while (s && *s && count < len) {
			glyph = &font->glyphs[*s & 255];
      //int yadj = Assets.textFont.glyphs[text[i]].bottom + Assets.textFont.glyphs[text[i]].top;
      //float yadj = scale * (Assets.textFont.glyphs[text[i]].imageHeight - Assets.textFont.glyphs[text[i]].height);
			if ( Q_IsColorString( s ) ) {
				memcpy( newColor, g_color_table[ColorIndex(*(s+1))], sizeof( newColor ) );
				newColor[3] = color[3];
				trap_R_SetColor( newColor );
				s += 2;
				continue;
			} else {
				float yadj = useScale * glyph->top;
				if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
					int ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
					colorBlack[3] = newColor[3];
					trap_R_SetColor( colorBlack );
					CG_Text_PaintChar(x + ofs, y - yadj + ofs, 
														glyph->imageWidth,
														glyph->imageHeight,
														useScale, 
														glyph->s,
														glyph->t,
														glyph->s2,
														glyph->t2,
														glyph->glyph);
					colorBlack[3] = 1.0;
					trap_R_SetColor( newColor );
				}
				CG_Text_PaintChar(x, y - yadj, 
													glyph->imageWidth,
													glyph->imageHeight,
													useScale, 
													glyph->s,
													glyph->t,
													glyph->s2,
													glyph->t2,
													glyph->glyph);
				// CG_DrawPic(x, y - yadj, scale * cgDC.Assets.textFont.glyphs[text[i]].imageWidth, scale * cgDC.Assets.textFont.glyphs[text[i]].imageHeight, cgDC.Assets.textFont.glyphs[text[i]].glyph);
				x += (glyph->xSkip * useScale) + adjust;
				s++;
				count++;
			}
    }
	  trap_R_SetColor( NULL );
  }
}


#endif

/*
==============
CG_DrawField

Draws large numbers for status bar and powerups
==============
*/
#ifndef MISSIONPACK
static void CG_DrawField (int x, int y, int width, int value) {
	char	num[16], *ptr;
	int		l;
	int		frame;

	if ( width < 1 ) {
		return;
	}

	// draw number string
	if ( width > 5 ) {
		width = 5;
	}

	switch ( width ) {
	case 1:
		value = value > 9 ? 9 : value;
		value = value < 0 ? 0 : value;
		break;
	case 2:
		value = value > 99 ? 99 : value;
		value = value < -9 ? -9 : value;
		break;
	case 3:
		value = value > 999 ? 999 : value;
		value = value < -99 ? -99 : value;
		break;
	case 4:
		value = value > 9999 ? 9999 : value;
		value = value < -999 ? -999 : value;
		break;
	}

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += 2 + CHAR_WIDTH*(width - l);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		CG_DrawPic( x,y, CHAR_WIDTH, CHAR_HEIGHT, cgs.media.numberShaders[frame] );
		x += CHAR_WIDTH;
		ptr++;
		l--;
	}
}
#endif // MISSIONPACK

/*
================
CG_Draw3DModel

================
*/
void CG_Draw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles ) {
	refdef_t		refdef;
	refEntity_t		ent;

	if ( !cg_draw3dIcons.integer || !cg_drawIcons.integer ) {
		return;
	}

	CG_AdjustFrom640( &x, &y, &w, &h );

	memset( &refdef, 0, sizeof( refdef ) );

	memset( &ent, 0, sizeof( ent ) );
	AnglesToAxis( angles, ent.axis );
	VectorCopy( origin, ent.origin );
	ent.hModel = model;
	ent.customSkin = skin;
	ent.renderfx = RF_NOSHADOW;		// no stencil shadows

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	refdef.fov_x = 30;
	refdef.fov_y = 30;

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.time = cg.time;

	trap_R_ClearScene();
	trap_R_AddRefEntityToScene( &ent );
	trap_R_RenderScene( &refdef );
}

/*
================
CG_DrawHead

Used for both the status bar and the scoreboard
================
*/
void CG_DrawHead( float x, float y, float w, float h, int clientNum, vec3_t headAngles ) {
	clipHandle_t	cm;
	clientInfo_t	*ci;
	float			len;
	vec3_t			origin;
	vec3_t			mins, maxs;

	ci = &cgs.clientinfo[ clientNum ];

	if ( cg_draw3dIcons.integer ) {
		cm = ci->headModel;
		if ( !cm ) {
			return;
		}

		// offset the origin y and z to center the head
		trap_R_ModelBounds( cm, mins, maxs );

		origin[2] = -0.5 * ( mins[2] + maxs[2] );
		origin[1] = 0.5 * ( mins[1] + maxs[1] );

		// calculate distance so the head nearly fills the box
		// assume heads are taller than wide
		len = 0.7 * ( maxs[2] - mins[2] );		
		origin[0] = len / 0.268;	// len / tan( fov/2 )

		// allow per-model tweaking
		VectorAdd( origin, ci->headOffset, origin );

		CG_Draw3DModel( x, y, w, h, ci->headModel, ci->headSkin, origin, headAngles );
	} else if ( cg_drawIcons.integer ) {
		CG_DrawPic( x, y, w, h, ci->modelIcon );
	}

	// if they are deferred, draw a cross out
	if ( ci->deferred ) {
		CG_DrawPic( x, y, w, h, cgs.media.deferShader );
	}
}

/*
================
CG_DrawFlagModel

Used for both the status bar and the scoreboard
================
*/
void CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D ) {
	qhandle_t		cm;
	float			len;
	vec3_t			origin, angles;
	vec3_t			mins, maxs;
	qhandle_t		handle;

	if ( !force2D && cg_draw3dIcons.integer ) {

		VectorClear( angles );

		cm = cgs.media.redFlagModel;

		// offset the origin y and z to center the flag
		trap_R_ModelBounds( cm, mins, maxs );

		origin[2] = -0.5 * ( mins[2] + maxs[2] );
		origin[1] = 0.5 * ( mins[1] + maxs[1] );

		// calculate distance so the flag nearly fills the box
		// assume heads are taller than wide
		len = 0.5 * ( maxs[2] - mins[2] );		
		origin[0] = len / 0.268;	// len / tan( fov/2 )

		angles[YAW] = 60 * sin( cg.time / 2000.0 );;

		if( team == TEAM_RED ) {
			handle = cgs.media.redFlagModel;
		} else if( team == TEAM_BLUE ) {
			handle = cgs.media.blueFlagModel;
		} else if( team == TEAM_FREE ) {
			handle = cgs.media.neutralFlagModel;
		} else {
			return;
		}
		CG_Draw3DModel( x, y, w, h, handle, 0, origin, angles );
	} else if ( cg_drawIcons.integer ) {
		gitem_t *item;

		if( team == TEAM_RED ) {
			item = BG_FindItemForPowerup( PW_REDFLAG );
		} else if( team == TEAM_BLUE ) {
			item = BG_FindItemForPowerup( PW_BLUEFLAG );
		} else if( team == TEAM_FREE ) {
			item = BG_FindItemForPowerup( PW_NEUTRALFLAG );
		} else {
			return;
		}
		if (item) {
		  CG_DrawPic( x, y, w, h, cg_items[ ITEM_INDEX(item) ].icon );
		}
	}
}

/*
================
CG_DrawStatusBarHead

================
*/
#ifndef MISSIONPACK

static void CG_DrawStatusBarHead( float x ) {
	vec3_t		angles;
	float		size, stretch;
	float		frac;

	VectorClear( angles );

	if ( cg.damageTime && cg.time - cg.damageTime < DAMAGE_TIME ) {
		frac = (float)(cg.time - cg.damageTime ) / DAMAGE_TIME;
		size = ICON_SIZE * 1.25 * ( 1.5 - frac * 0.5 );

		stretch = size - ICON_SIZE * 1.25;
		// kick in the direction of damage
		x -= stretch * 0.5 + cg.damageX * stretch * 0.5;

		cg.headStartYaw = 180 + cg.damageX * 45;

		cg.headEndYaw = 180 + 20 * cos( crandom()*M_PI );
		cg.headEndPitch = 5 * cos( crandom()*M_PI );

		cg.headStartTime = cg.time;
		cg.headEndTime = cg.time + 100 + random() * 2000;
	} else {
		if ( cg.time >= cg.headEndTime ) {
			// select a new head angle
			cg.headStartYaw = cg.headEndYaw;
			cg.headStartPitch = cg.headEndPitch;
			cg.headStartTime = cg.headEndTime;
			cg.headEndTime = cg.time + 100 + random() * 2000;

			cg.headEndYaw = 180 + 20 * cos( crandom()*M_PI );
			cg.headEndPitch = 5 * cos( crandom()*M_PI );
		}

		size = ICON_SIZE * 1.25;
	}

	// if the server was frozen for a while we may have a bad head start time
	if ( cg.headStartTime > cg.time ) {
		cg.headStartTime = cg.time;
	}

	frac = ( cg.time - cg.headStartTime ) / (float)( cg.headEndTime - cg.headStartTime );
	frac = frac * frac * ( 3 - 2 * frac );
	angles[YAW] = cg.headStartYaw + ( cg.headEndYaw - cg.headStartYaw ) * frac;
	angles[PITCH] = cg.headStartPitch + ( cg.headEndPitch - cg.headStartPitch ) * frac;

	CG_DrawHead( x, 480 - size, size, size, 
				cg.snap->ps.clientNum, angles );
}
#endif // MISSIONPACK

#ifndef MISSIONPACK
/*
==============================================================================

THE HUD (cg_hudStyle 1)

Health left of the centre, the champion portrait with its ability ring in
the middle, armour to the right, ammo bottom right, a weapon bar down the
right edge, the clock and scores top centre and the kill feed top right
==============================================================================
*/

#define KILLFEED_QUEUE		16
#define KILLFEED_TIME		4000

typedef struct {
	char	killer[MAX_QPATH];
	char	victim[MAX_QPATH];
	int		weapon;			// icon, WP_NONE for none
	qhandle_t	killerPic, victimPic;	// champion portraits, 0 for none
	int		killerNum, victimNum;		// -1 for none
	int		killerTeam, victimTeam;
	qboolean	quad;				// the killer had Quad Damage
	int		time;
} killFeed_t;

static killFeed_t	killFeed[KILLFEED_QUEUE];
static int			killFeedHead;

/*
=================
CG_AddKillFeed
=================
*/
static qhandle_t CG_KillFeedPortrait( int clientNum ) {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || !cgs.clientinfo[clientNum].infoValid ) {
		return 0;
	}
	return CG_ChampionPortrait( BG_ChampionForModel( cgs.clientinfo[clientNum].modelName ), qtrue );
}

void CG_AddKillFeed( const char *killer, const char *victim, int weapon, int killerNum, int victimNum ) {
	killFeed_t	*k = &killFeed[killFeedHead++ % KILLFEED_QUEUE];

	Q_strncpyz( k->killer, killer, sizeof( k->killer ) );
	Q_strncpyz( k->victim, victim, sizeof( k->victim ) );
	k->weapon = weapon;
	k->killerPic = killer[0] ? CG_KillFeedPortrait( killerNum ) : 0;
	k->victimPic = CG_KillFeedPortrait( victimNum );
	k->killerNum = killer[0] ? killerNum : -1;
	k->victimNum = victimNum;
	k->killerTeam = killerNum >= 0 && killerNum < MAX_CLIENTS ? cgs.clientinfo[killerNum].team : TEAM_FREE;
	k->victimTeam = victimNum >= 0 && victimNum < MAX_CLIENTS ? cgs.clientinfo[victimNum].team : TEAM_FREE;
	// Quad: our own from the player state, others only while in the snapshot
	if ( killerNum == cg.snap->ps.clientNum ) {
		k->quad = cg.snap->ps.powerups[PW_QUAD] > 0;
	} else {
		k->quad = killerNum >= 0 && killerNum < MAX_CLIENTS && cg_entities[killerNum].currentValid
			&& ( cg_entities[killerNum].currentState.powerups & ( 1 << PW_QUAD ) );
	}
	k->time = cg.time;
}

/*
=================
CG_KillFeedName

A name in the feed: our team cyan, enemies red in team modes, white in
free-for-all (messages.md 2.1)
=================
*/
static void CG_KillFeedName( int x, int y, const char *name, int team, const vec4_t fade ) {
	vec4_t	c;

	Vector4Copy( fade, c );
	if ( cgs.gametype >= GT_TEAM && ( team == TEAM_RED || team == TEAM_BLUE ) ) {
		if ( team == cg.snap->ps.persistant[PERS_TEAM] ) {
			c[0] = 0.3f; c[1] = 0.9f; c[2] = 1;
		} else {
			c[0] = 1; c[1] = 0.3f; c[2] = 0.25f;
		}
		CG_DrawStringExt( x, y, name, c, qtrue, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
	} else {
		CG_DrawStringExt( x, y, name, c, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
	}
}

static void CG_DrawKillFeed( void ) {
	static qhandle_t	skull;
	killFeed_t	*k;
	int			i, n, rows = cg_obituaryRowSize.integer, y = 40, w, x, left, life, fadeTime, right;
	vec4_t		fade = { 1, 1, 1, 1 }, band;
	int			me = cg.snap->ps.clientNum;
	qboolean	modern;
	float		feedLeft = 0;

	if ( rows <= 0 || !cg_killFeed.integer ) {
		return;
	}
	if ( rows > 5 ) {
		rows = 5;
	}
	if ( !skull ) {
		skull = trap_R_RegisterShaderNoMip( "gfx/tremor/skull.tga" );
	}
	// the modern layout: top left, rows growing down, 2.5 s each with a
	// 0.3 s fade; the classic one: top right, 4 s with 0.5 s
	modern = CG_HudModern();
	life = modern ? 2500 : KILLFEED_TIME;
	fadeTime = modern ? 300 : 500;
	if ( modern ) {
		float	fx, fy;

		CG_HudPlace( "killfeed", &fx, &fy );
		feedLeft = fx;
		y = fy;
	}
	for ( i = 0, n = 0 ; i < KILLFEED_QUEUE && n < rows ; i++ ) {
		k = &killFeed[( killFeedHead - 1 - i + KILLFEED_QUEUE * 2 ) % KILLFEED_QUEUE];
		if ( !k->time || cg.time - k->time > life || k->time > cg.time ) {
			continue;
		}
		fade[3] = cg.time - k->time > life - fadeTime ? ( life - ( cg.time - k->time ) ) / (float)fadeTime : 1;

		// lay the row out right to left, then draw its band under it
		w = CG_DrawStrlen( k->victim ) * TINYCHAR_WIDTH + ( k->victimPic ? 18 : 0 ) + 20
			+ ( k->killer[0] ? CG_DrawStrlen( k->killer ) * TINYCHAR_WIDTH + 4 + ( k->killerPic ? 18 : 0 ) : 0 )
			+ ( k->quad ? 12 : 0 );
		right = modern ? feedLeft + w + ( k->victimNum == me ? 22 : 0 ) : 636;
		left = right - w;
		if ( k->killerNum == me && k->victimNum != me ) {
			band[0] = 0.45f; band[1] = 0.05f; band[2] = 0.04f; band[3] = 0.6f * fade[3];
			CG_FillRect( left - 4, y - 1, w + 6, 18, band );
		} else if ( k->victimNum == me ) {
			band[0] = band[1] = band[2] = 0.05f; band[3] = 0.7f * fade[3];
			CG_FillRect( left - 22, y - 1, w + 24, 18, band );
			if ( skull ) {
				trap_R_SetColor( fade );
				CG_DrawPic( left - 20, y, 16, 16, skull );
				trap_R_SetColor( NULL );
			}
		}

		x = right;
		w = CG_DrawStrlen( k->victim ) * TINYCHAR_WIDTH;
		x -= w;
		CG_KillFeedName( x, y + 4, k->victim, k->victimTeam, fade );
		trap_R_SetColor( fade );
		if ( k->victimPic ) {
			x -= 18;
			CG_DrawPic( x, y, 16, 16, k->victimPic );
		}
		trap_R_SetColor( NULL );
		x -= 20;
		trap_R_SetColor( fade );
		if ( k->weapon > WP_NONE && k->weapon < WP_NUM_WEAPONS && cg_weapons[k->weapon].weaponIcon ) {
			CG_DrawPic( x + 2, y, 16, 16, cg_weapons[k->weapon].weaponIcon );
		} else if ( skull ) {
			CG_DrawPic( x + 2, y, 16, 16, skull );		// suicide or the world
		}
		trap_R_SetColor( NULL );
		if ( k->quad && cgs.media.quadShader ) {
			x -= 12;
			CG_DrawStringExt( x + 2, y + 4, "Q", colorBlue, qtrue, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
		if ( k->killer[0] ) {
			w = CG_DrawStrlen( k->killer ) * TINYCHAR_WIDTH;
			x -= w + 4;
			CG_KillFeedName( x, y + 4, k->killer, k->killerTeam, fade );
			if ( k->killerPic ) {
				x -= 18;
				trap_R_SetColor( fade );
				CG_DrawPic( x, y, 16, 16, k->killerPic );
				trap_R_SetColor( NULL );
			}
		}
		y += 18;
		n++;
	}
}

/*
==============================================================================

CHAT BOX

Bottom left: the last cg_chatHistoryLength lines for a few seconds each;
holding +chathistory shows the whole history

==============================================================================
*/

#define CHAT_HISTORY		24
#define CHAT_TIME			7000
#define CHAT_WIDTH			56		// characters a line, wrapped at spaces
#define CHAT_Y				360		// bottom line

typedef struct {
	char	text[MAX_SAY_TEXT];
	int		time;
	qboolean	system;		// grey, shorter-lived: connects, team changes, votes
} chatLine_t;

static chatLine_t	chatLines[CHAT_HISTORY];
static int			chatHead;

/*
=================
CG_AddChatLine
=================
*/
static void CG_AddChatLine( const char *text, int len ) {
	chatLine_t	*c = &chatLines[chatHead++ % CHAT_HISTORY];

	if ( len >= (int)sizeof( c->text ) ) {
		len = sizeof( c->text ) - 1;
	}
	Q_strncpyz( c->text, text, len + 1 );
	c->time = cg.time;
	c->system = qfalse;
}

/*
=================
CG_AddSystemLine

A server notice in the chat area: grey, 6 s (messages.md 2.6)
=================
*/
#define SYSTEM_TIME		7000

void CG_AddSystemLine( const char *text ) {
	char	clean[MAX_SAY_TEXT];
	char	*p;

	Q_strncpyz( clean, text, sizeof( clean ) );
	Q_CleanStr( clean );
	while ( ( p = strchr( clean, '\n' ) ) != NULL ) {
		*p = ' ';
	}
	for ( p = clean + strlen( clean ) - 1 ; p >= clean && *p == ' ' ; p-- ) {
		*p = 0;
	}
	if ( !clean[0] || !cg_systemMessages.integer ) {
		return;
	}
	CG_AddChatLine( clean, strlen( clean ) );
	chatLines[( chatHead - 1 ) % CHAT_HISTORY].system = qtrue;
}

/*
=================
CG_AddChat

Wraps a chat message into lines; colour carries over to the next line
=================
*/
void CG_AddChat( const char *text ) {
	char		line[MAX_SAY_TEXT];
	const char	*p = text;
	int			visible, cut, i;
	char		lastColor = COLOR_WHITE;

	while ( *p ) {
		// find where CHAT_WIDTH visible characters end
		visible = 0;
		cut = 0;
		for ( i = 0 ; p[i] && visible < CHAT_WIDTH ; i++ ) {
			if ( Q_IsColorString( p + i ) ) {
				i++;
				continue;
			}
			visible++;
			if ( p[i] == ' ' ) {
				cut = i;
			}
		}
		if ( !p[i] || !cut ) {
			cut = i;
		}
		line[0] = Q_COLOR_ESCAPE;
		line[1] = lastColor;
		Q_strncpyz( line + 2, p, cut + 1 > (int)sizeof( line ) - 2 ? (int)sizeof( line ) - 2 : cut + 1 );
		for ( i = 0 ; i < cut ; i++ ) {
			if ( Q_IsColorString( p + i ) ) {
				lastColor = p[i + 1];
			}
		}
		CG_AddChatLine( line, strlen( line ) );
		p += cut;
		while ( *p == ' ' ) {
			p++;
		}
	}
}

/*
=================
CG_DrawChat
=================
*/
static void CG_DrawChat( void ) {
	chatLine_t	*c;
	int			i, rows, y = CHAT_Y, life;
	vec4_t		color = { 1, 1, 1, 1 };

	rows = cg.showChatHistory ? CHAT_HISTORY : cg_chatHistoryLength.integer;
	if ( rows > CHAT_HISTORY ) {
		rows = CHAT_HISTORY;
	}
	for ( i = 0 ; i < rows ; i++ ) {
		c = &chatLines[( chatHead - 1 - i + CHAT_HISTORY * 2 ) % CHAT_HISTORY];
		if ( !c->time || c->time > cg.time ) {
			break;
		}
		life = c->system ? SYSTEM_TIME : CHAT_TIME;
		if ( !cg.showChatHistory && cg.time - c->time > life ) {
			continue;
		}
		// in over 0.4 s, out over 0.47 s
		color[3] = 1;
		if ( !cg.showChatHistory && cg.time - c->time > life - 470 ) {
			color[3] = ( life - ( cg.time - c->time ) ) / 470.0f;
		} else if ( !cg.showChatHistory && cg.time - c->time < 400 ) {
			color[3] = ( cg.time - c->time ) / 400.0f;
		}
		if ( c->system ) {
			// grey and smaller, under the colour the line carries
			color[0] = color[1] = color[2] = 0.6f;
			CG_DrawStringExt( 8, y + 2, c->text, color, qtrue, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			color[0] = color[1] = color[2] = 1;
			y -= TINYCHAR_HEIGHT + 4;
		} else {
			CG_DrawStringExt( 8, y, c->text, color, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
			y -= SMALLCHAR_HEIGHT + 2;
		}
	}
}

/*
=================
CG_DrawDuelBars

Spectators of a duel: both players' portraits, names, health and armour
at the top
=================
*/
#define DUELBAR_Y	66		// under the "following" name

static void CG_DrawDuelBars( void ) {
	static const vec4_t	back = { 0, 0, 0, 0.5f };
	static const vec4_t	healthColor = { 0.9f, 0.2f, 0.2f, 0.9f };
	static const vec4_t	armorColor = { 1.0f, 0.8f, 0.1f, 0.9f };
	int		p, x, client, h, a;
	clientInfo_t	*ci;

	if ( cgs.gametype != GT_TOURNAMENT || cg.time - cg.duelHudTime > 2000 || cg.duelHudTime > cg.time ) {
		return;
	}
	if ( !cg.demoPlayback && !( cg.snap->ps.pm_flags & PMF_FOLLOW )
		&& cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		return;
	}
	for ( p = 0 ; p < 2 ; p++ ) {
		client = cg.duelHud[p][0];
		if ( client < 0 || client >= MAX_CLIENTS ) {
			continue;
		}
		ci = &cgs.clientinfo[client];
		h = cg.duelHud[p][1] < 0 ? 0 : cg.duelHud[p][1] > 200 ? 200 : cg.duelHud[p][1];
		a = cg.duelHud[p][2] < 0 ? 0 : cg.duelHud[p][2] > 200 ? 200 : cg.duelHud[p][2];
		x = p ? 352 : 128;
		CG_FillRect( x, DUELBAR_Y, 160, 26, back );
		if ( ci->modelIcon ) {
			CG_DrawPic( p ? x + 136 : x, DUELBAR_Y, 24, 24, ci->modelIcon );
		}
		CG_DrawStringExt( p ? x + 4 : x + 28, DUELBAR_Y, ci->name, colorWhite, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 16 );
		CG_FillRect( p ? x + 132 - h * 128 / 200 : x + 28, DUELBAR_Y + 10, h * 128 / 200, 6, healthColor );
		CG_FillRect( p ? x + 132 - a * 128 / 200 : x + 28, DUELBAR_Y + 18, a * 128 / 200, 4, armorColor );
	}
}

/*
=================
CG_DrawTimeout

TIMEOUT and the seconds left, while one runs
=================
*/
static void CG_DrawTimeout( void ) {
	int		end = atoi( CG_ConfigString( CS_TIMEOUT ) ), left, w;
	char	*s;

	if ( end <= cg.time ) {
		return;
	}
	left = ( end - cg.time + 999 ) / 1000;
	s = va( (char *)BG_String( STR_TO_LEFT ), left );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	CG_DrawBigStringColor( 320 - w / 2, 120, s, colorYellow );
}

/*
=================
CG_DrawSpecItemTimers

For spectators and demos: each major item's icon and the seconds until it
comes back, down the left edge
=================
*/
static void CG_DrawSpecItemTimers( void ) {
	char		*p;
	int			item, time, y = 200, left;
	char		buf[MAX_STRING_CHARS];
	vec4_t		color = { 1, 1, 1, 1 };

	if ( !cg_specItemTimers.integer ) {
		return;
	}
	if ( !cg.demoPlayback && !( cg.snap->ps.pm_flags & PMF_FOLLOW )
		&& cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		return;
	}

	Q_strncpyz( buf, CG_ConfigString( CS_ITEM_TIMERS ), sizeof( buf ) );
	p = buf;
	while ( 1 ) {
		char	*token = COM_Parse( &p );

		if ( !token[0] ) {
			break;
		}
		item = atoi( token );
		time = atoi( COM_Parse( &p ) );
		COM_Parse( &p );		// the spot, for the in-world timers
		COM_Parse( &p );
		COM_Parse( &p );
		if ( item <= 0 || item >= bg_numItems || !( BG_ItemTimerClass( &bg_itemlist[item] ) & cg_specItemTimers.integer ) ) {
			continue;
		}
		CG_RegisterItemVisuals( item );		// a spot may turn into an item the map doesn't place
		CG_DrawPic( 10, y, 20, 20, cg_items[item].icon );
		left = time ? ( time - cg.time + 999 ) / 1000 : 0;
		if ( left > 0 ) {
			color[0] = color[1] = color[2] = 1;
			CG_DrawStringExt( 34, y + 2, va( "%i", left ), color, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		} else {
			color[0] = 0.3f; color[1] = 1; color[2] = 0.3f;
			CG_DrawStringExt( 34, y + 2, "up", color, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		}
		y += 24;
	}
}

static qboolean CG_WorldToScreen( const vec3_t point, float *x, float *y );

/*
=================
CG_DrawWorldItemTimers

Major items that are away: a marker on their spot and the seconds until
they return. Spectators always see them; players too unless the mode is
duel (cg_worldItemTimers 0 turns them off)
=================
*/
static void CG_DrawWorldItemTimers( void ) {
	static vec4_t	ring = { 0.83f, 0.1f, 0.12f, 0.8f };
	char		buf[MAX_STRING_CHARS], *p, *token;
	int			item, time, left;
	vec3_t		spot;
	float		x, y;
	qboolean	watching = cg.demoPlayback || ( cg.snap->ps.pm_flags & PMF_FOLLOW )
		|| cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR;

	if ( !cg_worldItemTimers.integer || ( !watching && cgs.gametype == GT_TOURNAMENT ) ) {
		return;
	}
	Q_strncpyz( buf, CG_ConfigString( CS_ITEM_TIMERS ), sizeof( buf ) );
	p = buf;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		item = atoi( token );
		time = atoi( COM_Parse( &p ) );
		spot[0] = atoi( COM_Parse( &p ) );
		spot[1] = atoi( COM_Parse( &p ) );
		spot[2] = atoi( COM_Parse( &p ) );
		if ( item <= 0 || item >= bg_numItems || !time ) {
			continue;
		}
		left = ( time - cg.time + 999 ) / 1000;
		if ( left <= 0 || !CG_WorldToScreen( spot, &x, &y ) ) {
			continue;
		}
		// the marker: the item's icon in a red frame, the seconds under it
		CG_DrawRect( x - 13, y - 13, 26, 26, 1, ring );
		CG_RegisterItemVisuals( item );
		CG_DrawPic( x - 10, y - 10, 20, 20, cg_items[item].icon );
		CG_DrawStringExt( x - CG_DrawStrlen( va( "%i", left ) ) * SMALLCHAR_WIDTH / 2, y + 15, va( "%i", left ),
			colorWhite, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
	}
}

/*
=================
CG_DrawAccuracy

While +acc is held: shots, hits and accuracy for each weapon fired this
match, asked from the server twice a second
=================
*/
static void CG_DrawAccuracy( void ) {
	int		w, y, rows = 0;
	vec4_t	back = { 0, 0, 0, 0.6f };
	char	*s;

	if ( !cg.showAcc ) {
		return;
	}
	if ( cg.time - cg.accRequestTime > 500 || cg.accRequestTime > cg.time ) {
		cg.accRequestTime = cg.time;
		trap_SendClientCommand( "acc" );
	}
	for ( w = WP_GAUNTLET + 1 ; w < WP_NUM_WEAPONS ; w++ ) {
		if ( cg.accShots[w] ) {
			rows++;
		}
	}
	y = 240 - ( rows * 20 + 24 ) / 2;
	CG_FillRect( 200, y - 4, 240, rows * 20 + 32, back );
	CG_DrawStringExt( 320 - CG_DrawStrlen( BG_String( STR_ACC_TITLE ) ) * SMALLCHAR_WIDTH / 2, y,
		BG_String( STR_ACC_TITLE ), colorWhite, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
	y += 24;
	for ( w = WP_GAUNTLET + 1 ; w < WP_NUM_WEAPONS ; w++ ) {
		if ( !cg.accShots[w] ) {
			continue;
		}
		CG_RegisterWeapon( w );
		if ( cg_weapons[w].weaponIcon ) {
			CG_DrawPic( 210, y, 16, 16, cg_weapons[w].weaponIcon );
		}
		s = va( "%3i%%  %i / %i", cg.accHits[w] * 100 / cg.accShots[w], cg.accHits[w], cg.accShots[w] );
		CG_DrawStringExt( 236, y, s, colorWhite, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		y += 20;
	}
}

/*
=================
CG_HudNumberColor

White normally, yellow at 50 and below, red at 25 and below
=================
*/
static void CG_HudNumberColor( int value, vec4_t out ) {
	CG_HealthPalette( value <= 25 ? 0 : value <= 50 ? 1 : 2, out );
}

/*
=================
CG_DrawChargeIcons

One icon per charge under the portrait: lit when ready, its shadow while
recharging. A pack may give gfx/abilities/charges/<ability> and
<ability>_shadow; otherwise the ability's own icon, dimmed for the shadow.
=================
*/
static float	chargeIconsY = 470;		// the modern layout lifts them above the ring

static void CG_DrawChargeIcons( int active, int charges, int maxCharges ) {
	static qhandle_t	lit[ACTIVE_NUM_ABILITIES], shadow[ACTIVE_NUM_ABILITIES];
	static qboolean		loaded[ACTIVE_NUM_ABILITIES];
	static const vec4_t	dim = { 0.3f, 0.3f, 0.3f, 0.8f };
	float				size = 12;
	int					i;

	if ( active <= ACTIVE_NONE || active >= ACTIVE_NUM_ABILITIES ) {
		return;
	}
	if ( !loaded[active] ) {
		const char *name = bg_abilityInfo[active].cvarName;

		lit[active] = trap_R_RegisterShaderNoMip( va( "gfx/abilities/charges/%s", name ) );
		shadow[active] = trap_R_RegisterShaderNoMip( va( "gfx/abilities/charges/%s_shadow", name ) );
		if ( !lit[active] ) {
			lit[active] = trap_R_RegisterShaderNoMip( va( "gfx/tremor/abilities/%s", name ) );
		}
		loaded[active] = qtrue;
	}
	for ( i = 0 ; i < maxCharges ; i++ ) {
		float	x = 320 - maxCharges * ( size + 2 ) * 0.5f + i * ( size + 2 );

		if ( !lit[active] ) {
			CG_FillRect( x + 3, 474, size - 6, 4, i < charges ? colorWhite : colorMdGrey );
			continue;
		}
		if ( i < charges ) {
			CG_DrawPic( x, chargeIconsY, size, size, lit[active] );
		} else if ( shadow[active] ) {
			CG_DrawPic( x, chargeIconsY, size, size, shadow[active] );
		} else {
			trap_R_SetColor( dim );
			CG_DrawPic( x, chargeIconsY, size, size, lit[active] );
			trap_R_SetColor( NULL );
		}
	}
}

/*
=================
CG_DrawAbilityRing

Dots around a centre: the share of the ability's recharge done, full and
bright when a charge is ready
=================
*/
static void CG_DrawAbilityRing( float cx, float cy, float radius, float startDeg, float spanDeg, int dots, float dotSize ) {
	playerState_t		*ps = &cg.snap->ps;
	const champion_t	*champ = BG_Champion( ps->stats[STAT_CHAMPION] );
	vec4_t				on = { 1.0f, 0.75f, 0.2f, 1.0f }, off = { 0.3f, 0.3f, 0.3f, 0.6f };
	float				frac, a;
	int					i, recharge, left;

	if ( champ->active == ACTIVE_NONE ) {
		return;
	}
	recharge = BG_AbilityParam( champ->active, AP_RECHARGE );
	left = ps->stats[STAT_ABILITY_RECHARGE] * 100;
	if ( BG_AbilityCharges( ps ) > 0 && left <= 0 ) {
		frac = 1.0f;
		on[0] = on[1] = on[2] = 1.0f;		// ready: bright
	} else {
		frac = recharge > 0 ? 1.0f - (float)left / recharge : 0;
	}
	if ( BG_AbilityState( ps ) == ABILITY_ACTIVE || BG_AbilityState( ps ) == ABILITY_RUSH ) {
		VectorSet( on, 0.3f, 0.9f, 1.0f );
	}
	// just ready: one flash, the dots swelling and fading back
	if ( cg.abilityReadyTime && cg.time - cg.abilityReadyTime < 400 ) {
		dotSize *= 1.0f + 0.8f * ( 1.0f - ( cg.time - cg.abilityReadyTime ) / 400.0f );
	}
	for ( i = 0 ; i < dots ; i++ ) {
		a = DEG2RAD( startDeg + spanDeg * ( i + 0.5f ) / dots );
		CG_FillRect( cx + cos( a ) * radius - dotSize / 2, cy + sin( a ) * radius - dotSize / 2, dotSize, dotSize,
			( i + 0.5f ) / dots <= frac ? on : off );
	}
}

/*
=================
CG_DrawHudClock

Top centre: the clock counting down to the limit, and the scores
=================
*/
static void CG_DrawHudClock( void ) {
	const char	*info = CG_ConfigString( CS_SERVERINFO );
	int			limit = cgs.timelimit, msec, mins, secs, w, ty, sw;
	qboolean	finalSeconds;
	const char	*s;
	vec4_t		red = { 1, 0.3f, 0.3f, 1 }, blue = { 0.3f, 0.5f, 1, 1 };

	if ( !limit && ( cgs.gametype == GT_TOURNAMENT || cgs.gametype == GT_TEAM ) ) {
		limit = 10;
	}
	if ( !limit && cgs.gametype == GT_CTF && !atoi( Info_ValueForKey( info, "g_sacrifice" ) ) ) {
		limit = 14;
	}
	msec = cg.time - cgs.levelStartTime;
	if ( cg.warmup ) {
		msec = 0;
	}
	if ( limit ) {
		msec = limit * 60000 - msec;
		if ( msec < 0 ) {
			msec = 0;
		}
	}
	finalSeconds = limit && !cg.warmup && msec < 10000;		// red for the last 10 s
	secs = msec / 1000;
	mins = secs / 60;
	secs %= 60;
	s = va( "%i:%02i", mins, secs );
	if ( CG_HudModern() ) {
		// large clock at top + 38, the mode under it at top + 97
		float	cx, cy;
		int		cw = 24, ch = 28;

		CG_HudPlace( "clock", &cx, &cy );
		cg_hudAnchor = ANCHOR_CENTER;
		w = CG_DrawStrlen( s ) * cw;
		CG_DrawStringExt( 320 - w / 2, cy - ch / 2, s, finalSeconds ? red : colorWhite, qfalse, qtrue, cw, ch, 0 );
		CG_HudPlace( "clockline", &cx, &cy );
		cg_hudAnchor = ANCHOR_CENTER;
		s = cg.warmup ? BG_String( STR_WARMUP ) : CG_ModeString();
		CG_DrawStringExt( 320 - CG_DrawStrlen( s ) * SMALLCHAR_WIDTH / 2, cy - SMALLCHAR_HEIGHT / 2, s, colorMdGrey,
			qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		s = va( "%i:%02i", mins, secs );
		w = CG_DrawStrlen( s ) * cw;
		CG_HudPlace( "clock", &cx, &cy );
		cg_hudAnchor = ANCHOR_CENTER;
		ty = cy - ch / 2;
		sw = cw;
	} else {
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		CG_DrawBigStringColor( 320 - w / 2, 4, s, finalSeconds ? red : colorWhite );
		ty = 4;
		sw = BIGCHAR_WIDTH;
	}

	// scores either side of the clock
	if ( cgs.gametype >= GT_TEAM ) {
		s = va( "%i", cgs.scores1 );
		CG_DrawStringExt( 320 - w / 2 - 12 - CG_DrawStrlen( s ) * BIGCHAR_WIDTH, ty, s, red, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
		s = va( "%i", cgs.scores2 );
		CG_DrawStringExt( 320 + w / 2 + 12, ty, s, blue, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	} else if ( cgs.scores1 != SCORE_NOT_PRESENT ) {
		s = va( "%i", cg.snap->ps.persistant[PERS_SCORE] );
		CG_DrawStringExt( 320 - w / 2 - 12 - CG_DrawStrlen( s ) * BIGCHAR_WIDTH, ty, s, colorWhite, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
		// the best other score
		s = va( "%i", cgs.scores1 == cg.snap->ps.persistant[PERS_SCORE] && cgs.scores2 != SCORE_NOT_PRESENT ? cgs.scores2 : cgs.scores1 );
		CG_DrawStringExt( 320 + w / 2 + 12, ty, s, colorMdGrey, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	}
}

/*
=================
CG_DrawWeaponBar

Down the right edge: owned weapons bright, missing dim, the current one framed
=================
*/
static void CG_DrawWeaponBar( void ) {
	playerState_t	*ps = &cg.snap->ps;
	vec4_t			dim = { 1, 1, 1, 0.25f }, frame = { 1, 0.75f, 0.2f, 1 };
	int				w, y = 130, have;
	const char		*s;

	for ( w = WP_GAUNTLET ; w < WP_NUM_WEAPONS ; w++ ) {
		if ( w == WP_GRAPPLING_HOOK || !cg_weapons[w].registered || !cg_weapons[w].weaponIcon ) {
			continue;
		}
		have = ps->stats[STAT_WEAPONS] & ( 1 << w );
		if ( !have && BG_AmmoPartner( w ) && ( ps->stats[STAT_WEAPONS] & ( 1 << BG_AmmoPartner( w ) ) ) ) {
			continue;		// a pair sharing a slot shows the one held
		}
		trap_R_SetColor( have ? NULL : dim );
		CG_DrawPic( 612, y, 24, 24, cg_weapons[w].weaponIcon );
		trap_R_SetColor( NULL );
		if ( have && ps->ammo[w] >= 0 ) {
			s = va( "%i", ps->ammo[w] );
			CG_DrawStringExt( 608 - CG_DrawStrlen( s ) * TINYCHAR_WIDTH, y + 8, s, have ? colorWhite : dim,
				qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
		if ( w == ps->weapon ) {
			CG_DrawRect( 610, y - 2, 28, 28, 1, frame );
		}
		y += 30;
	}
}

/*
=================
CG_DrawHud
=================
*/
/*
=================
CG_DrawHudPowerups

Held power-ups above the health number, each with its seconds left
=================
*/
static void CG_DrawHudPowerups( void ) {
	playerState_t	*ps = &cg.snap->ps;
	gitem_t			*item;
	int				i, left, x = 296 - 8 - CHAR_WIDTH * 3 - 32;

	for ( i = PW_QUAD ; i <= PW_FLIGHT ; i++ ) {
		if ( !ps->powerups[i] || ps->powerups[i] == INT_MAX || ps->powerups[i] < cg.time ) {
			continue;
		}
		item = BG_FindItemForPowerup( i );
		if ( !item ) {
			continue;
		}
		left = ( ps->powerups[i] - cg.time + 999 ) / 1000;
		CG_DrawPic( x, 400, 24, 24, trap_R_RegisterShader( item->icon ) );
		CG_DrawStringExt( x + 28, 404, va( "%i", left ), left <= 5 ? colorRed : colorWhite,
			qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		x += 64;
	}
}

/*
=================
CG_DrawHudPickup

What was just picked up: icon and name, low and left of the centre
=================
*/
static void CG_DrawHudPickup( void ) {
	float	*fadeColor;
	int		value = cg.itemPickup;

	if ( !value || cg.snap->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}
	fadeColor = CG_FadeColor( cg.itemPickupTime, 3000 );
	if ( !fadeColor ) {
		return;
	}
	CG_RegisterItemVisuals( value );
	trap_R_SetColor( fadeColor );
	CG_DrawPic( 120, 372, 20, 20, cg_items[value].icon );
	trap_R_SetColor( NULL );
	CG_DrawStringExt( 144, 374, bg_itemlist[value].pickup_name, fadeColor, qfalse, qtrue,
		SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

/*
=================
CG_DrawPickupFeed

The modern layout's pickup feed, top right: a row per item for 1.5 s,
the same item again merging into a count (hud.md 7.6)
=================
*/
#define PICKUP_ROWS		5
#define PICKUP_TIME		4600

static struct {
	int		item;
	int		count;
	int		time;
} pickupFeed[PICKUP_ROWS];

static void CG_DrawPickupFeed( void ) {
	static int	seen;
	float		x, y, row = 34 * DP;
	vec4_t		c = { 1, 1, 1, 1 };
	int			i;
	const char	*s;

	if ( !cg_drawItemPickups.integer ) {
		return;
	}
	// a new pickup: merge with the same item still showing, else push in
	if ( cg.itemPickup && cg.itemPickupTime != seen ) {
		seen = cg.itemPickupTime;
		for ( i = 0 ; i < PICKUP_ROWS ; i++ ) {
			if ( pickupFeed[i].item == cg.itemPickup && cg.time - pickupFeed[i].time < PICKUP_TIME ) {
				break;
			}
		}
		if ( i < PICKUP_ROWS ) {
			// the predicted pickup and the server's arrive close together:
			// count them once
			if ( cg.time - pickupFeed[i].time > 150 ) {
				pickupFeed[i].count++;
			}
			pickupFeed[i].time = cg.time;
		} else {
			memmove( pickupFeed + 1, pickupFeed, sizeof( pickupFeed[0] ) * ( PICKUP_ROWS - 1 ) );
			pickupFeed[0].item = cg.itemPickup;
			pickupFeed[0].count = 1;
			pickupFeed[0].time = cg.time;
		}
	}
	CG_HudPlace( "pickups", &x, &y );
	for ( i = 0 ; i < PICKUP_ROWS ; i++ ) {
		int	t = cg.time - pickupFeed[i].time;

		if ( !pickupFeed[i].item || t < 0 || t >= PICKUP_TIME ) {
			continue;
		}
		c[3] = t > PICKUP_TIME - 300 ? ( PICKUP_TIME - t ) / 300.0f : 1;
		s = pickupFeed[i].count > 1 ? va( "%s x%i", bg_itemlist[pickupFeed[i].item].pickup_name, pickupFeed[i].count )
			: bg_itemlist[pickupFeed[i].item].pickup_name;
		CG_RegisterItemVisuals( pickupFeed[i].item );
		trap_R_SetColor( c );
		CG_DrawPic( x - 16, y, 16, 16, cg_items[pickupFeed[i].item].icon );
		trap_R_SetColor( NULL );
		CG_DrawStringExt( x - 20 - CG_DrawStrlen( s ) * SMALLCHAR_WIDTH, y + 1, s, c, qfalse, qtrue,
			SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		y += row;
	}
	cg_hudAnchor = ANCHOR_CENTER;
}

/*
=================
CG_DrawPassiveState

A passive with a state of its own, above the portrait: Slash's and
Strogg's slide charge
=================
*/
static void CG_DrawPassiveState( void ) {
	playerState_t	*ps = &cg.snap->ps;
	char			*s;

	if ( BG_Champion( ps->stats[STAT_CHAMPION] )->moveStyle == MOVE_SLIDE ) {
		float	px = 320, py = 410;

		s = va( (char *)BG_String( STR_HUD_SLIDE ), BG_SlideCharge( ps ) / 1000.0f );
		if ( CG_HudModern() ) {
			CG_HudPlace( "passive", &px, &py );		// under the portrait, off the ring
		}
		CG_DrawStringExt( px - CG_DrawStrlen( s ) * TINYCHAR_WIDTH / 2, py, s, colorWhite,
			qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		cg_hudAnchor = ANCHOR_CENTER;
	}
}

/*
=================
CG_DrawSpeedometer

cg_drawSpeed 1: horizontal speed under the crosshair
=================
*/
static void CG_DrawSpeedometer( void ) {
	char	*s;
	float	speed;

	if ( !cg_drawSpeed.integer && !cgs.movementPractice ) {
		return;
	}
	speed = sqrt( cg.predictedPlayerState.velocity[0] * cg.predictedPlayerState.velocity[0]
		+ cg.predictedPlayerState.velocity[1] * cg.predictedPlayerState.velocity[1] );
	s = va( "%i", (int)speed );
	CG_DrawStringExt( 320 - CG_DrawStrlen( s ) * SMALLCHAR_WIDTH / 2, 290, s, colorWhite,
		qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

static void CG_DrawHud( void ) {
	playerState_t	*ps = &cg.snap->ps;
	centity_t		*cent = &cg_entities[ps->clientNum];
	vec4_t			color;
	int				value, i, charges, maxCharges;
	static qhandle_t	healthIcon;

	if ( cg_drawStatus.integer == 0 ) {
		return;
	}
	if ( !healthIcon ) {
		healthIcon = trap_R_RegisterShaderNoMip( "icons/iconh_yellow" );
	}

	// health, left of the centre
	value = ps->stats[STAT_HEALTH];
	CG_HudNumberColor( value, color );
	trap_R_SetColor( color );
	CG_DrawField( 296 - 8 - CHAR_WIDTH * 3, 432, 3, value );
	trap_R_SetColor( NULL );
	if ( healthIcon ) {
		CG_DrawPic( 296 - 8 - CHAR_WIDTH * 3 - 32, 440, 28, 28, healthIcon );
	}

	// the portrait, with the ability ring around it and charge pips below
	CG_DrawStatusBarHead( 296 );
	CG_DrawAbilityRing( 320, 452, 32, 0, 360, 24, 3 );
	maxCharges = BG_AbilityParam( BG_Champion( ps->stats[STAT_CHAMPION] )->active, AP_CHARGES );
	charges = BG_AbilityCharges( ps );
	if ( maxCharges > 1 ) {
		CG_DrawChargeIcons( BG_Champion( ps->stats[STAT_CHAMPION] )->active, charges, maxCharges );
	}
	// Xaero's banked Leaping Kicks
	if ( ( BG_Champion( ps->stats[STAT_CHAMPION] )->passives & PASSIVE_LEAPINGKICK ) && KICK_COUNT( ps ) > 0 ) {
		CG_DrawSmallStringColor( 344 + 8, 470, va( "KICK x%i", KICK_COUNT( ps ) ), colorYellow );
	}

	// armour, right of the centre
	value = ps->stats[STAT_ARMOR];
	CG_HudNumberColor( value > 50 ? 100 : value, color );
	trap_R_SetColor( color );
	CG_DrawField( 344 + 8, 432, 3, value );
	trap_R_SetColor( NULL );
	CG_DrawPic( 344 + 8 + CHAR_WIDTH * 3 + 4, 440, 28, 28, cgs.media.armorIcon );

	// ammo, bottom right
	cg_hudAnchor = ANCHOR_RIGHT;
	if ( cent->currentState.weapon && ps->ammo[cent->currentState.weapon] > -1 ) {
		value = ps->ammo[cent->currentState.weapon];
		trap_R_SetColor( value <= 5 ? colorRed : NULL );
		CG_DrawField( 640 - CHAR_WIDTH * 3 - 4, 432, 3, value );
		trap_R_SetColor( NULL );
		if ( cg_weapons[cent->currentState.weapon].ammoIcon ) {
			CG_DrawPic( 640 - CHAR_WIDTH * 3 - 40, 440, 28, 28, cg_weapons[cent->currentState.weapon].ammoIcon );
		}
	}
	cg_hudAnchor = ANCHOR_CENTER;

	// the ability arc by the crosshair: 1 bottom (default 3), 2 top, 4 left, 5 right
	switch ( cg_drawCooldownArc.integer ) {
	case 2:		CG_DrawAbilityRing( 320, 240, 26, 225, 90, 12, 2 ); break;
	case 3:		CG_DrawAbilityRing( 320, 240, 26, 45, 90, 12, 2 ); break;
	case 4:		CG_DrawAbilityRing( 320, 240, 26, 135, 90, 12, 2 ); break;
	case 5:		CG_DrawAbilityRing( 320, 240, 26, -45, 90, 12, 2 ); break;
	default:	break;
	}

	cg_hudAnchor = ANCHOR_RIGHT;
	CG_DrawWeaponBar();
	cg_hudAnchor = ANCHOR_CENTER;
	CG_DrawHudPowerups();
	CG_DrawHudPickup();
	CG_DrawPassiveState();
	CG_DrawSpeedometer();
}
#endif

/*
================
CG_DrawStatusBarFlag

================
*/
#ifndef MISSIONPACK
static void CG_DrawStatusBarFlag( float x, int team ) {
	CG_DrawFlagModel( x, 480 - ICON_SIZE, ICON_SIZE, ICON_SIZE, team, qfalse );
}
#endif // MISSIONPACK

/*
================
CG_DrawTeamBackground

================
*/
/*
================
CG_TeamPalette

Red and blue, or a pair told apart with cg_colorBlind: 1 deuteranopia
(orange and blue), 2 protanopia (yellow and blue), 3 tritanopia (red and
teal)
================
*/
void CG_TeamPalette( int team, vec4_t out ) {
	static const float	pairs[4][2][3] = {
		{ { 1, 0, 0 }, { 0, 0, 1 } },
		{ { 1, 0.55f, 0 }, { 0.1f, 0.45f, 1 } },
		{ { 1, 0.85f, 0.1f }, { 0.1f, 0.45f, 1 } },
		{ { 1, 0.15f, 0.2f }, { 0, 0.75f, 0.75f } }
	};
	int		p = cg_colorBlind.integer >= 0 && cg_colorBlind.integer <= 3 ? cg_colorBlind.integer : 0;
	int		t = team == TEAM_BLUE ? 1 : 0;

	out[0] = pairs[p][t][0];
	out[1] = pairs[p][t][1];
	out[2] = pairs[p][t][2];
}

void CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team )
{
	vec4_t		hcolor;

	hcolor[3] = alpha;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		CG_TeamPalette( team, hcolor );
	} else {
		return;
	}
	trap_R_SetColor( hcolor );
	CG_DrawPic( x, y, w, h, cgs.media.teamStatusBar );
	trap_R_SetColor( NULL );
}

/*
================
CG_DrawStatusBar

================
*/
#ifndef MISSIONPACK
static void CG_DrawStatusBar( void ) {
	int			color;
	centity_t	*cent;
	playerState_t	*ps;
	int			value;
	vec4_t		hcolor;
	vec3_t		angles;
	vec3_t		origin;

	static float colors[4][4] = { 
//		{ 0.2, 1.0, 0.2, 1.0 } , { 1.0, 0.2, 0.2, 1.0 }, {0.5, 0.5, 0.5, 1} };
		{ 1.0f, 0.69f, 0.0f, 1.0f },    // normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },     // low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },     // weapon firing
		{ 1.0f, 1.0f, 1.0f, 1.0f } };   // health > 100

	if ( cg_drawStatus.integer == 0 ) {
		return;
	}

	// draw the team background
	CG_DrawTeamBackground( 0, 420, 640, 60, 0.33f, cg.snap->ps.persistant[PERS_TEAM] );

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	VectorClear( angles );

	// draw any 3D icons first, so the changes back to 2D are minimized
	if ( cent->currentState.weapon && cg_weapons[ cent->currentState.weapon ].ammoModel ) {
		origin[0] = 70;
		origin[1] = 0;
		origin[2] = 0;
		angles[YAW] = 90 + 20 * sin( cg.time / 1000.0 );
		CG_Draw3DModel( CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
					   cg_weapons[ cent->currentState.weapon ].ammoModel, 0, origin, angles );
	}

	CG_DrawStatusBarHead( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE );

	if( cg.predictedPlayerState.powerups[PW_REDFLAG] ) {
		CG_DrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_RED );
	} else if( cg.predictedPlayerState.powerups[PW_BLUEFLAG] ) {
		CG_DrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_BLUE );
	} else if( cg.predictedPlayerState.powerups[PW_NEUTRALFLAG] ) {
		CG_DrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_FREE );
	}

	if ( ps->stats[ STAT_ARMOR ] ) {
		origin[0] = 90;
		origin[1] = 0;
		origin[2] = -10;
		angles[YAW] = ( cg.time & 2047 ) * 360 / 2048.0;
		CG_Draw3DModel( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
					   cgs.media.armorModel, 0, origin, angles );
	}
	//
	// ammo
	//
	if ( cent->currentState.weapon ) {
		value = ps->ammo[cent->currentState.weapon];
		if ( value > -1 ) {
			if ( cg.predictedPlayerState.weaponstate == WEAPON_FIRING
				&& cg.predictedPlayerState.weaponTime > 100 ) {
				// draw as dark grey when reloading
				color = 2;	// dark grey
			} else {
				if ( value >= 0 ) {
					color = 0;	// green
				} else {
					color = 1;	// red
				}
			}
			trap_R_SetColor( colors[color] );
			
			CG_DrawField (0, 432, 3, value);
			trap_R_SetColor( NULL );

			// if we didn't draw a 3D icon, draw a 2D icon for ammo
			if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
				qhandle_t	icon;

				icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
				if ( icon ) {
					CG_DrawPic( CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE, icon );
				}
			}
		}
	}

	//
	// health
	//
	value = ps->stats[STAT_HEALTH];
	if ( value > 100 ) {
		trap_R_SetColor( colors[3] );		// white
	} else if (value > 25) {
		trap_R_SetColor( colors[0] );	// green
	} else if (value > 0) {
		color = (cg.time >> 8) & 1;	// flash
		trap_R_SetColor( colors[color] );
	} else {
		trap_R_SetColor( colors[1] );	// red
	}

	// stretch the health up when taking damage
	CG_DrawField ( 185, 432, 3, value);
	CG_ColorForHealth( hcolor );
	trap_R_SetColor( hcolor );


	//
	// armor
	//
	value = ps->stats[STAT_ARMOR];
	if (value > 0 ) {
		trap_R_SetColor( colors[0] );
		CG_DrawField (370, 432, 3, value);
		trap_R_SetColor( NULL );
		// if we didn't draw a 3D icon, draw a 2D icon for armor
		if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
			CG_DrawPic( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE, cgs.media.armorIcon );
		}

	}
}
#endif

/*
===========================================================================================

  UPPER RIGHT CORNER

===========================================================================================
*/

/*
================
CG_DrawAttacker

================
*/
static float CG_DrawAttacker( float y ) {
	int			t;
	float		size;
	vec3_t		angles;
	const char	*info;
	const char	*name;
	int			clientNum;

	if ( cg.predictedPlayerState.stats[STAT_HEALTH] <= 0 ) {
		return y;
	}

	if ( !cg.attackerTime ) {
		return y;
	}

	clientNum = cg.predictedPlayerState.persistant[PERS_ATTACKER];
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || clientNum == cg.snap->ps.clientNum ) {
		return y;
	}

	if ( !cgs.clientinfo[clientNum].infoValid ) {
		cg.attackerTime = 0;
		return y;
	}

	t = cg.time - cg.attackerTime;
	if ( t > ATTACKER_HEAD_TIME ) {
		cg.attackerTime = 0;
		return y;
	}

	size = ICON_SIZE * 1.25;

	angles[PITCH] = 0;
	angles[YAW] = 180;
	angles[ROLL] = 0;
	CG_DrawHead( 640 - size, y, size, size, clientNum, angles );

	info = CG_ConfigString( CS_PLAYERS + clientNum );
	name = Info_ValueForKey(  info, "n" );
	y += size;
	CG_DrawBigString( 640 - ( Q_PrintStrlen( name ) * BIGCHAR_WIDTH), y, name, 0.5 );

	return y + BIGCHAR_HEIGHT + 2;
}

/*
==================
CG_DrawSnapshot
==================
*/
static float CG_DrawSnapshot( float y ) {
	char		*s;
	int			w;

	s = va( "time:%i snap:%i cmd:%i", cg.snap->serverTime, 
		cg.latestSnapshotNum, cgs.serverCommandSequence );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

	CG_DrawBigString( 635 - w, y + 2, s, 1.0F);

	return y + BIGCHAR_HEIGHT + 4;
}

/*
==================
CG_DrawFPS
==================
*/
#define	FPS_FRAMES	4
static float CG_DrawFPS( float y ) {
	char		*s;
	int			w;
	static int	previousTimes[FPS_FRAMES];
	static int	index;
	int		i, total;
	int		fps;
	static	int	previous;
	int		t, frameTime;

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	t = trap_Milliseconds();
	frameTime = t - previous;
	previous = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;
	if ( index > FPS_FRAMES ) {
		// average multiple frames together to smooth changes out a bit
		total = 0;
		for ( i = 0 ; i < FPS_FRAMES ; i++ ) {
			total += previousTimes[i];
		}
		if ( !total ) {
			total = 1;
		}
		fps = 1000 * FPS_FRAMES / total;

		// a capped game reads as its cap, not one below it
		{
			char	buf[16];
			int		cap;

			trap_Cvar_VariableStringBuffer( "com_maxfps", buf, sizeof( buf ) );
			cap = atoi( buf );
			if ( cap > 0 && abs( fps - cap ) <= 1 ) {
				fps = cap;
			}
		}

		s = va( "%ifps", fps );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

		CG_DrawBigString( 635 - w, y + 2, s, 1.0F);
	}

	return y + BIGCHAR_HEIGHT + 4;
}

/*
=================
CG_DrawTimer
=================
*/
static float CG_DrawTimer( float y ) {
	char		*s;
	int			w;
	int			mins, seconds, tens;
	int			msec;

	msec = cg.time - cgs.levelStartTime;

	seconds = msec / 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	s = va( "%i:%i%i", mins, tens, seconds );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

	CG_DrawBigString( 635 - w, y + 2, s, 1.0F);

	return y + BIGCHAR_HEIGHT + 4;
}


/*
=================
CG_DrawTeamOverlay
=================
*/

/*
=================
CG_VoipFlag

A player's bit in one of the engine's voice chat masks (cl_voipTalking,
cl_voipMuted): a hex digit per four players
=================
*/
qboolean CG_VoipFlag( const vmCvar_t *mask, int clientNum ) {
	int		digit;
	char	c;

	if ( clientNum < 0 || clientNum / 4 >= (int)strlen( mask->string ) ) {
		return qfalse;
	}
	c = mask->string[clientNum / 4];
	digit = c >= 'a' ? c - 'a' + 10 : c - '0';
	return ( digit >> ( clientNum % 4 ) ) & 1;
}

/*
=================
CG_DrawVoip

A speaker for a player talking on voice chat, a crossed-out one for a
muted player
=================
*/
void CG_DrawVoip( float x, float y, float size, int clientNum ) {
	if ( CG_VoipFlag( &cl_voipMuted, clientNum ) ) {
		CG_DrawPic( x, y, size, size, cgs.media.speakerMutedShader );
	} else if ( CG_VoipFlag( &cl_voipTalking, clientNum ) ) {
		CG_DrawPic( x, y, size, size, cgs.media.speakerShader );
	}
}

static float CG_DrawTeamOverlay( float y, qboolean right, qboolean upper ) {
	int x, w, h, xx;
	int i, j, len;
	const char *p;
	vec4_t		hcolor;
	int pwidth, lwidth;
	int plyrs;
	char st[16];
	clientInfo_t *ci;
	gitem_t	*item;
	int ret_y, count;
	// a free-flying spectator sees both teams (cg.overlayTeam picks which)
	int overlayTeam = cg.overlayTeam ? cg.overlayTeam : cg.snap->ps.persistant[PERS_TEAM];

	if ( !cg_drawTeamOverlay.integer ) {
		return y;
	}

	if ( overlayTeam != TEAM_RED && overlayTeam != TEAM_BLUE ) {
		return y; // Not on any team
	}

	plyrs = 0;

	// max player name width
	pwidth = 0;
	count = (numSortedTeamPlayers > ( cg.overlayTeam ? 16 : 8 )) ? ( cg.overlayTeam ? 16 : 8 ) : numSortedTeamPlayers;
	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == overlayTeam) {
			plyrs++;
			len = CG_DrawStrlen(ci->name);
			if (len > pwidth)
				pwidth = len;
		}
	}

	if (!plyrs)
		return y;

	if (pwidth > TEAM_OVERLAY_MAXNAME_WIDTH)
		pwidth = TEAM_OVERLAY_MAXNAME_WIDTH;

	// max location name width
	lwidth = 0;
	for (i = 1; i < MAX_LOCATIONS; i++) {
		p = CG_ConfigString(CS_LOCATIONS + i);
		if (p && *p) {
			len = CG_DrawStrlen(p);
			if (len > lwidth)
				lwidth = len;
		}
	}

	if (lwidth > TEAM_OVERLAY_MAXLOCATION_WIDTH)
		lwidth = TEAM_OVERLAY_MAXLOCATION_WIDTH;

	w = (pwidth + lwidth + 4 + 7 + 2) * TINYCHAR_WIDTH;	// + the champion icon

	if ( right )
		x = 640 - w;
	else
		x = 0;

	h = plyrs * TINYCHAR_HEIGHT;

	if ( upper ) {
		ret_y = y + h;
	} else {
		y -= h;
		ret_y = y;
	}

	CG_TeamPalette( overlayTeam, hcolor );
	hcolor[3] = 0.33f;
	trap_R_SetColor( hcolor );
	CG_DrawPic( x, y, w, h, cgs.media.teamStatusBar );
	trap_R_SetColor( NULL );

	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == overlayTeam) {

			hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0;

			// the teammate's champion, and a dot for its ability: green ready,
			// blue in use, grey recharging
			{
				int			champ = BG_ChampionForModel( ci->modelName );
				qhandle_t	pic = CG_ChampionPortrait( champ, qtrue );
				vec4_t		dot;
				int			state = ( ci->abilityStat & 0xffff ) >> ABILITY_STATE_SHIFT;

				if ( pic || ci->modelIcon ) {
					CG_DrawPic( x + TINYCHAR_WIDTH, y, TINYCHAR_HEIGHT, TINYCHAR_HEIGHT, pic ? pic : ci->modelIcon );
				}
				if ( state != ABILITY_IDLE ) {
					dot[0] = 0.4f; dot[1] = 0.7f; dot[2] = 1; dot[3] = 1;
				} else if ( ci->abilityStat & ABILITY_CHARGES_MASK ) {
					dot[0] = 0.3f; dot[1] = 1; dot[2] = 0.3f; dot[3] = 1;
				} else {
					dot[0] = dot[1] = dot[2] = 0.4f; dot[3] = 1;
				}
				CG_FillRect( x + TINYCHAR_WIDTH + TINYCHAR_HEIGHT - 3, y + TINYCHAR_HEIGHT - 3, 3, 3, dot );
			}
			CG_DrawVoip( x, y, TINYCHAR_HEIGHT, sortedTeamPlayers[i] );
			x += TINYCHAR_WIDTH * 2;

			xx = x + TINYCHAR_WIDTH;

			CG_DrawStringExt( xx, y,
				ci->name, hcolor, qfalse, qfalse,
				TINYCHAR_WIDTH, TINYCHAR_HEIGHT, TEAM_OVERLAY_MAXNAME_WIDTH);

			if (lwidth) {
				p = CG_ConfigString(CS_LOCATIONS + ci->location);
				if (!p || !*p)
					p = "unknown";
//				len = CG_DrawStrlen(p);
//				if (len > lwidth)
//					len = lwidth;

//				xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth + 
//					((lwidth/2 - len/2) * TINYCHAR_WIDTH);
				xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth;
				CG_DrawStringExt( xx, y,
					p, hcolor, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
					TEAM_OVERLAY_MAXLOCATION_WIDTH);
			}

			CG_GetColorForHealth( ci->health, ci->armor, hcolor );

			Com_sprintf (st, sizeof(st), "%3i %3i", ci->health,	ci->armor);

			xx = x + TINYCHAR_WIDTH * 3 + 
				TINYCHAR_WIDTH * pwidth + TINYCHAR_WIDTH * lwidth;

			CG_DrawStringExt( xx, y,
				st, hcolor, qfalse, qfalse,
				TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

			// draw weapon icon
			xx += TINYCHAR_WIDTH * 3;

			if ( cg_weapons[ci->curWeapon].weaponIcon ) {
				CG_DrawPic( xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 
					cg_weapons[ci->curWeapon].weaponIcon );
			} else {
				CG_DrawPic( xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 
					cgs.media.deferShader );
			}

			// Draw powerup icons
			x -= TINYCHAR_WIDTH * 2;
			if (right) {
				xx = x;
			} else {
				xx = x + w - TINYCHAR_WIDTH;
			}
			for (j = 0; j <= PW_NUM_POWERUPS; j++) {
				if (ci->powerups & (1 << j)) {

					item = BG_FindItemForPowerup( j );

					if (item) {
						CG_DrawPic( xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 
						trap_R_RegisterShader( item->icon ) );
						if (right) {
							xx -= TINYCHAR_WIDTH;
						} else {
							xx += TINYCHAR_WIDTH;
						}
					}
				}
			}

			y += TINYCHAR_HEIGHT;
		}
	}

	return ret_y;
//#endif
}


/*
=====================
CG_DrawUpperRight

=====================
*/
/*
=================
CG_DrawSacrifice

Sacrifice: both meters, where the soul is, and extraction progress;
round duel: both rosters
=================
*/
static float CG_DrawSacrifice( float y ) {
	const char	*cs = CG_ConfigString( CS_SACRIFICE );
	int			v[6], i, w;
	const char	*s;

	if ( !cs[0] ) {
		return y;
	}

	// round duel: "rd|first roster|second roster"
	if ( !Q_strncmp( cs, "rd|", 3 ) ) {
		char	line[256];
		int		n;

		cs += 3;
		while ( *cs && *cs != '#' ) {
			for ( n = 0 ; *cs && *cs != '|' && n < (int)sizeof( line ) - 1 ; n++ ) {
				line[n] = *cs++;
			}
			line[n] = 0;
			if ( *cs == '|' ) {
				cs++;
			}
			w = CG_DrawStrlen( line ) * SMALLCHAR_WIDTH;
			CG_DrawSmallString( 635 - w, y + 2, line, 1.0F );
			y += SMALLCHAR_HEIGHT + 4;
		}
		return y;
	}

	if ( cgs.gametype != GT_CTF ) {
		return y;
	}
	for ( i = 0 ; i < 6 ; i++ ) {
		v[i] = atoi( cs );
		while ( *cs && *cs != ' ' ) {
			cs++;
		}
		while ( *cs == ' ' ) {
			cs++;
		}
	}
	if ( v[5] <= 0 ) {
		v[5] = 1;
	}

	s = va( (char *)BG_String( STR_SAC_HUD ), v[0] * 100 / v[5], v[1] * 100 / v[5] );
	w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;
	CG_DrawSmallString( 635 - w, y + 2, s, 1.0F );
	y += SMALLCHAR_HEIGHT + 4;

	switch ( v[2] ) {
	case 0:		s = BG_String( STR_SAC_SOUL_AT_SPAWN ); break;
	case 1:		s = BG_String( STR_SAC_SOUL_GROUND ); break;
	case 2:
		s = va( (char *)BG_String( STR_SAC_SOUL_CARRIED ),
			( v[3] >= 0 && v[3] < MAX_CLIENTS ) ? cgs.clientinfo[v[3]].name : "?" );
		break;
	default:
		s = BG_String( v[2] - 10 == TEAM_RED ? STR_SAC_SOUL_RED : STR_SAC_SOUL_BLUE );
		break;
	}
	w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;
	CG_DrawSmallString( 635 - w, y + 2, s, 1.0F );
	y += SMALLCHAR_HEIGHT + 4;

	if ( v[4] > 0 ) {
		s = va( (char *)BG_String( STR_SAC_EXTRACTING ), v[4] * 100 / 2500 );
		w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;
		CG_DrawSmallString( 635 - w, y + 2, s, 1.0F );
		y += SMALLCHAR_HEIGHT + 4;
	}
	return y;
}

static void CG_DrawUpperRight(stereoFrame_t stereoFrame)
{
	float	y;

	y = 0;

	y = CG_DrawSacrifice( y );

	if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 1 && !cg_hudStyle.integer ) {
		y = CG_DrawTeamOverlay( y, qtrue, qtrue );
	} 
	if ( cg_drawSnapshot.integer ) {
		y = CG_DrawSnapshot( y );
	}
	if (cg_drawFPS.integer && (stereoFrame == STEREO_CENTER || stereoFrame == STEREO_RIGHT)) {
		y = CG_DrawFPS( y );
	}
	if ( cg_drawTimer.integer || cgs.movementPractice ) {
		y = CG_DrawTimer( y );
	}
	if ( cg_drawAttacker.integer ) {
		CG_DrawAttacker( y );
	}

}

/*
===========================================================================================

  LOWER RIGHT CORNER

===========================================================================================
*/

/*
=================
CG_DrawScores

Draw the small two score display
=================
*/
#ifndef MISSIONPACK
static float CG_DrawScores( float y ) {
	const char	*s;
	int			s1, s2, score;
	int			x, w;
	int			v;
	vec4_t		color;
	float		y1;
	gitem_t		*item;

	s1 = cgs.scores1;
	s2 = cgs.scores2;

	y -=  BIGCHAR_HEIGHT + 8;

	y1 = y;

	// draw from the right side to left
	if ( cgs.gametype >= GT_TEAM ) {
		x = 640;
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 1.0f;
		color[3] = 0.33f;
		s = va( "%2i", s2 );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
		x -= w;
		CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE ) {
			CG_DrawPic( x, y-4, w, BIGCHAR_HEIGHT+8, cgs.media.selectShader );
		}
		CG_DrawBigString( x + 4, y, s, 1.0F);

		if ( cgs.gametype == GT_CTF ) {
			// Display flag status
			item = BG_FindItemForPowerup( PW_BLUEFLAG );

			if (item) {
				y1 = y - BIGCHAR_HEIGHT - 8;
				if( cgs.blueflag >= 0 && cgs.blueflag <= 2 ) {
					CG_DrawPic( x, y1-4, w, BIGCHAR_HEIGHT+8, cgs.media.blueFlagShader[cgs.blueflag] );
				}
			}
		}
		color[0] = 1.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 0.33f;
		s = va( "%2i", s1 );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
		x -= w;
		CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED ) {
			CG_DrawPic( x, y-4, w, BIGCHAR_HEIGHT+8, cgs.media.selectShader );
		}
		CG_DrawBigString( x + 4, y, s, 1.0F);

		if ( cgs.gametype == GT_CTF ) {
			// Display flag status
			item = BG_FindItemForPowerup( PW_REDFLAG );

			if (item) {
				y1 = y - BIGCHAR_HEIGHT - 8;
				if( cgs.redflag >= 0 && cgs.redflag <= 2 ) {
					CG_DrawPic( x, y1-4, w, BIGCHAR_HEIGHT+8, cgs.media.redFlagShader[cgs.redflag] );
				}
			}
		}

		if ( cgs.gametype >= GT_CTF ) {
			v = cgs.capturelimit;
		} else {
			v = cgs.fraglimit;
		}
		if ( v ) {
			s = va( "%2i", v );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
			x -= w;
			CG_DrawBigString( x + 4, y, s, 1.0F);
		}

	} else {
		qboolean	spectator;

		x = 640;
		score = cg.snap->ps.persistant[PERS_SCORE];
		spectator = ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR );

		// always show your score in the second box if not in first place
		if ( s1 != score ) {
			s2 = score;
		}
		if ( s2 != SCORE_NOT_PRESENT ) {
			s = va( "%2i", s2 );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
			x -= w;
			if ( !spectator && score == s2 && score != s1 ) {
				color[0] = 1.0f;
				color[1] = 0.0f;
				color[2] = 0.0f;
				color[3] = 0.33f;
				CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
				CG_DrawPic( x, y-4, w, BIGCHAR_HEIGHT+8, cgs.media.selectShader );
			} else {
				color[0] = 0.5f;
				color[1] = 0.5f;
				color[2] = 0.5f;
				color[3] = 0.33f;
				CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
			}	
			CG_DrawBigString( x + 4, y, s, 1.0F);
		}

		// first place
		if ( s1 != SCORE_NOT_PRESENT ) {
			s = va( "%2i", s1 );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
			x -= w;
			if ( !spectator && score == s1 ) {
				color[0] = 0.0f;
				color[1] = 0.0f;
				color[2] = 1.0f;
				color[3] = 0.33f;
				CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
				CG_DrawPic( x, y-4, w, BIGCHAR_HEIGHT+8, cgs.media.selectShader );
			} else {
				color[0] = 0.5f;
				color[1] = 0.5f;
				color[2] = 0.5f;
				color[3] = 0.33f;
				CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
			}	
			CG_DrawBigString( x + 4, y, s, 1.0F);
		}

		if ( cgs.fraglimit ) {
			s = va( "%2i", cgs.fraglimit );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
			x -= w;
			CG_DrawBigString( x + 4, y, s, 1.0F);
		}

	}

	return y1 - 8;
}
#endif // MISSIONPACK

/*
================
CG_DrawPowerups
================
*/
#ifndef MISSIONPACK
static float CG_DrawPowerups( float y ) {
	int		sorted[MAX_POWERUPS];
	int		sortedTime[MAX_POWERUPS];
	int		i, j, k;
	int		active;
	playerState_t	*ps;
	int		t;
	gitem_t	*item;
	int		x;
	int		color;
	float	size;
	float	f;
	static float colors[2][4] = { 
    { 0.2f, 1.0f, 0.2f, 1.0f } , 
    { 1.0f, 0.2f, 0.2f, 1.0f } 
  };

	ps = &cg.snap->ps;

	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		return y;
	}

	// sort the list by time remaining
	active = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( !ps->powerups[ i ] ) {
			continue;
		}

		// ZOID--don't draw if the power up has unlimited time
		// This is true of the CTF flags
		if ( ps->powerups[ i ] == INT_MAX ) {
			continue;
		}

		t = ps->powerups[ i ] - cg.time;
		if ( t <= 0 ) {
			continue;
		}

		// insert into the list
		for ( j = 0 ; j < active ; j++ ) {
			if ( sortedTime[j] >= t ) {
				for ( k = active - 1 ; k >= j ; k-- ) {
					sorted[k+1] = sorted[k];
					sortedTime[k+1] = sortedTime[k];
				}
				break;
			}
		}
		sorted[j] = i;
		sortedTime[j] = t;
		active++;
	}

	// draw the icons and timers
	x = 640 - ICON_SIZE - CHAR_WIDTH * 2;
	for ( i = 0 ; i < active ; i++ ) {
		item = BG_FindItemForPowerup( sorted[i] );

    if (item) {

		  color = 1;

		  y -= ICON_SIZE;

		  trap_R_SetColor( colors[color] );
		  CG_DrawField( x, y, 2, sortedTime[ i ] / 1000 );

		  t = ps->powerups[ sorted[i] ];
		  if ( t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME ) {
			  trap_R_SetColor( NULL );
		  } else {
			  vec4_t	modulate;

			  f = (float)( t - cg.time ) / POWERUP_BLINK_TIME;
			  f -= (int)f;
			  modulate[0] = modulate[1] = modulate[2] = modulate[3] = f;
			  trap_R_SetColor( modulate );
		  }

		  if ( cg.powerupActive == sorted[i] && 
			  cg.time - cg.powerupTime < PULSE_TIME ) {
			  f = 1.0 - ( ( (float)cg.time - cg.powerupTime ) / PULSE_TIME );
			  size = ICON_SIZE * ( 1.0 + ( PULSE_SCALE - 1.0 ) * f );
		  } else {
			  size = ICON_SIZE;
		  }

		  CG_DrawPic( 640 - size, y + ICON_SIZE / 2 - size / 2, 
			  size, size, trap_R_RegisterShader( item->icon ) );
    }
	}
	trap_R_SetColor( NULL );

	return y;
}
#endif // MISSIONPACK

/*
=====================
CG_DrawLowerRight

=====================
*/
#ifndef MISSIONPACK
static void CG_DrawLowerRight( void ) {
	float	y;

	y = 480 - ICON_SIZE;

	if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 2 ) {
		y = CG_DrawTeamOverlay( y, qtrue, qfalse );
	} 

	y = CG_DrawScores( y );
	CG_DrawPowerups( y );
}
#endif // MISSIONPACK

/*
===================
CG_DrawPickupItem
===================
*/
#ifndef MISSIONPACK
static int CG_DrawPickupItem( int y ) {
	int		value;
	float	*fadeColor;

	if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 ) {
		return y;
	}

	y -= ICON_SIZE;

	value = cg.itemPickup;
	if ( value ) {
		fadeColor = CG_FadeColor( cg.itemPickupTime, 3000 );
		if ( fadeColor ) {
			CG_RegisterItemVisuals( value );
			trap_R_SetColor( fadeColor );
			CG_DrawPic( 8, y, ICON_SIZE, ICON_SIZE, cg_items[ value ].icon );
			CG_DrawBigString( ICON_SIZE + 16, y + (ICON_SIZE/2 - BIGCHAR_HEIGHT/2), bg_itemlist[ value ].pickup_name, fadeColor[0] );
			trap_R_SetColor( NULL );
		}
	}
	
	return y;
}
#endif // MISSIONPACK

/*
=====================
CG_DrawLowerLeft

=====================
*/
#ifndef MISSIONPACK
static void CG_DrawLowerLeft( void ) {
	float	y;

	y = 480 - ICON_SIZE;

	if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 3 ) {
		y = CG_DrawTeamOverlay( y, qfalse, qfalse );
	} 

	if ( !cg_hudStyle.integer ) {
		CG_DrawPickupItem( y );
	}
}
#endif // MISSIONPACK


//===========================================================================================

/*
=================
CG_DrawTeamInfo
=================
*/
#ifndef MISSIONPACK
static void CG_DrawTeamInfo( void ) {
	int h;
	int i;
	vec4_t		hcolor;
	int		chatHeight;

#define CHATLOC_Y 420 // bottom end
#define CHATLOC_X 0

	if (cg_teamChatHeight.integer < TEAMCHAT_HEIGHT)
		chatHeight = cg_teamChatHeight.integer;
	else
		chatHeight = TEAMCHAT_HEIGHT;
	if (chatHeight <= 0)
		return; // disabled

	if (cgs.teamLastChatPos != cgs.teamChatPos) {
		if (cg.time - cgs.teamChatMsgTimes[cgs.teamLastChatPos % chatHeight] > cg_teamChatTime.integer) {
			cgs.teamLastChatPos++;
		}

		h = (cgs.teamChatPos - cgs.teamLastChatPos) * TINYCHAR_HEIGHT;

		if ( cgs.clientinfo[cg.clientNum].team == TEAM_RED ) {
			hcolor[0] = 1.0f;
			hcolor[1] = 0.0f;
			hcolor[2] = 0.0f;
			hcolor[3] = 0.33f;
		} else if ( cgs.clientinfo[cg.clientNum].team == TEAM_BLUE ) {
			hcolor[0] = 0.0f;
			hcolor[1] = 0.0f;
			hcolor[2] = 1.0f;
			hcolor[3] = 0.33f;
		} else {
			hcolor[0] = 0.0f;
			hcolor[1] = 1.0f;
			hcolor[2] = 0.0f;
			hcolor[3] = 0.33f;
		}

		trap_R_SetColor( hcolor );
		CG_DrawPic( CHATLOC_X, CHATLOC_Y - h, 640, h, cgs.media.teamStatusBar );
		trap_R_SetColor( NULL );

		hcolor[0] = hcolor[1] = hcolor[2] = 1.0f;
		hcolor[3] = 1.0f;

		for (i = cgs.teamChatPos - 1; i >= cgs.teamLastChatPos; i--) {
			CG_DrawStringExt( CHATLOC_X + TINYCHAR_WIDTH, 
				CHATLOC_Y - (cgs.teamChatPos - i)*TINYCHAR_HEIGHT, 
				cgs.teamChatMsgs[i % chatHeight], hcolor, qfalse, qfalse,
				TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
	}
}
#endif // MISSIONPACK

/*
===================
CG_DrawHoldableItem
===================
*/
#ifndef MISSIONPACK
static void CG_DrawHoldableItem( void ) { 
	int		value;

	value = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];
	if ( value ) {
		CG_RegisterItemVisuals( value );
		CG_DrawPic( 640-ICON_SIZE, (SCREEN_HEIGHT-ICON_SIZE)/2, ICON_SIZE, ICON_SIZE, cg_items[ value ].icon );
	}

}
#endif // MISSIONPACK

/*
===================
CG_WorldToScreen

Projects a world point to 640x480 screen coordinates; qfalse if behind
===================
*/
static qboolean CG_WorldToScreen( const vec3_t point, float *x, float *y ) {
	vec3_t	vec;
	float	front, left, up;

	VectorSubtract( point, cg.refdef.vieworg, vec );
	front = DotProduct( vec, cg.refdef.viewaxis[0] );
	if ( front < 1 ) {
		return qfalse;
	}
	left = DotProduct( vec, cg.refdef.viewaxis[1] );
	up = DotProduct( vec, cg.refdef.viewaxis[2] );
	*x = 320 - left / front * 320 / tan( DEG2RAD( cg.refdef.fov_x * 0.5f ) );
	*y = 240 - up / front * 240 / tan( DEG2RAD( cg.refdef.fov_y * 0.5f ) );
	return qtrue;
}

/*
===================
CG_DrawShellShock

Shell-shocked: dark, pulsing edges for as long as it lasts
===================
*/
static void CG_DrawShellShock( void ) {
	vec4_t	edge = { 0, 0, 0, 0.55f };
	int		left = cg.snap->ps.powerups[PW_SHELLSHOCK] - cg.time;
	int		i;

	if ( left <= 0 ) {
		return;
	}
	edge[3] = 0.4f + 0.15f * sin( cg.time * 0.01f );
	for ( i = 0 ; i < 4 ; i++ ) {
		float	w = 24 + i * 16;

		edge[3] *= 0.75f;
		CG_FillRect( 0, 0, 640, w, edge );
		CG_FillRect( 0, 480 - w, 640, w, edge );
		CG_FillRect( 0, w, w, 480 - 2 * w, edge );
		CG_FillRect( 640 - w, w, w, 480 - 2 * w, edge );
	}
}

/*
===================
CG_PlayerRevealed

Piercing sight shows every enemy; marks and storms show the players they caught
===================
*/
qboolean CG_PlayerRevealed( int clientNum ) {
	playerState_t	*ps = &cg.snap->ps;
	centity_t		*cent = &cg_entities[clientNum];

	if ( clientNum == ps->clientNum || !cent->currentValid || cent->currentState.eType != ET_PLAYER
		|| ( cent->currentState.eFlags & EF_DEAD ) ) {
		return qfalse;
	}
	if ( cgs.gametype >= GT_TEAM && cgs.clientinfo[clientNum].team == ps->persistant[PERS_TEAM] ) {
		return qfalse;
	}
	if ( cent->currentState.eFlags & EF_MARKED ) {
		return qtrue;
	}
	return BG_Champion( ps->stats[STAT_CHAMPION] )->active == ACTIVE_PIERCINGSIGHT
		&& BG_AbilityState( ps ) == ABILITY_ACTIVE;
}

/*
===================
CG_RevealModel

Keeps a copy of a revealed player's model part, drawn again after the world
in a pass of its own so it shows through walls
===================
*/
void CG_RevealModel( const refEntity_t *ent, int clientNum ) {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS || cg.numRevealModels >= MAX_REVEAL_MODELS
		|| !CG_PlayerRevealed( clientNum ) ) {
		return;
	}
	cg.revealModels[cg.numRevealModels] = *ent;
	cg.revealModels[cg.numRevealModels].customShader = cgs.media.revealShader;
	cg.revealModels[cg.numRevealModels].renderfx &= ~( RF_FIRST_PERSON | RF_THIRD_PERSON );
	cg.numRevealModels++;
}

/*
===================
CG_DrawRevealedModels

The revealed players' silhouettes, over the rendered world
===================
*/
static void CG_DrawRevealedModels( void ) {
	refdef_t	refdef;
	int			i;

	if ( !cg.numRevealModels ) {
		return;
	}
	refdef = cg.refdef;
	refdef.rdflags |= RDF_NOWORLDMODEL;
	trap_R_ClearScene();
	for ( i = 0 ; i < cg.numRevealModels ; i++ ) {
		trap_R_AddRefEntityToScene( &cg.revealModels[i] );
	}
	trap_R_RenderScene( &refdef );
	cg.numRevealModels = 0;
}

#ifndef MISSIONPACK
/*
===================
CG_DrawAbility

Active ability icon, name, charges left and time to the next charge
===================
*/
static void CG_DrawAbility( void ) {
	static qhandle_t	icons[ACTIVE_NUM_ABILITIES];
	static const vec4_t	spent = { 1, 1, 1, 0.45f };
	const champion_t	*champ;
	playerState_t		*ps;
	const char			*s;
	int					y, tenths, w;
	float				*color;

	ps = &cg.snap->ps;
	champ = BG_Champion( ps->stats[STAT_CHAMPION] );
	if ( champ->active == ACTIVE_NONE ) {
		return;
	}

	color = BG_AbilityCharges( ps ) > 0 ? colorWhite : colorMdGrey;
	if ( BG_AbilityState( ps ) == ABILITY_ACTIVE || BG_AbilityState( ps ) == ABILITY_RUSH ) {
		color = colorYellow;
	}
	s = va( "%s %i", BG_String( bg_abilityInfo[champ->active].nameString ), BG_AbilityCharges( ps ) );
	y = ( SCREEN_HEIGHT + ICON_SIZE ) / 2 + 8;
	w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;
	CG_DrawStringExt( 640 - 8 - w, y, s, color, qfalse, qtrue,
		SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );

	// the icon left of the name, faded while there is no charge
	if ( !icons[champ->active] ) {
		icons[champ->active] = trap_R_RegisterShaderNoMip( va( "gfx/tremor/abilities/%s", bg_abilityInfo[champ->active].cvarName ) );
	}
	if ( icons[champ->active] ) {
		trap_R_SetColor( BG_AbilityCharges( ps ) > 0 ? NULL : spent );
		CG_DrawPic( 640 - 8 - w - 6 - 34, y - 1, 34, 34, icons[champ->active] );
		trap_R_SetColor( NULL );
	}

	tenths = ps->stats[STAT_ABILITY_RECHARGE];
	if ( tenths > 0 ) {
		// whole seconds, tenths under 3 s
		s = va( (char *)BG_String( STR_ABILITY_RECHARGE ),
			tenths < 30 ? va( "%i.%i", tenths / 10, tenths % 10 ) : va( "%i", ( tenths + 9 ) / 10 ) );
		CG_DrawStringExt( 640 - 8 - CG_DrawStrlen( s ) * SMALLCHAR_WIDTH, y + SMALLCHAR_HEIGHT + 2, s,
			colorMdGrey, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
	}
}
#endif // MISSIONPACK

#ifdef MISSIONPACK
/*
===================
CG_DrawPersistantPowerup
===================
*/
#if 0 // sos001208 - DEAD
static void CG_DrawPersistantPowerup( void ) { 
	int		value;

	value = cg.snap->ps.stats[STAT_PERSISTANT_POWERUP];
	if ( value ) {
		CG_RegisterItemVisuals( value );
		CG_DrawPic( 640-ICON_SIZE, (SCREEN_HEIGHT-ICON_SIZE)/2 - ICON_SIZE, ICON_SIZE, ICON_SIZE, cg_items[ value ].icon );
	}
}
#endif
#endif // MISSIONPACK


/*
===================
CG_DrawReward
===================
*/
#define MEDAL_ROW		4
#define MEDAL_SHOW		2000
#define MEDAL_FADE		300
#define MEDAL_POP		150
#define MEDAL_SIZE		44
#define MEDAL_Y			60

static struct {
	int			name;		// string id, 0 for the Quake III awards
	qhandle_t	shader;
	int			count;
} medalRow[MEDAL_ROW];
static int	medalRowCount, medalRowTime;

static void CG_DrawReward( void ) {
	static vec4_t	plate = { 0.65f, 0.08f, 0.06f, 0.9f };
	vec4_t			color = { 1, 1, 1, 1 }, c;
	int				i, t;
	float			size, a, x;
	const char		*name;

	if ( !cg_drawRewards.integer || !cg_medals.integer ) {
		cg.rewardStack = 0;
		medalRowCount = 0;
		return;
	}

	// the row is done: up to four waiting medals line up together; the
	// first one's announcer line plays, the others chime
	t = cg.time - medalRowTime;
	// medals wait while a banner word shows
	if ( CG_HudModern() && ( !medalRowCount || t >= MEDAL_SHOW + MEDAL_FADE ) && CG_BannerWordShowing() ) {
		medalRowCount = 0;
		return;
	}
	if ( !medalRowCount || t >= MEDAL_SHOW + MEDAL_FADE || t < 0 ) {
		medalRowCount = 0;
		while ( cg.rewardStack > 0 && medalRowCount < MEDAL_ROW ) {
			medalRow[medalRowCount].name = cg.rewardName[1];
			medalRow[medalRowCount].shader = cg.rewardShader[1];
			medalRow[medalRowCount].count = cg.rewardCount[1];
			if ( medalRowCount == 0 ) {
				trap_S_StartLocalSound( cg.rewardSound[1], CHAN_ANNOUNCER );
			}
			medalRowCount++;
			for ( i = 1 ; i < cg.rewardStack ; i++ ) {
				cg.rewardSound[i] = cg.rewardSound[i+1];
				cg.rewardShader[i] = cg.rewardShader[i+1];
				cg.rewardCount[i] = cg.rewardCount[i+1];
				cg.rewardName[i] = cg.rewardName[i+1];
			}
			cg.rewardStack--;
		}
		medalRowTime = cg.time;
		if ( !medalRowCount ) {
			return;
		}
		t = 0;
	}

	color[3] = t > MEDAL_SHOW ? 1.0f - (float)( t - MEDAL_SHOW ) / MEDAL_FADE : 1;
	if ( CG_HudModern() ) {
		// a row pinned by its bottom edge: 34 dp icons, each name beside its icon
		float	mx, my, icon = 34 * DP * ( t < MEDAL_POP ? 1.2f - 0.2f * t / MEDAL_POP : 1 ), total = 0, step[MEDAL_ROW];

		CG_HudPlace( "medals", &mx, &my );
		cg_hudAnchor = ANCHOR_CENTER;
		for ( i = 0 ; i < medalRowCount ; i++ ) {
			name = medalRow[i].name ? BG_String( medalRow[i].name ) : "";
			step[i] = 34 * DP + 4 + CG_DrawStrlen( name ) * SMALLCHAR_WIDTH
				+ ( medalRow[i].count > 1 ? 4 * TINYCHAR_WIDTH : 0 ) + 16;
			total += step[i];
		}
		x = mx - total / 2;
		for ( i = 0 ; i < medalRowCount ; i++ ) {
			name = medalRow[i].name ? BG_String( medalRow[i].name ) : "";
			if ( medalRow[i].shader ) {
				trap_R_SetColor( color );
				CG_DrawPic( x + ( 34 * DP - icon ) / 2, my - 34 * DP + ( 34 * DP - icon ) / 2, icon, icon, medalRow[i].shader );
				trap_R_SetColor( NULL );
			}
			CG_DrawStringExt( x + 34 * DP + 4, my - 17 * DP - SMALLCHAR_HEIGHT / 2, name, color, qfalse, qtrue,
				SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
			if ( medalRow[i].count > 1 ) {
				CG_DrawStringExt( x + 34 * DP + 8 + CG_DrawStrlen( name ) * SMALLCHAR_WIDTH, my - 17 * DP - TINYCHAR_HEIGHT / 2,
					va( "x%i", medalRow[i].count ), color, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			}
			x += step[i];
		}
		return;
	}
	// pops in from 1.2 times its size over 150 ms
	size = MEDAL_SIZE * ( t < MEDAL_POP ? 1.2f - 0.2f * t / MEDAL_POP : 1 );
	for ( i = 0 ; i < medalRowCount ; i++ ) {
		x = 320 + ( i - ( medalRowCount - 1 ) * 0.5f ) * ( MEDAL_SIZE + 36 );
		name = medalRow[i].name ? BG_String( medalRow[i].name ) : "";
		if ( medalRow[i].shader ) {
			trap_R_SetColor( color );
			CG_DrawPic( x - size / 2, MEDAL_Y + ( MEDAL_SIZE - size ) / 2, size, size, medalRow[i].shader );
			trap_R_SetColor( NULL );
		} else {
			Vector4Copy( plate, c );
			c[3] *= color[3];
			CG_FillRect( x - size / 2, MEDAL_Y + ( MEDAL_SIZE - size ) / 2, size, size, c );
			if ( name[0] ) {
				CG_DrawStringExt( x - BIGCHAR_WIDTH / 2, MEDAL_Y + ( MEDAL_SIZE - BIGCHAR_HEIGHT ) / 2, va( "%c", name[0] ),
					color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
			}
		}
		if ( name[0] ) {
			a = CG_DrawStrlen( name ) * TINYCHAR_WIDTH;
			CG_DrawStringExt( x - a / 2, MEDAL_Y + MEDAL_SIZE + 3, name, color, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
		if ( medalRow[i].count > 1 ) {
			CG_DrawStringExt( x + MEDAL_SIZE / 2 - 4, MEDAL_Y, va( "x%i", medalRow[i].count ), color, qfalse, qtrue,
				TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
	}
}


/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define	LAG_SAMPLES		128


typedef struct {
	int		frameSamples[LAG_SAMPLES];
	int		frameCount;
	int		snapshotFlags[LAG_SAMPLES];
	int		snapshotSamples[LAG_SAMPLES];
	int		snapshotCount;
} lagometer_t;

lagometer_t		lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo( void ) {
	int			offset;

	offset = cg.time - cg.latestSnapshotTime;
	lagometer.frameSamples[ lagometer.frameCount & ( LAG_SAMPLES - 1) ] = offset;
	lagometer.frameCount++;
}

/*
==============
CG_AddLagometerSnapshotInfo

Each time a snapshot is received, log its ping time and
the number of snapshots that were dropped before it.

Pass NULL for a dropped packet.
==============
*/
void CG_AddLagometerSnapshotInfo( snapshot_t *snap ) {
	// dropped packet
	if ( !snap ) {
		lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = -1;
		lagometer.snapshotCount++;
		return;
	}

	// add this snapshot's info
	lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = snap->ping;
	lagometer.snapshotFlags[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = snap->snapFlags;
	lagometer.snapshotCount++;
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect( void ) {
	float		x, y;
	int			cmdNum;
	usercmd_t	cmd;
	const char		*s;
	int			w;

	// draw the phone jack if we are completely past our buffers
	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd( cmdNum, &cmd );
	if ( cmd.serverTime <= cg.snap->ps.commandTime
		|| cmd.serverTime > cg.time ) {	// special check for map_restart
		return;
	}

	// also add text in center of screen
	s = "Connection Interrupted";
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	CG_DrawBigString( 320 - w/2, 100, s, 1.0F);

	// blink the icon
	if ( ( cg.time >> 9 ) & 1 ) {
		return;
	}

#ifdef MISSIONPACK
	x = 640 - 48;
	y = 480 - 144;
#else
	x = 640 - 48;
	y = 480 - 48;
#endif

	CG_DrawPic( x, y, 48, 48, trap_R_RegisterShader("gfx/2d/net.tga" ) );
}


#define	MAX_LAGOMETER_PING	900
#define	MAX_LAGOMETER_RANGE	300

/*
==============
CG_DrawLagometer
==============
*/
static void CG_DrawLagometer( void ) {
	int		a, x, y, i;
	float	v;
	float	ax, ay, aw, ah, mid, range;
	int		color;
	float	vscale;

	if ( !cg_lagometer.integer || cgs.localServer ) {
		CG_DrawDisconnect();
		return;
	}

	//
	// draw the graph
	//
#ifdef MISSIONPACK
	x = 640 - 48;
	y = 480 - 144;
#else
	x = 640 - 48;
	y = 480 - 48;
#endif

	trap_R_SetColor( NULL );
	CG_DrawPic( x, y, 48, 48, cgs.media.lagometerShader );

	ax = x;
	ay = y;
	aw = 48;
	ah = 48;
	CG_AdjustFrom640( &ax, &ay, &aw, &ah );

	color = -1;
	range = ah / 3;
	mid = ay + range;

	vscale = range / MAX_LAGOMETER_RANGE;

	// draw the frame interpoalte / extrapolate graph
	for ( a = 0 ; a < aw ; a++ ) {
		i = ( lagometer.frameCount - 1 - a ) & (LAG_SAMPLES - 1);
		v = lagometer.frameSamples[i];
		v *= vscale;
		if ( v > 0 ) {
			if ( color != 1 ) {
				color = 1;
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
			}
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPic ( ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		} else if ( v < 0 ) {
			if ( color != 2 ) {
				color = 2;
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_BLUE)] );
			}
			v = -v;
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPic( ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		}
	}

	// draw the snapshot latency / drop graph
	range = ah / 2;
	vscale = range / MAX_LAGOMETER_PING;

	for ( a = 0 ; a < aw ; a++ ) {
		i = ( lagometer.snapshotCount - 1 - a ) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];
		if ( v > 0 ) {
			if ( lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED ) {
				if ( color != 5 ) {
					color = 5;	// YELLOW for rate delay
					trap_R_SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
				}
			} else {
				if ( color != 3 ) {
					color = 3;
					trap_R_SetColor( g_color_table[ColorIndex(COLOR_GREEN)] );
				}
			}
			v = v * vscale;
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPic( ax + aw - a, ay + ah - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
		} else if ( v < 0 ) {
			if ( color != 4 ) {
				color = 4;		// RED for dropped snapshots
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_RED)] );
			}
			trap_R_DrawStretchPic( ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, cgs.media.whiteShader );
		}
	}

	trap_R_SetColor( NULL );

	if ( cg_nopredict.integer || cg_synchronousClients.integer ) {
		CG_DrawBigString( x, y, "snc", 1.0 );
	}

	CG_DrawDisconnect();
}



/*
===============================================================================

CENTER PRINTING

===============================================================================
*/


/*
==============================================================================

FRAG BANNER

Centre of the upper third when you frag someone: "FRAGGED" small, the
victim large, then the score line; 2 s, fading 0.3 s; quick frags within
3 s count up "x2", "x3" (messages.md 2.2)

==============================================================================
*/

#define FRAG_TIME		2000
#define FRAG_FADE		1000
#define FRAG_FADE_IN	200
#define FRAG_QUICK		3000
#define FRAG_Y			134

static struct {
	char	fragger[MAX_QPATH];		// empty: YOU
	char	victim[MAX_QPATH];
	char	line[64];
	int		time;
	int		count;
} fragBanner;

void CG_FragBanner( const char *fragger, const char *victim, const char *line ) {
	Q_strncpyz( fragBanner.fragger, fragger ? fragger : "", sizeof( fragBanner.fragger ) );
	fragBanner.count = fragBanner.time && cg.time - fragBanner.time < FRAG_QUICK ? fragBanner.count + 1 : 1;
	Q_strncpyz( fragBanner.victim, victim, sizeof( fragBanner.victim ) );
	Q_strncpyz( fragBanner.line, line, sizeof( fragBanner.line ) );
	fragBanner.time = cg.time;
}

static void CG_DrawFragBanner( void ) {
	vec4_t		color = { 1, 1, 1, 1 }, red = { 0.83f, 0.1f, 0.12f, 1 };
	const char	*fragged = BG_String( STR_MSG_FRAGGED );
	int			t = cg.time - fragBanner.time, w;

	if ( !cg_fragBanner.integer || !fragBanner.time || t < 0 || t >= FRAG_TIME + FRAG_FADE ) {
		return;
	}
	color[3] = red[3] = t > FRAG_TIME ? 1.0f - ( t - FRAG_TIME ) / (float)FRAG_FADE : 1;
	if ( CG_HudModern() ) {
		// "YOU FRAGGED <name>" 30 dp tall, its top at + 412, fading in over
		// 0.2 s; in FFA the place line under it, 18 dp grey, its top at + 444
		float	fx, fy, ch = 30 * DP, cw = ch * 0.8f, ph = 18 * DP, pw = ph * 0.8f;
		vec4_t	grey = { 0.74f, 0.74f, 0.74f, 1 };
		char	line[128];

		if ( t < FRAG_FADE_IN ) {
			color[3] = t / (float)FRAG_FADE_IN;
		}
		grey[3] = red[3] = color[3];
		CG_HudPlace( "fragline", &fx, &fy );
		cg_hudAnchor = ANCHOR_CENTER;
		if ( fragBanner.fragger[0] ) {
			Com_sprintf( line, sizeof( line ), "%s^7 %s %s", fragBanner.fragger, BG_String( STR_MSG_FRAGGED ), fragBanner.victim );
		} else {
			Com_sprintf( line, sizeof( line ), "%s %s", BG_String( STR_MSG_YOUFRAGGED ), fragBanner.victim );
		}
		w = CG_DrawStrlen( line ) * cw;
		CG_DrawStringExt( fx - w / 2, fy, line, color, qfalse, qtrue, cw, ch, 0 );
		if ( fragBanner.count > 1 ) {
			// quick frags within 3 s: x2, x3
			CG_DrawStringExt( fx + w / 2 + 6, fy, va( "x%i", fragBanner.count ), red, qtrue, qtrue, cw, ch, 0 );
		}
		if ( fragBanner.line[0] && cgs.gametype == GT_FFA ) {
			w = CG_DrawStrlen( fragBanner.line ) * pw;
			CG_DrawStringExt( fx - w / 2, fy + 32 * DP, fragBanner.line, grey, qtrue, qtrue, pw, ph, 0 );
		}
		return;
	}
	w = CG_DrawStrlen( fragged ) * SMALLCHAR_WIDTH;
	CG_DrawStringExt( 320 - w / 2, FRAG_Y, fragged, red, qtrue, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
	w = CG_DrawStrlen( fragBanner.victim ) * BIGCHAR_WIDTH;
	CG_DrawStringExt( 320 - w / 2, FRAG_Y + 18, fragBanner.victim, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	if ( fragBanner.count > 1 ) {
		CG_DrawStringExt( 320 + w / 2 + 6, FRAG_Y + 18, va( "x%i", fragBanner.count ), red, qtrue, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	}
	if ( fragBanner.line[0] ) {
		w = CG_DrawStrlen( fragBanner.line ) * SMALLCHAR_WIDTH;
		CG_DrawStringExt( 320 - w / 2, FRAG_Y + 40, fragBanner.line, color, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
	}
}

/*
==============================================================================

EVENT BANNER

Top centre under the clock: one message at a time, 3 s each, queued by
priority (1 match flow, 2 objectives, 3 power-ups, 4 lead and time, 5
server and other text); a higher priority cuts a lower one short
(messages.md 2.4)

==============================================================================
*/

#define BANNER_QUEUE	8
#define BANNER_TIME		3000
#define BANNER_Y		34

typedef struct {
	char	text[256];
	int		priority;
	int		time;		// when shown; 0 while queued
} banner_t;

static banner_t	bannerShown;
static banner_t	bannerQueue[BANNER_QUEUE];
static int		bannerQueued;

static qboolean CG_BannerWordShowing( void ) {
	return bannerShown.time && bannerShown.priority == 1 && cg.time - bannerShown.time < BANNER_TIME;
}

/*
==============
CG_EventBanner
==============
*/
void CG_EventBanner( const char *text, int priority ) {
	banner_t	*b;

	if ( !text[0] ) {
		return;
	}
	CG_Printf( "[skipnotify]%s\n", text );		// the console keeps every line
	if ( bannerQueued == BANNER_QUEUE ) {
		// full: the least important waiting message goes (the oldest of those)
		int		worst = 0, i;

		for ( i = 1 ; i < BANNER_QUEUE ; i++ ) {
			if ( bannerQueue[i].priority > bannerQueue[worst].priority ) {
				worst = i;
			}
		}
		if ( bannerQueue[worst].priority < priority ) {
			return;		// everything waiting matters more
		}
		memmove( bannerQueue + worst, bannerQueue + worst + 1, sizeof( bannerQueue[0] ) * ( BANNER_QUEUE - worst - 1 ) );
		bannerQueued--;
	}
	b = &bannerQueue[bannerQueued++];
	Q_strncpyz( b->text, text, sizeof( b->text ) );
	b->priority = priority;
	b->time = 0;
}

/*
==============
CG_CenterPrint

Quake III's centre text, now an event banner: "FIGHT!" leads the match
flow, everything else (server text) comes last
==============
*/
void CG_CenterPrint( const char *str, int y, int charWidth ) {
	CG_EventBanner( str, !Q_stricmp( str, "FIGHT!" ) ? 1 : 5 );
}

/*
===================
CG_DrawCenterString

The event banner: picks the next message and draws it
===================
*/
static void CG_DrawCenterString( void ) {
	int			i, best = -1, line, y, w;
	const char	*start;
	char		linebuffer[256];
	vec4_t		color = { 1, 1, 1, 1 };
	float		fade;

	if ( !cg_eventBanner.integer ) {
		bannerQueued = 0;
		return;
	}
	for ( i = 0 ; i < bannerQueued ; i++ ) {
		if ( best < 0 || bannerQueue[i].priority < bannerQueue[best].priority ) {
			best = i;
		}
	}
	// the next one when the shown one is done, or at once if it matters more
	if ( best >= 0 && ( !bannerShown.time || cg.time - bannerShown.time >= BANNER_TIME
		|| bannerQueue[best].priority < bannerShown.priority ) ) {
		bannerShown = bannerQueue[best];
		bannerShown.time = cg.time;
		memmove( bannerQueue + best, bannerQueue + best + 1, sizeof( bannerQueue[0] ) * ( bannerQueued - best - 1 ) );
		bannerQueued--;
	}
	if ( !bannerShown.time || cg.time - bannerShown.time >= BANNER_TIME || bannerShown.time > cg.time ) {
		bannerShown.time = 0;
		return;
	}
	fade = BANNER_TIME - ( cg.time - bannerShown.time );
	color[3] = fade < 100 ? fade / 100.0f : 1;

	y = BANNER_Y;
	if ( CG_HudModern() ) {
		float	bx, by;

		if ( bannerShown.priority == 1 ) {
			// the banner word: large, its foot at top + 350
			CG_HudPlace( "bannerword", &bx, &by );
			cg_hudAnchor = ANCHOR_CENTER;
			w = CG_DrawStrlen( bannerShown.text ) * GIANTCHAR_WIDTH;
			CG_DrawStringExt( 320 - w / 2, by - GIANTCHAR_HEIGHT / 2, bannerShown.text, color, qfalse, qtrue,
				GIANTCHAR_WIDTH, GIANTCHAR_HEIGHT, 0 );
			return;
		}
		if ( cg.deathTime && cg.snap->ps.stats[STAT_HEALTH] <= 0 ) {
			return;		// the fragged-by panel has the centre
		}
		CG_HudPlace( "eventline", &bx, &by );
		cg_hudAnchor = ANCHOR_CENTER;
		y = by - BIGCHAR_HEIGHT / 2;
	}
	start = bannerShown.text;
	for ( line = 0 ; line < 3 && *start ; line++ ) {
		for ( i = 0 ; start[i] && start[i] != '\n' && i < (int)sizeof( linebuffer ) - 1 ; i++ ) {
			linebuffer[i] = start[i];
		}
		linebuffer[i] = 0;
		w = BIGCHAR_WIDTH * CG_DrawStrlen( linebuffer );
		CG_DrawStringExt( ( SCREEN_WIDTH - w ) / 2, y, linebuffer, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
		y += BIGCHAR_HEIGHT + 4;
		start += i;
		while ( *start == '\n' ) {
			start++;
		}
	}
}



/*
================================================================================

CROSSHAIR

================================================================================
*/


/*
=================
CG_DrawCrosshair
=================
*/
static void CG_DrawCrosshair(void)
{
	float		w, h;
	qhandle_t	hShader;
	float		f;
	float		x, y;
	int			ca;

	if ( !cg_drawCrosshair.integer ) {
		return;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		return;
	}

	if ( cg.renderingThirdPerson ) {
		return;
	}

	// the chosen colour, else colour by health
	if ( cg_crosshairColor.integer > 0 ) {
		static const vec4_t	palette[] = {
			{ 1, 1, 1, 1 }, { 1, 0.2f, 0.2f, 1 }, { 0.2f, 1, 0.2f, 1 }, { 1, 1, 0.2f, 1 },
			{ 0.2f, 0.4f, 1, 1 }, { 0.2f, 1, 1, 1 }, { 1, 0.3f, 1, 1 }, { 1, 0.6f, 0.1f, 1 }
		};

		trap_R_SetColor( palette[cg_crosshairColor.integer % ARRAY_LEN( palette )] );
	} else if ( cg_crosshairHealth.integer ) {
		vec4_t		hcolor;

		CG_ColorForHealth( hcolor );
		trap_R_SetColor( hcolor );
	} else {
		trap_R_SetColor( NULL );
	}

	w = h = cg_crosshairSize.value;

	// pulse the size of the crosshair when picking up items
	f = cg.time - cg.itemPickupBlendTime;
	if ( f > 0 && f < ITEM_BLOB_TIME ) {
		f /= ITEM_BLOB_TIME;
		w *= ( 1 + f );
		h *= ( 1 + f );
	}

	x = cg_crosshairX.integer;
	y = cg_crosshairY.integer;
	// an offset from the view's centre, so scaled but not anchored
	x *= cgs.screenXScale;
	y *= cgs.screenYScale;
	w *= cgs.screenXScale;
	h *= cgs.screenYScale;

	ca = cg_drawCrosshair.integer;
	if (ca < 0) {
		ca = 0;
	}
	hShader = cgs.media.crosshairShader[ ca % NUM_CROSSHAIRS ];

	trap_R_DrawStretchPic( x + cg.refdef.x + 0.5 * (cg.refdef.width - w), 
		y + cg.refdef.y + 0.5 * (cg.refdef.height - h), 
		w, h, 0, 0, 1, 1, hShader );

	trap_R_SetColor( NULL );
}

/*
=================
CG_DrawCrosshair3D
=================
*/
static void CG_DrawCrosshair3D(void)
{
	float		w;
	qhandle_t	hShader;
	float		f;
	int			ca;

	trace_t trace;
	vec3_t endpos;
	float stereoSep, zProj, maxdist, xmax;
	char rendererinfos[128];
	refEntity_t ent;

	if ( !cg_drawCrosshair.integer ) {
		return;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		return;
	}

	if ( cg.renderingThirdPerson ) {
		return;
	}

	w = cg_crosshairSize.value;

	// pulse the size of the crosshair when picking up items
	f = cg.time - cg.itemPickupBlendTime;
	if ( f > 0 && f < ITEM_BLOB_TIME ) {
		f /= ITEM_BLOB_TIME;
		w *= ( 1 + f );
	}

	ca = cg_drawCrosshair.integer;
	if (ca < 0) {
		ca = 0;
	}
	hShader = cgs.media.crosshairShader[ ca % NUM_CROSSHAIRS ];

	// Use a different method rendering the crosshair so players don't see two of them when
	// focusing their eyes at distant objects with high stereo separation
	// We are going to trace to the next shootable object and place the crosshair in front of it.

	// first get all the important renderer information
	trap_Cvar_VariableStringBuffer("r_zProj", rendererinfos, sizeof(rendererinfos));
	zProj = atof(rendererinfos);
	trap_Cvar_VariableStringBuffer("r_stereoSeparation", rendererinfos, sizeof(rendererinfos));
	stereoSep = zProj / atof(rendererinfos);
	
	xmax = zProj * tan(cg.refdef.fov_x * M_PI / 360.0f);
	
	// let the trace run through until a change in stereo separation of the crosshair becomes less than one pixel.
	maxdist = cgs.glconfig.vidWidth * stereoSep * zProj / (2 * xmax);
	VectorMA(cg.refdef.vieworg, maxdist, cg.refdef.viewaxis[0], endpos);
	CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, endpos, 0, MASK_SHOT);
	
	memset(&ent, 0, sizeof(ent));
	ent.reType = RT_SPRITE;
	ent.renderfx = RF_DEPTHHACK | RF_CROSSHAIR;
	
	VectorCopy(trace.endpos, ent.origin);
	
	// scale the crosshair so it appears the same size for all distances
	ent.radius = w / 640 * xmax * trace.fraction * maxdist / zProj;
	ent.customShader = hShader;

	trap_R_AddRefEntityToScene(&ent);
}



/*
=================
CG_ScanForCrosshairEntity
=================
*/
static void CG_ScanForCrosshairEntity( void ) {
	trace_t		trace;
	vec3_t		start, end;
	int			content;

	VectorCopy( cg.refdef.vieworg, start );
	VectorMA( start, 131072, cg.refdef.viewaxis[0], end );

	CG_Trace( &trace, start, vec3_origin, vec3_origin, end, 
		cg.snap->ps.clientNum, CONTENTS_SOLID|CONTENTS_BODY );
	if ( trace.entityNum >= MAX_CLIENTS ) {
		return;
	}

	// if the player is in fog, don't show it
	content = CG_PointContents( trace.endpos, 0 );
	if ( content & CONTENTS_FOG ) {
		return;
	}

	// if the player is invisible, don't show it
	if ( cg_entities[ trace.entityNum ].currentState.powerups & ( 1 << PW_INVIS ) ) {
		return;
	}

	// update the fade timer
	cg.crosshairClientNum = trace.entityNum;
	cg.crosshairClientTime = cg.time;
}


/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void CG_DrawCrosshairNames( void ) {
	float		*color;
	char		*name;
	float		w;

	if ( !cg_drawCrosshair.integer ) {
		return;
	}
	if ( !cg_drawCrosshairNames.integer ) {
		return;
	}
	if ( cg.renderingThirdPerson ) {
		return;
	}

	// scan the known entities to see if the crosshair is sighted on one
	CG_ScanForCrosshairEntity();

	// draw the name of the player being looked at
	color = CG_FadeColor( cg.crosshairClientTime, 1000 );
	if ( !color ) {
		trap_R_SetColor( NULL );
		return;
	}

	name = cgs.clientinfo[ cg.crosshairClientNum ].name;
#ifdef MISSIONPACK
	color[3] *= 0.5f;
	w = CG_Text_Width(name, 0.3f, 0);
	CG_Text_Paint( 320 - w / 2, 190, 0.3f, color, name, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
#else
	w = CG_DrawStrlen( name ) * BIGCHAR_WIDTH;
	CG_DrawBigString( 320 - w / 2, 170, name, color[3] * 0.5f );
#endif

	// a teammate: their health and armour (from the team overlay info)
	if ( cgs.gametype >= GT_TEAM && cgs.clientinfo[cg.crosshairClientNum].team == cg.snap->ps.persistant[PERS_TEAM]
		&& cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		const char	*s = va( "%i / %i", cgs.clientinfo[cg.crosshairClientNum].health,
			cgs.clientinfo[cg.crosshairClientNum].armor );

		CG_DrawSmallStringColor( 320 - CG_DrawStrlen( s ) * SMALLCHAR_WIDTH / 2, 170 + BIGCHAR_HEIGHT + 2, s, color );
	}
	trap_R_SetColor( NULL );
}

/*
=================
CG_DrawDamageArc

A red arc around the crosshair toward where damage came from, one for
each of the last four hits, fading over 1.2 s, brighter for bigger hits
=================
*/
#define DAMAGE_ARC_TIME		1200
#define DAMAGE_ARC_RADIUS	56
#define DAMAGE_ARC_SPAN		40		// degrees
static void CG_DrawDamageArc( void ) {
	vec4_t	color = { 0.9f, 0.08f, 0.05f, 1 };
	float	t, a, rad, x, y, value, dot;
	int		i, n;

	if ( !cg_damageArc.integer ) {
		return;
	}
	for ( n = 0 ; n < DAMAGE_ARCS ; n++ ) {
		if ( !cg.damageArcTime[n] ) {
			continue;
		}
		t = cg.time - cg.damageArcTime[n];
		if ( t < 0 || t >= DAMAGE_ARC_TIME ) {
			continue;
		}
		value = cg.damageArcValue[n];
		dot = value >= 50 ? 6 : 4;		// a hit of 50 or more draws the big arc
		color[3] = ( 1.0f - t / DAMAGE_ARC_TIME ) * ( 0.4f + 0.6f * ( value > 50 ? 1.0f : value / 50.0f ) );
		for ( i = 0 ; i <= 10 ; i++ ) {
			a = cg.damageArcYaw[n] + DAMAGE_ARC_SPAN * ( i / 10.0f - 0.5f );
			rad = DEG2RAD( a );
			x = 320 - sin( rad ) * DAMAGE_ARC_RADIUS;
			y = 240 - cos( rad ) * DAMAGE_ARC_RADIUS;
			CG_FillRect( x - dot / 2, y - dot / 2, dot, dot, color );
		}
	}
}

/*
=================
CG_DrawLowHealth

Below 25 health: a pulsing red edge and a heartbeat each second, when the
install has a heartbeat sound
=================
*/
#define LOW_HEALTH			25
static void CG_DrawLowHealth( void ) {
	static qboolean		loaded;
	static sfxHandle_t	beat;
	vec4_t	color = { 0.8f, 0.0f, 0.0f, 0 };
	int		health = cg.snap->ps.stats[STAT_HEALTH];

	if ( !cg_lowHealthWarning.integer || health <= 0 || health >= LOW_HEALTH ) {
		return;
	}
	color[3] = 0.2f + 0.15f * sin( cg.time * 0.00628f );
	CG_FillRect( 0, 0, SCREEN_WIDTH, 10, color );
	CG_FillRect( 0, SCREEN_HEIGHT - 10, SCREEN_WIDTH, 10, color );
	cg_hudAnchor = ANCHOR_LEFT;
	CG_FillRect( 0, 10, 10, SCREEN_HEIGHT - 20, color );
	cg_hudAnchor = ANCHOR_RIGHT;
	CG_FillRect( SCREEN_WIDTH - 10, 10, 10, SCREEN_HEIGHT - 20, color );
	cg_hudAnchor = ANCHOR_CENTER;

	if ( !loaded ) {
		loaded = qtrue;
		beat = trap_S_RegisterSound( "sound/feedback/heartbeat.wav", qfalse );
	}
	if ( beat && cg.time - cg.lowHealthBeat >= 1000 ) {
		cg.lowHealthBeat = cg.time;
		trap_S_StartLocalSound( beat, CHAN_LOCAL_SOUND );
	}
}


//==============================================================================

/*
=================
CG_DrawSpectator
=================
*/
static void CG_DrawSpectatorAbilities( void ) {
	int		i, x, y, w, state, charges, left;
	float	*color;
	char	name[MAX_QPATH];
	vec4_t	back = { 0, 0, 0, 0.5f };
	vec4_t	ready = { 0.3f, 1, 0.3f, 1 }, cooling = { 1, 0.8f, 0.2f, 1 }, active = { 0.4f, 0.7f, 1, 1 };

	if ( !cg.specAbilityCount || cg.time - cg.specAbilityTime > 3000 ) {
		return;
	}
	w = 640 / ( cg.specAbilityCount > 8 ? 8 : cg.specAbilityCount );
	if ( w > 96 ) {
		w = 96;
	}
	for ( i = 0; i < cg.specAbilityCount; i++ ) {
		clientInfo_t *ci = &cgs.clientinfo[cg.specAbilityClient[i] & ( MAX_CLIENTS - 1 )];

		x = 320 - ( ( cg.specAbilityCount > 8 ? 8 : cg.specAbilityCount ) * w ) / 2 + ( i % 8 ) * w;
		y = 380 - ( i / 8 ) * 30;
		CG_FillRect( x + 1, y, w - 2, 26, back );
		{
			qhandle_t	pic = CG_ChampionPortrait( BG_ChampionForModel( ci->modelName ), qtrue );

			if ( pic ) {
				CG_DrawPic( x + w - 26, y + 1, 24, 24, pic );
			}
			CG_DrawStringExt( x + w - 26 - 7 * TINYCHAR_WIDTH, y + 18, va( "%3i %3i", cg.specHealth[i], cg.specArmor[i] ),
				colorWhite, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
		// big items held: power-ups and flags, small over the portrait's left
		{
			centity_t	*cent = &cg_entities[cg.specAbilityClient[i] & ( MAX_CLIENTS - 1 )];
			int			powerups = cent->currentValid ? cent->currentState.powerups : ci->powerups;
			int			pw, n = 0;

			for ( pw = PW_QUAD ; pw < PW_NUM_POWERUPS && n < 3 ; pw++ ) {
				gitem_t	*item;

				if ( !( powerups & ( 1 << pw ) ) || !( item = BG_FindItemForPowerup( pw ) ) ) {
					continue;
				}
				CG_DrawPic( x + w - 38, y + 1 + n * 8, 10, 10, trap_R_RegisterShaderNoMip( item->icon ) );
				n++;
			}
		}
		Q_strncpyz( name, ci->name, sizeof( name ) );
		Q_CleanStr( name );
		name[8] = 0;
		CG_DrawSmallStringColor( x + 4, y + 1, name,
			ci->team == TEAM_RED ? colorRed : ci->team == TEAM_BLUE ? colorBlue : colorWhite );

		state = ( cg.specAbilityStat[i] & 0xffff ) >> ABILITY_STATE_SHIFT;
		charges = cg.specAbilityStat[i] & ABILITY_CHARGES_MASK;
		left = cg.specAbilityRecharge[i] * 100 - ( cg.time - cg.specAbilityTime );
		if ( state != ABILITY_IDLE ) {
			color = active;
			Q_strncpyz( name, "ACTIVE", sizeof( name ) );
		} else if ( charges > 0 ) {
			color = ready;
			Q_strncpyz( name, charges > 1 ? va( "READY x%i", charges ) : "READY", sizeof( name ) );
		} else {
			color = cooling;
			Com_sprintf( name, sizeof( name ), "%is", left > 0 ? ( left + 999 ) / 1000 : 0 );
		}
		CG_DrawSmallStringColor( x + 4, y + 13, name, color );
	}
}

/*
=================
CG_DrawNextSpawn

"Next spawn: <champion>" while the champion picked in the menu waits for a
respawn (not where champions are fixed for the match)
=================
*/
static void CG_DrawDeathScreen( void ) {
	char			line[128];
	const char		*how = "";
	clientInfo_t	*ci;
	gitem_t			*item;
	vec4_t			back = { 0, 0, 0, 0.6f }, grey = { 0.65f, 0.65f, 0.65f, 1 }, red = { 0.83f, 0.1f, 0.12f, 1 };
	int				left, y = 300, w;

	if ( !cg.deathTime || cg.snap->ps.stats[STAT_HEALTH] > 0 || cg.time - cg.deathTime > 30000 ) {
		return;
	}
	CG_FillRect( 140, y - 6, 360, cg.deathKiller >= 0 ? 104 : 46, back );

	if ( cg.deathKiller >= 0 && cg.deathKiller < MAX_CLIENTS ) {
		int champ;

		ci = &cgs.clientinfo[cg.deathKiller];
		champ = BG_ChampionForModel( ci->modelName );
		// a weapon, or the champion's ability for ability deaths
		if ( cg.deathMod >= MOD_HEAVYWEIGHT && champ >= 0 && BG_Champion( champ )->active > ACTIVE_NONE ) {
			how = BG_String( bg_abilityInfo[BG_Champion( champ )->active].nameString );
		} else if ( cg.deathWeapon > WP_NONE && cg.deathWeapon < WP_NUM_WEAPONS && cg_weapons[cg.deathWeapon].item ) {
			// (a killer already dead holds no weapon: a trade kill)
			item = cg_weapons[cg.deathWeapon].item;
			how = item->pickup_name;
		}
		Com_sprintf( line, sizeof( line ), BG_String( STR_DEATH_KILLEDBY ), ci->name );
		w = CG_DrawStrlen( line ) * BIGCHAR_WIDTH;
		CG_DrawBigString( 320 - w / 2, y, line, 1.0f );
		y += BIGCHAR_HEIGHT + 4;
		Com_sprintf( line, sizeof( line ), "%s  -  %s", champ >= 0 ? BG_String( BG_Champion( champ )->nameString ) : "", how );
		w = CG_DrawStrlen( line ) * SMALLCHAR_WIDTH;
		CG_DrawSmallStringColor( 320 - w / 2, y, line, grey );
		y += SMALLCHAR_HEIGHT + 8;
		Com_sprintf( line, sizeof( line ), BG_String( STR_DEATH_LEFT ), cg.deathHealth, cg.deathArmor );
		w = CG_DrawStrlen( line ) * SMALLCHAR_WIDTH;
		CG_DrawSmallStringColor( 320 - w / 2, y, line, red );
		y += SMALLCHAR_HEIGHT + 4;
		Com_sprintf( line, sizeof( line ), BG_String( STR_DEATH_DAMAGE ), cg.deathDealt, cg.deathTaken );
		w = CG_DrawStrlen( line ) * SMALLCHAR_WIDTH;
		CG_DrawSmallStringColor( 320 - w / 2, y, line, colorWhite );
		y += SMALLCHAR_HEIGHT + 8;
	}

	left = cg.deathRespawn - ( cg.time - cg.deathTime );
	if ( left > 0 ) {
		Com_sprintf( line, sizeof( line ), BG_String( STR_DEATH_RESPAWN_IN ), ( left + 999 ) / 1000 );
	} else {
		Q_strncpyz( line, BG_String( STR_DEATH_RESPAWN ), sizeof( line ) );
	}
	w = CG_DrawStrlen( line ) * SMALLCHAR_WIDTH;
	CG_DrawSmallStringColor( 320 - w / 2, y, line, grey );
}

static void CG_DrawNextSpawn( void ) {
	char		name[MAX_QPATH];
	const char	*info = CG_ConfigString( CS_SERVERINFO );
	int			picked, current = cg.snap->ps.stats[STAT_CHAMPION];
	vec4_t		color = { 1, 1, 1, 0.8f };

	if ( atoi( Info_ValueForKey( info, "g_roundDuel" ) ) || ( cgs.gametype == GT_TOURNAMENT && !cg.warmup ) ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( "champion", name, sizeof( name ) );
	picked = BG_ChampionForName( name );
	if ( picked < 0 || picked == current ) {
		return;
	}
	Com_sprintf( name, sizeof( name ), BG_String( STR_HUD_NEXT_SPAWN ), BG_String( BG_Champion( picked )->nameString ) );
	CG_DrawSmallStringColor( 320 - CG_DrawStrlen( name ) * SMALLCHAR_WIDTH / 2, 360, name, color );
}

/*
=================
CG_DrawTrailGrey

Settings > Champions: the world greyed while you lay Plasma Trail
=================
*/
static void CG_DrawTrailGrey( void ) {
	playerState_t	*ps = &cg.snap->ps;
	vec4_t			grey;

	if ( cg_trailGrey.value <= 0 || BG_Champion( ps->stats[STAT_CHAMPION] )->active != ACTIVE_PLASMATRAIL
		|| BG_AbilityState( ps ) == ABILITY_IDLE ) {
		return;
	}
	grey[0] = grey[1] = grey[2] = 0.35f;
	grey[3] = 0.45f * ( cg_trailGrey.value > 1 ? 1 : cg_trailGrey.value );
	CG_FillRect( -cgs.screenXBias / cgs.screenXScale, 0, 640 + 2 * cgs.screenXBias / cgs.screenXScale, 480, grey );
}

static void CG_DrawSpectator(void) {
	// the ability strip only in free camera; following shows that player's HUD
	if ( !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
		CG_DrawSpectatorAbilities();
	}
	CG_DrawBigString(320 - 9 * 8, 440, "SPECTATOR", 1.0F);
	if ( cgs.gametype == GT_TOURNAMENT ) {
		CG_DrawBigString(320 - 15 * 8, 460, "waiting to play", 1.0F);
	}
	else if ( cgs.gametype >= GT_TEAM ) {
		CG_DrawBigString(320 - 39 * 8, 460, "press ESC and use the JOIN menu to play", 1.0F);
	}
}

/*
=================
CG_DrawVote
=================
*/
/*
=================
CG_BindName

A short name for the key bound to a command: F1-F15, a letter or digit,
else "?"
=================
*/
static const char *CG_BindName( const char *binding ) {
	int		key = trap_Key_GetKey( binding );

	if ( key >= K_F1 && key <= K_F15 ) {
		return va( "F%i", key - K_F1 + 1 );
	}
	if ( key > K_SPACE && key < 127 ) {
		return va( "%c", toupper( key ) );
	}
	return "?";
}

static void CG_DrawVote( float y ) {
	char	*s;
	int		sec;

	if ( !cgs.voteTime ) {
		return;
	}

	// play a talk beep whenever it is modified
	if ( cgs.voteModified ) {
		cgs.voteModified = qfalse;
		trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
	}

	sec = ( VOTE_TIME - ( cg.time - cgs.voteTime ) ) / 1000;
	if ( sec < 0 ) {
		sec = 0;
	}
	// one format, no nesting: "VOTE(Ns): what  Yes(F1): N  No(F2): N"
	{
		char	yesKey[8], noKey[8];

		Q_strncpyz( yesKey, CG_BindName( "vote yes" ), sizeof( yesKey ) );
		Q_strncpyz( noKey, CG_BindName( "vote no" ), sizeof( noKey ) );
		s = va( (char *)BG_String( STR_VOTE_LINE ), sec, cgs.voteString, yesKey, cgs.voteYes, noKey, cgs.voteNo );
	}
	cg_hudAnchor = ANCHOR_LEFT;
	CG_DrawSmallString( 8, y, s, 1.0F );
	CG_DrawSmallStringColor( 8, y + SMALLCHAR_HEIGHT + 2, (char *)BG_String( STR_VOTE_ESC ), colorLtGrey );
	cg_hudAnchor = ANCHOR_CENTER;
}

/*
=================
CG_DrawTeamVote
=================
*/
static void CG_DrawTeamVote(void) {
	char	*s;
	int		sec, cs_offset;

	if ( cgs.clientinfo[cg.clientNum].team == TEAM_RED )
		cs_offset = 0;
	else if ( cgs.clientinfo[cg.clientNum].team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !cgs.teamVoteTime[cs_offset] ) {
		return;
	}

	// play a talk beep whenever it is modified
	if ( cgs.teamVoteModified[cs_offset] ) {
		cgs.teamVoteModified[cs_offset] = qfalse;
		trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
	}

	sec = ( VOTE_TIME - ( cg.time - cgs.teamVoteTime[cs_offset] ) ) / 1000;
	if ( sec < 0 ) {
		sec = 0;
	}
	s = va("TEAMVOTE(%i):%s yes:%i no:%i", sec, cgs.teamVoteString[cs_offset],
							cgs.teamVoteYes[cs_offset], cgs.teamVoteNo[cs_offset] );
	CG_DrawSmallString( 0, 90, s, 1.0F );
}


static qboolean CG_DrawScoreboard( void ) {
#ifdef MISSIONPACK
	static qboolean firstTime = qtrue;

	if (menuScoreboard) {
		menuScoreboard->window.flags &= ~WINDOW_FORCED;
	}
	if (cg_paused.integer) {
		cg.deferredPlayerLoading = 0;
		firstTime = qtrue;
		return qfalse;
	}

	// should never happen in Team Arena
	if (cgs.gametype == GT_SINGLE_PLAYER && cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		cg.deferredPlayerLoading = 0;
		firstTime = qtrue;
		return qfalse;
	}

	// don't draw scoreboard during death while warmup up
	if ( cg.warmup && !cg.showScores ) {
		return qfalse;
	}

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD || cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
	} else {
		if ( !CG_FadeColor( cg.scoreFadeTime, FADE_TIME ) ) {
			// next time scoreboard comes up, don't print killer
			cg.deferredPlayerLoading = 0;
			cg.killerName[0] = 0;
			firstTime = qtrue;
			return qfalse;
		}
	}

	if (menuScoreboard == NULL) {
		if ( cgs.gametype >= GT_TEAM ) {
			menuScoreboard = Menus_FindByName("teamscore_menu");
		} else {
			menuScoreboard = Menus_FindByName("score_menu");
		}
	}

	if (menuScoreboard) {
		if (firstTime) {
			CG_SetScoreSelection(menuScoreboard);
			firstTime = qfalse;
		}
		Menu_Paint(menuScoreboard, qtrue);
	}

	// load any models that have been deferred
	if ( ++cg.deferredPlayerLoading > 10 ) {
		CG_LoadDeferredPlayers();
	}

	return qtrue;
#else
	return CG_DrawOldScoreboard();
#endif
}

/*
=================
CG_DrawIntermission
=================
*/
/*
=================
CG_DrawMapVote

The end-of-match map vote: three maps across the bottom with their votes
=================
*/
static void CG_DrawMapVote( void ) {
	static vec4_t	panel = { 0.06f, 0.06f, 0.08f, 0.85f };
	static vec4_t	mine = { 0.85f, 0.12f, 0.1f, 1 };
	char		maps[3][MAX_QPATH];
	int			endTime, votes[3], i, x, left;
	char		*p;
	qhandle_t	shot;
	const char	*cs = CG_ConfigString( CS_MAPVOTE );

	static char	shownFor[MAX_STRING_CHARS];		// the vote state the document shows
	char		rmlOk[8];

	trap_Cvar_VariableStringBuffer( "rml_ok", rmlOk, sizeof( rmlOk ) );
	p = (char *)cs;
	endTime = atoi( COM_Parse( &p ) );
	for ( i = 0 ; i < 3 ; i++ ) {
		Q_strncpyz( maps[i], COM_Parse( &p ), sizeof( maps[i] ) );
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		votes[i] = atoi( COM_Parse( &p ) );
	}
	if ( !cs[0] || !maps[0][0] || cg.time >= endTime ) {
		if ( shownFor[0] ) {
			shownFor[0] = 0;
			trap_SendConsoleCommand( "rml_hide\n" );
		}
		return;
	}
	if ( cg.mapVoteTime != endTime ) {
		cg.mapVoteTime = endTime;		// a new vote: nothing chosen yet
		cg.mapVote = 0;
		trap_Cvar_Set( "mv_pick", "0" );
	}
	{
		char	pick[8];

		// a map clicked in the document
		trap_Cvar_VariableStringBuffer( "mv_pick", pick, sizeof( pick ) );
		if ( atoi( pick ) >= 1 && atoi( pick ) <= 3 ) {
			cg.mapVote = atoi( pick );
		}
	}

	// with RmlUi the vote is a document of clickable maps (ui/mapvote.rml),
	// shown again whenever the counts or this player's choice change
	if ( atoi( rmlOk ) ) {
		const char	*state = va( "%s %i", cs, cg.mapVote );

		if ( strcmp( state, shownFor ) ) {
			Q_strncpyz( shownFor, state, sizeof( shownFor ) );
			for ( i = 0 ; i < 3 ; i++ ) {
				trap_Cvar_Set( va( "mv_name%i", i + 1 ), maps[i][0] == '-' ? "" : maps[i] );
				trap_Cvar_Set( va( "mv_shot%i", i + 1 ), maps[i][0] == '-' ? "menu/art/unknownmap" : va( "levelshots/%s", maps[i] ) );
				trap_Cvar_Set( va( "mv_votes%i", i + 1 ), va( "%i", votes[i] ) );
			}
			trap_Cvar_Set( "mv_mine", va( "%i", cg.mapVote ) );
			trap_SendConsoleCommand( "rml_hide\nrml_show ui/mapvote.rml\n" );
		}
		left = ( endTime - cg.time + 999 ) / 1000;
		CG_DrawSmallStringColor( SCREEN_WIDTH / 2 - CG_DrawStrlen( va( (char *)BG_String( STR_MV_ENDS ), left ) ) * SMALLCHAR_WIDTH / 2,
			24, va( (char *)BG_String( STR_MV_ENDS ), left ), colorWhite );
		return;
	}

	CG_FillRect( 0, 318, SCREEN_WIDTH, 162, panel );
	UI_DrawProportionalString( SCREEN_WIDTH / 2, 322, BG_String( STR_MV_TITLE ), UI_CENTER | UI_SMALLFONT, colorWhite );
	left = ( endTime - cg.time + 999 ) / 1000;
	CG_DrawSmallStringColor( SCREEN_WIDTH / 2 - CG_DrawStrlen( va( (char *)BG_String( STR_MV_ENDS ), left ) ) * SMALLCHAR_WIDTH / 2,
		342, va( (char *)BG_String( STR_MV_ENDS ), left ), colorLtGrey );

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( maps[i][0] == '-' ) {
			continue;
		}
		x = 110 + i * 150;
		if ( cg.mapVote == i + 1 ) {
			CG_FillRect( x - 3, 359, 126, 100, mine );
		}
		shot = trap_R_RegisterShaderNoMip( va( "levelshots/%s.tga", maps[i] ) );
		if ( !shot ) {
			shot = trap_R_RegisterShaderNoMip( "gfx/tremor/splash/nomap" );
		}
		CG_DrawPic( x, 362, 120, 66, shot );
		CG_DrawSmallStringColor( x, 430, va( "%i %s", i + 1, maps[i] ), colorWhite );
		CG_DrawSmallStringColor( x, 444, va( (char *)BG_String( STR_MV_VOTES ), votes[i] ), colorLtGrey );
	}
	CG_DrawSmallStringColor( SCREEN_WIDTH / 2 - CG_DrawStrlen( BG_String( STR_MV_HELP ) ) * SMALLCHAR_WIDTH / 2,
		464, BG_String( STR_MV_HELP ), colorLtGrey );
}

static void CG_DrawIntermission( void ) {
//	int key;
#ifdef MISSIONPACK
	//if (cg_singlePlayer.integer) {
	//	CG_DrawCenterString();
	//	return;
	//}
#else
	if ( cgs.gametype == GT_SINGLE_PLAYER ) {
		CG_DrawCenterString();
		CG_DrawFragBanner();
		return;
	}
#endif
	cg.scoreFadeTime = cg.time;
	cg.scoreBoardShowing = CG_DrawScoreboard();
	CG_DrawMapVote();
}

/*
=================
CG_DrawFollow
=================
*/
static qboolean CG_DrawFollow( void ) {
	float		x;
	vec4_t		color;
	const char	*name;

	if ( !(cg.snap->ps.pm_flags & PMF_FOLLOW) ) {
		return qfalse;
	}
	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;


	CG_DrawBigString( 320 - 9 * 8, 24, "following", 1.0F );

	name = cgs.clientinfo[ cg.snap->ps.clientNum ].name;

	x = 0.5 * ( 640 - GIANT_WIDTH * CG_DrawStrlen( name ) );

	CG_DrawStringExt( x, 40, name, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );

	return qtrue;
}



/*
=================
CG_DrawAmmoWarning
=================
*/
static void CG_DrawAmmoWarning( void ) {
	const char	*s;
	int			w;

	if ( cg_drawAmmoWarning.integer == 0 ) {
		return;
	}

	if ( !cg.lowAmmoWarning ) {
		return;
	}

	if ( cg.lowAmmoWarning == 2 ) {
		s = "OUT OF AMMO";
	} else {
		s = "LOW AMMO WARNING";
	}
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	CG_DrawBigString(320 - w / 2, 64, s, 1.0F);
}


#ifdef MISSIONPACK
/*
=================
CG_DrawProxWarning
=================
*/
static void CG_DrawProxWarning( void ) {
	char s [32];
	int			w;
  static int proxTime;
  int proxTick;

	if( !(cg.snap->ps.eFlags & EF_TICKING ) ) {
    proxTime = 0;
		return;
	}

  if (proxTime == 0) {
    proxTime = cg.time;
  }

  proxTick = 10 - ((cg.time - proxTime) / 1000);

  if (proxTick > 0 && proxTick <= 5) {
    Com_sprintf(s, sizeof(s), "INTERNAL COMBUSTION IN: %i", proxTick);
  } else {
    Com_sprintf(s, sizeof(s), "YOU HAVE BEEN MINED");
  }

	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	CG_DrawBigStringColor( 320 - w / 2, 64 + BIGCHAR_HEIGHT, s, g_color_table[ColorIndex(COLOR_RED)] );
}
#endif


/*
=================
CG_DrawWarmup
=================
*/
/*
==============================================================================

PILLARS: the pre-match lineup and the post-match podium

Champions standing on hexagonal columns drawn from polygons, in a scene of
their own in front of the darkened screen

==============================================================================
*/

#define PILLAR_RADIUS	24		// centre to corner; each face is this wide
#define PILLAR_DIST		300		// how far in front of the camera the row stands
#define PILLAR_GAP		56		// least distance between pillar centres
#define PILLAR_V_SCALE	128		// side texture: v runs over this many units down

/*
=================
CG_PillarPoly

One textured quad, every corner in the glow colour
=================
*/
static void CG_PillarPoly( qhandle_t shader, vec3_t xyz[4], float st[4][2], const byte *glow ) {
	polyVert_t	v[4];
	int			i;

	for ( i = 0 ; i < 4 ; i++ ) {
		VectorCopy( xyz[i], v[i].xyz );
		v[i].st[0] = st[i][0];
		v[i].st[1] = st[i][1];
		v[i].modulate[0] = glow[0];
		v[i].modulate[1] = glow[1];
		v[i].modulate[2] = glow[2];
		v[i].modulate[3] = 255;
	}
	trap_R_AddPolyToScene( shader, 4, v );
}

/*
=================
CG_AddPillar

A hexagonal column from the floor up to its top at base: metal sides and
cap whose lines glow in `glow`; rank 1-3 adds the place on the face toward
the camera
=================
*/
static void CG_AddPillar( const vec3_t base, const byte *glow, int rank ) {
	vec3_t	corner[6], q[4];
	float	st[4][2], top = base[2], bottom = base[2] - 400;
	int		i, j;

	for ( i = 0 ; i < 6 ; i++ ) {
		float	a = DEG2RAD( 30 + i * 60 );

		VectorSet( corner[i], base[0] + cos( a ) * PILLAR_RADIUS, base[1] + sin( a ) * PILLAR_RADIUS, top );
	}

	// sides: u across the face, v down from the top, wound to face outward
	// (the other way the near faces are culled and the inside shows)
	for ( i = 0 ; i < 6 ; i++ ) {
		j = ( i + 1 ) % 6;
		VectorCopy( corner[j], q[0] );
		VectorCopy( corner[i], q[1] );
		VectorCopy( corner[i], q[2] );
		VectorCopy( corner[j], q[3] );
		q[0][2] = q[1][2] = bottom;
		st[0][0] = 0; st[0][1] = ( top - bottom ) / PILLAR_V_SCALE;
		st[1][0] = 1; st[1][1] = ( top - bottom ) / PILLAR_V_SCALE;
		st[2][0] = 1; st[2][1] = 0;
		st[3][0] = 0; st[3][1] = 0;
		CG_PillarPoly( cgs.media.pillarSideShader, q, st, glow );
	}

	// cap: mapped flat over 48 x 48 units, two quads of the hexagon
	for ( i = 0 ; i < 6 ; i += 3 ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			VectorCopy( corner[( i + j ) % 6], q[3 - j] );
			st[3 - j][0] = ( q[3 - j][0] - base[0] ) / 48 + 0.5f;
			st[3 - j][1] = ( q[3 - j][1] - base[1] ) / 48 + 0.5f;
		}
		CG_PillarPoly( cgs.media.pillarCapShader, q, st, glow );
	}

	// the podium place on the face toward the camera (-x), just in front of it
	if ( rank >= 1 && rank <= 3 ) {
		float	x = base[0] - PILLAR_RADIUS * cos( DEG2RAD( 30 ) ) - 0.1f;
		float	y0 = base[1] + PILLAR_RADIUS / 2 - 4, y1 = base[1] + PILLAR_RADIUS / 2 - 20;

		VectorSet( q[0], x, y0, top - 33 );
		VectorSet( q[1], x, y1, top - 33 );
		VectorSet( q[2], x, y1, top - 17 );
		VectorSet( q[3], x, y0, top - 17 );
		st[0][0] = 0; st[0][1] = 1;
		st[1][0] = 1; st[1][1] = 1;
		st[2][0] = 1; st[2][1] = 0;
		st[3][0] = 0; st[3][1] = 0;
		CG_PillarPoly( cgs.media.pillarRankShader[rank - 1], q, st, glow );
	}
}

/*
=================
CG_AddPillarPlayer

The player's champion standing still on top of a pillar, facing the camera
=================
*/
static void CG_AddPillarPlayer( int clientNum, const vec3_t feet ) {
	clientInfo_t	*ci = &cgs.clientinfo[clientNum];
	refEntity_t		legs, torso, head;
	vec3_t			angles;

	if ( !ci->infoValid || !ci->legsModel ) {
		return;
	}
	VectorSet( angles, 0, 180, 0 );

	memset( &legs, 0, sizeof( legs ) );
	AnglesToAxis( angles, legs.axis );
	VectorCopy( feet, legs.origin );
	legs.origin[2] += 24;		// model origins are at the waist
	VectorCopy( legs.origin, legs.oldorigin );
	VectorCopy( legs.origin, legs.lightingOrigin );
	legs.hModel = ci->legsModel;
	legs.customSkin = ci->legsSkin;
	legs.frame = legs.oldframe = ci->animations[LEGS_IDLE].firstFrame;
	legs.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
	trap_R_AddRefEntityToScene( &legs );

	memset( &torso, 0, sizeof( torso ) );
	AxisClear( torso.axis );
	torso.hModel = ci->torsoModel;
	torso.customSkin = ci->torsoSkin;
	torso.frame = torso.oldframe = ci->animations[TORSO_STAND].firstFrame;
	VectorCopy( legs.origin, torso.lightingOrigin );
	torso.renderfx = legs.renderfx;
	CG_PositionRotatedEntityOnTag( &torso, &legs, ci->legsModel, "tag_torso" );
	trap_R_AddRefEntityToScene( &torso );

	memset( &head, 0, sizeof( head ) );
	AxisClear( head.axis );
	head.hModel = ci->headModel;
	head.customSkin = ci->headSkin;
	VectorCopy( legs.origin, head.lightingOrigin );
	head.renderfx = legs.renderfx;
	CG_PositionRotatedEntityOnTag( &head, &torso, ci->torsoModel, "tag_head" );
	trap_R_AddRefEntityToScene( &head );
}

/*
=================
CG_DrawPillars

Up to 16 players on pillars across the screen: `height` per slot (0 for
the same height), names under them. Positions are spread from left to right.
=================
*/
static void CG_DrawPillars( const int *clients, const int *heights, const int *teams, int count, const char **labels, const int *ranks,
	int readyBits ) {
	// glow: logo red when neutral, else the team colour
	static const byte	glows[3][4] = { { 211, 25, 31, 255 }, { 208, 26, 26, 255 }, { 40, 90, 220, 255 } };
	float		dist;
	refdef_t	refdef;
	vec3_t		base;
	float		spacing, y, sx;
	int			i;
	clientInfo_t	*ci;

	if ( count <= 0 ) {
		return;
	}
	memset( &refdef, 0, sizeof( refdef ) );
	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear( refdef.viewaxis );
	refdef.fov_x = 60;
	refdef.fov_y = 60 * cgs.glconfig.vidHeight / (float)cgs.glconfig.vidWidth;
	refdef.x = 0;
	refdef.y = 0;
	refdef.width = cgs.glconfig.vidWidth;
	refdef.height = cgs.glconfig.vidHeight;
	refdef.time = cg.time;

	trap_R_ClearScene();
	// a long row stands further back, so the pillars never overlap
	dist = PILLAR_DIST;
	if ( count > 1 && ( count - 1 ) * PILLAR_GAP > 2 * dist * tan( DEG2RAD( 25 ) ) ) {
		dist = ( count - 1 ) * PILLAR_GAP / ( 2 * tan( DEG2RAD( 25 ) ) );
	}
	spacing = count > 1 ? 2 * dist * tan( DEG2RAD( 25 ) ) / ( count - 1 ) : 0;
	if ( spacing > 110 ) {
		spacing = 110;
	}
	for ( i = 0 ; i < count ; i++ ) {
		y = ( ( count - 1 ) / 2.0f - i ) * spacing;
		VectorSet( base, dist, y, -60 + ( heights ? heights[i] : 0 ) );
		CG_AddPillar( base, glows[teams ? teams[i] : 0], ranks ? ranks[i] : 0 );
		CG_AddPillarPlayer( clients[i], base );
	}
	trap_R_AddLightToScene( vec3_origin, 600, 1, 1, 1 );
	trap_R_RenderScene( &refdef );

	// names under the pillars' tops
	for ( i = 0 ; i < count ; i++ ) {
		y = ( ( count - 1 ) / 2.0f - i ) * spacing;
		// the pillar's place on screen, in 640x480 units (the scene spans the whole width)
		sx = 320 - y / ( dist * tan( DEG2RAD( 30 ) ) ) * 0.5f * cgs.glconfig.vidWidth * 480 / cgs.glconfig.vidHeight;
		ci = &cgs.clientinfo[clients[i]];
		CG_DrawStringExt( sx - CG_DrawStrlen( ci->name ) * SMALLCHAR_WIDTH / 2, 404, ci->name, colorWhite,
			qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 16 );
		if ( labels && labels[i] ) {
			float		lx = sx - CG_DrawStrlen( labels[i] ) * TINYCHAR_WIDTH / 2;
			qhandle_t	pic = ranks ? 0 : CG_ChampionPortrait( BG_ChampionForModel( ci->modelName ), qtrue );

			CG_DrawStringExt( lx, 424, labels[i], colorLtGrey, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			// the champion's portrait beside its name on the lineup
			if ( pic ) {
				CG_DrawPic( lx - 14, 422, 12, 12, pic );
			}
		}
		// ready-up in warm-up: who has readied
		if ( readyBits & ( 1 << clients[i] ) ) {
			const char	*ready = BG_String( STR_LINEUP_READY );

			CG_DrawStringExt( sx - CG_DrawStrlen( ready ) * SMALLCHAR_WIDTH / 2, 436, ready, colorGreen,
				qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		}
	}
}

/*
=================
CG_DraftState

Round duel's draft from its config string: whether it is on, the two
players and their picks (-1 not yet)
=================
*/
static qboolean CG_DraftState( int players[2], int picks[2][3] ) {
	const char	*cs = strchr( CG_ConfigString( CS_SACRIFICE ), '#' );
	char		*p;
	int			i;

	if ( !cs || Q_strncmp( CG_ConfigString( CS_SACRIFICE ), "rd|", 3 ) ) {
		return qfalse;
	}
	p = (char *)cs + 1;
	if ( !atoi( COM_Parse( &p ) ) ) {
		return qfalse;
	}
	players[0] = atoi( COM_Parse( &p ) );
	players[1] = atoi( COM_Parse( &p ) );
	for ( i = 0 ; i < 6 ; i++ ) {
		picks[i / 3][i % 3] = atoi( COM_Parse( &p ) );
	}
	return qtrue;
}

/*
=================
CG_DrawDraft

Round duel's draft in the lineup look: each player's three pedestals, the
local player on the left, filling with the champions as they are picked.
The picking itself is ui/draft.rml over it.
=================
*/
static qboolean CG_DrawDraft( void ) {
	static const vec4_t	dim = { 0.05f, 0.05f, 0.06f, 1.0f };
	static const byte	glows[2][4] = { { 211, 25, 31, 255 }, { 40, 90, 220, 255 } };
	int			players[2], picks[2][3], side, k, local = 0;
	refdef_t	refdef;
	vec3_t		base;
	float		y, sx, dist = 420;
	char		doc[MAX_QPATH], active[8];
	qboolean	shown;

	trap_Cvar_VariableStringBuffer( "rml_doc", doc, sizeof( doc ) );
	trap_Cvar_VariableStringBuffer( "rml_active", active, sizeof( active ) );
	shown = !Q_stricmp( doc, "ui/draft.rml" ) && atoi( active );
	if ( !CG_DraftState( players, picks ) ) {
		// the draft is over: put the picking document away
		if ( shown ) {
			trap_SendConsoleCommand( "rml_hide\n" );
		}
		return qfalse;
	}
	if ( !shown ) {
		trap_SendConsoleCommand( "rml_show ui/draft.rml\n" );
	}
	if ( players[1] == cg.snap->ps.clientNum ) {
		local = 1;		// the local player on the left
	}

	CG_FillRect( -1000, 0, 3000, 480, dim );
	memset( &refdef, 0, sizeof( refdef ) );
	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear( refdef.viewaxis );
	refdef.fov_x = 60;
	refdef.fov_y = 60 * cgs.glconfig.vidHeight / (float)cgs.glconfig.vidWidth;
	refdef.width = cgs.glconfig.vidWidth;
	refdef.height = cgs.glconfig.vidHeight;
	refdef.time = cg.time;
	trap_R_ClearScene();
	for ( side = 0 ; side < 2 ; side++ ) {
		for ( k = 0 ; k < 3 ; k++ ) {
			y = ( side ? -1 : 1 ) * ( 60 + k * 54 );
			VectorSet( base, dist, y, -40 );
			CG_AddPillar( base, glows[side], 0 );
		}
	}
	trap_R_AddLightToScene( vec3_origin, 600, 1, 1, 1 );
	trap_R_RenderScene( &refdef );

	// the picks over their pedestals, the players' names above each group
	for ( side = 0 ; side < 2 ; side++ ) {
		int			who = side ? players[!local] : players[local];
		const int	*row = side ? picks[!local] : picks[local];

		for ( k = 0 ; k < 3 ; k++ ) {
			y = ( side ? -1 : 1 ) * ( 60 + k * 54 );
			sx = 320 - y / ( dist * tan( DEG2RAD( 30 ) ) ) * 0.5f * cgs.glconfig.vidWidth * 480 / cgs.glconfig.vidHeight;
			if ( row[k] >= 0 && row[k] < bg_numChampions ) {
				const champion_t	*c = BG_Champion( row[k] );
				char				model[MAX_QPATH], *skin;
				qhandle_t			icon;

				Q_strncpyz( model, c->model, sizeof( model ) );
				skin = strchr( model, '/' );
				if ( skin ) {
					*skin++ = 0;
				}
				icon = trap_R_RegisterShaderNoMip( va( "models/players/%s/icon_%s", model, skin ? skin : "default" ) );
				CG_DrawPic( sx - 24, 258, 48, 48, icon );
				CG_DrawStringExt( sx - CG_DrawStrlen( BG_String( c->nameString ) ) * TINYCHAR_WIDTH / 2, 310,
					BG_String( c->nameString ), colorWhite, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
			}
		}
		sx = side ? 470 : 170;
		if ( who >= 0 && who < MAX_CLIENTS ) {
			CG_DrawStringExt( sx - CG_DrawStrlen( cgs.clientinfo[who].name ) * BIGCHAR_WIDTH / 2, 220,
				cgs.clientinfo[who].name, colorWhite, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
		}
	}
	return qtrue;
}

/*
=================
CG_DrawLineup

During the countdown: everyone in the match on a pillar, red team left,
blue right; in duel the local player is on the left
=================
*/
static void CG_DrawLineup( void ) {
	static const vec4_t	dim = { 0.05f, 0.05f, 0.06f, 1.0f };		// a screen of its own, the world hidden
	int		clients[16], teams[16], count = 0, i, t, local = cg.snap->ps.clientNum;
	const char	*labels[16];

	if ( ( cg.warmup <= 0 || cg.warmup < cg.time ) && !cg_showLineup.integer ) {
		return;
	}
	// the local player first in duel; in team modes red, then blue
	for ( t = 0 ; t < 3 ; t++ ) {
		for ( i = 0 ; i < MAX_CLIENTS && count < 16 ; i++ ) {
			clientInfo_t	*ci = &cgs.clientinfo[i];

			if ( !ci->infoValid || ci->team == TEAM_SPECTATOR ) {
				continue;
			}
			if ( cgs.gametype >= GT_TEAM ) {
				if ( ci->team != ( t == 0 ? TEAM_RED : TEAM_BLUE ) || t == 2 ) {
					continue;
				}
			} else if ( t == 0 ? i != local : t == 1 ? i == local : qtrue ) {
				continue;
			}
			clients[count] = i;
			teams[count] = ci->team == TEAM_RED ? 1 : ci->team == TEAM_BLUE ? 2 : 0;
			count++;
		}
	}
	if ( count < 2 ) {
		return;
	}
	// each player's champion under their name
	for ( i = 0 ; i < count ; i++ ) {
		int		champ = BG_ChampionForModel( cgs.clientinfo[clients[i]].modelName );

		labels[i] = champ >= 0 ? BG_String( BG_Champion( champ )->nameString ) : NULL;
	}
	CG_FillRect( -1000, 0, 3000, 480, dim );
	CG_DrawPillars( clients, NULL, teams, count, labels, NULL,
		atoi( CG_ConfigString( CS_READYUP ) ) && cg.warmup ? cg.snap->ps.stats[STAT_CLIENTS_READY] : 0 );
	UI_DrawProportionalString( 320, 40, CG_ModeString(), UI_CENTER | UI_DROPSHADOW, colorWhite );
	UI_DrawProportionalString( 320, 66, CG_ConfigString( CS_MESSAGE )[0] ? CG_ConfigString( CS_MESSAGE ) : cgs.mapname + 5,
		UI_CENTER | UI_SMALLFONT | UI_DROPSHADOW, colorLtGrey );
}

/*
=================
CG_DrawPodium

The first seconds of the intermission: the top three on pillars of
heights 3, 2 and 1, the winner in the middle
=================
*/
static void CG_DrawPodium( void ) {
	static const vec4_t	dim = { 0.05f, 0.05f, 0.06f, 1.0f };		// a screen of its own
	int		clients[3], heights[3], teams[3], ranks[3], count = 0, order[3] = { 1, 0, 2 }, i, n;
	const char	*labels[3];

	static char	scoreText[3][16];

	n = cg.numScores < 3 ? cg.numScores : 3;
	if ( n < 1 ) {
		return;
	}
	// left: second, middle: first, right: third
	for ( i = 0 ; i < 3 ; i++ ) {
		int		rank = order[i];

		if ( rank >= n ) {
			continue;
		}
		clients[count] = cg.scores[rank].client;
		heights[count] = ( 3 - rank ) * 20;
		ranks[count] = rank + 1;
		teams[count] = cg.scores[rank].team == TEAM_RED ? 1 : cg.scores[rank].team == TEAM_BLUE ? 2 : 0;
		Com_sprintf( scoreText[count], sizeof( scoreText[count] ), "%i", cg.scores[rank].score );
		labels[count] = scoreText[count];
		count++;
	}
	CG_FillRect( -1000, 0, 3000, 480, dim );
	CG_DrawPillars( clients, heights, teams, count, labels, ranks, 0 );

	// the awards above, in two columns
	for ( i = 0 ; i < cg.numAwards ; i++ ) {
		float	x = i & 1 ? 330 : 10, y = 20 + ( i / 2 ) * 14;

		CG_DrawStringExt( x, y, cg.awardLines[i], colorLtGrey, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
	}
}

/*
=================
CG_DrawWarmupLine

"WARM-UP" and what the start waits for: players, ready players, or the
clock (CS_READYUP)
=================
*/
static void CG_DrawWarmupLine( void ) {
	char		line[128], *p;
	char		info[64];
	int			readyUp, ready, players, needed, startTime, i, count[TEAM_NUM_TEAMS], w, top;
	qboolean	enough;

	Q_strncpyz( info, CG_ConfigString( CS_READYUP ), sizeof( info ) );
	p = info;
	readyUp = atoi( COM_Parse( &p ) );
	COM_Parse( &p );		// share
	ready = atoi( COM_Parse( &p ) );
	players = atoi( COM_Parse( &p ) );
	needed = atoi( COM_Parse( &p ) );
	startTime = atoi( COM_Parse( &p ) );

	memset( count, 0, sizeof( count ) );
	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		if ( cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team != TEAM_SPECTATOR ) {
			count[cgs.clientinfo[i].team]++;
		}
	}
	if ( cgs.gametype >= GT_TEAM ) {
		enough = count[TEAM_RED] && count[TEAM_BLUE];
	} else {
		enough = count[TEAM_FREE] >= 2;
	}

	// the modern layout: the panel under the clock area (top + 249 / 278)
	top = 24;
	if ( CG_HudModern() ) {
		float	wx, wy;

		CG_HudPlace( "warmup", &wx, &wy );
		cg_hudAnchor = ANCHOR_CENTER;
		top = wy - BIGCHAR_HEIGHT / 2;
	}
	w = CG_DrawStrlen( BG_String( STR_WARMUP ) ) * BIGCHAR_WIDTH;
	CG_DrawBigString( 320 - w / 2, top, BG_String( STR_WARMUP ), 1.0F );

	if ( !enough ) {
		Com_sprintf( line, sizeof( line ), BG_String( STR_WARMUP_WAITING ),
			cgs.gametype >= GT_TEAM ? ( count[TEAM_RED] > 0 ) + ( count[TEAM_BLUE] > 0 ) : count[TEAM_FREE], 2 );
	} else if ( !readyUp ) {
		Com_sprintf( line, sizeof( line ), BG_String( STR_WARMUP_STARTS ),
			startTime > cg.time ? ( startTime - cg.time + 999 ) / 1000 : 0 );
	} else {
		const char *key = CG_BindName( "readyup" );

		Com_sprintf( line, sizeof( line ), BG_String( STR_WARMUP_READY ), key[0] == '?' ? "readyup" : key, ready, players );
		if ( startTime > cg.time ) {
			Q_strcat( line, sizeof( line ), "  -  " );
			Q_strcat( line, sizeof( line ), va( (char *)BG_String( STR_WARMUP_ANYWAY ), ( startTime - cg.time + 999 ) / 1000 ) );
		}
	}
	w = CG_DrawStrlen( line ) * SMALLCHAR_WIDTH;
	CG_DrawSmallString( 320 - w / 2, top + ( CG_HudModern() ? 29 * DP + BIGCHAR_HEIGHT / 2 : 22 ), line, 1.0F );
	(void)needed;
}

static qboolean	lineupShown;	// the countdown lineup is up this frame

static void CG_DrawWarmup( void ) {
	int			w;
	int			sec;
	int			i;
#ifdef MISSIONPACK
	float		scale;
#else
	int			cw;
#endif
	clientInfo_t	*ci1, *ci2;
	const char	*s;

	sec = cg.warmup;
	if ( !sec ) {
		return;
	}

	if ( sec < 0 ) {
		CG_DrawWarmupLine();
		cg.warmupCount = 0;
		return;
	}

	// the lineup has the title, the modern clock line the mode; only the
	// count goes over them
	if ( lineupShown || CG_HudModern() ) {
	} else if (cgs.gametype == GT_TOURNAMENT) {
		// find the two active players
		ci1 = NULL;
		ci2 = NULL;
		for ( i = 0 ; i < cgs.maxclients ; i++ ) {
			if ( cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_FREE ) {
				if ( !ci1 ) {
					ci1 = &cgs.clientinfo[i];
				} else {
					ci2 = &cgs.clientinfo[i];
				}
			}
		}

		if ( ci1 && ci2 ) {
			s = va( "%s vs %s", ci1->name, ci2->name );
#ifdef MISSIONPACK
			w = CG_Text_Width(s, 0.6f, 0);
			CG_Text_Paint(320 - w / 2, 60, 0.6f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
#else
			w = CG_DrawStrlen( s );
			if ( w > 640 / GIANT_WIDTH ) {
				cw = 640 / w;
			} else {
				cw = GIANT_WIDTH;
			}
			CG_DrawStringExt( 320 - w * cw/2, 20,s, colorWhite, 
					qfalse, qtrue, cw, (int)(cw * 1.5f), 0 );
#endif
		}
	} else {
		if ( cgs.gametype == GT_FFA ) {
			s = "Free For All";
		} else if ( cgs.gametype == GT_TEAM ) {
			s = "Team Deathmatch";
		} else if ( cgs.gametype == GT_CTF ) {
			s = "Capture the Flag";
#ifdef MISSIONPACK
		} else if ( cgs.gametype == GT_1FCTF ) {
			s = "One Flag CTF";
		} else if ( cgs.gametype == GT_OBELISK ) {
			s = "Overload";
		} else if ( cgs.gametype == GT_HARVESTER ) {
			s = "Harvester";
#endif
		} else {
			s = "";
		}
#ifdef MISSIONPACK
		w = CG_Text_Width(s, 0.6f, 0);
		CG_Text_Paint(320 - w / 2, 90, 0.6f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
#else
		w = CG_DrawStrlen( s );
		if ( w > 640 / GIANT_WIDTH ) {
			cw = 640 / w;
		} else {
			cw = GIANT_WIDTH;
		}
		CG_DrawStringExt( 320 - w * cw/2, 25,s, colorWhite, 
				qfalse, qtrue, cw, (int)(cw * 1.1f), 0 );
#endif
	}

	sec = ( sec - cg.time ) / 1000;
	if ( sec < 0 ) {
		cg.warmup = 0;
		sec = 0;
	}
	s = va( "Starts in: %i", sec + 1 );
	if ( sec != cg.warmupCount ) {
		cg.warmupCount = sec;
		switch ( sec ) {
		case 0:
			trap_S_StartLocalSound( cgs.media.count1Sound, CHAN_ANNOUNCER );
			break;
		case 1:
			trap_S_StartLocalSound( cgs.media.count2Sound, CHAN_ANNOUNCER );
			break;
		case 2:
			trap_S_StartLocalSound( cgs.media.count3Sound, CHAN_ANNOUNCER );
			break;
		default:
			break;
		}
	}
	if ( sec > 2 ) {
		return;		// "Starts in N" only for the last 3 s
	}

#ifdef MISSIONPACK
	switch ( cg.warmupCount ) {
	case 0:
		scale = 0.54f;
		break;
	case 1:
		scale = 0.51f;
		break;
	case 2:
		scale = 0.48f;
		break;
	default:
		scale = 0.45f;
		break;
	}

	w = CG_Text_Width(s, scale, 0);
	CG_Text_Paint(320 - w / 2, 125, scale, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
#else
	switch ( cg.warmupCount ) {
	case 0:
		cw = 28;
		break;
	case 1:
		cw = 24;
		break;
	case 2:
		cw = 20;
		break;
	default:
		cw = 16;
		break;
	}

	w = CG_DrawStrlen( s );
	if ( CG_HudModern() ) {
		float	cx, cy;

		CG_HudPlace( "countdown", &cx, &cy );
		cg_hudAnchor = ANCHOR_CENTER;
		CG_DrawStringExt( 320 - w * cw/2, cy - cw * 0.75f, s, colorWhite, qfalse, qtrue, cw, (int)(cw * 1.5), 0 );
		return;
	}
	CG_DrawStringExt( 320 - w * cw/2, 70, s, colorWhite, 
			qfalse, qtrue, cw, (int)(cw * 1.5), 0 );
#endif
}

//==================================================================================
#ifdef MISSIONPACK
/* 
=================
CG_DrawTimedMenus
=================
*/
void CG_DrawTimedMenus( void ) {
	if (cg.voiceTime) {
		int t = cg.time - cg.voiceTime;
		if ( t > 2500 ) {
			Menus_CloseByName("voiceMenu");
			trap_Cvar_Set("cl_conXOffset", "0");
			cg.voiceTime = 0;
		}
	}
}
#endif
#ifndef MISSIONPACK
/*
=================
CG_DrawStackBar

A value against its class base: a block per 25 points, lit up to the
value, the blocks over base brighter; then "value / base"
=================
*/
static void CG_DrawStackBar( float x, float y, int value, int base, const vec4_t tint ) {
	vec4_t	off = { 0.2f, 0.2f, 0.22f, 0.8f }, lit, over;
	int		blocks = ( base + 75 ) / 25, i;
	float	bw = 22 * DP, gap = 3 * DP;
	char	*s;

	Vector4Copy( tint, lit );
	lit[0] *= 0.7f; lit[1] *= 0.7f; lit[2] *= 0.7f;
	Vector4Copy( tint, over );
	for ( i = 0 ; i < blocks ; i++ ) {
		CG_FillRect( x + i * ( bw + gap ), y, bw, 10 * DP, value > i * 25 ? ( i * 25 >= base ? over : lit ) : off );
	}
	s = va( "%i", value );
	CG_DrawStringExt( x, y - 34 * DP - BIGCHAR_HEIGHT * 0.5f, s, value <= 25 ? colorRed : tint, qtrue, qtrue,
		BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	CG_DrawStringExt( x + CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 4, y - 34 * DP, va( "/ %i", base ), colorMdGrey, qtrue, qtrue,
		SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

/*
=================
CG_DrawModernHud

The modern layout's own elements: you bottom left, the ability bottom
centre, the weapon bottom right
=================
*/
static void CG_DrawModernHud( void ) {
	playerState_t		*ps = &cg.snap->ps;
	centity_t			*cent = &cg_entities[ps->clientNum];
	const champion_t	*champ = BG_Champion( ps->stats[STAT_CHAMPION] );
	const classInfo_t	*ci = &bg_classInfo[champ->bodyClass];
	vec4_t				healthTint = { 1, 1, 1, 1 }, armorTint = { 0.35f, 0.95f, 0.45f, 1 };
	float				x, y, size;
	qhandle_t			pic;
	int					value, maxCharges;
	const char			*s;

	if ( cg_drawStatus.integer == 0 ) {
		return;
	}

	// portrait under the numbers
	size = 150 * DP * CG_HudPlace( "portrait", &x, &y );
	pic = CG_ChampionPortrait( ps->stats[STAT_CHAMPION], qfalse );
	if ( pic ) {
		CG_DrawPic( x - size, y - size, size, size, pic );
	}

	// armour and health against the class base
	CG_HudPlace( "armor", &x, &y );
	CG_DrawPic( x - 14 * DP, y - 30 * DP, 28 * DP * 1.6f, 28 * DP * 1.6f, cgs.media.armorIcon );
	CG_DrawStackBar( x + 44 * DP, y + 12 * DP, ps->stats[STAT_ARMOR], ci->baseArmor, armorTint );
	CG_HudPlace( "health", &x, &y );
	CG_DrawStackBar( x + 44 * DP, y + 12 * DP, ps->stats[STAT_HEALTH], 100, healthTint );

	// the ability: ring, charges above it, key and name under it
	CG_HudPlace( "ability", &x, &y );
	cg_hudAnchor = ANCHOR_CENTER;
	CG_DrawAbilityRing( x, y, 30, 0, 360, 24, 3 );
	maxCharges = BG_AbilityParam( champ->active, AP_CHARGES );
	if ( maxCharges > 1 ) {
		chargeIconsY = y - 30 - 14;		// on top of the ring, clear of the name
		CG_DrawChargeIcons( champ->active, BG_AbilityCharges( ps ), maxCharges );
		chargeIconsY = 470;
	}
	if ( champ->active > ACTIVE_NONE ) {
		CG_HudPlace( "abilitykey", &x, &y );
		s = CG_BindName( "+button12" );
		CG_DrawStringExt( x - CG_DrawStrlen( s ) * SMALLCHAR_WIDTH / 2, y - SMALLCHAR_HEIGHT / 2, s, colorWhite, qfalse, qtrue,
			SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		CG_HudPlace( "abilityname", &x, &y );
		s = BG_String( bg_abilityInfo[champ->active].nameString );
		CG_DrawStringExt( x - CG_DrawStrlen( s ) * TINYCHAR_WIDTH / 2, y - TINYCHAR_HEIGHT / 2, s, colorMdGrey, qfalse, qtrue,
			TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
	}
	if ( ( champ->passives & PASSIVE_LEAPINGKICK ) && KICK_COUNT( ps ) > 0 ) {
		CG_DrawSmallStringColor( x + 60, y - 30, va( "KICK x%i", KICK_COUNT( ps ) ), colorYellow );
	}

	// ammo of the held weapon, large, with its icon
	CG_HudPlace( "ammo", &x, &y );
	if ( cent->currentState.weapon && ps->ammo[cent->currentState.weapon] > -1 ) {
		value = ps->ammo[cent->currentState.weapon];
		trap_R_SetColor( value <= 5 ? colorRed : NULL );
		CG_DrawField( x - CHAR_WIDTH * 3 + 30, y - CHAR_HEIGHT / 2, 3, value );
		trap_R_SetColor( NULL );
		if ( cg_weapons[cent->currentState.weapon].ammoIcon ) {
			CG_DrawPic( x - CHAR_WIDTH * 3 - 6, y - 12, 24, 24, cg_weapons[cent->currentState.weapon].ammoIcon );
		}
	}

	// the crosshair arc repeats the ring
	cg_hudAnchor = ANCHOR_CENTER;
	switch ( cg_drawCooldownArc.integer ) {
	case 2:		CG_DrawAbilityRing( 320, 240, 26, 225, 90, 12, 2 ); break;
	case 3:		CG_DrawAbilityRing( 320, 240, 26, 45, 90, 12, 2 ); break;
	case 4:		CG_DrawAbilityRing( 320, 240, 26, 135, 90, 12, 2 ); break;
	case 5:		CG_DrawAbilityRing( 320, 240, 26, -45, 90, 12, 2 ); break;
	default:	break;
	}

	cg_hudAnchor = ANCHOR_RIGHT;
	CG_DrawWeaponBar();
	cg_hudAnchor = ANCHOR_CENTER;
	CG_DrawHudPowerups();
	CG_DrawPickupFeed();
	CG_DrawPassiveState();
	CG_DrawSpeedometer();
}
#endif // MISSIONPACK

/*
=================
CG_Draw2D
=================
*/
static void CG_Draw2D(stereoFrame_t stereoFrame)
{
	float	voteY = 0;		// the vote line goes under the team overlay

	CG_HudLayout();		// every state draws from the layout

#ifdef MISSIONPACK
	if (cgs.orderPending && cg.time > cgs.orderTime) {
		CG_CheckOrderPending();
	}
#endif
	// if we are taking a levelshot for the menu, don't draw anything
	if ( cg.levelShot ) {
		return;
	}

	if ( cg_draw2D.integer == 0 ) {
		return;
	}

	// a menu over the game hides the HUD and centre prints under it
	if ( trap_Key_GetCatcher() & KEYCATCH_UI ) {
		return;
	}

	if ( cg.snap->ps.pm_type == PM_INTERMISSION ) {
		if ( !cg.podiumTime ) {
			cg.podiumTime = cg.time;
		}
		if ( cg.time - cg.podiumTime < 6000 && cg_drawLineup.integer ) {
			CG_DrawPodium();
			return;
		}
		CG_DrawIntermission();
		return;
	}
	cg.podiumTime = 0;

	// round duel's draft: its own screen
	if ( CG_DrawDraft() ) {
		return;
	}

	// everyone on pillars: only on request now (cg_showLineup); the
	// countdown after ready-up stays in the game, the lineup comes before
	// the server starts (the menu's pre-lineup)
	if ( cg_showLineup.integer ) {
		CG_DrawLineup();
		lineupShown = qtrue;
		CG_DrawWarmup();
		lineupShown = qfalse;
		return;
	}

/*
	if (cg.cameraMode) {
		return;
	}
*/
	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		CG_DrawSpectator();

		if(stereoFrame == STEREO_CENTER)
			CG_DrawCrosshair();

		CG_DrawCrosshairNames();
#ifndef MISSIONPACK
		CG_DrawWorldItemTimers();
#endif
	} else {
		if ( !cg.showScores && !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
			CG_DrawNextSpawn();
		}
		if ( !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
			CG_DrawDeathScreen();
			CG_DrawTrailGrey();
		}

		// don't draw any status if dead or the scoreboard is being explicitly shown
		if ( !cg.showScores && cg.snap->ps.stats[STAT_HEALTH] > 0 ) {

#ifdef MISSIONPACK
			if ( cg_drawStatus.integer ) {
				Menu_PaintAll();
				CG_DrawTimedMenus();
			}
#else
			if ( CG_HudModern() ) {
				CG_DrawModernHud();
			} else if ( cg_hudStyle.integer ) {
				CG_DrawHud();
			} else {
				CG_DrawStatusBar();
			}
#endif
      
			CG_DrawAmmoWarning();

#ifdef MISSIONPACK
			CG_DrawProxWarning();
#endif      
			CG_DrawShellShock();
			if(stereoFrame == STEREO_CENTER)
				CG_DrawCrosshair();
			CG_DrawCrosshairNames();
			CG_DrawDamageArc();
#ifndef MISSIONPACK
			CG_DrawWorldItemTimers();
#endif
			CG_DrawLowHealth();
			// the classic pop-up weapon row; the modern layout has its bar on the right
			if ( !CG_HudModern() ) {
				CG_DrawWeaponSelect();
			}

#ifndef MISSIONPACK
			CG_DrawHoldableItem();
			if ( !cg_hudStyle.integer ) {
				CG_DrawAbility();
			}
#else
			//CG_DrawPersistantPowerup();
#endif
			CG_DrawReward();
		}
	}

	if ( cgs.gametype >= GT_TEAM ) {
#ifndef MISSIONPACK
		CG_DrawTeamInfo();
#endif
	}

	CG_DrawTeamVote();

	cg_hudAnchor = ANCHOR_RIGHT;
	CG_DrawLagometer();
	cg_hudAnchor = ANCHOR_CENTER;

#ifdef MISSIONPACK
	if (!cg_paused.integer) {
		cg_hudAnchor = ANCHOR_RIGHT;
		CG_DrawUpperRight(stereoFrame);
		cg_hudAnchor = ANCHOR_CENTER;
	}
#else
	cg_hudAnchor = ANCHOR_RIGHT;
	CG_DrawUpperRight(stereoFrame);
	cg_hudAnchor = ANCHOR_CENTER;
#endif

#ifndef MISSIONPACK
	if ( cg_hudStyle.integer ) {
		if ( cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
			CG_DrawHudClock();
		}
		cg_hudAnchor = ANCHOR_RIGHT;
		CG_DrawKillFeed();
		cg_hudAnchor = ANCHOR_CENTER;
		if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 1 ) {
			float	ox, oy;

			// top left; in the modern layout under the kill feed
			CG_HudPlace( "teamoverlay", &ox, &oy );
			cg_hudAnchor = ANCHOR_LEFT;
			voteY = CG_DrawTeamOverlay( CG_HudModern() ? oy : 28, qfalse, qtrue ) + 4;
			cg_hudAnchor = ANCHOR_CENTER;
		}
		// free-flying spectators: red top left, blue top right
		if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 1
			&& cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR && !( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
			cg.overlayTeam = TEAM_RED;
			cg_hudAnchor = ANCHOR_LEFT;
			voteY = CG_DrawTeamOverlay( 28, qfalse, qtrue ) + 4;
			cg.overlayTeam = TEAM_BLUE;
			cg_hudAnchor = ANCHOR_RIGHT;
			CG_DrawTeamOverlay( 28, qtrue, qtrue );
			cg_hudAnchor = ANCHOR_CENTER;
			cg.overlayTeam = 0;
		}
		cg_hudAnchor = ANCHOR_LEFT;
		CG_DrawLowerLeft();
		cg_hudAnchor = ANCHOR_CENTER;
	} else {
		cg_hudAnchor = ANCHOR_RIGHT;
		CG_DrawLowerRight();
		cg_hudAnchor = ANCHOR_CENTER;
		cg_hudAnchor = ANCHOR_LEFT;
		CG_DrawLowerLeft();
		cg_hudAnchor = ANCHOR_CENTER;
	}
	cg_hudAnchor = ANCHOR_LEFT;
	CG_DrawChat();
	cg_hudAnchor = ANCHOR_CENTER;
	CG_DrawVote( voteY > 58 ? voteY : 58 );
	cg_hudAnchor = ANCHOR_LEFT;
	CG_DrawSpecItemTimers();
	cg_hudAnchor = ANCHOR_CENTER;
	CG_DrawDuelBars();
	CG_DrawTimeout();
	CG_DrawAccuracy();
#endif

	if ( !CG_DrawFollow() ) {
		CG_DrawWarmup();
	}

	// don't draw center string if scoreboard is up
	cg.scoreBoardShowing = CG_DrawScoreboard();
	if ( !cg.scoreBoardShowing) {
		CG_DrawCenterString();
		CG_DrawFragBanner();
	}
}


/*
=====================
CG_DrawActive

Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive( stereoFrame_t stereoView ) {
	// optionally draw the info screen instead
	if ( !cg.snap ) {
		CG_DrawInformation();
		return;
	}

	// optionally draw the tournement scoreboard instead
	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
		( cg.snap->ps.pm_flags & PMF_SCOREBOARD ) ) {
		CG_DrawTourneyScoreboard();
		return;
	}

	// clear around the rendered view if sized down
	CG_TileClear();

	if(stereoView != STEREO_CENTER)
		CG_DrawCrosshair3D();

	// draw 3D view
	trap_R_RenderScene( &cg.refdef );
	CG_DrawRevealedModels();

	// draw status bar and other floating elements
 	CG_Draw2D(stereoView);
	CG_LayoutFrameEnd();
}



