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
// g_rewind.c -- hit detection for instant-hit shots: other players are moved
// back to where the shooter saw them (within a capped window), and players
// are hit as upright cylinders sized by body class

#include "g_local.h"

#define	REWIND_RECORDS		8		// per player, one per server tick
#define	REWIND_TELEPORT		64		// a move this long in one tick forgets the history

static void G_Lerp( const vec3_t a, const vec3_t b, float frac, vec3_t out ) {
	out[0] = a[0] + ( b[0] - a[0] ) * frac;
	out[1] = a[1] + ( b[1] - a[1] ) * frac;
	out[2] = a[2] + ( b[2] - a[2] ) * frac;
}

typedef struct {
	int			time;
	vec3_t		origin;
	vec3_t		mins, maxs;
	int			contents;
} rewindRecord_t;

typedef struct {
	rewindRecord_t	records[REWIND_RECORDS];
	int				count;			// valid records, newest at head
	int				head;
	int				teleportBit;

	// saved while a shot is being checked
	qboolean		moved;
	vec3_t			savedOrigin;
	vec3_t			savedMins, savedMaxs;
	int				savedContents;
} rewindHistory_t;

static rewindHistory_t	rewindHistory[MAX_CLIENTS];
static qboolean			rewindActive;
static int				rewindLimit;		// g_lagHaxMs, latched at map start

/*
================
G_RewindInit

The rewind limit takes effect on the next map
================
*/
void G_RewindInit( void ) {
	memset( rewindHistory, 0, sizeof( rewindHistory ) );
	rewindLimit = g_lagHaxMs.integer;
	if ( rewindLimit < 0 ) {
		rewindLimit = 0;
	} else if ( rewindLimit > 120 ) {
		rewindLimit = 120;
	}
	rewindActive = qfalse;
}

/*
================
G_RewindReset

Forget a player's history (respawn, teleport)
================
*/
void G_RewindReset( gentity_t *ent ) {
	if ( ent->s.number >= 0 && ent->s.number < MAX_CLIENTS ) {
		rewindHistory[ent->s.number].count = 0;
	}
}

/*
================
G_RewindRecord

End of each server tick, after the moves: remember where every player is
================
*/
void G_RewindRecord( void ) {
	gentity_t		*ent;
	rewindHistory_t	*h;
	rewindRecord_t	*rec, *last;
	int				i, teleportBit;

	for ( i = 0, ent = g_entities ; i < level.maxclients ; i++, ent++ ) {
		h = &rewindHistory[i];
		if ( !ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED
			|| ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			h->count = 0;
			continue;
		}

		teleportBit = ent->client->ps.eFlags & EF_TELEPORT_BIT;
		if ( h->count ) {
			last = &h->records[h->head];
			if ( teleportBit != h->teleportBit
				|| Distance( last->origin, ent->r.currentOrigin ) > REWIND_TELEPORT ) {
				h->count = 0;
			}
		}
		h->teleportBit = teleportBit;

		h->head = ( h->head + 1 ) % REWIND_RECORDS;
		rec = &h->records[h->head];
		rec->time = level.time;
		VectorCopy( ent->r.currentOrigin, rec->origin );
		VectorCopy( ent->r.mins, rec->mins );
		VectorCopy( ent->r.maxs, rec->maxs );
		rec->contents = ent->r.contents;
		if ( h->count < REWIND_RECORDS ) {
			h->count++;
		}
	}
}

/*
================
G_RewindPosition

Where a player was at a given time, from the history
================
*/
static void G_RewindPosition( rewindHistory_t *h, int time, vec3_t origin, vec3_t mins, vec3_t maxs, int *contents ) {
	rewindRecord_t	*newer, *older;
	float			frac;
	int				i, index;

	newer = &h->records[h->head];
	for ( i = 1 ; i < h->count ; i++ ) {
		index = ( h->head - i + REWIND_RECORDS ) % REWIND_RECORDS;
		older = &h->records[index];
		if ( older->time <= time ) {
			frac = 0;
			if ( newer->time > older->time ) {
				frac = (float)( time - older->time ) / ( newer->time - older->time );
			}
			G_Lerp( older->origin, newer->origin, frac, origin );
			VectorCopy( newer->mins, mins );
			VectorCopy( newer->maxs, maxs );
			*contents = newer->contents;
			return;
		}
		newer = older;
	}
	// not enough history: the oldest record
	VectorCopy( newer->origin, origin );
	VectorCopy( newer->mins, mins );
	VectorCopy( newer->maxs, maxs );
	*contents = newer->contents;
}

