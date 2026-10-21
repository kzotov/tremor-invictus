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
// cl_demo.c -- demo extras: a header, keyframes every 10 s with an index at
// the end of the file, and playback controls (pause, speed, seeking)
//
// File layout: the usual sequence of [sequence][length][message] blocks,
// ended by [-1][-1]. Extra blocks use negative sequence numbers that older
// readers never see in a normal stream:
//   DEMO_BLOCK_HEADER    first in the file, an info string about the match
//   DEMO_BLOCK_KEYFRAME  just before a full snapshot: the config strings and
//                        command sequence at that point
// After the terminator: the index (keyframe offsets and times) and a fixed
// trailer pointing to it.

#include "client.h"

#define DEMO_BLOCK_HEADER		-2
#define DEMO_BLOCK_KEYFRAME		-4
#define DEMO_KEYFRAME_MSEC		10000
#define DEMO_MAX_KEYFRAMES		4096
#define DEMO_TRAILER_MAGIC		0x58444d54		// "TMDX"
#define DEMO_SEEK_SKIP_MSEC		15000			// jump to a keyframe when it is at least this far ahead
#define DEMO_BAR_MSEC			3000			// controls stay on screen this long after use

typedef struct {
	int		offset;
	int		serverTime;
} demoKeyframe_t;

static struct {
	// recording
	qboolean		keyframeWanted;
	int				nextKeyframeTime;
	int				firstTime, lastTime;
	demoKeyframe_t	written[DEMO_MAX_KEYFRAMES];
	int				numWritten;

	// playback
	demoKeyframe_t	index[DEMO_MAX_KEYFRAMES];
	int				numIndex;
	int				duration;			// msec, 0 when unknown
	int				startTime;			// server time of the first snapshot
	qboolean		paused;
	int				speedStep;			// 0 is 1x; -3..3 for 1/8x..8x
	float			speedCarry;
	int				pendingSeek;		// server time to reach after a restart, 0 for none
	int				controlTime;		// cls.realtime of the last control use
} demo;

static const float demoSpeeds[7] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };

/*
=======================================================================

RECORDING

=======================================================================
*/

/*
====================
CL_DemoWriteBlock
====================
*/
static void CL_DemoWriteBlock( int type, const void *data, int len ) {
	int		v;

	v = LittleLong( type );
	FS_Write( &v, 4, clc.demofile );
	v = LittleLong( len );
	FS_Write( &v, 4, clc.demofile );
	FS_Write( data, len, clc.demofile );
}

/*
====================
CL_DemoWriteHeader

Map, mode, server, date, protocol and the players, so a demo list can
show what is inside without playing it
====================
*/
void CL_DemoWriteHeader( void ) {
	char		info[BIG_INFO_STRING];
	char		players[MAX_INFO_VALUE];
	const char	*serverinfo, *s;
	qtime_t		now;
	int			i;

	info[0] = 0;
	serverinfo = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
	Info_SetValueForKey_Big( info, "map", Info_ValueForKey( serverinfo, "mapname" ) );
	Info_SetValueForKey_Big( info, "gametype", Info_ValueForKey( serverinfo, "g_gametype" ) );
	Info_SetValueForKey_Big( info, "variant", va( "%s%s%s",
		atoi( Info_ValueForKey( serverinfo, "g_clanArena" ) ) ? "ca" : "",
		atoi( Info_ValueForKey( serverinfo, "g_sacrifice" ) ) ? "sacrifice" : "",
		atoi( Info_ValueForKey( serverinfo, "g_roundDuel" ) ) ? "roundduel" : "" ) );
	Info_SetValueForKey_Big( info, "server", Info_ValueForKey( serverinfo, "sv_hostname" ) );
	Com_RealTime( &now );
	Info_SetValueForKey_Big( info, "date", va( "%04i-%02i-%02i %02i:%02i",
		now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min ) );
	Info_SetValueForKey_Big( info, "protocol", va( "%i", com_protocol->integer ) );
	Info_SetValueForKey_Big( info, "version", Cvar_VariableString( "version" ) );

	// "name/champion" for everyone in the game
	players[0] = 0;
	for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
		if ( !cl.gameState.stringOffsets[CS_PLAYERS + i] ) {
			continue;
		}
		s = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS + i];
		if ( !s[0] ) {
			continue;
		}
		Q_strcat( players, sizeof( players ), va( "%s%s/%s", players[0] ? "," : "",
			Info_ValueForKey( s, "n" ), Info_ValueForKey( s, "model" ) ) );
	}
	Info_SetValueForKey_Big( info, "players", players );

	CL_DemoWriteBlock( DEMO_BLOCK_HEADER, info, strlen( info ) + 1 );

	demo.keyframeWanted = qfalse;
	demo.nextKeyframeTime = 0;
	demo.firstTime = demo.lastTime = 0;
	demo.numWritten = 0;
}

