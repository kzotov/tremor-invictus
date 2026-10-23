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
// g_sacrifice.c -- sacrifice, played on capture the flag maps: the two
// flag stands are the obelisks, a soul appears between them, and a team
// wins a round once its soul has rested at its obelisk long enough

#include "g_local.h"

#define SAC_SOUL_DELAY			10000	// after the round starts
#define SAC_ATTACK_CALL_GAP		10000	// "obelisk under attack" at most this often
#define SAC_SOUL_WARNING		15000	// the soul is called 15 s before it appears
#define SAC_RESULT_MSEC			3000
#define SAC_ROUND_MSEC			600000
#define SAC_POINT_MSEC			10000	// a point to each living teammate per 10 s at the obelisk
#define SAC_THROW_SPEED			400
#define SAC_THROW_UP			200
#define SAC_PICKUP_RANGE		32

enum {
	SOUL_NONE,			// not spawned yet this round
	SOUL_GROUND,
	SOUL_CARRIED,
	SOUL_PLANTED
};

static struct {
	qboolean	mapReady;
	vec3_t		obelisk[2];			// [0] where the red flag stood, [1] the blue
	int			numObelisks;
	int			numPlaced;		// team_sacrifice_obelisk entities seen
	vec3_t		soulSpot;		// from team_sacrifice_soul
	qboolean	hasSoulSpot;
	vec3_t		soulSpawn;
	gentity_t	*obeliskEnt[2];

	int			state;				// SAC_* round state
	int			stateTime;
	int			roundStart;
	int			round;

	int			owner[2];			// TEAM_* that claimed each obelisk, or TEAM_FREE
	int			meter[TEAM_NUM_TEAMS];		// msec of soul time
	int			pointTime[TEAM_NUM_TEAMS];	// msec toward the next resting point
	qboolean	suddenDeath;
	int			suddenMeter[TEAM_NUM_TEAMS];

	gentity_t	*soul;
	int			soulState;
	int			soulAt;				// obelisk index while planted
	gentity_t	*carrier;
	qboolean	warned;
	int			extract;			// msec of extraction progress
	int			hudTime;
	int			attackCallTime;		// level.time "obelisk under attack" was last called
} sac;

enum {
	SAC_WAITING,
	SAC_PLAYING,
	SAC_RESULT
};

/*
=================
G_Sacrifice
=================
*/
qboolean G_Sacrifice( void ) {
	return g_gametype.integer == GT_CTF && g_sacrifice.integer;
}

/*
=================
G_SacrificeObeliskSpot

The map's flags are not spawned in sacrifice; their stands become the
obelisks
=================
*/
void G_SacrificeObeliskSpot( gentity_t *ent, gitem_t *item ) {
	int		i = item->giTag == PW_BLUEFLAG ? 1 : 0;

	if ( sac.numPlaced ) {
		return;		// the map places its own obelisks
	}
	VectorCopy( ent->s.origin, sac.obelisk[i] );
	sac.numObelisks |= 1 << i;
}

/*QUAKED team_sacrifice_obelisk (1 0 1) (-16 -16 -16) (16 16 32)
A Sacrifice obelisk; maps place two. They are neutral until claimed.
*/
void SP_team_sacrifice_obelisk( gentity_t *ent ) {
	if ( sac.numPlaced < 2 ) {
		VectorCopy( ent->s.origin, sac.obelisk[sac.numPlaced] );
		sac.numPlaced++;
		if ( sac.numPlaced == 2 ) {
			sac.numObelisks = 3;
		}
	}
	G_FreeEntity( ent );
}

/*QUAKED team_sacrifice_soul (1 0 1) (-16 -16 -16) (16 16 16)
Where the Sacrifice soul appears
*/
void SP_team_sacrifice_soul( gentity_t *ent ) {
	VectorCopy( ent->s.origin, sac.soulSpot );
	sac.hasSoulSpot = qtrue;
	G_FreeEntity( ent );
}

