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
// cg_info.c -- display information while data is being loading

#include "cg_local.h"

// how far loading has got, 0 to 1, from the stages CG_Init reports
static float		loadingFraction;

/*
======================
CG_LoadingString

======================
*/
void CG_LoadingString( const char *s ) {
	float	stage = loadingFraction;

	Q_strncpyz( cg.infoScreenText, s, sizeof( cg.infoScreenText ) );

	if ( !Q_stricmp( s, "collision map" ) ) {
		stage = 0.1f;
	} else if ( !Q_stricmp( s, "sounds" ) ) {
		stage = 0.2f;
	} else if ( !Q_stricmp( s, "graphics" ) || !Q_stricmp( s, "game media" ) ) {
		stage = 0.4f;
	} else if ( !Q_stricmp( s, "clients" ) ) {
		stage = 0.8f;
	} else if ( !s[0] ) {
		stage = 1.0f;
	}
	if ( stage > loadingFraction ) {
		loadingFraction = stage;
	}

	trap_UpdateScreen();
}

/*
===================
CG_LoadingItem
===================
*/
void CG_LoadingItem( int itemNum ) {
	float	stage;

	// items load during the graphics stage
	stage = 0.4f + 0.35f * itemNum / ( bg_numItems > 0 ? bg_numItems : 1 );
	if ( stage > loadingFraction ) {
		loadingFraction = stage;
	}
	CG_LoadingString( bg_itemlist[itemNum].pickup_name );
}

/*
===================
CG_LoadingClient
===================
*/
void CG_LoadingClient( int clientNum ) {
	char	name[MAX_QPATH];

	if ( loadingFraction >= 0.8f && loadingFraction < 0.98f ) {
		loadingFraction += 0.01f;
	}
	Q_strncpyz( name, Info_ValueForKey( CG_ConfigString( CS_PLAYERS + clientNum ), "n" ), sizeof( name ) );
	Q_CleanStr( name );
	CG_LoadingString( name );
}

/*
===================
CG_ModeString

The running mode's name, from the server's mode key
===================
*/
const char *CG_ModeString( void ) {
	const char	*mode = Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "g_mode" );

	if ( !Q_stricmp( mode, "duel" ) )		return BG_String( STR_MODE_DUEL );
	if ( !Q_stricmp( mode, "tdm" ) )		return BG_String( STR_MODE_TDM );
	if ( !Q_stricmp( mode, "ctf" ) )		return BG_String( STR_MODE_CTF );
	if ( !Q_stricmp( mode, "ca" ) )			return BG_String( STR_MODE_CA );
	if ( !Q_stricmp( mode, "sacrifice" ) )	return BG_String( STR_MODE_SACRIFICE );
	if ( !Q_stricmp( mode, "roundduel" ) )	return BG_String( STR_MODE_ROUNDDUEL );
	if ( !Q_stricmp( mode, "instagib" ) )	return BG_String( STR_MODE_INSTAGIB );
	if ( !Q_stricmp( mode, "unholytrinity" ) )	return BG_String( STR_MODE_TRINITY );
	return BG_String( STR_MODE_FFA );
}

/*
====================
CG_PickTip

A tip for this load: 40% the selected champion, 30% the mode, 30% general,
skipping the last 10 shown (kept in cg_tipHistory)
====================
*/
static int CG_PickTip( void ) {
	char		history[256], champion[64], *p, *token;
	const char	*mode = Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "g_mode" );
	int			recent[10], numRecent = 0, candidates[64], count, area, i, j, roll, pick;

	trap_Cvar_VariableStringBuffer( "cg_tipHistory", history, sizeof( history ) );
	trap_Cvar_VariableStringBuffer( "champion", champion, sizeof( champion ) );
	p = history;
	while ( numRecent < 10 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		recent[numRecent++] = atoi( token );
	}

	roll = ( trap_Milliseconds() * 7919 + 13 ) % 100;
	area = roll < 40 ? TIP_CHAMPION : roll < 70 ? TIP_MODE : TIP_GENERAL;
	for ( ; area >= TIP_GENERAL ; area-- ) {
		count = 0;
		for ( i = 0 ; i < bg_numTips && count < ARRAY_LEN( candidates ) ; i++ ) {
			if ( bg_tips[i].area != area ) {
				continue;
			}
			if ( area == TIP_MODE && Q_stricmp( bg_tips[i].key, mode ) ) {
				continue;
			}
			if ( area == TIP_CHAMPION && Q_stricmp( bg_tips[i].key, champion ) ) {
				continue;
			}
			for ( j = 0 ; j < numRecent && recent[j] != i ; j++ ) {
			}
			if ( j == numRecent ) {
				candidates[count++] = i;
			}
		}
		if ( count ) {
			break;
		}
		if ( area == TIP_CHAMPION ) {
			area = TIP_GENERAL + 1;		// no champion tip left: try general next
		}
	}
	if ( area < TIP_GENERAL || !count ) {
		trap_Cvar_Set( "cg_tipHistory", "" );
		return ( trap_Milliseconds() % bg_numTips );
	}
	pick = candidates[( trap_Milliseconds() / 7 ) % count];

	// remember it, the newest first
	Q_strncpyz( history, va( "%i", pick ), sizeof( history ) );
	for ( i = 0 ; i < numRecent && i < 9 ; i++ ) {
		Q_strcat( history, sizeof( history ), va( " %i", recent[i] ) );
	}
	trap_Cvar_Set( "cg_tipHistory", history );
	return pick;
}