/*
====================
CL_DemoWantsFullSnapshot

True while a keyframe is due: the next snapshot is asked for without delta
====================
*/
qboolean CL_DemoWantsFullSnapshot( void ) {
	return clc.demorecording && demo.keyframeWanted;
}

/*
====================
CL_DemoBeforeWrite

Called with each message about to be written: notes the times, and puts a
keyframe in front of a full snapshot when one is due
====================
*/
void CL_DemoBeforeWrite( void ) {
	msg_t	buf;
	byte	bufData[MAX_MSGLEN];
	int		i;
	char	*s;

	if ( !cl.snap.valid || cl.snap.messageNum != clc.serverMessageSequence ) {
		return;		// no snapshot in this message
	}
	if ( !demo.firstTime ) {
		demo.firstTime = cl.snap.serverTime;
		demo.nextKeyframeTime = cl.snap.serverTime + DEMO_KEYFRAME_MSEC;
	}
	demo.lastTime = cl.snap.serverTime;

	if ( cl.snap.serverTime >= demo.nextKeyframeTime && !demo.keyframeWanted ) {
		demo.keyframeWanted = qtrue;
	}
	if ( !demo.keyframeWanted || cl.snap.deltaNum != -1 || demo.numWritten >= DEMO_MAX_KEYFRAMES ) {
		return;
	}

	// the config strings and command sequence at this point
	MSG_Init( &buf, bufData, sizeof( bufData ) );
	MSG_WriteLong( &buf, clc.serverCommandSequence );
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( !cl.gameState.stringOffsets[i] ) {
			continue;
		}
		s = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		MSG_WriteShort( &buf, i );
		MSG_WriteBigString( &buf, s );
	}
	MSG_WriteShort( &buf, -1 );

	demo.written[demo.numWritten].offset = FS_FTell( clc.demofile );
	demo.written[demo.numWritten].serverTime = cl.snap.serverTime;
	demo.numWritten++;
	CL_DemoWriteBlock( DEMO_BLOCK_KEYFRAME, buf.data, buf.cursize );

	demo.keyframeWanted = qfalse;
	demo.nextKeyframeTime = cl.snap.serverTime + DEMO_KEYFRAME_MSEC;
}

/*
====================
CL_DemoWriteIndex

After the end marker: the keyframe index and the trailer
====================
*/
void CL_DemoWriteIndex( void ) {
	int		indexOffset, v, i;

	indexOffset = FS_FTell( clc.demofile );
	v = LittleLong( demo.numWritten );
	FS_Write( &v, 4, clc.demofile );
	for ( i = 0 ; i < demo.numWritten ; i++ ) {
		v = LittleLong( demo.written[i].offset );
		FS_Write( &v, 4, clc.demofile );
		v = LittleLong( demo.written[i].serverTime );
		FS_Write( &v, 4, clc.demofile );
	}
	// trailer: index offset, first server time, duration, magic
	v = LittleLong( indexOffset );
	FS_Write( &v, 4, clc.demofile );
	v = LittleLong( demo.firstTime );
	FS_Write( &v, 4, clc.demofile );
	v = LittleLong( demo.lastTime - demo.firstTime );
	FS_Write( &v, 4, clc.demofile );
	v = LittleLong( DEMO_TRAILER_MAGIC );
	FS_Write( &v, 4, clc.demofile );
}

