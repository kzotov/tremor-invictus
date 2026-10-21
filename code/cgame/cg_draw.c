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
#define KILLFEED_TIME		3000

typedef struct {
	char	killer[MAX_QPATH];
	char	victim[MAX_QPATH];
	int		weapon;			// icon, WP_NONE for none
	int		time;
} killFeed_t;

static killFeed_t	killFeed[KILLFEED_QUEUE];
static int			killFeedHead;

/*
=================
CG_AddKillFeed
=================
*/
void CG_AddKillFeed( const char *killer, const char *victim, int weapon ) {
	killFeed_t	*k = &killFeed[killFeedHead++ % KILLFEED_QUEUE];

	Q_strncpyz( k->killer, killer, sizeof( k->killer ) );
	Q_strncpyz( k->victim, victim, sizeof( k->victim ) );
	k->weapon = weapon;
	k->time = cg.time;
}

static void CG_DrawKillFeed( void ) {
	killFeed_t	*k;
	int			i, n, rows = cg_obituaryRowSize.integer, y = 40, w, x;
	vec4_t		fade = { 1, 1, 1, 1 };

	if ( rows <= 0 ) {
		return;
	}
	if ( rows > 5 ) {
		rows = 5;
	}
	for ( i = 0, n = 0 ; i < KILLFEED_QUEUE && n < rows ; i++ ) {
		k = &killFeed[( killFeedHead - 1 - i + KILLFEED_QUEUE * 2 ) % KILLFEED_QUEUE];
		if ( !k->time || cg.time - k->time > KILLFEED_TIME || k->time > cg.time ) {
			continue;
		}
		fade[3] = cg.time - k->time > KILLFEED_TIME - 500 ? ( KILLFEED_TIME - ( cg.time - k->time ) ) / 500.0f : 1;
		x = 636;
		w = CG_DrawStrlen( k->victim ) * TINYCHAR_WIDTH;
		x -= w;
		CG_DrawStringExt( x, y + 4, k->victim, fade, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		x -= 20;
		if ( k->weapon > WP_NONE && k->weapon < WP_NUM_WEAPONS && cg_weapons[k->weapon].weaponIcon ) {
			trap_R_SetColor( fade );
			CG_DrawPic( x + 2, y, 16, 16, cg_weapons[k->weapon].weaponIcon );
			trap_R_SetColor( NULL );
		} else {
			CG_DrawStringExt( x + 4, y + 4, ">", fade, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
		if ( k->killer[0] ) {
			w = CG_DrawStrlen( k->killer ) * TINYCHAR_WIDTH;
			x -= w + 4;
			CG_DrawStringExt( x, y + 4, k->killer, fade, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
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
#define CHAT_TIME			8000
#define CHAT_WIDTH			56		// characters a line, wrapped at spaces
#define CHAT_Y				360		// bottom line

typedef struct {
	char	text[MAX_SAY_TEXT];
	int		time;
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
	int			i, rows, y = CHAT_Y;
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
		if ( !cg.showChatHistory && cg.time - c->time > CHAT_TIME ) {
			break;
		}
		color[3] = !cg.showChatHistory && cg.time - c->time > CHAT_TIME - 500
			? ( CHAT_TIME - ( cg.time - c->time ) ) / 500.0f : 1;
		CG_DrawStringExt( 8, y, c->text, color, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		y -= SMALLCHAR_HEIGHT + 2;
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
		if ( item <= 0 || item >= bg_numItems || !( BG_ItemTimerClass( &bg_itemlist[item] ) & cg_specItemTimers.integer ) ) {
			continue;
		}
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
	if ( value <= 25 ) {
		VectorSet( out, 1.0f, 0.2f, 0.2f );
	} else if ( value <= 50 ) {
		VectorSet( out, 1.0f, 0.85f, 0.2f );
	} else {
		VectorSet( out, 1.0f, 1.0f, 1.0f );
	}
	out[3] = 1.0f;
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
	int			limit = cgs.timelimit, msec, mins, secs, w;
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
	secs = msec / 1000;
	mins = secs / 60;
	secs %= 60;
	s = va( "%i:%02i", mins, secs );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	CG_DrawBigString( 320 - w / 2, 4, s, 1.0f );

	// scores either side of the clock
	if ( cgs.gametype >= GT_TEAM ) {
		s = va( "%i", cgs.scores1 );
		CG_DrawStringExt( 320 - w / 2 - 12 - CG_DrawStrlen( s ) * BIGCHAR_WIDTH, 4, s, red, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
		s = va( "%i", cgs.scores2 );
		CG_DrawStringExt( 320 + w / 2 + 12, 4, s, blue, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	} else if ( cgs.scores1 != SCORE_NOT_PRESENT ) {
		s = va( "%i", cg.snap->ps.persistant[PERS_SCORE] );
		CG_DrawStringExt( 320 - w / 2 - 12 - CG_DrawStrlen( s ) * BIGCHAR_WIDTH, 4, s, colorWhite, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
		// the best other score
		s = va( "%i", cgs.scores1 == cg.snap->ps.persistant[PERS_SCORE] && cgs.scores2 != SCORE_NOT_PRESENT ? cgs.scores2 : cgs.scores1 );
		CG_DrawStringExt( 320 + w / 2 + 12, 4, s, colorMdGrey, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
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
CG_DrawPassiveState

A passive with a state of its own, above the portrait: Slash's and
Strogg's slide charge
=================
*/
static void CG_DrawPassiveState( void ) {
	playerState_t	*ps = &cg.snap->ps;
	char			*s;

	if ( BG_Champion( ps->stats[STAT_CHAMPION] )->moveStyle == MOVE_SLIDE ) {
		s = va( (char *)BG_String( STR_HUD_SLIDE ), BG_SlideCharge( ps ) / 1000.0f );
		CG_DrawStringExt( 320 - CG_DrawStrlen( s ) * TINYCHAR_WIDTH / 2, 410, s, colorWhite,
			qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
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

	if ( !cg_drawSpeed.integer ) {
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
		for ( i = 0 ; i < maxCharges ; i++ ) {
			CG_FillRect( 320 - maxCharges * 5 + i * 10 + 2, 474, 6, 4, i < charges ? colorWhite : colorMdGrey );
		}
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
void CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team )
{
	vec4_t		hcolor;

	hcolor[3] = alpha;
	if ( team == TEAM_RED ) {
		hcolor[0] = 1;
		hcolor[1] = 0;
		hcolor[2] = 0;
	} else if ( team == TEAM_BLUE ) {
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 1;
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

	if ( !cg_drawTeamOverlay.integer ) {
		return y;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] != TEAM_RED && cg.snap->ps.persistant[PERS_TEAM] != TEAM_BLUE ) {
		return y; // Not on any team
	}

	plyrs = 0;

	// max player name width
	pwidth = 0;
	count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
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

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED ) {
		hcolor[0] = 1.0f;
		hcolor[1] = 0.0f;
		hcolor[2] = 0.0f;
		hcolor[3] = 0.33f;
	} else { // if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE )
		hcolor[0] = 0.0f;
		hcolor[1] = 0.0f;
		hcolor[2] = 1.0f;
		hcolor[3] = 0.33f;
	}
	trap_R_SetColor( hcolor );
	CG_DrawPic( x, y, w, h, cgs.media.teamStatusBar );
	trap_R_SetColor( NULL );

	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {

			hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0;

			// the teammate's champion
			if ( ci->modelIcon ) {
				CG_DrawPic( x + TINYCHAR_WIDTH, y, TINYCHAR_HEIGHT, TINYCHAR_HEIGHT, ci->modelIcon );
			}
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
		while ( *cs ) {
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
	if ( cg_drawTimer.integer ) {
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
CG_DrawRevealedPlayers

Piercing sight: outline enemies through walls
===================
*/
static void CG_DrawRevealedPlayers( void ) {
	playerState_t	*ps = &cg.snap->ps;
	centity_t		*cent;
	vec3_t			feet, head;
	float			x1, y1, x2, y2, w;
	int				i, myTeam;
	static vec4_t	revealColor = { 1.0f, 0.2f, 0.1f, 0.8f };
	qboolean		sight;

	// piercing sight shows every enemy; marks and storms show the players they caught
	sight = BG_Champion( ps->stats[STAT_CHAMPION] )->active == ACTIVE_PIERCINGSIGHT
		&& BG_AbilityState( ps ) == ABILITY_ACTIVE;

	myTeam = ps->persistant[PERS_TEAM];
	for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
		cent = &cg_entities[i];
		if ( i == ps->clientNum || !cent->currentValid || cent->currentState.eType != ET_PLAYER ) {
			continue;
		}
		if ( !sight && !( cent->currentState.eFlags & EF_MARKED ) ) {
			continue;
		}
		if ( cent->currentState.eFlags & EF_DEAD ) {
			continue;
		}
		if ( cgs.gametype >= GT_TEAM && cgs.clientinfo[i].team == myTeam ) {
			continue;
		}
		VectorCopy( cent->lerpOrigin, feet );
		VectorCopy( cent->lerpOrigin, head );
		feet[2] += MINS_Z;
		head[2] += DEFAULT_HEIGHT;
		if ( !CG_WorldToScreen( feet, &x1, &y1 ) || !CG_WorldToScreen( head, &x2, &y2 ) ) {
			continue;
		}
		w = ( y1 - y2 ) * 0.4f;
		CG_DrawRect( x1 - w * 0.5f, y2, w, y1 - y2, 1, revealColor );
	}
}

#ifndef MISSIONPACK
/*
===================
CG_DrawAbility

Active ability name, charges left and time to the next charge
===================
*/
static void CG_DrawAbility( void ) {
	const champion_t	*champ;
	playerState_t		*ps;
	const char			*s;
	int					y, tenths;
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
	CG_DrawStringExt( 640 - 8 - CG_DrawStrlen( s ) * SMALLCHAR_WIDTH, y, s, color, qfalse, qtrue,
		SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );

	tenths = ps->stats[STAT_ABILITY_RECHARGE];
	if ( tenths > 0 ) {
		s = va( (char *)BG_String( STR_ABILITY_RECHARGE ), ( tenths + 9 ) / 10 );
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
static void CG_DrawReward( void ) { 
	float	*color;
	int		i, count;
	float	x, y;
	char	buf[32];

	if ( !cg_drawRewards.integer ) {
		return;
	}

	color = CG_FadeColor( cg.rewardTime, REWARD_TIME );
	if ( !color ) {
		if (cg.rewardStack > 0) {
			for(i = 0; i < cg.rewardStack; i++) {
				cg.rewardSound[i] = cg.rewardSound[i+1];
				cg.rewardShader[i] = cg.rewardShader[i+1];
				cg.rewardCount[i] = cg.rewardCount[i+1];
				cg.rewardName[i] = cg.rewardName[i+1];
			}
			cg.rewardTime = cg.time;
			cg.rewardStack--;
			color = CG_FadeColor( cg.rewardTime, REWARD_TIME );
			trap_S_StartLocalSound(cg.rewardSound[0], CHAN_ANNOUNCER);
		} else {
			return;
		}
	}

	// a medal: its icon (or a plate when the install has none), name and count
	if ( cg.rewardName[0] ) {
		static vec4_t	plate = { 0.5f, 0.08f, 0.06f, 0.85f };
		vec4_t			c;
		const char		*name = BG_String( cg.rewardName[0] );

		Vector4Copy( plate, c );
		c[3] *= color[3];
		if ( cg.rewardShader[0] ) {
			trap_R_SetColor( color );
			CG_DrawPic( 320 - ICON_SIZE / 2, 56, ICON_SIZE - 4, ICON_SIZE - 4, cg.rewardShader[0] );
			trap_R_SetColor( NULL );
		} else {
			CG_FillRect( 320 - ICON_SIZE / 2, 56, ICON_SIZE - 4, ICON_SIZE - 4, c );
			CG_DrawStringExt( 320 - SMALLCHAR_WIDTH / 2 - 2, 56 + ( ICON_SIZE - 4 - SMALLCHAR_HEIGHT ) / 2,
				va( "%c", name[0] ), color, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		}
		CG_DrawStringExt( 320 - CG_DrawStrlen( name ) * SMALLCHAR_WIDTH / 2, 56 + ICON_SIZE, name, color,
			qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		if ( cg.rewardCount[0] > 1 ) {
			name = va( "x%i", cg.rewardCount[0] );
			CG_DrawStringExt( 320 - CG_DrawStrlen( name ) * SMALLCHAR_WIDTH / 2, 56 + ICON_SIZE + SMALLCHAR_HEIGHT + 2,
				name, color, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
		}
		return;
	}

	trap_R_SetColor( color );

	/*
	count = cg.rewardCount[0]/10;				// number of big rewards to draw

	if (count) {
		y = 4;
		x = 320 - count * ICON_SIZE;
		for ( i = 0 ; i < count ; i++ ) {
			CG_DrawPic( x, y, (ICON_SIZE*2)-4, (ICON_SIZE*2)-4, cg.rewardShader[0] );
			x += (ICON_SIZE*2);
		}
	}

	count = cg.rewardCount[0] - count*10;		// number of small rewards to draw
	*/

	if ( cg.rewardCount[0] >= 2 ) {
		// one medal with its count, QL style
		y = 56;
		x = 320 - ICON_SIZE/2;
		CG_DrawPic( x, y, ICON_SIZE-4, ICON_SIZE-4, cg.rewardShader[0] );
		Com_sprintf(buf, sizeof(buf), "%d", cg.rewardCount[0]);
		x = ( SCREEN_WIDTH - SMALLCHAR_WIDTH * CG_DrawStrlen( buf ) ) / 2;
		CG_DrawStringExt( x, y+ICON_SIZE, buf, color, qfalse, qtrue,
								SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
	}
	else {

		count = cg.rewardCount[0];

		y = 56;
		x = 320 - count * ICON_SIZE/2;
		for ( i = 0 ; i < count ; i++ ) {
			CG_DrawPic( x, y, ICON_SIZE-4, ICON_SIZE-4, cg.rewardShader[0] );
			x += ICON_SIZE;
		}
	}
	trap_R_SetColor( NULL );
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
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint( const char *str, int y, int charWidth ) {
	char	*s;

	Q_strncpyz( cg.centerPrint, str, sizeof(cg.centerPrint) );

	cg.centerPrintTime = cg.time;
	cg.centerPrintY = y;
	cg.centerPrintCharWidth = charWidth;

	// count the number of lines for centering
	cg.centerPrintLines = 1;
	s = cg.centerPrint;
	while( *s ) {
		if (*s == '\n')
			cg.centerPrintLines++;
		s++;
	}
}


/*
===================
CG_DrawCenterString
===================
*/
static void CG_DrawCenterString( void ) {
	char	*start;
	int		l;
	int		x, y, w;
#ifdef MISSIONPACK
	int h;
#endif
	float	*color;

	if ( !cg.centerPrintTime ) {
		return;
	}

	color = CG_FadeColor( cg.centerPrintTime, 1000 * cg_centertime.value );
	if ( !color ) {
		return;
	}

	trap_R_SetColor( color );

	start = cg.centerPrint;

	y = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;

	while ( 1 ) {
		char linebuffer[1024];

		for ( l = 0; l < 50; l++ ) {
			if ( !start[l] || start[l] == '\n' ) {
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

#ifdef MISSIONPACK
		w = CG_Text_Width(linebuffer, 0.5, 0);
		h = CG_Text_Height(linebuffer, 0.5, 0);
		x = (SCREEN_WIDTH - w) / 2;
		CG_Text_Paint(x, y + h, 0.5, color, linebuffer, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		y += h + 6;
#else
		w = cg.centerPrintCharWidth * CG_DrawStrlen( linebuffer );

		x = ( SCREEN_WIDTH - w ) / 2;

		CG_DrawStringExt( x, y, linebuffer, color, qfalse, qtrue,
			cg.centerPrintCharWidth, (int)(cg.centerPrintCharWidth * 1.5), 0 );

		y += cg.centerPrintCharWidth * 1.5;
#endif
		while ( *start && ( *start != '\n' ) ) {
			start++;
		}
		if ( !*start ) {
			break;
		}
		start++;
	}

	trap_R_SetColor( NULL );
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
	trap_R_SetColor( NULL );
}


//==============================================================================

/*
=================
CG_DrawSpectator
=================
*/
static void CG_DrawSpectator(void) {
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

	if ( !cs[0] ) {
		return;
	}
	p = (char *)cs;
	endTime = atoi( COM_Parse( &p ) );
	for ( i = 0 ; i < 3 ; i++ ) {
		Q_strncpyz( maps[i], COM_Parse( &p ), sizeof( maps[i] ) );
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		votes[i] = atoi( COM_Parse( &p ) );
	}
	if ( !maps[0][0] || cg.time >= endTime ) {
		return;
	}
	if ( cg.mapVoteTime != endTime ) {
		cg.mapVoteTime = endTime;		// a new vote: nothing chosen yet
		cg.mapVote = 0;
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
			shot = trap_R_RegisterShaderNoMip( "menu/art/unknownmap" );
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

#define PILLAR_RADIUS	24
#define PILLAR_DIST		300		// how far in front of the camera the row stands

/*
=================
CG_PillarPoly
=================
*/
static void CG_PillarPoly( vec3_t a, vec3_t b, vec3_t c, vec3_t d, const byte *rgba ) {
	polyVert_t	v[4];
	int			i;

	VectorCopy( a, v[0].xyz );
	VectorCopy( b, v[1].xyz );
	VectorCopy( c, v[2].xyz );
	VectorCopy( d, v[3].xyz );
	for ( i = 0 ; i < 4 ; i++ ) {
		v[i].st[0] = v[i].st[1] = 0;
		v[i].modulate[0] = rgba[0];
		v[i].modulate[1] = rgba[1];
		v[i].modulate[2] = rgba[2];
		v[i].modulate[3] = rgba[3];
	}
	trap_R_AddPolyToScene( cgs.media.whiteShader, 4, v );
}

/*
=================
CG_AddPillar

A hexagonal column from the floor up to `top`, with a coloured strip
around its top edge
=================
*/
static void CG_AddPillar( const vec3_t base, float top, const byte *strip ) {
	static const byte	side[4] = { 18, 18, 22, 255 };
	static const byte	cap[4] = { 34, 34, 40, 255 };
	vec3_t	lo[6], hi[6], st[6], center;
	int		i, j;

	for ( i = 0 ; i < 6 ; i++ ) {
		float	a = DEG2RAD( 30 + i * 60 );

		VectorSet( lo[i], base[0] + cos( a ) * PILLAR_RADIUS, base[1] + sin( a ) * PILLAR_RADIUS, base[2] - 400 );
		VectorSet( hi[i], lo[i][0], lo[i][1], base[2] + top );
		VectorSet( st[i], lo[i][0], lo[i][1], base[2] + top - 4 );
	}
	for ( i = 0 ; i < 6 ; i++ ) {
		j = ( i + 1 ) % 6;
		CG_PillarPoly( lo[i], lo[j], st[j], st[i], side );
		CG_PillarPoly( st[i], st[j], hi[j], hi[i], strip );
	}
	VectorSet( center, base[0], base[1], base[2] + top );
	for ( i = 0 ; i < 6 ; i++ ) {
		j = ( i + 1 ) % 6;
		CG_PillarPoly( center, hi[j], hi[i], center, cap );
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
static void CG_DrawPillars( const int *clients, const int *heights, const int *teams, int count, const char **labels ) {
	static const byte	strips[3][4] = { { 179, 178, 173, 255 }, { 208, 26, 26, 255 }, { 40, 90, 220, 255 } };
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
	spacing = count > 1 ? 2 * PILLAR_DIST * tan( DEG2RAD( 25 ) ) / ( count - 1 ) : 0;
	if ( spacing > 110 ) {
		spacing = 110;
	}
	for ( i = 0 ; i < count ; i++ ) {
		y = ( ( count - 1 ) / 2.0f - i ) * spacing;
		VectorSet( base, PILLAR_DIST, y, -60 + ( heights ? heights[i] : 0 ) );
		CG_AddPillar( base, 0, strips[teams ? teams[i] : 0] );
		CG_AddPillarPlayer( clients[i], base );
	}
	trap_R_AddLightToScene( vec3_origin, 600, 1, 1, 1 );
	trap_R_RenderScene( &refdef );

	// names under the pillars' tops
	for ( i = 0 ; i < count ; i++ ) {
		y = ( ( count - 1 ) / 2.0f - i ) * spacing;
		// the pillar's place on screen, in 640x480 units (the scene spans the whole width)
		sx = 320 - y / ( PILLAR_DIST * tan( DEG2RAD( 30 ) ) ) * 0.5f * cgs.glconfig.vidWidth * 480 / cgs.glconfig.vidHeight;
		ci = &cgs.clientinfo[clients[i]];
		CG_DrawStringExt( sx - CG_DrawStrlen( ci->name ) * SMALLCHAR_WIDTH / 2, 404, ci->name, colorWhite,
			qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 16 );
		if ( labels && labels[i] ) {
			CG_DrawStringExt( sx - CG_DrawStrlen( labels[i] ) * TINYCHAR_WIDTH / 2, 424, labels[i], colorLtGrey,
				qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
	}
}

/*
=================
CG_DrawLineup

During the countdown: everyone in the match on a pillar, red team left,
blue right; in duel the local player is on the left
=================
*/
static void CG_DrawLineup( void ) {
	static const vec4_t	dim = { 0.05f, 0.05f, 0.06f, 0.85f };
	int		clients[16], teams[16], count = 0, i, t, local = cg.snap->ps.clientNum;

	if ( cg.warmup <= 0 || cg.warmup < cg.time ) {
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
	CG_FillRect( -1000, 0, 3000, 480, dim );
	CG_DrawPillars( clients, NULL, teams, count, NULL );
}

/*
=================
CG_DrawPodium

The first seconds of the intermission: the top three on pillars of
heights 3, 2 and 1, the winner in the middle
=================
*/
static void CG_DrawPodium( void ) {
	static const vec4_t	dim = { 0.05f, 0.05f, 0.06f, 0.85f };
	int		clients[3], heights[3], teams[3], count = 0, order[3] = { 1, 0, 2 }, i, n;
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
		teams[count] = cg.scores[rank].team == TEAM_RED ? 1 : cg.scores[rank].team == TEAM_BLUE ? 2 : 0;
		Com_sprintf( scoreText[count], sizeof( scoreText[count] ), "%i", cg.scores[rank].score );
		labels[count] = scoreText[count];
		count++;
	}
	CG_FillRect( -1000, 0, 3000, 480, dim );
	CG_DrawPillars( clients, heights, teams, count, labels );
}

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
		s = "Waiting for players";		
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		CG_DrawBigString(320 - w / 2, 24, s, 1.0F);
		cg.warmupCount = 0;
		return;
	}

	if (cgs.gametype == GT_TOURNAMENT) {
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
/*
=================
CG_Draw2D
=================
*/
static void CG_Draw2D(stereoFrame_t stereoFrame)
{
	float	voteY = 0;		// the vote line goes under the team overlay

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

	// the countdown: everyone on pillars
	if ( cg.warmup > cg.time && cg_drawLineup.integer ) {
		CG_DrawLineup();
		CG_DrawWarmup();
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
	} else {
		// don't draw any status if dead or the scoreboard is being explicitly shown
		if ( !cg.showScores && cg.snap->ps.stats[STAT_HEALTH] > 0 ) {

#ifdef MISSIONPACK
			if ( cg_drawStatus.integer ) {
				Menu_PaintAll();
				CG_DrawTimedMenus();
			}
#else
			if ( cg_hudStyle.integer ) {
				CG_DrawHud();
			} else {
				CG_DrawStatusBar();
			}
#endif
      
			CG_DrawAmmoWarning();

#ifdef MISSIONPACK
			CG_DrawProxWarning();
#endif      
			CG_DrawRevealedPlayers();
			CG_DrawShellShock();
			if(stereoFrame == STEREO_CENTER)
				CG_DrawCrosshair();
			CG_DrawCrosshairNames();
			CG_DrawWeaponSelect();

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
			cg_hudAnchor = ANCHOR_LEFT;
			voteY = CG_DrawTeamOverlay( 28, qfalse, qtrue ) + 4;	// top left, under the clock
			cg_hudAnchor = ANCHOR_CENTER;
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

	// draw status bar and other floating elements
 	CG_Draw2D(stereoView);
	CG_LayoutFrameEnd();
}



