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
// cg_demo.c -- recording every match on its own, the REC mark, and the
// status line and timeline while a demo plays

#include "cg_local.h"

#define AUTO_RECORD			1		// cg_autoAction bits
#define AUTO_SCREENSHOT		2
#define AUTO_STOP_MSEC		4000	// keep recording this long into the intermission

static int	intermissionSeen;		// cg.time the intermission was first seen

/*
=================
CG_DemoSafe

Letters, digits and _-() only
=================
*/
static void CG_DemoSafe( char *out, int size, const char *in ) {
	char	clean[MAX_QPATH];
	int		n = 0;

	Q_strncpyz( clean, in, sizeof( clean ) );
	Q_CleanStr( clean );
	for ( in = clean ; *in && n < size - 1 ; in++ ) {
		if ( ( *in >= 'a' && *in <= 'z' ) || ( *in >= 'A' && *in <= 'Z' ) || ( *in >= '0' && *in <= '9' )
			|| *in == '_' || *in == '-' || *in == '(' || *in == ')' ) {
			out[n++] = *in;
		}
	}
	out[n] = 0;
}

static const char *CG_DemoModeTag( void ) {
	if ( cgs.gametype == GT_TOURNAMENT ) {
		return atoi( Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "g_roundDuel" ) ) ? "rduel" : "duel";
	}
	if ( cgs.gametype == GT_TEAM ) {
		return atoi( Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "g_clanArena" ) ) ? "ca" : "tdm";
	}
	if ( cgs.gametype == GT_CTF ) {
		return atoi( Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "g_sacrifice" ) ) ? "sac" : "ctf";
	}
	return "ffa";
}

/*
=================
CG_DemoName

MODE-A(POV)-vs-B_map_YYYY-MM-DD_HH-MM in duels, MODE-A(POV)_map_... otherwise
=================
*/
static void CG_DemoName( char *name, int size ) {
	char	me[64], other[64], map[64];
	qtime_t	now;
	int		i;

	CG_DemoSafe( me, sizeof( me ), cgs.clientinfo[cg.clientNum].name );
	CG_DemoSafe( map, sizeof( map ), Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ) );
	trap_RealTime( &now );

	other[0] = 0;
	if ( cgs.gametype == GT_TOURNAMENT ) {
		for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
			if ( i != cg.clientNum && cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_FREE ) {
				CG_DemoSafe( other, sizeof( other ), cgs.clientinfo[i].name );
				break;
			}
		}
	}
	Com_sprintf( name, size, "%s-%s(POV)%s%s_%s_%04i-%02i-%02i_%02i-%02i", CG_DemoModeTag(), me,
		other[0] ? "-vs-" : "", other, map,
		now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min );
}

/*
=================
CG_WriteHistory

At the end of a match, a line for the profile's match history in
history.txt: "time|mode|map|result|score|demo"
=================
*/
static void CG_WriteHistory( void ) {
	static int		writtenFor = -1;
	fileHandle_t	f;
	qtime_t			now;
	char			line[512], demo[MAX_QPATH], mapName[64];
	const char		*result;
	int				team = cg.snap->ps.persistant[PERS_TEAM], score;

	if ( cg.demoPlayback || team == TEAM_SPECTATOR || writtenFor == cgs.levelStartTime ) {
		return;
	}
	writtenFor = cgs.levelStartTime;
	if ( cgs.gametype >= GT_TEAM ) {
		int mine = team == TEAM_RED ? cg.teamScores[0] : cg.teamScores[1];
		int theirs = team == TEAM_RED ? cg.teamScores[1] : cg.teamScores[0];

		result = mine > theirs ? "WIN" : mine < theirs ? "LOSS" : "DRAW";
		score = cg.snap->ps.persistant[PERS_SCORE];
	} else {
		int rank = cg.snap->ps.persistant[PERS_RANK] & ~RANK_TIED_FLAG;

		result = rank == 0 ? "WIN" : cgs.gametype == GT_TOURNAMENT ? "LOSS" : va( "#%i", rank + 1 );
		score = cg.snap->ps.persistant[PERS_SCORE];
	}
	trap_Cvar_VariableStringBuffer( "cg_autoDemoName", demo, sizeof( demo ) );
	trap_RealTime( &now );
	Q_strncpyz( mapName, Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "mapname" ), sizeof( mapName ) );
	Com_sprintf( line, sizeof( line ), "%04i-%02i-%02i %02i:%02i|%s|%s|%s|%i|%s\n", now.tm_year + 1900, now.tm_mon + 1,
		now.tm_mday, now.tm_hour, now.tm_min, CG_ModeString(), mapName, result, score, demo );
	trap_FS_FOpenFile( "history.txt", &f, FS_APPEND );
	if ( f ) {
		trap_FS_Write( line, strlen( line ), f );
		trap_FS_FCloseFile( f );
	}
	trap_Cvar_Set( "cg_autoDemoName", "" );
}