/*
=======================================================================

PLAYBACK

=======================================================================
*/

/*
====================
CL_DemoLoadIndex

On opening a demo: the index and duration from its trailer, if it has one
====================
*/
void CL_DemoLoadIndex( void ) {
	int		v[4], count, i, pair[2];

	demo.numIndex = 0;
	demo.duration = 0;
	demo.startTime = 0;
	if ( !demo.pendingSeek ) {
		// a fresh start; a restart for seeking keeps pause and speed
		demo.paused = qfalse;
		demo.speedStep = 0;
		demo.controlTime = 0;
	}
	demo.speedCarry = 0;

	if ( FS_Seek( clc.demofile, -16, FS_SEEK_END ) == 0 && FS_Read( v, 16, clc.demofile ) == 16
		&& LittleLong( v[3] ) == DEMO_TRAILER_MAGIC ) {
		demo.startTime = LittleLong( v[1] );
		demo.duration = LittleLong( v[2] );
		if ( FS_Seek( clc.demofile, LittleLong( v[0] ), FS_SEEK_SET ) == 0
			&& FS_Read( &count, 4, clc.demofile ) == 4 ) {
			count = LittleLong( count );
			for ( i = 0 ; i < count && i < DEMO_MAX_KEYFRAMES ; i++ ) {
				if ( FS_Read( pair, 8, clc.demofile ) != 8 ) {
					break;
				}
				demo.index[i].offset = LittleLong( pair[0] );
				demo.index[i].serverTime = LittleLong( pair[1] );
			}
			demo.numIndex = i;
		}
	}
	FS_Seek( clc.demofile, 0, FS_SEEK_SET );
}

/*
====================
CL_DemoSkipBlock

Reading the stream: true if the sequence number marks one of our extra
blocks, which normal playback passes over
====================
*/
qboolean CL_DemoSkipBlock( int sequence ) {
	int		len;
	byte	skip[1024];

	if ( sequence != DEMO_BLOCK_HEADER && sequence != DEMO_BLOCK_KEYFRAME ) {
		return qfalse;
	}
	if ( FS_Read( &len, 4, clc.demofile ) != 4 ) {
		return qfalse;
	}
	len = LittleLong( len );
	while ( len > 0 ) {
		int		n = len > (int)sizeof( skip ) ? (int)sizeof( skip ) : len;

		if ( FS_Read( skip, n, clc.demofile ) != n ) {
			break;
		}
		len -= n;
	}
	return qtrue;
}

