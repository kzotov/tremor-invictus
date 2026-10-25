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
#define RD_PICK_MSEC		15000
#define RD_COUNTDOWN_MSEC	10000	// "prepare to fight" at 10 s, "round begins in" at 5, 3-2-1
#define RD_RESULT_MSEC		5000

enum {
	RD_WAITING,			// match not started
	RD_DRAFT,
	RD_DRAFTED,			// picked in the warm-up, waiting for the match
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
	int			said;				// countdown lines already played (bits)
	qboolean	quickStart;			// round 1 straight after the warm-up's countdown
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
G_RoundDuelHidesPicks

round-duel.md 3: the first champions stay hidden through the countdown,
so the players look like the default champion until the fight starts
=================
*/
qboolean G_RoundDuelHidesPicks( void ) {
	return G_RoundDuel() && rd.state == RD_COUNTDOWN;
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

	if ( side < 0 || level.warmupTime || rd.state == RD_WAITING || rd.state == RD_DRAFT || rd.state == RD_DRAFTED ) {
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
	// then, for the draft screen and the top bar: "#<drafting> <clientA>
	// <clientB> <a1 a2 a3> <b1 b2 b3> <deadA> <deadB> <currentA> <currentB>",
	// -1 not picked yet / not fighting; dead: a bit per roster index
	{
		int		pick[2][RD_CHAMPIONS], n[2] = { 0, 0 }, s;

		for ( side = 0 ; side < 2 ; side++ ) {
			for ( i = 0 ; i < RD_CHAMPIONS ; i++ ) {
				pick[side][i] = -1;
			}
		}
		for ( i = 0 ; i < RD_CHAMPIONS * 2 && i < rd.numPicks ; i++ ) {
			s = rdPickOrder[i];
			pick[s][n[s]] = rd.picks[s][n[s]];
			n[s]++;
		}
		trap_SetConfigstring( CS_SACRIFICE, va( "rd|%s|%s|#%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i", line[0], line[1],
			rd.state == RD_DRAFT, rd.player[0], rd.player[1], pick[0][0], pick[0][1], pick[0][2],
			pick[1][0], pick[1][1], pick[1][2], rd.dead[0], rd.dead[1],
			// round-duel.md 3: the first champion stays hidden through the countdown
			rd.state == RD_FIGHTING || rd.state == RD_RESULT ? rd.current[0] : -1,
			rd.state == RD_FIGHTING || rd.state == RD_RESULT ? rd.current[1] : -1,
			// then whose pick it is (-1 none) and its deadline in server time
			rd.state == RD_DRAFT && rd.numPicks < RD_CHAMPIONS * 2 ? rdPickOrder[rd.numPicks] : -1,
			rd.state == RD_DRAFT ? rd.stateTime : 0 ) );
	}
}

static void G_RDAnnounce( int id, const char *a, const char *b ) {
	char	msg[MAX_STRING_CHARS];

	if ( id == STR_RD_SUDDEN_DEATH ) {
		G_BannerWord( id );
	} else {
		G_EventLine( -1, id, a, b );
	}
	if ( id == STR_RD_ROUND ) {
		Com_sprintf( msg, sizeof( msg ), BG_String( id ), atoi( a ) );
	} else {
		Com_sprintf( msg, sizeof( msg ), BG_String( id ), a, b );
	}
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
	rd.hudTime = 0;		// the next player's prompt at once
	G_RDAnnounce( STR_RD_PICKED, level.clients[rd.player[side]].pers.netname,
		BG_String( BG_Champion( champion )->nameString ) );
	G_RDUpdateHud();
	return qtrue;
}

static void G_RDPickRandom( int side ) {
	// bots draft by a fixed preference, then at random
	static const char	*botPreference[] = { "ranger", "galena", "scalebearer", "visor", "doomslayer",
		"anarki", "clutch", "slash", "nyx", "eisen", "bj", "keel", "athena", "sorlag", "strogg", "deathknight" };
	int		tries, i;

	if ( g_entities[rd.player[side]].r.svFlags & SVF_BOT ) {
		for ( i = 0 ; i < ARRAY_LEN( botPreference ) ; i++ ) {
			int		champion = BG_ChampionForName( botPreference[i] );

			if ( champion >= 0 && !Q_stricmp( BG_Champion( champion )->name, botPreference[i] ) && G_RDPick( side, champion ) ) {
				return;
			}
		}
	}

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

	if ( !G_RoundDuel() || rd.state != RD_DRAFT || side < 0 || rd.numPicks >= RD_CHAMPIONS * 2
		|| rdPickOrder[rd.numPicks] != side ) {
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
	int		side;

	rd.round++;
	G_RoundInfo( rd.round, 0, qfalse );
	rd.state = RD_COUNTDOWN;
	rd.stateTime = level.time + RD_COUNTDOWN_MSEC;
	rd.suddenDeath = qfalse;
	G_ResetMapItems();
	G_ClearRoundObjects();		// the last round's missiles and ability objects
	level.abilityRefill = qtrue;		// every champion's first life starts ready
	for ( side = 0 ; side < 2 ; side++ ) {
		rd.dead[side] = 0;
		rd.current[side] = 0;
		ClientSpawn( &g_entities[rd.player[side]] );
	}
	level.abilityRefill = qfalse;
	G_RDAnnounce( STR_RD_ROUND, va( "%i", rd.round ), NULL );
	// the pre-round lines (konstantin-list-audit F2 1): "prepare to fight"
	// now, 10 s out; the rest from G_RoundDuelCheckRules
	rd.said = 0;
	if ( rd.quickStart ) {
		rd.stateTime = level.time;		// (no countdown, no lines)
		rd.said = 15;
	} else {
		G_Announce( -1, "preparetofight|prepare" );
	}
	G_RDUpdateHud();
}

static void G_RDEndRound( int winner ) {
	gclient_t	*cl = &level.clients[rd.player[winner]];

	rd.state = RD_RESULT;
	rd.stateTime = level.time + RD_RESULT_MSEC;
	rd.quickStart = qfalse;		// later rounds count down
	cl->ps.persistant[PERS_SCORE]++;
	G_RDAnnounce( STR_RD_WINS_ROUND, cl->pers.netname, NULL );
	// "Round N complete" (the reference has no won / lost lines per round;
	// they stand in without the recording)
	if ( rd.round >= 1 && rd.round <= 5 ) {
		G_Announce( rd.player[winner], va( "round%icomplete|youwintheround", rd.round ) );
		G_Announce( rd.player[!winner], va( "round%icomplete|youlosetheround", rd.round ) );
	} else {
		G_Announce( rd.player[winner], "youwintheround" );
		G_Announce( rd.player[!winner], "youlosetheround" );
	}
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
G_RDPairPlaying

The drafting pair is still the two in the duel (always, before a draft)
=================
*/
static qboolean G_RDPairPlaying( void ) {
	int		i;

	if ( rd.state == RD_WAITING ) {
		return qtrue;
	}
	for ( i = 0 ; i < 2 ; i++ ) {
		gclient_t	*cl = &level.clients[rd.player[i]];

		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam != TEAM_FREE ) {
			return qfalse;
		}
	}
	return qtrue;
}

/*
=================
G_RoundDuelDrafting

The draft is on: the players can't hurt each other (one of them is
looking at the picking page)
=================
*/
qboolean G_RoundDuelDrafting( void ) {
	return G_RoundDuel() && rd.state == RD_DRAFT;
}

/*
=================
G_RDBeginDraft

Both players found: A is whoever has been in the game longer
=================
*/
static void G_RDBeginDraft( void ) {
	trap_Cvar_Set( "g_rdDraft", "" );		// (a draft from an earlier warm-up)
	memset( &rd, 0, sizeof( rd ) );
	rd.player[0] = level.sortedClients[0];
	rd.player[1] = level.sortedClients[1];
	if ( level.clients[rd.player[1]].pers.enterTime < level.clients[rd.player[0]].pers.enterTime ) {
		rd.player[0] = level.sortedClients[1];
		rd.player[1] = level.sortedClients[0];
	}
	rd.state = RD_DRAFT;
	rd.stateTime = level.time + RD_PICK_MSEC;
	G_RDUpdateHud();
}

/*
=================
G_RDDraftFrame

The pick whose turn it is: bots at once, people get RD_PICK_MSEC, then a
random one. True once all six are in.
=================
*/
static qboolean G_RDDraftFrame( void ) {
	int		side;

	if ( rd.numPicks >= RD_CHAMPIONS * 2 ) {
		return qtrue;
	}
	side = rdPickOrder[rd.numPicks];
	if ( ( g_entities[rd.player[side]].r.svFlags & SVF_BOT ) || level.time >= rd.stateTime ) {
		G_RDPickRandom( side );
	} else if ( level.time >= rd.hudTime ) {
		// (the draft view shows the turn and the seconds; a centre message
		// would wait behind the page and show up in the fight)
		rd.hudTime = level.time + 5000;
		if ( rd.said != rd.numPicks + 1 ) {
			rd.said = rd.numPicks + 1;		// "your pick" once a turn
			G_Announce( rd.player[side], "yourpick" );
		}
	}
	return rd.numPicks >= RD_CHAMPIONS * 2;
}

/*
=================
G_RDSaveDraft / G_RDLoadDraft

The warm-up's draft kept across the map restart that starts the match:
"<clientA> <clientB> <a1 a2 a3> <b1 b2 b3>"
=================
*/
static void G_RDSaveDraft( void ) {
	trap_Cvar_Set( "g_rdDraft", va( "%i %i %i %i %i %i %i %i", rd.player[0], rd.player[1],
		rd.picks[0][0], rd.picks[0][1], rd.picks[0][2], rd.picks[1][0], rd.picks[1][1], rd.picks[1][2] ) );
}

static int G_RDLoadDraft( void ) {
	char	buf[MAX_CVAR_VALUE_STRING], *p;
	int		v[8], i;

	trap_Cvar_VariableStringBuffer( "g_rdDraft", buf, sizeof( buf ) );
	if ( !buf[0] ) {
		return 0;
	}
	p = buf;
	for ( i = 0 ; i < 8 ; i++ ) {
		v[i] = atoi( COM_Parse( &p ) );
	}
	for ( i = 0 ; i < 2 ; i++ ) {
		if ( v[i] >= 0 && v[i] < level.maxclients && level.clients[v[i]].pers.connected == CON_CONNECTING ) {
			return -1;		// still coming back from the map restart
		}
	}
	trap_Cvar_Set( "g_rdDraft", "" );
	for ( i = 0 ; i < 2 ; i++ ) {
		if ( v[i] < 0 || v[i] >= level.maxclients || level.clients[v[i]].pers.connected != CON_CONNECTED
			|| level.clients[v[i]].sess.sessionTeam != TEAM_FREE ) {
			return 0;		// the players changed: draft again
		}
	}
	for ( i = 2 ; i < 8 ; i++ ) {
		if ( v[i] < 0 || v[i] >= bg_numChampions ) {
			return 0;
		}
	}
	memset( &rd, 0, sizeof( rd ) );
	rd.player[0] = v[0];
	rd.player[1] = v[1];
	for ( i = 0 ; i < RD_CHAMPIONS ; i++ ) {
		rd.picks[0][i] = v[2 + i];
		rd.picks[1][i] = v[5 + i];
	}
	rd.numPicks = RD_CHAMPIONS * 2;
	return 1;
}

/*
=================
G_RoundDuelDrafted

The warm-up's ready countdown waits for the draft
=================
*/
qboolean G_RoundDuelDrafted( void ) {
	return !G_RoundDuel() || rd.state == RD_DRAFTED;
}

/*
=================
G_RoundDuelCheckRules

Called every frame instead of the usual exit rules
=================
*/
void G_RoundDuelCheckRules( void ) {
	gentity_t	*a, *b;
	int			side, n, stack[2];

	// the draft runs in the warm-up, once both players are in, and the
	// ready countdown waits for it (G_RoundDuelDrafted); never after "FIGHT"
	if ( level.warmupTime ) {
		// no pair yet, the pair changed (a queued player came in) or the
		// lobby is still up: no draft (it starts again)
		if ( level.numPlayingClients < 2 || !G_RDPairPlaying() || G_LobbyActive() ) {
			if ( rd.state != RD_WAITING ) {
				memset( &rd, 0, sizeof( rd ) );
				G_RDUpdateHud();
			}
			return;
		}
		if ( rd.state == RD_WAITING || ( rd.state != RD_DRAFT && rd.state != RD_DRAFTED ) ) {
			G_RDBeginDraft();
		}
		if ( rd.state == RD_DRAFT && G_RDDraftFrame() ) {
			rd.state = RD_DRAFTED;
			G_RDSaveDraft();
			G_RDUpdateHud();
		}
		return;
	}

	switch ( rd.state ) {
	case RD_WAITING:
		if ( level.numPlayingClients < 2 ) {
			return;
		}
		// the warm-up's draft, across the match's map restart: the warm-up
		// counted down and said FIGHT, so round 1 starts at once
		switch ( G_RDLoadDraft() ) {
		case 1:
			rd.quickStart = qtrue;
			G_RDStartRound();
			return;
		case -1:
			return;		// (wait for both to be back)
		default:
			break;
		}
		G_RDBeginDraft();		// (no warm-up: the draft before the first round)
		break;

	case RD_DRAFT:
		if ( G_RDDraftFrame() ) {
			G_RDStartRound();
		}
		break;

	case RD_DRAFTED:
		G_RDStartRound();
		break;

	case RD_COUNTDOWN:
		{
			static const char	*lines[] = { "roundbegins", "three", "two", "one" };
			static const int	at[] = { 5000, 3000, 2000, 1000 };
			int					k;

			for ( k = 0 ; k < 4 ; k++ ) {
				if ( rd.stateTime - level.time <= at[k] && !( rd.said & ( 1 << k ) ) ) {
					rd.said |= ( 1 << k ) - 1 | ( 1 << k );		// (a late frame skips the earlier ones)
					G_Announce( -1, lines[k] );
					break;
				}
			}
		}
		if ( level.time >= rd.stateTime ) {
			// "Round N, fight" (the client falls back to "fight"); the first
			// round right after the warm-up's FIGHT has none of its own
			if ( !rd.quickStart ) {
				G_Announce( -1, rd.round <= 5 ? va( "round%ifight|fight", rd.round ) : "fight" );
			}

			rd.state = RD_FIGHTING;
			rd.roundStart = level.time;
			G_RDUpdateHud();
			// the fight: the first champions show (G_RoundDuelHidesPicks)
			for ( side = 0 ; side < 2 ; side++ ) {
				ClientUserinfoChanged( rd.player[side] );
			}
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
		G_RoundInfo( rd.round, 0, qtrue );
		G_RDAnnounce( STR_RD_SUDDEN_DEATH, NULL, NULL );
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