/*
=================
CG_AutoRecord

Records each match from its countdown (or from joining one already under
way) to a little into the intermission, then takes a screenshot
=================
*/
static void CG_AutoRecord( void ) {
	char	buf[MAX_QPATH];
	int		action = cg_autoAction.integer;
	qboolean	recording, ours;
	int			state;

	if ( cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION ) {
		CG_WriteHistory();
	}
	if ( cg.demoPlayback || !( action & ( AUTO_RECORD | AUTO_SCREENSHOT ) ) ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( "cl_demoRecording", buf, sizeof( buf ) );
	recording = atoi( buf );
	// cg_autoRecording: 0 none yet, 1 recording, 2 done or stopped by hand this match
	trap_Cvar_VariableStringBuffer( "cg_autoRecording", buf, sizeof( buf ) );
	state = atoi( buf );
	ours = state == 1 && recording;
	if ( state == 1 && !recording ) {
		trap_Cvar_Set( "cg_autoRecording", "2" );		// stopped by hand: leave it
		state = 2;
	}

	if ( cg.snap->ps.pm_type == PM_INTERMISSION ) {
		if ( !intermissionSeen ) {
			intermissionSeen = cg.time;
		}
		if ( cg.time - intermissionSeen >= AUTO_STOP_MSEC && intermissionSeen > 0 ) {
			intermissionSeen = -1;		// once
			if ( ours ) {
				trap_SendConsoleCommand( "stoprecord\n" );
			}
			trap_Cvar_Set( "cg_autoRecording", "2" );
			if ( action & AUTO_SCREENSHOT ) {
				CG_DemoName( buf, sizeof( buf ) );
				trap_SendConsoleCommand( va( "screenshotJPEG \"%s\"\n", buf ) );
			}
		}
		return;
	}
	intermissionSeen = 0;

	if ( cg.warmup < 0 ) {
		// back to waiting for players: that recording is over, the next match records again
		if ( ours ) {
			trap_SendConsoleCommand( "stoprecord\n" );
		}
		trap_Cvar_Set( "cg_autoRecording", "0" );
		return;
	}
	if ( !recording && state == 0 && ( action & AUTO_RECORD ) && cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		CG_DemoName( buf, sizeof( buf ) );
		trap_SendConsoleCommand( va( "record \"%s\"\n", buf ) );
		trap_Cvar_Set( "cg_autoRecording", "1" );
		trap_Cvar_Set( "cg_autoDemoName", buf );		// for the match history
	}
}

/*
=================
CG_DrawDemoControls

Status line and timeline while a demo plays: shown when paused or just
after a control was used
=================
*/
static void CG_DrawDemoControls( void ) {
	static const vec4_t	back = { 0, 0, 0, 0.6f };
	static const vec4_t	fill = { 0.83f, 0.1f, 0.12f, 1 };
	char		buf[128];
	const char	*s;
	int			cur, total, speed, paused, show, w, px = -1, py = -1;
	float		frac;

	trap_Cvar_VariableStringBuffer( "cl_demoState", buf, sizeof( buf ) );
	if ( sscanf( buf, "%i %i %i %i %i %i %i", &cur, &total, &speed, &paused, &show, &px, &py ) < 5 || !show ) {
		return;
	}

	// the bar
	// above the status bar, so the HUD's numbers stay readable
	CG_FillRect( 20, 414, 600, 6, back );
	frac = total > 0 ? (float)cur / total : 0;
	if ( frac < 0 ) {
		frac = 0;
	} else if ( frac > 1 ) {
		frac = 1;
	}
	CG_FillRect( 20, 414, 600 * frac, 6, fill );

	// time, speed or pause
	if ( paused ) {
		s = va( "%i:%02i / %i:%02i   %s", cur / 60000, ( cur / 1000 ) % 60, total / 60000, ( total / 1000 ) % 60,
			BG_String( STR_DEMO_PAUSED ) );
	} else if ( speed != 1000 ) {
		s = va( "%i:%02i / %i:%02i   %s%ix", cur / 60000, ( cur / 1000 ) % 60, total / 60000, ( total / 1000 ) % 60,
			speed < 1000 ? "1/" : "", speed < 1000 ? 1000 / speed : speed / 1000 );
	} else {
		s = va( "%i:%02i / %i:%02i", cur / 60000, ( cur / 1000 ) % 60, total / 60000, ( total / 1000 ) % 60 );
	}
	w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;
	CG_DrawSmallString( 320 - w / 2, 396, s, 1.0f );

	if ( paused ) {
		s = BG_String( STR_DEMO_HELP );
		w = CG_DrawStrlen( s ) * TINYCHAR_WIDTH;
		CG_DrawStringExt( 320 - w / 2, 424, s, colorWhite, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
	}

	// the pointer, once the mouse has moved; over the bar it marks the spot a click seeks to
	if ( px > 0 || py > 0 ) {
		if ( py >= 404 && py <= 430 && px >= 16 && px <= 624 ) {
			CG_FillRect( px - 1, 410, 2, 14, colorWhite );
		}
		CG_DrawPic( px - 8, py - 8, 16, 16, cgs.media.demoPointer );
	}
}

/*
=================
CG_DrawRecordMark

A small REC in the corner while recording by hand (cl_demoRecordMessage 2);
the automatic match recording (cg_autoAction) stays quiet
=================
*/
static void CG_DrawRecordMark( void ) {
	char	buf[16];

	if ( cg.demoPlayback ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( "cl_demoRecording", buf, sizeof( buf ) );
	if ( !atoi( buf ) ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( "cg_autoRecording", buf, sizeof( buf ) );
	if ( atoi( buf ) == 1 ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( "cl_demoRecordMessage", buf, sizeof( buf ) );
	if ( atoi( buf ) != 2 ) {
		return;
	}
	CG_DrawStringExt( 4, 4, BG_String( STR_DEMO_REC ), colorRed, qtrue, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
}

/*
=================
CG_DemoFrame
=================
*/
void CG_DemoFrame( void ) {
	CG_AutoRecord();
	if ( cg.demoPlayback ) {
		CG_DrawDemoControls();
	} else {
		CG_DrawRecordMark();
	}
}
