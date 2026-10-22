/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2026 tremor-invictus contributors

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
// ui_voicerml.c -- the players in the game, with a voice chat mute for each,
// as a generated RmlUi page (ui-gen/players.rml) from the in-game menu
//
// Cvars for the page: ui_pm<n> 1 while player n is muted. Commands:
// ui_players shows the page, ui_playerMute <n> mutes or unmutes player n.

#include "ui_local.h"

/*
=================
UI_VoipMuted

Whether the engine has player n muted (cl_voipMuted: a hex digit per four
players)
=================
*/
static qboolean UI_VoipMuted( int n ) {
	char	mask[MAX_CLIENTS / 4 + 1];
	int		digit;
	char	c;

	trap_Cvar_VariableStringBuffer( "cl_voipMuted", mask, sizeof( mask ) );
	if ( n < 0 || n / 4 >= (int)strlen( mask ) ) {
		return qfalse;
	}
	c = mask[n / 4];
	digit = c >= 'a' ? c - 'a' + 10 : c - '0';
	return ( digit >> ( n % 4 ) ) & 1;
}

/*
=================
UI_PlayersRml

Writes the page: a row per player (not bots, not us) with a mute button
=================
*/
static void UI_PlayersRml( void ) {
	static char		page[32768];
	char			info[MAX_INFO_STRING], name[64];
	uiClientState_t	cs;
	fileHandle_t	f;
	int				n, rows = 0;

	trap_GetClientState( &cs );
	page[0] = 0;
	Q_strcat( page, sizeof( page ), "<rml>\n<head>\n<title>Players</title>\n<link type=\"text/rcss\" href=\"/ui/tremor.rcss\"/>\n</head>\n"
		"<body data-escape=\"rml_show ui/ingame.rml\">\n<div class=\"fs\">\n<h1>[pl_title]</h1>\n"
		"<div class=\"fshead\"><span class=\"d1\">[pl_name]</span></div>\n<div class=\"fslist\">\n" );
	for ( n = 0; n < MAX_CLIENTS; n++ ) {
		trap_GetConfigString( CS_PLAYERS + n, info, sizeof( info ) );
		if ( !info[0] || n == cs.clientNum || atoi( Info_ValueForKey( info, "skill" ) ) ) {
			continue;
		}
		Q_strncpyz( name, Info_ValueForKey( info, "n" ), sizeof( name ) );
		Q_CleanStr( name );
		trap_Cvar_SetValue( va( "ui_pm%i", n ), UI_VoipMuted( n ) );
		Q_strcat( page, sizeof( page ), va( "<div class=\"fsrow\"><span class=\"d1\">%s</span>", UI_RmlText( name ) ) );
		Q_strcat( page, sizeof( page ), va( "<button class=\"opt\" data-when=\"ui_pm%i=0\" onclick=\"ui_playerMute %i\">%s</button>",
			n, n, BG_String( STR_SCORE_MUTE ) ) );
		Q_strcat( page, sizeof( page ), va( "<button class=\"opt on\" data-when=\"ui_pm%i=1\" onclick=\"ui_playerMute %i\">%s</button></div>\n",
			n, n, BG_String( STR_SCORE_UNMUTE ) ) );
		rows++;
	}
	if ( !rows ) {
		Q_strcat( page, sizeof( page ), "<div class=\"fsrow\">[pl_none]</div>\n" );
	}
	Q_strcat( page, sizeof( page ), "</div>\n<div class=\"fsactions\"><button onclick=\"rml_show ui/ingame.rml\">[menu_back]</button></div>\n"
		"</div>\n</body>\n</rml>\n" );

	trap_FS_FOpenFile( "ui-gen/players.rml", &f, FS_WRITE );
	if ( !f ) {
		return;
	}
	trap_FS_Write( page, strlen( page ), f );
	trap_FS_FCloseFile( f );
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui-gen/players.rml\n" );
}

/*
=================
UI_VoiceCommand
=================
*/
qboolean UI_VoiceCommand( const char *cmd ) {
	int		n;

	if ( !Q_stricmp( cmd, "ui_players" ) ) {
		UI_PlayersRml();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_playerMute" ) ) {
		n = atoi( UI_Argv( 1 ) );
		if ( n < 0 || n >= MAX_CLIENTS ) {
			return qtrue;
		}
		// our own cvar follows at once; the engine's mask on its next frame
		if ( trap_Cvar_VariableValue( va( "ui_pm%i", n ) ) ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "voip unignore %i\n", n ) );
			trap_Cvar_SetValue( va( "ui_pm%i", n ), 0 );
		} else {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "voip ignore %i\n", n ) );
			trap_Cvar_SetValue( va( "ui_pm%i", n ), 1 );
		}
		return qtrue;
	}
	return qfalse;
}
