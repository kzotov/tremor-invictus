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
// ui_botsrml.c -- bots during a game, as an RmlUi page (ui/bots.rml):
// add a bot for a champion, remove bots, and team orders to them
//
// Cvars for the page: ui_bot<n>_name and ui_bot<n>_mate (on our team) for the bots in
// the game, ui_botTeamGame 1 in team modes, ui_botCtf 1 in capture the flag.
// Commands: ui_botAdd <champion>, ui_botRemove <slot>, ui_botOrder <slot>
// <order>, ui_botLeader <0|1>.
//

#include "ui_local.h"

#define BOT_SLOTS	16

static int	botClients[BOT_SLOTS];
static char	botNames[BOT_SLOTS][36];

static void Bots_Publish( void ) {
	char	info[MAX_INFO_STRING];
	int		n, count = 0, maxClients, myTeam;
	uiClientState_t	cs;

	trap_GetConfigString( CS_SERVERINFO, info, sizeof( info ) );
	maxClients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
	trap_Cvar_Set( "ui_botTeamGame", atoi( Info_ValueForKey( info, "g_gametype" ) ) >= GT_TEAM ? "1" : "0" );
	trap_Cvar_Set( "ui_botCtf", atoi( Info_ValueForKey( info, "g_gametype" ) ) == GT_CTF ? "1" : "0" );
	trap_GetClientState( &cs );
	trap_GetConfigString( CS_PLAYERS + cs.clientNum, info, sizeof( info ) );
	myTeam = atoi( Info_ValueForKey( info, "t" ) );
	for ( n = 0; n < maxClients && n < MAX_CLIENTS && count < BOT_SLOTS; n++ ) {
		trap_GetConfigString( CS_PLAYERS + n, info, sizeof( info ) );
		if ( !atoi( Info_ValueForKey( info, "skill" ) ) ) {
			continue;
		}
		botClients[count] = n;
		Q_strncpyz( botNames[count], Info_ValueForKey( info, "n" ), sizeof( botNames[count] ) );
		Q_CleanStr( botNames[count] );
		trap_Cvar_Set( va( "ui_bot%i_name", count ), botNames[count] );
		trap_Cvar_Set( va( "ui_bot%i_mate", count ), atoi( Info_ValueForKey( info, "t" ) ) == myTeam ? "1" : "0" );
		count++;
	}
	for ( n = count; n < BOT_SLOTS; n++ ) {
		trap_Cvar_Set( va( "ui_bot%i_name", n ), "" );
	}
}

/*
=================
UI_BotsRml

tab: 0 add, 1 remove, 2 team orders
=================
*/
void UI_BotsRml( int tab ) {
	trap_Cvar_Register( NULL, "ui_botSkill", "3", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_botTeam", "", CVAR_TEMP );
	trap_Cvar_SetValue( "ui_botTab", tab );
	Bots_Publish();
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/bots.rml\n" );
}

// a bot whose model is the champion's, or any
static const char *Bots_For( int champion ) {
	int		i, n = UI_GetNumBots();

	for ( i = 0; i < n; i++ ) {
		if ( BG_ChampionForModel( Info_ValueForKey( UI_GetBotInfoByNumber( i ), "model" ) ) == champion ) {
			return Info_ValueForKey( UI_GetBotInfoByNumber( i ), "name" );
		}
	}
	return n > 0 ? Info_ValueForKey( UI_GetBotInfoByNumber( rand() % n ), "name" ) : "sarge";
}

/*
=================
UI_BotsCommand
=================
*/
qboolean UI_BotsCommand( const char *cmd ) {
	static const char	*orders[] = { "%s follow me", "%s roam", "%s defend the base", "%s get enemy flag", "%s camp here", "%s report" };
	int					i;

	if ( !Q_stricmp( cmd, "ui_botAdd" ) ) {
		i = BG_ChampionForName( UI_Argv( 1 ) );
		if ( i >= 0 ) {
			char	team[16];

			trap_Cvar_VariableStringBuffer( "ui_botTeam", team, sizeof( team ) );
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "addbot %s %i %s\n", (char *)Bots_For( i ),
				(int)Com_Clamp( 1, 5, trap_Cvar_VariableValue( "ui_botSkill" ) ), team ) );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_botRemove" ) ) {
		i = atoi( UI_Argv( 1 ) );
		if ( i >= 0 && i < BOT_SLOTS && botNames[i][0] ) {
			trap_Cmd_ExecuteText( EXEC_NOW, va( "clientkick %i\n", botClients[i] ) );
			trap_Cvar_Set( va( "ui_bot%i_name", i ), "" );
			botNames[i][0] = 0;
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_botOrder" ) ) {
		int order = atoi( UI_Argv( 2 ) );

		i = atoi( UI_Argv( 1 ) );
		if ( i >= 0 && i < BOT_SLOTS && order >= 0 && order < ARRAY_LEN( orders ) && botNames[i][0] ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "say_team \"%s\"\n", va( (char *)orders[order], botNames[i] ) ) );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_botLeader" ) ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, atoi( UI_Argv( 1 ) ) ? "say_team \"i am the leader\"\n" : "say_team \"i stop being the leader\"\n" );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_botRefresh" ) ) {
		Bots_Publish();
		return qtrue;
	}
	return qfalse;
}
