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
// ui_voterml.c -- the Call Vote page (callvote.md 4) from the in-game menu,
// generated as ui-gen/callvote.rml: only the votes the server allows
// (g_voteFlags in its serverinfo)
//
// Commands: ui_callvote shows the page; ui_cvMap calls the vote on the
// picked map; ui_cvStep <setting> <+n|-n> changes a number shown in
// ui_cv_<setting>; ui_cvCall <setting> calls the vote with that number.
// The map list is the Create Server one (ui_csMap<n>, ui_csPick).

#include "ui_local.h"

typedef struct {
	const char	*vote;			// the callvote type
	const char	*label;			// its text key
	int			flag;			// its VOTEFLAG_*
	const char	*from;			// serverinfo key the number starts at
	int			step, least, most;
} cvNumber_t;

static const cvNumber_t	cvNumbers[] = {
	{ "timelimit", "cv_timelimit", VOTEFLAG_TIMELIMIT, "timelimit", 5, 0, 60 },
	{ "fraglimit", "cv_fraglimit", VOTEFLAG_FRAGLIMIT, "fraglimit", 5, 0, 200 },
	{ "teamsize", "cv_teamsize", VOTEFLAG_TEAMSIZE, "g_teamSize", 1, 0, 32 },
	{ "random", "cv_random", VOTEFLAG_COINTOSS, NULL, 1, 2, 100 }
};

static const char	*cvModes[] = { "ffa", "duel", "tdm", "tdm2v2", "ctf", "ca", "sacrifice", "roundduel",
	"instagib", "unholytrinity" };

// every vote button calls, then puts the menu away
#define CV_DONE		"; rml_hide; ui_close"