static int G_MeterTime( void ) {
	return ( g_sacMeterTime.integer > 0 ? g_sacMeterTime.integer : 120 ) * 1000;
}

static int G_ObeliskRadius( void ) {
	return g_sacObeliskRadius.integer > 0 ? g_sacObeliskRadius.integer : 192;
}

/*
=================
G_SacrificeUpdateHud

Meters, the soul's whereabouts and extraction progress for the clients
=================
*/
static void G_SacrificeUpdateHud( void ) {
	int		state;
	int		carrier = -1;

	if ( sac.soulState == SOUL_CARRIED && sac.carrier ) {
		carrier = sac.carrier - g_entities;
	}
	state = sac.soulState;
	if ( state == SOUL_PLANTED ) {
		state = 10 + sac.owner[sac.soulAt];
	}
	trap_SetConfigstring( CS_SACRIFICE, va( "%i %i %i %i %i %i",
		sac.meter[TEAM_RED], sac.meter[TEAM_BLUE], state, carrier, sac.extract, G_MeterTime() ) );
}

static void G_SacrificeAnnounce( int id, const char *name ) {
	char	msg[MAX_STRING_CHARS];

	if ( name ) {
		Com_sprintf( msg, sizeof( msg ), BG_String( id ), name );
	} else {
		Q_strncpyz( msg, BG_String( id ), sizeof( msg ) );
	}
	if ( id == STR_SAC_SUDDEN_DEATH ) {
		G_BannerWord( id );
	} else {
		G_EventLine( -1, id, name, NULL );
	}
	trap_SendServerCommand( -1, va( "print \"%s\n\"", msg ) );
}

/*
=================
G_ObeliskVisual

A claimed obelisk shows its team's flag
=================
*/
static void G_ObeliskVisual( int i ) {
	gentity_t	*ent = sac.obeliskEnt[i];

	if ( !ent ) {
		return;
	}
	if ( sac.owner[i] == TEAM_RED ) {
		ent->s.modelindex = G_ModelIndex( "models/flags/r_flag.md3" );
		ent->r.svFlags &= ~SVF_NOCLIENT;
	} else if ( sac.owner[i] == TEAM_BLUE ) {
		ent->s.modelindex = G_ModelIndex( "models/flags/b_flag.md3" );
		ent->r.svFlags &= ~SVF_NOCLIENT;
	} else {
		ent->r.svFlags |= SVF_NOCLIENT;
	}
	trap_LinkEntity( ent );
}

/*
=================
G_SoulPlace

Puts the soul somewhere: lying (falling) at a point, carried, or planted
=================
*/
static void G_SoulPlace( vec3_t origin, vec3_t velocity ) {
	gentity_t	*soul = sac.soul;

	sac.soulState = SOUL_GROUND;
	sac.carrier = NULL;
	soul->r.svFlags &= ~SVF_NOCLIENT;
	soul->r.contents = CONTENTS_TRIGGER;
	soul->s.pos.trType = velocity ? TR_GRAVITY : TR_STATIONARY;
	soul->s.pos.trTime = level.time;
	VectorCopy( origin, soul->s.pos.trBase );
	if ( velocity ) {
		VectorCopy( velocity, soul->s.pos.trDelta );
	} else {
		VectorClear( soul->s.pos.trDelta );
	}
	VectorCopy( origin, soul->r.currentOrigin );
	soul->timestamp = level.time;		// for the return timer
	trap_LinkEntity( soul );
}

static void G_SoulReturn( void ) {
	G_SoulPlace( sac.soulSpawn, NULL );
	sac.soul->enemy = NULL;
	G_SacrificeAnnounce( STR_SAC_RETURNED, NULL );
}

static void G_SoulTake( gentity_t *player ) {
	gentity_t	*soul = sac.soul;

	sac.soulState = SOUL_CARRIED;
	sac.carrier = player;
	sac.extract = 0;
	player->client->ps.powerups[PW_NEUTRALFLAG] = INT_MAX;
	soul->r.svFlags |= SVF_NOCLIENT;
	soul->r.contents = 0;
	trap_LinkEntity( soul );
}