/*
================
G_RewindPlayers

Before an instant-hit shot: move every other player back to the shooter's
command time, no further than the limit, and widen their boxes to hold the
hit cylinder. Bots get no compensation.
================
*/
void G_RewindPlayers( gentity_t *shooter ) {
	gentity_t		*ent;
	rewindHistory_t	*h;
	vec3_t			origin, mins, maxs;
	int				i, contents, time;
	float			radius;
	qboolean		compensate;

	if ( rewindActive ) {
		return;
	}
	rewindActive = qtrue;

	time = shooter->client->ps.commandTime;
	if ( time < level.time - rewindLimit ) {
		time = level.time - rewindLimit;
	}
	compensate = rewindLimit > 0 && !( shooter->r.svFlags & SVF_BOT ) && time < level.time;

	for ( i = 0, ent = g_entities ; i < level.maxclients ; i++, ent++ ) {
		h = &rewindHistory[i];
		h->moved = qfalse;
		if ( ent == shooter || !ent->inuse || !ent->client || ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		VectorCopy( ent->r.currentOrigin, h->savedOrigin );
		VectorCopy( ent->r.mins, h->savedMins );
		VectorCopy( ent->r.maxs, h->savedMaxs );
		h->savedContents = ent->r.contents;
		h->moved = qtrue;

		if ( compensate && h->count ) {
			G_RewindPosition( h, time, origin, mins, maxs, &contents );
			if ( g_rewindDebug.integer ) {
				G_Printf( "rewind %i: %i ms, %.1f u\n", i, level.time - time, Distance( origin, ent->r.currentOrigin ) );
			}
		} else {
			VectorCopy( ent->r.currentOrigin, origin );
			VectorCopy( ent->r.mins, mins );
			VectorCopy( ent->r.maxs, maxs );
			contents = ent->r.contents;
		}

		// the box must hold the shot cylinder
		radius = maxs[0] * g_cylinderScale.value;
		mins[0] = mins[1] = -radius;
		maxs[0] = maxs[1] = radius;

		VectorCopy( origin, ent->r.currentOrigin );
		VectorCopy( mins, ent->r.mins );
		VectorCopy( maxs, ent->r.maxs );
		ent->r.contents = contents;
		trap_LinkEntity( ent );
	}
}

/*
================
G_RestorePlayers

After the shot: everyone back where they are now
================
*/
void G_RestorePlayers( void ) {
	gentity_t		*ent;
	rewindHistory_t	*h;
	int				i;

	if ( !rewindActive ) {
		return;
	}
	rewindActive = qfalse;

	for ( i = 0, ent = g_entities ; i < level.maxclients ; i++, ent++ ) {
		h = &rewindHistory[i];
		if ( !h->moved ) {
			continue;
		}
		h->moved = qfalse;
		if ( !ent->inuse ) {
			continue;
		}
		VectorCopy( h->savedOrigin, ent->r.currentOrigin );
		VectorCopy( h->savedMins, ent->r.mins );
		VectorCopy( h->savedMaxs, ent->r.maxs );
		ent->r.contents = h->savedContents;
		trap_LinkEntity( ent );
	}
}

/*
================
G_CylinderHit

Where a segment enters a player's upright cylinder (the box's height,
radius half its width), as a fraction of the segment; qfalse if it misses
================
*/
static qboolean G_CylinderHit( gentity_t *ent, const vec3_t start, const vec3_t end, float *fraction ) {
	vec3_t	center, d, s;
	float	radius, a, b, c, disc, t, z, bottom, top;

	VectorCopy( ent->r.currentOrigin, center );
	radius = ent->r.maxs[0];
	bottom = center[2] + ent->r.mins[2];
	top = center[2] + ent->r.maxs[2];

	VectorSubtract( end, start, d );
	VectorSubtract( start, center, s );

	// already inside the circle: hit at the start if the height fits
	if ( s[0] * s[0] + s[1] * s[1] <= radius * radius ) {
		t = 0;
	} else {
		a = d[0] * d[0] + d[1] * d[1];
		if ( a < 0.0001f ) {
			return qfalse;
		}
		b = 2 * ( s[0] * d[0] + s[1] * d[1] );
		c = s[0] * s[0] + s[1] * s[1] - radius * radius;
		disc = b * b - 4 * a * c;
		if ( disc < 0 ) {
			return qfalse;
		}
		t = ( -b - sqrt( disc ) ) / ( 2 * a );
		if ( t < 0 || t > 1 ) {
			return qfalse;
		}
	}
	z = start[2] + d[2] * t;
	if ( z < bottom || z > top ) {
		// it may still come in through the top or bottom cap
		float	cap = d[2] > 0 ? bottom : top;
		float	tc, x, y;

		if ( fabs( d[2] ) < 0.0001f ) {
			return qfalse;
		}
		tc = ( cap - start[2] ) / d[2];
		if ( tc < 0 || tc > 1 ) {
			return qfalse;
		}
		x = s[0] + d[0] * tc;
		y = s[1] + d[1] * tc;
		if ( x * x + y * y > radius * radius ) {
			return qfalse;
		}
		t = tc;
	}
	*fraction = t;
	return qtrue;
}

/*
================
G_ShotTrace

trap_Trace for instant-hit shots: a player box hit only counts if the
segment also enters the player's cylinder; otherwise the shot goes on
================
*/
void G_ShotTrace( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs,
	const vec3_t end, int passEntityNum, int contentmask ) {
	gentity_t	*skipped[8];
	gentity_t	*hit;
	float		fraction;
	int			numSkipped = 0, i;

	while ( 1 ) {
		trap_Trace( tr, start, mins, maxs, end, passEntityNum, contentmask );
		if ( tr->entityNum >= MAX_CLIENTS || !rewindActive || numSkipped == ARRAY_LEN( skipped ) ) {
			break;
		}
		hit = &g_entities[tr->entityNum];
		if ( G_CylinderHit( hit, start, end, &fraction ) ) {
			tr->fraction = fraction;
			G_Lerp( start, end, fraction, tr->endpos );
			break;
		}
		// passed by the corner of the box: ignore this player
		trap_UnlinkEntity( hit );
		skipped[numSkipped++] = hit;
	}

	for ( i = 0 ; i < numSkipped ; i++ ) {
		trap_LinkEntity( skipped[i] );
	}
}
