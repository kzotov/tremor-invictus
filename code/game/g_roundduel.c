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
// g_roundduel.c -- round duel: a snake draft of three champions each, then
// rounds in which every champion has one life

#include "g_local.h"

#define RD_CHAMPIONS		3
#define RD_PICK_MSEC		20000
#define RD_COUNTDOWN_MSEC	5000
#define RD_RESULT_MSEC		5000

enum {
	RD_WAITING,			// match not started
	RD_DRAFT,
	RD_COUNTDOWN,
	RD_FIGHTING,
	RD_RESULT
};

// snake order: A, B, B, A, A, B
static const int rdPickOrder[RD_CHAMPIONS * 2] = { 0, 1, 1, 0, 0, 1 };

static struct {
	int			state;
	int			stateTime;			// deadline of a pick, end of the countdown or result
	int			roundStart;
	int			round;
	qboolean	suddenDeath;
	int			player[2];			// client numbers, A first
	int			picks[2][RD_CHAMPIONS];
	int			numPicks;			// picks made, both players
	int			current[2];			// roster index in play
	int			dead[2];			// bit per roster index
	int			hudTime;
} rd;

/*
=================
G_RoundDuel
=================
*/
qboolean G_RoundDuel( void ) {
	return g_gametype.integer == GT_TOURNAMENT && g_roundDuel.integer;
}

/*
=================
G_RoundDuelHolds

True while players are held in place: the draft, the countdown, the result
=================
*/
qboolean G_RoundDuelHolds( void ) {
	return G_RoundDuel() && !level.warmupTime && rd.state != RD_FIGHTING && rd.state != RD_WAITING;
}

static int G_RDSide( gclient_t *client ) {
	int		num = client - level.clients;

	if ( rd.player[0] == num ) {
		return 0;
	}
	if ( rd.player[1] == num ) {
		return 1;
	}
	return -1;
}

/*
=================
G_RoundDuelChampion

The champion a duelist spawns as: the one in play from their roster
=================
*/
int G_RoundDuelChampion( gclient_t *client ) {
	int		side = G_RDSide( client );

	if ( side < 0 || rd.state == RD_WAITING || rd.state == RD_DRAFT ) {
		return client->pers.champion;
	}
	return rd.picks[side][rd.current[side]];
}

static int G_RDLeft( int side ) {
	int		i, n = 0;

	for ( i = 0 ; i < RD_CHAMPIONS ; i++ ) {
		if ( !( rd.dead[side] & ( 1 << i ) ) ) {
			n++;
		}
	}
	return n;
}

/*
=================
G_RDUpdateHud

Both rosters, the dead crossed out, for the clients
=================
*/
static void G_RDUpdateHud( void ) {
	char	line[2][256];
	int		side, i, picked;

	for ( side = 0 ; side < 2 ; side++ ) {
		Q_strncpyz( line[side], level.clients[rd.player[side]].pers.netname, sizeof( line[side] ) );
		Q_strcat( line[side], sizeof( line[side] ), S_COLOR_WHITE ":" );
		picked = 0;
		for ( i = 0 ; i < RD_CHAMPIONS * 2 && i < rd.numPicks ; i++ ) {
			if ( rdPickOrder[i] == side ) {
				picked++;
			}
		}
		for ( i = 0 ; i < picked ; i++ ) {
			Q_strcat( line[side], sizeof( line[side] ), " " );
			if ( rd.dead[side] & ( 1 << i ) ) {
				Q_strcat( line[side], sizeof( line[side] ), S_COLOR_RED "x" );
			} else if ( rd.state == RD_FIGHTING && rd.current[side] == i ) {
				Q_strcat( line[side], sizeof( line[side] ), S_COLOR_GREEN );
			}
			Q_strcat( line[side], sizeof( line[side] ), BG_String( BG_Champion( rd.picks[side][i] )->nameString ) );
			Q_strcat( line[side], sizeof( line[side] ), S_COLOR_WHITE );
		}
	}
	trap_SetConfigstring( CS_SACRIFICE, va( "rd|%s|%s", line[0], line[1] ) );
}

static void G_RDAnnounce( const char *msg ) {
	trap_SendServerCommand( -1, va( "cp \"%s\"", msg ) );
	trap_SendServerCommand( -1, va( "print \"%s\n\"", msg ) );
}