/*
=================
G_SoulTouch
=================
*/
static void G_SoulTouch( gentity_t *soul, gentity_t *other, trace_t *trace ) {
	if ( sac.soulState != SOUL_GROUND || !other->client || other->health <= 0 ) {
		return;
	}
	if ( other->client->sess.sessionTeam != TEAM_RED && other->client->sess.sessionTeam != TEAM_BLUE ) {
		return;
	}
	// a hidden player can't take it, and the thrower can't catch it at once
	if ( G_AbilityActive( other, ACTIVE_GHOSTWALK ) || ( other->client->ps.eFlags & EF_DRONE ) ) {
		return;
	}
	if ( soul->enemy == other && level.time < soul->wait ) {
		return;
	}
	G_SoulTake( other );
}

/*
=================
G_SoulThink

The soul falls to the floor, and goes home after 20 s untouched or at once
from a pit or lava
=================
*/
static void G_SoulThink( gentity_t *soul ) {
	trace_t		tr;
	vec3_t		origin;
	gentity_t	*other;
	int			contents, i;

	soul->nextthink = level.time + FRAMETIME;
	if ( sac.soulState != SOUL_GROUND ) {
		return;
	}

	if ( soul->s.pos.trType == TR_GRAVITY ) {
		BG_EvaluateTrajectory( &soul->s.pos, level.time, origin );
		trap_Trace( &tr, soul->r.currentOrigin, soul->r.mins, soul->r.maxs, origin, soul->s.number, MASK_PLAYERSOLID & ~CONTENTS_BODY );
		VectorCopy( tr.endpos, soul->r.currentOrigin );
		if ( tr.fraction < 1.0f || tr.startsolid ) {
			// come to rest where it landed
			soul->s.pos.trType = TR_STATIONARY;
			VectorCopy( tr.endpos, soul->s.pos.trBase );
			VectorClear( soul->s.pos.trDelta );
		}
		trap_LinkEntity( soul );
	}

	// anyone close enough picks it up
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( other->inuse && other->client && other->health > 0
			&& Distance( other->client->ps.origin, soul->r.currentOrigin ) < SAC_PICKUP_RANGE ) {
			G_SoulTouch( soul, other, NULL );
			if ( sac.soulState != SOUL_GROUND ) {
				return;
			}
		}
	}

	contents = trap_PointContents( soul->r.currentOrigin, -1 );
	if ( ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_NODROP ) ) || soul->r.currentOrigin[2] < -65536
		|| ( level.time - soul->timestamp > g_sacSoulReturn.integer * 1000
			&& Distance( soul->r.currentOrigin, sac.soulSpawn ) > 1 ) ) {
		G_SoulReturn();
	}
}

/*
=================
G_SacrificeDropSoul

The carrier lets go: thrown forward (dropsoul), dropped where they died,
or left behind by a teleport
=================
*/
void G_SacrificeDropSoul( gentity_t *ent, qboolean thrown ) {
	vec3_t		origin, velocity, forward;

	if ( !G_Sacrifice() || sac.soulState != SOUL_CARRIED || sac.carrier != ent ) {
		return;
	}
	ent->client->ps.powerups[PW_NEUTRALFLAG] = 0;
	VectorCopy( ent->client->ps.origin, origin );
	if ( thrown ) {
		AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
		origin[2] += ent->client->ps.viewheight;
		VectorScale( forward, SAC_THROW_SPEED, velocity );
		velocity[2] += SAC_THROW_UP;
	} else {
		VectorClear( velocity );
	}
	G_SoulPlace( origin, velocity );
	sac.soul->enemy = ent;
	sac.soul->wait = level.time + 1000;
	if ( !thrown ) {
		G_SacrificeAnnounce( STR_SAC_DROPPED, NULL );
	}
}