/*
====================
CL_DemoJumpToKeyframe

Moves the stream to a keyframe: the config strings it holds replace the
current ones (changes are passed to the game as config string commands),
and the full snapshot after it is read next
====================
*/
static qboolean CL_DemoJumpToKeyframe( const demoKeyframe_t *kf ) {
	msg_t		buf;
	byte		bufData[MAX_MSGLEN];
	gameState_t	old = cl.gameState;
	int			v[2], i, index, len, sequence, injected, first;
	char		*s;

	if ( FS_Seek( clc.demofile, kf->offset, FS_SEEK_SET ) != 0 || FS_Read( v, 8, clc.demofile ) != 8
		|| LittleLong( v[0] ) != DEMO_BLOCK_KEYFRAME ) {
		return qfalse;
	}
	len = LittleLong( v[1] );
	if ( len <= 0 || len > (int)sizeof( bufData ) || FS_Read( bufData, len, clc.demofile ) != len ) {
		return qfalse;
	}
	MSG_Init( &buf, bufData, sizeof( bufData ) );
	buf.cursize = len;
	sequence = MSG_ReadLong( &buf );

	// rebuild the config strings
	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
	cl.gameState.dataCount = 1;
	while ( ( index = MSG_ReadShort( &buf ) ) >= 0 && index < MAX_CONFIGSTRINGS ) {
		s = MSG_ReadBigString( &buf );
		len = strlen( s );
		if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			break;
		}
		cl.gameState.stringOffsets[index] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, s, len + 1 );
		cl.gameState.dataCount += len + 1;
	}

	// commands from before the jump never arrive; tell the game what changed
	// as config string commands numbered up to the keyframe's sequence
	Com_Memset( clc.serverCommands, 0, sizeof( clc.serverCommands ) );
	injected = 0;
	first = sequence - MAX_RELIABLE_COMMANDS + 1;
	for ( i = 0 ; i < MAX_CONFIGSTRINGS && injected < MAX_RELIABLE_COMMANDS ; i++ ) {
		const char	*was = old.stringOffsets[i] ? old.stringData + old.stringOffsets[i] : "";
		const char	*now = cl.gameState.stringOffsets[i] ? cl.gameState.stringData + cl.gameState.stringOffsets[i] : "";

		if ( !strcmp( was, now ) || strchr( now, '"' ) ) {
			continue;
		}
		Com_sprintf( clc.serverCommands[( first + injected ) & ( MAX_RELIABLE_COMMANDS - 1 )],
			sizeof( clc.serverCommands[0] ), "cs %i \"%s\"", i, now );
		injected++;
	}
	clc.serverCommandSequence = sequence;
	return qtrue;
}

/*
====================
CL_DemoSeekTo

Seeks the playing demo to a server time: forward by jumping to a keyframe
when one is far enough ahead and reading on from there; backward by
starting the demo over and seeking forward once it is running
====================
*/
static void CL_DemoSeekTo( int target ) {
	int		i, best = -1;

	if ( !clc.demoplaying || !cl.snap.valid ) {
		return;
	}
	if ( demo.startTime && target < demo.startTime ) {
		target = demo.startTime;
	}
	if ( demo.duration && target > demo.startTime + demo.duration ) {
		target = demo.startTime + demo.duration;
	}

	if ( target < cl.serverTime ) {
		// time only runs forward in the game: start over and come back
		demo.pendingSeek = target;
		Cbuf_ExecuteText( EXEC_NOW, va( "demo \"%s\"\n", clc.demoName ) );
		return;
	}

	for ( i = 0 ; i < demo.numIndex ; i++ ) {
		if ( demo.index[i].serverTime <= target && demo.index[i].serverTime > cl.snap.serverTime + DEMO_SEEK_SKIP_MSEC ) {
			best = i;
		}
	}
	if ( best >= 0 ) {
		CL_DemoJumpToKeyframe( &demo.index[best] );
	}

	// reading catches up to the new time on the next frame
	cl.serverTimeDelta += target - cl.serverTime;
}

/*
====================
CL_DemoFirstSnapshot

The demo is running: note where it starts, and finish a seek that
restarted it
====================
*/
void CL_DemoFirstSnapshot( void ) {
	int		target = demo.pendingSeek;

	if ( !demo.startTime ) {
		demo.startTime = cl.snap.serverTime;
	}
	cl.serverTime = cl.snap.serverTime;
	if ( target ) {
		demo.pendingSeek = 0;
		CL_DemoSeekTo( target );
	}
}

/*
====================
CL_DemoAdvanceTime

Each frame of playback: pause holds the demo clock, speed stretches it
====================
*/
void CL_DemoAdvanceTime( void ) {
	float	speed;
	int		add;

	if ( demo.paused ) {
		cl.serverTimeDelta += cls.frametime;
	} else if ( demo.speedStep ) {
		speed = demoSpeeds[demo.speedStep + 3];
		demo.speedCarry += cls.frametime * ( speed - 1.0f );
		add = (int)demo.speedCarry;
		demo.speedCarry -= add;
		cl.serverTimeDelta += add;
	}

	// for the game's status line and timeline
	Cvar_Set( "cl_demoState", va( "%i %i %i %i %i",
		cl.serverTime - demo.startTime, demo.duration,
		(int)( demoSpeeds[demo.speedStep + 3] * 1000 ), demo.paused,
		demo.paused || ( demo.controlTime && cls.realtime - demo.controlTime < DEMO_BAR_MSEC ) ) );
}