/*
====================
CG_DrawTip

The tip under the progress bar, wrapped onto two lines when long; a new
one after 8 s of loading
====================
*/
static void CG_DrawTip( void ) {
	static int	tip = -1, tipTime;
	static vec4_t	accent = { 0.85f, 0.12f, 0.1f, 1 };
	static vec4_t	band = { 0.06f, 0.06f, 0.08f, 0.6f };
	char		line[2][128];
	const char	*text;
	int			len, cut, i, now = trap_Milliseconds();

	if ( !cg_loadingTips.integer || bg_numTips <= 0 ) {
		return;
	}
	if ( tip < 0 || now - tipTime > 8000 ) {
		tip = CG_PickTip();
		tipTime = now;
	}
	text = BG_String( bg_tips[tip].text );
	len = strlen( text );
	cut = len;
	if ( len > 70 ) {
		for ( cut = 70 ; cut > 0 && text[cut] != ' ' ; cut-- ) {
		}
		if ( !cut ) {
			cut = 70;
		}
	}
	Q_strncpyz( line[0], text, cut + 1 < sizeof( line[0] ) ? cut + 1 : sizeof( line[0] ) );
	Q_strncpyz( line[1], text[cut] ? text + cut + 1 : "", sizeof( line[1] ) );

	CG_FillRect( 0, 340, SCREEN_WIDTH, line[1][0] ? 66 : 48, band );
	CG_DrawSmallStringColor( 320 - CG_DrawStrlen( BG_String( STR_TIP ) ) * SMALLCHAR_WIDTH / 2, 346, BG_String( STR_TIP ), accent );
	for ( i = 0 ; i < 2 ; i++ ) {
		if ( line[i][0] ) {
			CG_DrawSmallStringColor( 320 - CG_DrawStrlen( line[i] ) * SMALLCHAR_WIDTH / 2, 364 + i * 18, line[i], colorLtGrey );
		}
	}
}

/*
====================
CG_DrawLevelshot

The map's picture over the whole screen, cropped to keep its shape
(CG_LevelshotAspect: a classic square levelshot or the missing-map picture
is 4:3, a 1920 x 1080 one 16:9)
====================
*/
static void CG_DrawLevelshot( qhandle_t shader, float picture ) {
	float	screen = (float)cgs.glconfig.vidWidth / cgs.glconfig.vidHeight;
	float	s0 = 0, s1 = 1, t0 = 0, t1 = 1;

	if ( screen > picture ) {
		t0 = 0.5f - 0.5f * picture / screen;
		t1 = 1.0f - t0;
	} else {
		s0 = 0.5f - 0.5f * screen / picture;
		s1 = 1.0f - s0;
	}
	trap_R_DrawStretchPic( 0, 0, cgs.glconfig.vidWidth, cgs.glconfig.vidHeight, s0, t0, s1, t1, shader );
}

/*
====================
CG_DrawInformation

The loading screen: the levelshot, our logo and the mode on a bar at the top,
the map's title and authors at the bottom left, and a progress bar
====================
*/
void CG_DrawInformation( void ) {
	static vec4_t	barColor = { 0.06f, 0.06f, 0.08f, 0.75f };
	static vec4_t	trackColor = { 1, 1, 1, 0.2f };
	static vec4_t	fillColor = { 0.85f, 0.12f, 0.1f, 1 };
	const char	*info = CG_ConfigString( CS_SERVERINFO );
	const char	*authors = CG_ConfigString( CS_MAPAUTHORS );
	const char	*title, *s;
	qhandle_t	levelshot;
	int			y;

	s = Info_ValueForKey( info, "mapname" );
	levelshot = trap_R_RegisterShaderNoMip( va( "levelshots/%s.tga", s ) );
	if ( !levelshot ) {
		levelshot = trap_R_RegisterShaderNoMip( "gfx/tremor/splash/nomap" );
	}
	trap_R_SetColor( NULL );
	CG_DrawLevelshot( levelshot, CG_LevelshotAspect( s ) );

	// top: logo left, mode right, on a dark bar across the screen
	CG_FillRect( 0, 0, SCREEN_WIDTH, 48, barColor );
	cg_hudAnchor = ANCHOR_LEFT;
	CG_DrawLogo( 14, 5, 38 );
	cg_hudAnchor = ANCHOR_RIGHT;
	UI_DrawProportionalString( SCREEN_WIDTH - 16, 14, CG_ModeString(), UI_RIGHT | UI_SMALLFONT | UI_DROPSHADOW, colorWhite );

	// bottom left: the map's title and up to two authors
	cg_hudAnchor = ANCHOR_LEFT;
	title = CG_ConfigString( CS_MESSAGE );
	if ( !title[0] ) {
		title = s;
	}
	y = SCREEN_HEIGHT - 32;
	if ( Info_ValueForKey( authors, "a2" )[0] ) {
		UI_DrawProportionalString( 16, y, Info_ValueForKey( authors, "a2" ), UI_LEFT | UI_SMALLFONT | UI_DROPSHADOW, colorLtGrey );
		y -= 18;
	}
	if ( Info_ValueForKey( authors, "a1" )[0] ) {
		UI_DrawProportionalString( 16, y, Info_ValueForKey( authors, "a1" ), UI_LEFT | UI_SMALLFONT | UI_DROPSHADOW, colorLtGrey );
		y -= 18;
	}
	UI_DrawProportionalString( 16, y - 10, title, UI_LEFT | UI_DROPSHADOW, colorWhite );
	cg_hudAnchor = ANCHOR_CENTER;

	// centre: "Loading" and the bar
	UI_DrawProportionalString( SCREEN_WIDTH / 2, 300, BG_String( STR_LOADING ), UI_CENTER | UI_SMALLFONT | UI_DROPSHADOW, colorWhite );
	CG_FillRect( 220, 324, 200, 4, trackColor );
	CG_FillRect( 220, 324, 200 * loadingFraction, 4, fillColor );
	CG_DrawTip();
}