/*
=================
G_SacrificeKill

A frag's extra point: the enemy soul carrier, or an enemy inside the
killer's own obelisk. Also drops the soul a dying carrier held.
=================
*/
void G_SacrificeKill( gentity_t *victim, gentity_t *attacker ) {
	int		i;

	if ( !G_Sacrifice() ) {
		return;
	}
	if ( attacker && attacker->client && attacker != victim && !OnSameTeam( attacker, victim ) && !level.warmupTime ) {
		if ( sac.soulState == SOUL_CARRIED && sac.carrier == victim ) {
			AddScore( attacker, victim->r.currentOrigin, 1 );
		} else {
			for ( i = 0 ; i < 2 ; i++ ) {
				if ( sac.owner[i] == attacker->client->sess.sessionTeam
					&& Distance( victim->r.currentOrigin, sac.obelisk[i] ) < G_ObeliskRadius() ) {
					AddScore( attacker, victim->r.currentOrigin, 1 );
					break;
				}
			}
		}
	}
	G_SacrificeDropSoul( victim, qfalse );
}

/*
=================
G_SacrificeClear

Before the map's entities spawn
=================
*/
void G_SacrificeClear( void ) {
	memset( &sac, 0, sizeof( sac ) );
}

/*
=================
G_SacrificeInit

After the map's entities: the obelisks, and the soul's spawn, the item or
spawn spot nearest the middle between the obelisks
=================
*/
void G_SacrificeInit( void ) {
	gentity_t	*ent, *best = NULL;
	vec3_t		mid;
	float		d, bestDist = 0;
	int			i;

	if ( !G_Sacrifice() ) {
		return;
	}
	if ( !G_Find( NULL, FOFS( classname ), "info_player_deathmatch" ) && !G_Find( NULL, FOFS( classname ), "team_CTF_redspawn" ) ) {
		return;
	}
	// a map without both flags: make do with the first two spawn spots
	if ( sac.numObelisks != 3 ) {
		G_Printf( "sacrifice: this map has no flag stands for obelisks\n" );
		return;
	}

	VectorAdd( sac.obelisk[0], sac.obelisk[1], mid );
	VectorScale( mid, 0.5f, mid );
	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( !ent->inuse || !ent->classname ) {
			continue;
		}
		if ( !ent->item && Q_stricmp( ent->classname, "info_player_deathmatch" )
			&& Q_stricmp( ent->classname, "team_CTF_redspawn" ) && Q_stricmp( ent->classname, "team_CTF_bluespawn" ) ) {
			continue;
		}
		d = Distance( ent->s.origin, mid );
		if ( !best || d < bestDist ) {
			best = ent;
			bestDist = d;
		}
	}
	if ( sac.hasSoulSpot ) {
		VectorCopy( sac.soulSpot, sac.soulSpawn );
	} else if ( !best ) {
		return;
	} else {
		VectorCopy( best->s.origin, sac.soulSpawn );
		sac.soulSpawn[2] += 16;
	}

	for ( i = 0 ; i < 2 ; i++ ) {
		ent = G_Spawn();
		ent->classname = "sacrifice_obelisk";
		ent->s.eType = ET_GENERAL;
		G_SetOrigin( ent, sac.obelisk[i] );
		sac.obeliskEnt[i] = ent;
		sac.owner[i] = TEAM_FREE;
		G_ObeliskVisual( i );
	}

	ent = G_Spawn();
	ent->classname = "sacrifice_soul";
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = G_ModelIndex( "models/gibs/skull.md3" );
	VectorSet( ent->r.mins, -16, -16, -16 );
	VectorSet( ent->r.maxs, 16, 16, 16 );
	ent->touch = G_SoulTouch;
	ent->think = G_SoulThink;
	ent->nextthink = level.time + FRAMETIME;
	ent->r.svFlags |= SVF_NOCLIENT;
	G_SetOrigin( ent, sac.soulSpawn );
	trap_LinkEntity( ent );
	sac.soul = ent;
	sac.soulState = SOUL_NONE;
	sac.mapReady = qtrue;
	if ( trap_Cvar_VariableIntegerValue( "developer" ) ) G_Printf( "sacrifice: obelisks %s and %s, soul at %s\n", vtos( sac.obelisk[0] ), vtos( sac.obelisk[1] ), vtos( sac.soulSpawn ) );
	G_SacrificeUpdateHud();
}