/*
=================
G_RDPick

A champion for the player whose turn it is; taken champions can't be
picked again by either player
=================
*/
static qboolean G_RDPick( int side, int champion ) {
	char	msg[MAX_STRING_CHARS];
	int		i, s, n;

	if ( champion < 0 || champion >= bg_numChampions ) {
		return qfalse;
	}
	for ( s = 0 ; s < 2 ; s++ ) {
		n = 0;
		for ( i = 0 ; i < rd.numPicks ; i++ ) {
			if ( rdPickOrder[i] == s && rd.picks[s][n++] == champion ) {
				return qfalse;
			}
		}
	}
	n = 0;
	for ( i = 0 ; i < rd.numPicks ; i++ ) {
		if ( rdPickOrder[i] == side ) {
			n++;
		}
	}
	rd.picks[side][n] = champion;
	rd.numPicks++;
	rd.stateTime = level.time + RD_PICK_MSEC;
	Com_sprintf( msg, sizeof( msg ), BG_String( STR_RD_PICKED ), level.clients[rd.player[side]].pers.netname,
		BG_String( BG_Champion( champion )->nameString ) );
	G_RDAnnounce( msg );
	G_RDUpdateHud();
	return qtrue;
}

static void G_RDPickRandom( int side ) {
	int		tries;

	for ( tries = 0 ; tries < 200 ; tries++ ) {
		if ( G_RDPick( side, rand() % bg_numChampions ) ) {
			return;
		}
	}
}

/*
=================
Cmd_Pick_f

pick <champion> during the draft, on your turn
=================
*/
void Cmd_Pick_f( gentity_t *ent ) {
	char	name[MAX_TOKEN_CHARS];
	int		side = G_RDSide( ent->client );
	int		champion;

	if ( !G_RoundDuel() || rd.state != RD_DRAFT || side < 0 || rdPickOrder[rd.numPicks] != side ) {
		return;
	}
	trap_Argv( 1, name, sizeof( name ) );
	champion = BG_ChampionForName( name );
	if ( Q_stricmp( BG_Champion( champion )->name, name ) ) {
		return;		// unknown name
	}
	G_RDPick( side, champion );
}