/*
====================
Commands
====================
*/
static void CL_DemoControlUsed( void ) {
	demo.controlTime = cls.realtime;
}

static void CL_DemoPause_f( void ) {
	if ( !clc.demoplaying ) {
		return;
	}
	demo.paused = !demo.paused;
	CL_DemoControlUsed();
}

static void CL_DemoSpeed_f( void ) {
	const char	*arg = Cmd_Argv( 1 );

	if ( !clc.demoplaying ) {
		return;
	}
	if ( !Q_stricmp( arg, "up" ) ) {
		demo.speedStep++;
	} else if ( !Q_stricmp( arg, "down" ) ) {
		demo.speedStep--;
	} else {
		float	want = atof( arg );
		int		i;

		for ( i = 0 ; i < 7 ; i++ ) {
			if ( want <= demoSpeeds[i] * 1.01f ) {
				break;
			}
		}
		demo.speedStep = i - 3;
	}
	if ( demo.speedStep < -3 ) {
		demo.speedStep = -3;
	} else if ( demo.speedStep > 3 ) {
		demo.speedStep = 3;
	}
	demo.speedCarry = 0;
	CL_DemoControlUsed();
}

// demo_seek <seconds>: +N or -N from now, or N from the start
static void CL_DemoSeek_f( void ) {
	const char	*arg = Cmd_Argv( 1 );
	int			target;

	if ( !clc.demoplaying || !arg[0] ) {
		return;
	}
	if ( arg[0] == '+' || arg[0] == '-' ) {
		target = cl.serverTime + atoi( arg ) * 1000;
	} else {
		target = demo.startTime + atoi( arg ) * 1000;
	}
	CL_DemoControlUsed();
	CL_DemoSeekTo( target );
}

static void CL_DemoRestart_f( void ) {
	if ( !clc.demoplaying ) {
		return;
	}
	CL_DemoControlUsed();
	CL_DemoSeekTo( demo.startTime ? demo.startTime : 0 );
}

void CL_DemoInit( void ) {
	Cmd_AddCommand( "demo_pause", CL_DemoPause_f );
	Cmd_AddCommand( "demo_speed", CL_DemoSpeed_f );
	Cmd_AddCommand( "demo_seek", CL_DemoSeek_f );
	Cmd_AddCommand( "demo_restart", CL_DemoRestart_f );
	Cvar_Get( "cl_demoState", "", CVAR_ROM );
	Cvar_Get( "cl_demoRecordMessage", "2", CVAR_ARCHIVE );
}

/*
====================
CL_DemoKey

Playback keys; true if the key was used. Other keys keep their binds.
====================
*/
qboolean CL_DemoKey( int key ) {
	switch ( key ) {
	case K_SPACE:		Cbuf_AddText( "demo_pause\n" ); return qtrue;
	case K_UPARROW:		Cbuf_AddText( "demo_speed up\n" ); return qtrue;
	case K_DOWNARROW:	Cbuf_AddText( "demo_speed down\n" ); return qtrue;
	case K_LEFTARROW:	Cbuf_AddText( "demo_seek -10\n" ); return qtrue;
	case K_RIGHTARROW:	Cbuf_AddText( "demo_seek +10\n" ); return qtrue;
	case K_PGUP:		Cbuf_AddText( "demo_seek -60\n" ); return qtrue;
	case K_PGDN:		Cbuf_AddText( "demo_seek +60\n" ); return qtrue;
	case K_HOME:		Cbuf_AddText( "demo_restart\n" ); return qtrue;
	case 'h':
		Cvar_SetValue( "cg_draw2D", !Cvar_VariableIntegerValue( "cg_draw2D" ) );
		return qtrue;
	default:
		return qfalse;
	}
}