/*
=================
G_SacrificeSpawnPoint

Once a team has claimed its obelisk it spawns near it; before that, any
spot rated like team deathmatch
=================
*/
gentity_t *G_SacrificeSpawnPoint( gentity_t *ent, vec3_t origin, vec3_t angles, qboolean isbot ) {
	static const char	*classes[] = { "info_player_deathmatch", "team_CTF_redspawn", "team_CTF_bluespawn" };
	gentity_t	*spot, *list[64];
	float		dist[64], d;
	int			team = ent->client->sess.sessionTeam;
	int			i, j, k, num = 0, own = -1;

	for ( i = 0 ; i < 2 ; i++ ) {
		if ( sac.mapReady && sac.owner[i] == team ) {
			own = i;
		}
	}
	if ( own < 0 ) {
		return G_SelectDuelSpawnPoint( ent, qfalse, origin, angles, isbot );
	}

	for ( k = 0 ; k < 3 ; k++ ) {
		spot = NULL;
		while ( ( spot = G_Find( spot, FOFS( classname ), classes[k] ) ) != NULL && num < 64 ) {
			if ( SpotWouldTelefrag( spot ) ) {
				continue;
			}
			d = Distance( spot->s.origin, sac.obelisk[own] );
			for ( j = num ; j > 0 && dist[j - 1] > d ; j-- ) {
				dist[j] = dist[j - 1];
				list[j] = list[j - 1];
			}
			dist[j] = d;
			list[j] = spot;
			num++;
		}
	}
	if ( !num ) {
		return SelectSpawnPoint( ent->client->ps.origin, origin, angles, isbot );
	}
	// one of the nearest third
	k = num / 3 > 0 ? num / 3 : 1;
	i = (int)( random() * k );
	if ( i >= k ) {
		i = k - 1;
	}
	VectorCopy( list[i]->s.origin, origin );
	origin[2] += 9;
	VectorCopy( list[i]->s.angles, angles );
	return list[i];
}

/*
=================
G_SacrificeStartRound

Obelisks neutral, the soul gone until its delay is up, everyone respawns
and the map's items are back
=================
*/
static void G_SacrificeStartRound( void ) {
	gentity_t	*ent;
	int			i;

	sac.round++;
	G_RoundInfo( sac.round, 0, qfalse );
	sac.state = SAC_PLAYING;
	G_Announce( -1, "roundbegins" );
	sac.roundStart = level.time;
	sac.warned = qfalse;
	sac.suddenDeath = qfalse;
	sac.extract = 0;
	memset( sac.meter, 0, sizeof( sac.meter ) );
	memset( sac.pointTime, 0, sizeof( sac.pointTime ) );
	for ( i = 0 ; i < 2 ; i++ ) {
		sac.owner[i] = TEAM_FREE;
		G_ObeliskVisual( i );
	}
	if ( sac.carrier && sac.carrier->client ) {
		sac.carrier->client->ps.powerups[PW_NEUTRALFLAG] = 0;
	}
	sac.carrier = NULL;
	sac.soulState = SOUL_NONE;
	sac.soul->r.svFlags |= SVF_NOCLIENT;
	sac.soul->r.contents = 0;
	trap_LinkEntity( sac.soul );

	if ( sac.round > 1 ) {
		G_ResetMapItems();
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			ent = &g_entities[i];
			if ( ent->inuse && level.clients[i].pers.connected == CON_CONNECTED
				&& level.clients[i].sess.sessionTeam != TEAM_SPECTATOR ) {
				ClientSpawn( ent );
			}
		}
	}
	G_SacrificeUpdateHud();
}