/*
=================
Cmd_Next_f

next <1-3>: the champion to play after a death, or first in a round
=================
*/
void Cmd_Next_f( gentity_t *ent ) {
	char	arg[MAX_TOKEN_CHARS];
	int		side = G_RDSide( ent->client );
	int		i;

	if ( !G_RoundDuel() || side < 0 || ( rd.state == RD_FIGHTING && ent->health > 0 ) ) {
		return;
	}
	if ( rd.state != RD_COUNTDOWN && rd.state != RD_FIGHTING ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	i = atoi( arg ) - 1;
	if ( i < 0 || i >= RD_CHAMPIONS || ( rd.dead[side] & ( 1 << i ) ) ) {
		return;
	}
	rd.current[side] = i;
	if ( rd.state == RD_COUNTDOWN ) {
		ClientSpawn( ent );
	}
	G_RDUpdateHud();
}

/*
=================
G_RDStartRound

The map resets and both players spawn as their first living champion,
held for the countdown
=================
*/
static void G_RDStartRound( void ) {
	char	msg[MAX_STRING_CHARS];
	int		side;

	rd.round++;
	rd.state = RD_COUNTDOWN;
	rd.stateTime = level.time + RD_COUNTDOWN_MSEC;
	rd.suddenDeath = qfalse;
	G_ResetMapItems();
	for ( side = 0 ; side < 2 ; side++ ) {
		rd.dead[side] = 0;
		rd.current[side] = 0;
		ClientSpawn( &g_entities[rd.player[side]] );
	}
	Com_sprintf( msg, sizeof( msg ), BG_String( STR_RD_ROUND ), rd.round );
	G_RDAnnounce( msg );
	G_RDUpdateHud();
}

static void G_RDEndRound( int winner ) {
	char		msg[MAX_STRING_CHARS];
	gclient_t	*cl = &level.clients[rd.player[winner]];

	rd.state = RD_RESULT;
	rd.stateTime = level.time + RD_RESULT_MSEC;
	cl->ps.persistant[PERS_SCORE]++;
	Com_sprintf( msg, sizeof( msg ), BG_String( STR_RD_WINS_ROUND ), cl->pers.netname );
	G_RDAnnounce( msg );
	CalculateRanks();
}

/*
=================
G_RoundDuelKill

A champion's one life is over: the next one left comes in, or the round
is lost
=================
*/
void G_RoundDuelKill( gentity_t *victim ) {
	int		side, i;

	if ( !G_RoundDuel() || level.warmupTime || rd.state != RD_FIGHTING || !victim->client ) {
		return;
	}
	side = G_RDSide( victim->client );
	if ( side < 0 ) {
		return;
	}
	rd.dead[side] |= 1 << rd.current[side];
	if ( !G_RDLeft( side ) || rd.suddenDeath ) {
		G_RDEndRound( !side );
		return;
	}
	for ( i = 0 ; i < RD_CHAMPIONS ; i++ ) {
		if ( !( rd.dead[side] & ( 1 << i ) ) ) {
			rd.current[side] = i;
			break;
		}
	}
	trap_SendServerCommand( victim - g_entities, va( "cp \"%s\"", BG_String( STR_RD_NEXT ) ) );
	G_RDUpdateHud();
}

/*
=================
G_RoundDuelCheckRules

Called every frame instead of the usual exit rules
=================
*/
void G_RoundDuelCheckRules( void ) {
	char		msg[MAX_STRING_CHARS];
	gentity_t	*a, *b;
	int			side, n, stack[2];

	if ( level.warmupTime ) {
		rd.state = RD_WAITING;
		return;
	}

	switch ( rd.state ) {
	case RD_WAITING:
		if ( level.numPlayingClients < 2 ) {
			return;
		}
		memset( &rd, 0, sizeof( rd ) );
		// A is whoever has been in the game longer
		rd.player[0] = level.sortedClients[0];
		rd.player[1] = level.sortedClients[1];
		if ( level.clients[rd.player[1]].pers.enterTime < level.clients[rd.player[0]].pers.enterTime ) {
			rd.player[0] = level.sortedClients[1];
			rd.player[1] = level.sortedClients[0];
		}
		rd.state = RD_DRAFT;
		rd.stateTime = level.time + RD_PICK_MSEC;
		G_RDUpdateHud();
		break;

	case RD_DRAFT:
		side = rdPickOrder[rd.numPicks];
		// bots pick at once, people get 20 s
		if ( ( g_entities[rd.player[side]].r.svFlags & SVF_BOT ) || level.time >= rd.stateTime ) {
			G_RDPickRandom( side );
		} else if ( level.time >= rd.hudTime ) {
			rd.hudTime = level.time + 5000;
			Com_sprintf( msg, sizeof( msg ), BG_String( STR_RD_YOUR_PICK ), ( rd.stateTime - level.time + 999 ) / 1000 );
			trap_SendServerCommand( rd.player[side], va( "cp \"%s\"", msg ) );
		}
		if ( rd.numPicks >= RD_CHAMPIONS * 2 ) {
			G_RDStartRound();
		}
		break;

	case RD_COUNTDOWN:
		if ( level.time >= rd.stateTime ) {
			rd.state = RD_FIGHTING;
			rd.roundStart = level.time;
			G_RDUpdateHud();
		}
		break;

	case RD_FIGHTING:
		if ( rd.suddenDeath || g_rdRoundTime.integer <= 0 || level.time - rd.roundStart < g_rdRoundTime.integer * 1000 ) {
			break;
		}
		// time: more champions left, then more health plus armour now
		n = G_RDLeft( 0 ) - G_RDLeft( 1 );
		if ( n ) {
			G_RDEndRound( n > 0 ? 0 : 1 );
			break;
		}
		a = &g_entities[rd.player[0]];
		b = &g_entities[rd.player[1]];
		stack[0] = a->health > 0 ? a->health + a->client->ps.stats[STAT_ARMOR] : 0;
		stack[1] = b->health > 0 ? b->health + b->client->ps.stats[STAT_ARMOR] : 0;
		if ( stack[0] != stack[1] ) {
			G_RDEndRound( stack[0] > stack[1] ? 0 : 1 );
			break;
		}
		rd.suddenDeath = qtrue;
		G_RDAnnounce( BG_String( STR_RD_SUDDEN_DEATH ) );
		break;

	case RD_RESULT:
		if ( level.time < rd.stateTime ) {
			break;
		}
		for ( side = 0 ; side < 2 ; side++ ) {
			if ( level.clients[rd.player[side]].ps.persistant[PERS_SCORE] >= g_rdRounds.integer ) {
				LogExit( "Roundlimit hit." );
				return;
			}
		}
		G_RDStartRound();
		break;
	}
}