/*
=================
UI_CallVoteRml
=================
*/
static void UI_CallVoteRml( void ) {
	static char		page[32768];
	char			info[MAX_INFO_STRING], player[MAX_INFO_STRING], name[64];
	uiClientState_t	cs;
	fileHandle_t	f;
	int				flags, gametype, i, n, rows = 0;

	trap_GetClientState( &cs );
	trap_GetConfigString( CS_SERVERINFO, info, sizeof( info ) );
	flags = atoi( Info_ValueForKey( info, "g_voteFlags" ) );
	gametype = atoi( Info_ValueForKey( info, "g_gametype" ) );

	page[0] = 0;
	Q_strcat( page, sizeof( page ), "<rml>\n<head>\n<title>Call vote</title>\n<link type=\"text/rcss\" href=\"/ui/tremor.rcss\"/>\n</head>\n"
		"<body data-escape=\"rml_show ui/ingame.rml\">\n<div class=\"create\">\n<h1>[cv_title]</h1>\n" );

	// the running mode's maps, A to Z, with one preview
	if ( !( flags & VOTEFLAG_MAP ) ) {
		UI_CsShowMode( Info_ValueForKey( info, "g_mode" ) );
		Q_strcat( page, sizeof( page ),
			"<div class=\"maplist\">\n<div class=\"mrow\" data-repeat=\"64\" data-when=\"ui_csMap#!=\" data-class=\"on:ui_csPicked=#\" onclick=\"ui_csPick #\"><span data-text-cvar=\"ui_csMap#\"></span></div>\n</div>\n"
			"<div class=\"preview\" data-when=\"ui_csShotSel!=\">\n<img data-src-cvar=\"ui_csShotSel\"/>\n<div class=\"pname\" data-text-cvar=\"ui_csMapName\"></div>\n"
			"<button class=\"opt\" onclick=\"ui_cvMap" CV_DONE "\">[cv_map]</button>\n</div>\n"
			"<div class=\"pager\" data-when=\"ui_csPages>1\">\n<button class=\"opt\" onclick=\"ui_csPage -1\">&lt;</button>\n"
			"<span data-text-cvar=\"ui_csPageText\"></span>\n<button class=\"opt\" onclick=\"ui_csPage 1\">&gt;</button>\n</div>\n"
			);
		rows++;
	} else {
		Q_strcat( page, sizeof( page ), "<div class=\"note\">[cv_maps_off]</div>\n" );
	}
	Q_strcat( page, sizeof( page ), "</div>\n<div class=\"csopts\">\n" );

	// the mode (keeps the map)
	if ( !( flags & VOTEFLAG_MODE ) ) {
		Q_strcat( page, sizeof( page ), "<div class=\"row\"><span class=\"label\">[cv_mode]</span></div>\n<div class=\"row\">" );
		for ( i = 0 ; i < ARRAY_LEN( cvModes ) ; i++ ) {
			Q_strcat( page, sizeof( page ), va( "<button class=\"opt\" onclick=\"callvote mode %s" CV_DONE "\">[mode_%s]</button>",
				cvModes[i], cvModes[i] ) );
		}
		Q_strcat( page, sizeof( page ), "</div>\n" );
		rows++;
	}

	// the one-press votes
	Q_strcat( page, sizeof( page ), "<div class=\"row\">" );
	if ( !( flags & VOTEFLAG_MAP_RESTART ) ) {
		Q_strcat( page, sizeof( page ), "<button class=\"opt\" onclick=\"callvote map_restart" CV_DONE "\">[cv_restart]</button>" );
		rows++;
	}
	if ( !( flags & VOTEFLAG_NEXTMAP ) ) {
		Q_strcat( page, sizeof( page ), "<button class=\"opt\" onclick=\"callvote nextmap" CV_DONE "\">[cv_nextmap]</button>" );
		rows++;
	}
	if ( !( flags & VOTEFLAG_SHUFFLE ) && gametype >= GT_TEAM ) {
		Q_strcat( page, sizeof( page ), "<button class=\"opt\" onclick=\"callvote shuffle" CV_DONE "\">[cv_shuffle]</button>" );
		rows++;
	}
	if ( !( flags & VOTEFLAG_COINTOSS ) ) {
		Q_strcat( page, sizeof( page ), "<button class=\"opt\" onclick=\"callvote cointoss" CV_DONE "\">[cv_coin]</button>" );
		rows++;
	}
	Q_strcat( page, sizeof( page ), "</div>\n" );

	// the number votes: - value + VOTE
	for ( i = 0 ; i < ARRAY_LEN( cvNumbers ) ; i++ ) {
		const cvNumber_t	*v = &cvNumbers[i];

		if ( ( flags & v->flag ) || ( !Q_stricmp( v->vote, "teamsize" ) && gametype == GT_TOURNAMENT ) ) {
			continue;
		}
		n = v->from ? atoi( Info_ValueForKey( info, v->from ) ) : 10;
		trap_Cvar_SetValue( va( "ui_cv_%s", v->vote ), Com_Clamp( v->least, v->most, n ) );
		Q_strcat( page, sizeof( page ), va( "<div class=\"row\"><span class=\"label\">[%s]</span>"
			"<button class=\"opt\" onclick=\"ui_cvStep %s -%i\">-</button><span class=\"value\" data-text-cvar=\"ui_cv_%s\"></span>"
			"<button class=\"opt\" onclick=\"ui_cvStep %s %i\">+</button>"
			"<button class=\"opt\" onclick=\"ui_cvCall %s" CV_DONE "\">[cv_call]</button></div>\n",
			v->label, v->vote, v->step, v->vote, v->vote, v->step, v->vote ) );
		rows++;
	}

	// kick: everyone else on the server
	if ( !( flags & VOTEFLAG_KICK ) ) {
		int		others = 0;

		for ( n = 0; n < MAX_CLIENTS; n++ ) {
			trap_GetConfigString( CS_PLAYERS + n, player, sizeof( player ) );
			if ( !player[0] || n == cs.clientNum ) {
				continue;
			}
			if ( !others++ ) {
				Q_strcat( page, sizeof( page ), "<div class=\"row\"><span class=\"label\">[cv_kick]</span></div>\n<div class=\"row\">" );
			}
			Q_strncpyz( name, Info_ValueForKey( player, "n" ), sizeof( name ) );
			Q_CleanStr( name );
			Q_strcat( page, sizeof( page ), va( "<button class=\"opt\" onclick=\"callvote clientkick %i" CV_DONE "\">%s</button>",
				n, UI_RmlText( name ) ) );
		}
		if ( others ) {
			Q_strcat( page, sizeof( page ), "</div>\n" );
			rows++;
		}
	}

	if ( !rows ) {
		Q_strcat( page, sizeof( page ), "<div class=\"note\">[cv_none]</div>\n" );
	}
	Q_strcat( page, sizeof( page ), "<div class=\"csactions\"><button onclick=\"rml_show ui/ingame.rml\">[menu_back]</button></div>\n"
		"</div>\n</body>\n</rml>\n" );

	trap_FS_FOpenFile( "ui-gen/callvote.rml", &f, FS_WRITE );
	if ( !f ) {
		return;
	}
	trap_FS_Write( page, strlen( page ), f );
	trap_FS_FCloseFile( f );
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui-gen/callvote.rml\n" );
}

/*
=================
UI_VoteCommand
=================
*/
qboolean UI_VoteCommand( const char *cmd ) {
	int		i;

	if ( !Q_stricmp( cmd, "ui_callvote" ) ) {
		UI_CallVoteRml();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_cvMap" ) ) {
		if ( UI_CsPickedMap()[0] ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "callvote map %s\n", UI_CsPickedMap() ) );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_cvStep" ) || !Q_stricmp( cmd, "ui_cvCall" ) ) {
		for ( i = 0 ; i < ARRAY_LEN( cvNumbers ) ; i++ ) {
			const cvNumber_t	*v = &cvNumbers[i];
			int					n = (int)trap_Cvar_VariableValue( va( "ui_cv_%s", v->vote ) );

			if ( Q_stricmp( UI_Argv( 1 ), v->vote ) ) {
				continue;
			}
			if ( !Q_stricmp( cmd, "ui_cvStep" ) ) {
				trap_Cvar_SetValue( va( "ui_cv_%s", v->vote ), Com_Clamp( v->least, v->most, n + atoi( UI_Argv( 2 ) ) ) );
			} else {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "callvote %s %i\n", v->vote, n ) );
			}
		}
		return qtrue;
	}
	return qfalse;
}