/*
=================
G_SacrificeEndRound
=================
*/
static void G_SacrificeEndRound( int winner ) {
	sac.state = SAC_RESULT;
	sac.stateTime = level.time + SAC_RESULT_MSEC;
	G_SacrificeAnnounce( winner == TEAM_RED ? STR_SAC_RED_WINS : STR_SAC_BLUE_WINS, NULL );
	G_Announce( -1, winner == TEAM_RED ? "redwinsround" : "bluewinsround" );
	level.teamScores[winner]++;
	CalculateRanks();
}

/*
=================
G_SacrificeObelisk

Planting at an obelisk, the meter, resting points and extraction
=================
*/
static void G_SacrificeObelisk( int msec ) {
	gentity_t	*ent, *extractor = NULL, *planter;
	int			i, j, team, defenders, attackers;

	// planting: the carrier at their own obelisk, or a neutral one before claims
	if ( sac.soulState == SOUL_CARRIED && sac.carrier ) {
		team = sac.carrier->client->sess.sessionTeam;
		for ( i = 0 ; i < 2 ; i++ ) {
			if ( Distance( sac.carrier->client->ps.origin, sac.obelisk[i] ) > 48 ) {
				continue;
			}
			if ( sac.owner[i] == TEAM_FREE && sac.owner[!i] != team ) {
				// the first plant claims it, and the other obelisk goes to the other team
				sac.owner[i] = team;
				sac.owner[!i] = team == TEAM_RED ? TEAM_BLUE : TEAM_RED;
				G_ObeliskVisual( 0 );
				G_ObeliskVisual( 1 );
			}
			if ( sac.owner[i] != team ) {
				continue;
			}
			planter = sac.carrier;
			planter->client->ps.powerups[PW_NEUTRALFLAG] = 0;
			sac.carrier = NULL;
			sac.soulState = SOUL_PLANTED;
			sac.soulAt = i;
			sac.extract = 0;
			sac.soul->r.svFlags &= ~SVF_NOCLIENT;
			G_SetOrigin( sac.soul, sac.obelisk[i] );
			sac.soul->s.pos.trBase[2] += 48;
			sac.soul->r.currentOrigin[2] += 48;
			trap_LinkEntity( sac.soul );
			AddScore( planter, sac.obelisk[i], 5 );
			planter->client->pers.modeStat[0]++;
			G_SacrificeAnnounce( STR_SAC_PLANTED, planter->client->pers.netname );
			break;
		}
	}

	if ( sac.soulState != SOUL_PLANTED ) {
		return;
	}
	i = sac.soulAt;
	team = sac.owner[i];

	// the meter fills, and every 10 s each living teammate gets a point;
	// the announcer counts its last 10 s
	{
		int		before = ( G_MeterTime() - sac.meter[team] + 999 ) / 1000;
		int		after = ( G_MeterTime() - sac.meter[team] - msec + 999 ) / 1000;

		if ( after != before && after >= 1 && after <= 10 ) {
			static const char	*counts[] = { "", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };

			G_Announce( -1, counts[after] );
		}
	}
	sac.meter[team] += msec;
	sac.pointTime[team] += msec;
	if ( sac.pointTime[team] >= SAC_POINT_MSEC ) {
		sac.pointTime[team] -= SAC_POINT_MSEC;
		for ( j = 0 ; j < level.maxclients ; j++ ) {
			ent = &g_entities[j];
			if ( ent->inuse && ent->client && ent->health > 0 && ent->client->sess.sessionTeam == team ) {
				AddScore( ent, ent->r.currentOrigin, 1 );
			}
		}
	}

	// extraction: enemies inside with no defenders
	defenders = attackers = 0;
	for ( j = 0 ; j < level.maxclients ; j++ ) {
		ent = &g_entities[j];
		if ( !ent->inuse || !ent->client || ent->health <= 0
			|| Distance( ent->client->ps.origin, sac.obelisk[i] ) > G_ObeliskRadius() ) {
			continue;
		}
		if ( ent->client->sess.sessionTeam == team ) {
			defenders++;
		} else if ( ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE ) {
			attackers++;
			if ( !extractor ) {
				extractor = ent;
			}
		}
	}
	if ( attackers && !defenders ) {
		// the owners hear it, at most every 10 s
		if ( !sac.extract && level.time - sac.attackCallTime >= SAC_ATTACK_CALL_GAP ) {
			sac.attackCallTime = level.time;
			for ( j = 0 ; j < level.maxclients ; j++ ) {
				if ( g_entities[j].inuse && g_entities[j].client && g_entities[j].client->sess.sessionTeam == team ) {
					G_Announce( j, "obeliskunderattack" );
					G_EventLine( j, STR_EV_OBELISK_ATTACK, NULL, NULL );
				}
			}
		}
		sac.extract += msec;
	} else if ( !attackers ) {
		sac.extract -= msec;
		if ( sac.extract < 0 ) {
			sac.extract = 0;
		}
	}
	if ( extractor && sac.extract >= g_sacExtractTime.integer ) {
		G_SoulTake( extractor );
		AddScore( extractor, sac.obelisk[i], 5 );
		extractor->client->pers.modeStat[1]++;
		G_SacrificeAnnounce( STR_SAC_EXTRACTED, extractor->client->pers.netname );
	}
}

/*
=================
G_SacrificeCheckRules

Called every frame instead of the usual exit rules
=================
*/
void G_SacrificeCheckRules( void ) {
	static qboolean	running;
	int		msec = level.time - level.previousTime;
	int		team, winner = 0;

	if ( level.warmupTime || !sac.mapReady ) {
		sac.state = SAC_WAITING;
		return;
	}

	// scoring below checks the rules again; once per frame is enough
	if ( running ) {
		return;
	}
	running = qtrue;

	switch ( sac.state ) {
	case SAC_WAITING:
		G_SacrificeStartRound();
		break;

	case SAC_RESULT:
		if ( level.time < sac.stateTime ) {
			break;
		}
		if ( level.teamScores[TEAM_RED] >= g_sacRounds.integer || level.teamScores[TEAM_BLUE] >= g_sacRounds.integer ) {
			LogExit( "Roundlimit hit." );
			break;
		}
		G_SacrificeStartRound();
		break;

	case SAC_PLAYING:
		// the soul appears a while into the round
		if ( sac.soulState == SOUL_NONE ) {
			if ( !sac.warned && level.time - sac.roundStart >= SAC_SOUL_DELAY - SAC_SOUL_WARNING ) {
				char	msg[MAX_STRING_CHARS];

				// 15 s ahead, or at the round start when the soul comes sooner
				sac.warned = qtrue;
				Com_sprintf( msg, sizeof( msg ), "%i", ( SAC_SOUL_DELAY - ( level.time - sac.roundStart ) + 999 ) / 1000 );
				G_EventLine( -1, STR_SAC_SOUL_SOON, msg, NULL );
			}
			if ( level.time - sac.roundStart >= SAC_SOUL_DELAY ) {
				G_SoulPlace( sac.soulSpawn, NULL );
				G_SacrificeAnnounce( STR_SAC_SOUL_SPAWNED, NULL );
			}
		}
		if ( sac.soulState == SOUL_CARRIED && ( !sac.carrier || !sac.carrier->inuse || sac.carrier->health <= 0
			|| !sac.carrier->client->ps.powerups[PW_NEUTRALFLAG] ) ) {
			// the carrier left the game or lost it some other way
			G_SoulReturn();
		}

		G_SacrificeObelisk( msec );

		// a full meter wins; past the round time the fuller meter wins, else
		// the next team to gain meter time
		for ( team = TEAM_RED ; team <= TEAM_BLUE ; team++ ) {
			if ( sac.meter[team] >= G_MeterTime() ) {
				winner = team;
			}
		}
		if ( !winner && level.time - sac.roundStart >= SAC_ROUND_MSEC ) {
			if ( !sac.suddenDeath ) {
				if ( sac.meter[TEAM_RED] != sac.meter[TEAM_BLUE] ) {
					winner = sac.meter[TEAM_RED] > sac.meter[TEAM_BLUE] ? TEAM_RED : TEAM_BLUE;
				} else {
					sac.suddenDeath = qtrue;
					G_RoundInfo( sac.round, 0, qtrue );
					memcpy( sac.suddenMeter, sac.meter, sizeof( sac.meter ) );
					G_SacrificeAnnounce( STR_SAC_SUDDEN_DEATH, NULL );
				}
			} else {
				for ( team = TEAM_RED ; team <= TEAM_BLUE ; team++ ) {
					if ( sac.meter[team] > sac.suddenMeter[team] ) {
						winner = team;
					}
				}
			}
		}
		if ( winner ) {
			G_SacrificeEndRound( winner );
		}
		break;
	}

	// the HUD a few times a second
	if ( level.time >= sac.hudTime ) {
		sac.hudTime = level.time + 250;
		G_SacrificeUpdateHud();
	}
	running = qfalse;
}

/*
=================
G_SacrificeBotGoal

Where a bot should head in sacrifice, by role: per team, in client order,
the first is the carrier, the next two attackers and the fourth the
defender (then again). The carrier fetches the soul and takes it to our
obelisk; attackers go to extract from the enemy's obelisk; the defender
holds ours. False leaves the bot to its usual goals.
=================
*/
qboolean G_SacrificeBotGoal( int clientNum, vec3_t out ) {
	gentity_t	*ent = &g_entities[clientNum];
	int			team, i, index = 0, role, ours, theirs;

	if ( !G_Sacrifice() || sac.state != SAC_PLAYING || !ent->client || sac.numObelisks < 2 ) {
		return qfalse;
	}
	team = ent->client->sess.sessionTeam;
	if ( team != TEAM_RED && team != TEAM_BLUE ) {
		return qfalse;
	}
	for ( i = 0 ; i < clientNum ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED && level.clients[i].sess.sessionTeam == team ) {
			index++;
		}
	}
	role = index % 4;		// 0 carrier, 1 and 2 attackers, 3 defender

	// our obelisk: the one we claimed, else the one by our base
	if ( sac.owner[0] == team ) {
		ours = 0;
	} else if ( sac.owner[1] == team ) {
		ours = 1;
	} else {
		ours = team == TEAM_RED ? 0 : 1;
	}
	theirs = !ours;

	if ( sac.carrier == ent ) {
		VectorCopy( sac.obelisk[ours], out );
		return qtrue;
	}
	switch ( sac.soulState ) {
	case SOUL_GROUND:
		if ( role == 3 || !sac.soul ) {
			break;
		}
		VectorCopy( sac.soul->r.currentOrigin, out );		// everyone but the defender races for it
		return qtrue;
	case SOUL_CARRIED:
		if ( !sac.carrier || !sac.carrier->client ) {
			return qfalse;
		}
		if ( sac.carrier->client->sess.sessionTeam != team ) {
			if ( role == 3 ) {
				break;
			}
			VectorCopy( sac.carrier->r.currentOrigin, out );	// hunt the enemy carrier
			return qtrue;
		}
		if ( role == 0 ) {
			VectorCopy( sac.carrier->r.currentOrigin, out );	// escort ours
			return qtrue;
		}
		if ( role != 3 ) {
			VectorCopy( sac.obelisk[theirs], out );
			return qtrue;
		}
		break;
	case SOUL_PLANTED:
		// ours resting: guard it; theirs: go and extract it
		VectorCopy( sac.obelisk[sac.soulAt], out );
		if ( sac.owner[sac.soulAt] == team || role != 3 ) {
			return qtrue;
		}
		break;
	default:
		return qfalse;
	}
	VectorCopy( sac.obelisk[ours], out );
	return qtrue;
}
