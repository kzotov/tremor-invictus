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
// g_local.h -- local definitions for game module

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "g_public.h"

//==================================================================

// the "gameversion" client command will print this plus compile date
#define	GAMEVERSION	BASEGAME

#define BODY_QUEUE_SIZE		8

#define INFINITE			1000000

#define	FRAMETIME			100					// msec
#define	CARNAGE_REWARD_TIME	3000
#define REWARD_SPRITE_TIME	2000

#define	INTERMISSION_DELAY_TIME	1000
#define	SP_INTERMISSION_DELAY_TIME	5000

// gentity->flags
#define	FL_GODMODE				0x00000010
#define	FL_NOTARGET				0x00000020
#define	FL_TEAMMEMBER			0x00000400	// not the first on the team
#define FL_NO_KNOCKBACK			0x00000800
#define FL_DROPPED_ITEM			0x00001000
#define FL_NO_BOTS				0x00002000	// spawn point not for bot use
#define FL_NO_HUMANS			0x00004000	// spawn point just for bots
#define FL_FORCE_GESTURE		0x00008000	// force gesture on client

// movers are things like doors, plats, buttons, etc
typedef enum {
	MOVER_POS1,
	MOVER_POS2,
	MOVER_1TO2,
	MOVER_2TO1
} moverState_t;

#define SP_PODIUM_MODEL		"models/mapobjects/podium/podium4.md3"

//============================================================================

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

struct gentity_s {
	entityState_t	s;				// communicated by server to clients
	entityShared_t	r;				// shared by both the server system and game

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!
	//================================

	struct gclient_s	*client;			// NULL if not a client

	qboolean	inuse;

	char		*classname;			// set in QuakeEd
	int			spawnflags;			// set in QuakeEd

	qboolean	neverFree;			// if true, FreeEntity will only unlink
									// bodyque uses this

	int			flags;				// FL_* variables

	char		*model;
	char		*model2;
	int			freetime;			// level.time when the object was freed
	
	int			eventTime;			// events will be cleared EVENT_VALID_MSEC after set
	qboolean	freeAfterEvent;
	qboolean	unlinkAfterEvent;

	qboolean	physicsObject;		// if true, it can be pushed by movers and fall off edges
									// all game items are physicsObjects, 
	float		physicsBounce;		// 1.0 = continuous bounce, 0.0 = no bounce
	int			clipmask;			// brushes with this content value will be collided against
									// when moving.  items and corpses do not collide against
									// players, for instance

	// movers
	moverState_t moverState;
	int			soundPos1;
	int			sound1to2;
	int			sound2to1;
	int			soundPos2;
	int			soundLoop;
	gentity_t	*parent;
	gentity_t	*nextTrain;
	gentity_t	*prevTrain;
	vec3_t		pos1, pos2;

	char		*message;

	int			timestamp;		// body queue sinking, etc

	char		*target;
	char		*targetname;
	char		*team;
	char		*targetShaderName;
	char		*targetShaderNewName;
	gentity_t	*target_ent;

	float		speed;
	vec3_t		movedir;

	int			nextthink;
	void		(*think)(gentity_t *self);
	void		(*reached)(gentity_t *self);	// movers call this when hitting endpoint
	void		(*blocked)(gentity_t *self, gentity_t *other);
	void		(*touch)(gentity_t *self, gentity_t *other, trace_t *trace);
	void		(*use)(gentity_t *self, gentity_t *other, gentity_t *activator);
	void		(*pain)(gentity_t *self, gentity_t *attacker, int damage);
	void		(*die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);

	int			pain_debounce_time;
	int			fly_sound_debounce_time;	// wind tunnel
	int			last_move_time;

	int			health;

	qboolean	takedamage;

	int			damage;
	int			splashDamage;	// quad will increase this without increasing radius
	int			splashRadius;
	int			methodOfDeath;
	int			splashMethodOfDeath;

	int			count;

	gentity_t	*chain;
	gentity_t	*enemy;
	gentity_t	*activator;
	gentity_t	*teamchain;		// next entity in team
	gentity_t	*teammaster;	// master of the team

#ifdef MISSIONPACK
	int			kamikazeTime;
	int			kamikazeShockTime;
#endif

	int			watertype;
	int			waterlevel;

	int			noise_index;

	// timing variables
	float		wait;
	float		random;

	gitem_t		*item;			// for bonus items

	// champion ability entities
	void		(*abilityImpact)( gentity_t *self, trace_t *trace );	// replaces the missile impact
	int			abilityMask[MAX_CLIENTS / 32];	// players already affected once
	int			abilityTime;	// meaning depends on the entity
	int			itemDue;		// a power-up: level.time it appears (its think may be the warning first)
	int			medalKills;		// kills by this missile, for Collateral Damage
	int			medalSpawnTime;	// level.time this item last respawned, for Stopwatch

	// weapon numbers carried by a projectile (wpTuned set)
	qboolean	wpTuned;
	float		splashMin;		// splash damage at the edge
	float		splashFull;		// splash is full strength out to this distance
	qboolean	splashSparesOwner;	// the splash never hits whoever fired it
	float		wpKnock;		// knockback on others
	float		wpKnockSelf;	// knockback on the owner
};


typedef enum {
	CON_DISCONNECTED,
	CON_CONNECTING,
	CON_CONNECTED
} clientConnected_t;

typedef enum {
	SPECTATOR_NOT,
	SPECTATOR_FREE,
	SPECTATOR_FOLLOW,
	SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
	TEAM_BEGIN,		// Beginning a team game, spawn at base
	TEAM_ACTIVE		// Now actively playing
} playerTeamStateState_t;

typedef struct {
	playerTeamStateState_t	state;

	int			location;

	int			captures;
	int			basedefense;
	int			carrierdefense;
	int			flagrecovery;
	int			fragcarrier;
	int			assists;

	float		lasthurtcarrier;
	float		lastreturnedflag;
	float		flagsince;
	float		lastfraggedcarrier;
} playerTeamState_t;

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
typedef struct {
	team_t		sessionTeam;
	int			spectatorNum;		// for determining next-in-line to play
	spectatorState_t	spectatorState;
	int			spectatorClient;	// for chasecam and follow mode
	int			wins, losses;		// tournament stats
	qboolean	teamLeader;			// true when this client is a team leader
	qboolean	muted;				// an admin muted their chat
} clientSession_t;

//
#define MAX_NETNAME			36
#define	MAX_VOTE_COUNT		3

// client data that stays across multiple respawns, but is cleared
// on each level change or team change at ClientBegin()
typedef struct {
	clientConnected_t	connected;	
	usercmd_t	cmd;				// we would lose angles if not persistant
	qboolean	localClient;		// true if "ip" info key is "localhost"
	qboolean	initialSpawn;		// the first spawn should be at a cool location
	qboolean	predictItemPickup;	// based on cg_predictItems userinfo
	qboolean	pmoveFixed;			//
	int			champion;			// bg_championList index, applied on spawn
	qboolean	ready;				// duel warmup: readyup typed
	qboolean	championLocked;		// duel: the match's champion is fixed
	int			lockedChampion;
	int			caDamage;			// clan arena: damage to enemies not yet turned into points
	int			startWeapon;		// weapon_t chosen to spawn with
	qboolean	autoHop;			// userinfo cg_autoHop
	char		netname[MAX_NETNAME];
	int			maxHealth;			// for handicapping
	int			accShots[WP_NUM_WEAPONS];	// this match, per weapon
	int			accHits[WP_NUM_WEAPONS];
	int			damageGiven;		// to enemies, health and armour
	int			modeStat[2];		// scoreboard mode columns: sacrifice plants and extracts, clan arena rounds survived
	short		lifeDealt[MAX_CLIENTS];	// this life: damage to each player, for the death screen
	short		lifeTaken[MAX_CLIENTS];	// and from each
	int			damageTaken;
	int			pickups[4];			// mega health, heavy armour, light armour, power-ups
	int			enterTime;			// level.time the client entered the game
	playerTeamState_t teamState;	// status in teamplay games
	int			voteCount;			// to prevent people from constantly calling votes
	int			lastVoteTime;		// level.time this player last called a vote
	int			kickVotes;			// kick votes called this match
	int			kickVoteTime;		// level.time a kick vote last named this player
	qboolean	motdShown;
	int			teamVoteCount;		// to prevent people from constantly calling votes
	qboolean	teamInfo;			// send team overlay updates?
} clientPersistant_t;


// this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s {
	// ps MUST be the first element, because the server expects it
	playerState_t	ps;				// communicated by server to clients

	// the rest of the structure is private to game
	clientPersistant_t	pers;
	clientSession_t		sess;

	qboolean	readyToExit;		// wishes to leave the intermission

	qboolean	noclip;

	int			lastCmdTime;
	qboolean	specJumpHeld;		// a spectator's jump, for its press edge		// level.time of last usercmd_t, for EF_CONNECTION
									// we can't just use pers.lastCommand.time, because
									// of the g_sycronousclients case
	int			buttons;
	int			oldbuttons;
	int			latched_buttons;

	vec3_t		oldOrigin;
	vec3_t		oldVelocity;		// before this command's move (the drone leaves the body with it)

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int			damage_armor;		// damage absorbed by armor
	int			damage_blood;		// damage taken out of health
	int			damage_knockback;	// impact damage
	vec3_t		damage_from;		// origin for vector calculation
	qboolean	damage_fromWorld;	// if true, don't use the damage_from vector

	int			accurateCount;		// for "impressive" reward sound

	int			accuracy_shots;		// total number of shots
	int			accuracy_hits;		// total number of hits

	//
	int			lastkilled_client;	// last client that this client killed
	int			lasthurt_client;	// last client that damaged this client
	int			lasthurt_mod;		// type of damage the client did

	// timers
	int			respawnTime;		// can respawn when time > this, force after g_forcerespwan
	int			inactivityTime;		// kick players when time > this
	qboolean	inactivityWarning;	// qtrue if the five seoond warning has been given
	int			rewardTime;			// clear the EF_AWARD_IMPRESSIVE, etc when time > this

	int			airOutTime;

	int			lastKillTime;		// for multiple kill rewards

	// medals (g_medals.c)
	int			medals[MEDAL_NUM];	// this match
	int			medalKillTime;		// level.time of the last kill, for multi-kills
	int			multiKills;			// kills each within 3 s of the one before
	int			streak;				// kills this life
	int			lifeDamage;			// damage to enemies this life
	int			matchDamage;		// damage to enemies this match
	int			powerupKills;		// kills while holding this power-up
	int			killedBy;			// the client that killed this one last
	int			killedByStreak;		// how many times in a row they did
	int			weaponKills[WP_NUM_WEAPONS];
	int			assists;
	int			groundTime;			// level.time last on the ground
	int			hurtTime[MAX_CLIENTS];		// when each client last hurt this one
	int			comboTime[MAX_CLIENTS];		// ... with anything but the railgun
	int			lastEnemyHurt;		// the last enemy to hurt this one, for Ring Out
	int			lastEnemyHurtTime;
	int			deathTime;			// level.time and inflictor of this client's death
	int			deathInflictor;
	int			pointBlankTime;		// one Point Blank per shot
	qboolean	tankDone;			// one Tank per life
	int			spawnProtectEnd;	// level.time spawn protection ends

	qboolean	fireHeld;			// used for hook
	gentity_t	*hook;				// grapple hook if out
	int			abilityRecharge;	// msec toward the next ability charge

	// unreleased champions (g_champions2.c)
	int			shockBudget;		// msec the cooldown may still run back this shock
	int			slowEndTime;		// spines: run speed cut until then
	int			markEndTime;		// marked prey / storm: shown through walls until then
	int			trailBurnTime;		// Plasma Trail: no burn again before this
	float		slowScale;			// run speed while slowEndTime holds
	int			hasteEndTime;		// Ranger: +10% run speed after a teleport
	int			stunEndTime;		// no firing and no ability until then
	int			revealEndTime;		// seen by an enemy drone: shown through walls
	int			markTeam;			// team (or client + 1000 in free for all) whose hits are stronger
	// Xaero
	int			kickExpire[KICK_MAX];	// banked Leaping Kicks, level.time each runs out
	int			kickEndTime;			// passing through players until then
	int			kickHits[2];			// players already kicked this leap (client bits)
	int			kickAirEnd;				// kick air control until then
	int			modStartTime;			// Master of Death began
	int			modDeferEnd;			// a death held off until then
	int			modKiller;				// who would have killed him
	qboolean	modFragged;				// he fragged someone while held
	qboolean	modDeferred;
	int			invulnEndTime;		// no damage until then
	int			echoLeft;			// health still to heal back
	int			echoEndTime;
	int			echoResidual;
	int			hiveResidual;
	int			hookDamageTaken;	// while hooked; the rope breaks at g_hookBreakDamage
	int			armorRegenResidual;
	qboolean	timeoutUsed;		// this match
	qboolean	decaySkipped;		// slow decay: every other second
	vec3_t		anchorOrigin;
	struct gentity_s	*anchorEnt;
	int			abilityPrevState;	// ability state after the last think
	int			lastDamageTime;		// level.time this client last lost health
	int			regenResidual;		// msec of regeneration not yet turned into health

	gentity_t	*abilityEnt;		// orb in flight
	gentity_t	*fireballs[3];		// flame strike fireballs in flight
	gentity_t	*turret;			// placed sentry turret
	int			abilityShots;		// acid blobs spat so far
	int			abilityShotTime;	// level.time of the next blob

	int			barrierMode;		// 1 shield, 2 laser
	int			laserTime;			// level.time of the next laser shot
	int			laserEnd;			// level.time the laser stops
	int			barrierDamage;		// damage the shield has taken
	int			trailTime;			// level.time the trail started (0: no segment dropped yet)
	vec3_t		trailLast;			// where the last plasma trail charge was dropped
	gentity_t	*droneBody;			// the body left behind while flying the drone
	int			droneStartTime;
	int			droneEndTime;		// input waits 0.25 s after the drone ends
	vec3_t		droneBodyAngles;
	int			droneArmor;

	int			triboltShots;		// bolts still to come from this trigger pull
	int			triboltTime;		// level.time of the next bolt

	int			burnEndTime, burnNextTime, burnAttacker;
	int			poisonEndTime, poisonNextTime, poisonAttacker;
	int			collisionTime[MAX_CLIENTS];	// level.time of the last collision hit per victim
	int			berserkHitTime[MAX_CLIENTS];	// level.time of the last Berserk punch per victim

	int			switchTeamTime;		// time the player switched teams

	// timeResidual is used to handle events that happen every second
	// like health / armor countdowns and regeneration
	int			timeResidual;

#ifdef MISSIONPACK
	gentity_t	*persistantPowerup;
	int			portalID;
	int			ammoTimes[WP_NUM_WEAPONS];
	int			invulnerabilityTime;
#endif

	char		*areabits;
};


//
// this structure is cleared as each map is entered
//
#define	MAX_SPAWN_VARS			64
#define	MAX_SPAWN_VARS_CHARS	4096

typedef struct {
	struct gclient_s	*clients;		// [maxclients]

	struct gentity_s	*gentities;
	int			gentitySize;
	int			num_entities;		// MAX_CLIENTS <= num_entities <= ENTITYNUM_MAX_NORMAL

	int			warmupTime;			// restart match at this time

	fileHandle_t	logFile;

	// store latched cvars here that we want to get at often
	int			maxclients;

	int			framenum;
	int			time;					// in msec
	int			previousTime;			// so movers can back up when blocked

	int			startTime;				// level.time the map was started

	int			teamScores[TEAM_NUM_TEAMS];
	int			lastTeamLocationTime;		// last time of client team location update

	qboolean	newSession;				// don't use any old session data, because
										// we changed gametype

	qboolean	restarted;				// waiting for a map_restart to fire

	int			numConnectedClients;
	int			numNonSpectatorClients;	// includes connecting clients
	int			numPlayingClients;		// connected, non-spectators
	int			sortedClients[MAX_CLIENTS];		// sorted by score
	int			follow1, follow2;		// clientNums for auto-follow spectators

	int			snd_fry;				// sound index for standing in lava

	int			warmupModificationCount;	// for detecting if g_warmup is changed

	// voting state
	char		voteString[MAX_STRING_CHARS];
	char		voteDisplayString[MAX_STRING_CHARS];
	int			voteTime;				// level.time vote was called
	int			voteExecuteTime;		// time the vote is executed
	qboolean	voteKick;				// the running vote is a kick: 75% to pass
	int			voteYes;
	int			voteNo;
	int			numVotingClients;		// set by CalculateRanks

	// team voting state
	char		teamVoteString[2][MAX_STRING_CHARS];
	int			teamVoteTime[2];		// level.time vote was called
	int			teamVoteYes[2];
	int			teamVoteNo[2];
	int			numteamVotingClients[2];// set by CalculateRanks

	// spawn variables
	qboolean	spawning;				// the G_Spawn*() functions are valid
	int			numSpawnVars;
	char		*spawnVars[MAX_SPAWN_VARS][2];	// key / value pairs
	int			numSpawnVarChars;
	char		spawnVarChars[MAX_SPAWN_VARS_CHARS];

	// intermission state
	int			intermissionQueued;		// intermission was qualified, but
										// wait INTERMISSION_DELAY_TIME before
										// actually going there so the last
										// frag can be watched.  Disable future
										// kills during this delay
	int			intermissiontime;		// time the intermission was started
	char		*changemap;
	qboolean	readyToExit;			// at least one client wants to exit

	// duel
	int			overtimes;				// overtime periods added so far
	qboolean	duelForceReady;
	int			readyWaitStart;		// level.time the warm-up wait began
	char		readyUpString[64];	// last CS_READYUP sent

	// capture the flag
	int			ctfHalf;				// 0 or 1 the first half, 2 the second
	int			halfTimeEnd;			// level.time the half-time break ends, while in it
	int			timeoutEnd;				// level.time a timeout ends, 0 when none
	qboolean	forfeitPending;			// a duel player left the running match
	int			timeoutClockTime;		// level.time the clock was last sent during a timeout
	qboolean	teamTimeoutUsed[TEAM_NUM_TEAMS];

	// clan arena
	int			caState;				// CA_*
	int			caStateTime;			// when the countdown or the result ends
	int			caRoundStart;			// level.time the round's fighting began
	int			caRound;				// rounds played, this one included
	qboolean	caSpawning;				// the round start is spawning everyone
	int			exitTime;
	vec3_t		intermission_origin;	// also used for spectator spawns
	vec3_t		intermission_angle;

	qboolean	locationLinked;			// target_locations get linked
	gentity_t	*locationHead;			// head of the location list
	int			bodyQueIndex;			// dead bodies
	gentity_t	*bodyQue[BODY_QUEUE_SIZE];
#ifdef MISSIONPACK
	int			portalSequence;
#endif
	qboolean	caLastStanding[TEAM_NUM_TEAMS];	// clan arena: the lone survivor has been told
	qboolean	teamLocked[TEAM_NUM_TEAMS];	// admin lock: nobody joins
	qboolean	forcingTeam;		// an admin is moving a player: locks and limits do not apply
	qboolean	firstBlood;			// medals: the match's first kill has happened
	int			powerupClient;		// the last client to take a power-up
	int			powerupStreak;		// how many in a row they took
} level_locals_t;


//
// g_spawn.c
//
qboolean	G_SpawnString( const char *key, const char *defaultString, char **out );
// spawn string returns a temporary reference, you must CopyString() if you want to keep it
qboolean	G_SpawnFloat( const char *key, const char *defaultString, float *out );
qboolean	G_SpawnInt( const char *key, const char *defaultString, int *out );
qboolean	G_SpawnVector( const char *key, const char *defaultString, float *out );
void		G_SpawnEntitiesFromString( void );
char *G_NewString( const char *string );

//
// g_cmds.c
//
void Cmd_Score_f (gentity_t *ent);
void StopFollowing( gentity_t *ent );
void BroadcastTeamChange( gclient_t *client, int oldTeam );
void SetTeam( gentity_t *ent, const char *s );
void Cmd_FollowCycle_f( gentity_t *ent, int dir );

//
// g_items.c
//
void G_CheckTeamItems( void );
void G_RunItem( gentity_t *ent );
void RespawnItem( gentity_t *ent );

void UseHoldableItem( gentity_t *ent );
void PrecacheItem (gitem_t *it);
gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle );
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity );
void SetRespawn (gentity_t *ent, float delay);
void G_SpawnItem (gentity_t *ent, gitem_t *item);
void FinishSpawningItem( gentity_t *ent );
void Think_Weapon (gentity_t *ent);
int ArmorIndex (gentity_t *ent);
void	Add_Ammo (gentity_t *ent, int weapon, int count);
void Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace);

void ClearRegisteredItems( void );
void RegisterItem( gitem_t *item );
void SaveRegisteredItems( void );

//
// g_utils.c
//
int G_ModelIndex( char *name );
int		G_SoundIndex( char *name );
int		G_MapSoundIndex( const char *name, const vec3_t origin );
void	G_TeamCommand( team_t team, char *cmd );
void	G_KillBox (gentity_t *ent);
gentity_t *G_Find (gentity_t *from, int fieldofs, const char *match);
gentity_t *G_PickTarget (char *targetname);
void	G_UseTargets (gentity_t *ent, gentity_t *activator);
void	G_SetMovedir ( vec3_t angles, vec3_t movedir);

void	G_InitGentity( gentity_t *e );
gentity_t	*G_Spawn (void);
gentity_t *G_TempEntity( vec3_t origin, int event );
void	G_Sound( gentity_t *ent, int channel, int soundIndex );
void	G_FreeEntity( gentity_t *e );
qboolean	G_EntitiesFree( void );

void	G_TouchTriggers (gentity_t *ent);

float	*tv (float x, float y, float z);
char	*vtos( const vec3_t v );

float vectoyaw( const vec3_t vec );

void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm );
void G_AddEvent( gentity_t *ent, int event, int eventParm );
void G_SetOrigin( gentity_t *ent, vec3_t origin );
void AddRemap(const char *oldShader, const char *newShader, float timeOffset);
const char *BuildShaderStateConfig( void );

//
// g_combat.c
//
qboolean CanDamage (gentity_t *targ, vec3_t origin);
void G_Damage (gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod);
qboolean G_RadiusDamage (vec3_t origin, gentity_t *attacker, float damage, float radius, gentity_t *ignore, int mod);
int G_InvulnerabilityEffect( gentity_t *targ, vec3_t dir, vec3_t point, vec3_t impactpoint, vec3_t bouncedir );
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath );
void TossClientItems( gentity_t *self );
#ifdef MISSIONPACK
void TossClientPersistantPowerups( gentity_t *self );
#endif
void TossClientCubes( gentity_t *self );

// damage flags
#define DAMAGE_RADIUS				0x00000001	// damage was indirect
#define DAMAGE_NO_ARMOR				0x00000002	// armour does not protect from this damage
#define DAMAGE_NO_KNOCKBACK			0x00000004	// do not affect velocity, just view angles
#define DAMAGE_NO_PROTECTION		0x00000008  // armor, shields, invulnerability, and godmode have no effect
#ifdef MISSIONPACK
#define DAMAGE_NO_TEAM_PROTECTION	0x00000010  // armor, shields, invulnerability, and godmode have no effect
#endif

//
// g_missile.c
//
void G_RunMissile( gentity_t *ent );

gentity_t *fire_plasma (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_grenade (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_rocket (gentity_t *self, vec3_t start, vec3_t dir);
gentity_t *fire_bfg (gentity_t *self, vec3_t start, vec3_t dir);
gentity_t *fire_grapple (gentity_t *self, vec3_t start, vec3_t dir);
#ifdef MISSIONPACK
gentity_t *fire_nail( gentity_t *self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up );
gentity_t *fire_prox( gentity_t *self, vec3_t start, vec3_t aimdir );
#endif


//
// g_mover.c
//
void G_RunMover( gentity_t *ent );
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace );

//
// g_trigger.c
//
void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace );


//
// g_misc.c
//
void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );
#ifdef MISSIONPACK
void DropPortalSource( gentity_t *ent );
void DropPortalDestination( gentity_t *ent );
#endif


//
// g_weapon.c
//
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker );
void CalcMuzzlePoint ( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint );
void SnapVectorTowards( vec3_t v, vec3_t to );
qboolean CheckGauntletAttack( gentity_t *ent );
void Weapon_HookFree (gentity_t *ent);
void Ability_HookFire( gentity_t *ent );
void G_EndRush( gentity_t *ent );
void G_CutAbilityCooldown( gentity_t *ent, float fraction );
void G_UpdateRevealedPlayers( void );

//
// g_ability.c
//
void G_AbilityFire( gentity_t *ent );
void G_AbilityRepress( gentity_t *ent );
void G_AbilityClientThink( gentity_t *ent );
void G_AbilityClientDied( gentity_t *ent );
void G_Ignite( gentity_t *target, gentity_t *attacker );
void G_Poison( gentity_t *target, gentity_t *attacker );
void G_ClearDamageOverTime( gentity_t *ent );
void G_ExplodeMissile( gentity_t *ent );
void G_NextKnockback( float knock );
void G_NextTilt( float tilt );
void G_WeaponTuneMissile( gentity_t *m, int weapon );
void G_WeaponThink( gentity_t *ent );
void G_SchedulePowerup( gentity_t *ent, int seconds );
void G_SplashParams( float minDamage, float knock, float knockSelf );
void G_SplashShape( float full, qboolean sparesOwner );

//
void LogExit( const char *string );
qboolean ScoreIsTied( void );

// g_duel.c
qboolean	G_MatchMode( void );
int			G_MatchTimeLimit( void );
int			G_MatchFragLimit( void );
int			G_MaxPlayers( void );

//
// g_medals.c
//
void		G_MedalsDamage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, int take, int mod );
void		G_MedalsDeath( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int mod, qboolean abilityOn );
void		G_MedalsPickup( gentity_t *ent, gentity_t *item );
void		G_MedalsRail( gentity_t *ent, int hits );
void		G_MedalOffense( gentity_t *attacker, gentity_t *targ );
void		G_MedalsSpawn( gentity_t *ent );
void		G_SendAwards( void );
void		G_Announce( int clientNum, const char *key );
void		G_SysMsg( int clientNum, int id, const char *a, const char *b );
void		G_EventLine( int clientNum, int id, const char *a, const char *b );
void		G_BannerWord( int id );
void		G_RoundInfo( int round, int overtime, qboolean suddenDeath );
void		G_ServerLine( int clientNum, const char *text );

//
// g_arcade.c
//
qboolean	G_Instagib( void );
qboolean	G_UnholyTrinity( void );
qboolean	G_ArcadeKeepsItem( const gitem_t *item );
void		G_ArcadeLoadout( gentity_t *ent );
int			G_ArcadeDamage( gentity_t *targ, gentity_t *attacker, int damage, int mod );
void		G_ArcadeDamageDealt( gentity_t *attacker, gentity_t *targ, int damage );

//
// g_admin.c
//
void		G_LoadAccess( void );
qboolean	G_TempBanned( const char *ipPort );
qboolean	G_AdminConsoleCommand( void );
qboolean	G_PresetName( const char *name );
void		Cmd_Admin_f( gentity_t *ent );

//
// g_mapvote.c
//
void		G_MapVoteStart( void );
qboolean	G_MapVoteActive( void );
const char	*G_MapVoteWinner( void );
void		Cmd_MapVote_f( gentity_t *ent );
void		G_LobbyInit( int restart );
void		G_LobbyClientGone( int clientNum );
void		G_LobbyThink( void );
void		G_LobbyEndMatch( void );
qboolean	G_LobbyActive( void );
qboolean	G_LobbyHidesPicks( void );
int			G_LobbyFrozenChampion( int clientNum );
void		Cmd_LobbyLock_f( gentity_t *ent );
int			G_ArenaCount( void );
const char	*G_ArenaInfo( int n );
qboolean	G_DuelPlayersReady( void );
qboolean	G_WarmupPractice( void );
void		G_Slow( gentity_t *target, float scale, int msec );
void		G_WarmupLoadout( gentity_t *ent );
void		G_WarmupAmmo( gentity_t *ent );
qboolean	G_WarmupKill( gentity_t *ent );
int			G_ReadyShare( void );
qboolean	G_DuelCheckTime( void );
void		G_DuelCheckFraglimit( void );
void		Cmd_ReadyUp_f( gentity_t *ent );
void		Svcmd_AllReady_f( void );
void		Svcmd_Abort_f( void );
int			G_DuelSpawnChampion( gclient_t *client );
gentity_t	*G_SelectDuelSpawnPoint( gentity_t *ent, qboolean initial, vec3_t origin, vec3_t angles, qboolean isbot );

// g_ctf.c
void		G_DropCarriedFlag( gentity_t *ent, qboolean thrown );
void		Cmd_DropFlag_f( gentity_t *ent );
void		G_CTFCheckRules( void );

// g_ca.c
#define CA_WAITING		0		// match not started yet
#define CA_COUNTDOWN	1		// everyone at their spawn, held in place
#define CA_FIGHTING		2
#define CA_RESULT		3		// round over, showing who won
qboolean	G_ClanArena( void );
const char	*G_ModeName( void );
qboolean	G_ListHasWord( const char *list, const char *word );
void		G_CACheckRules( void );
void		G_CALoadout( gentity_t *ent );
void		G_CADamageDealt( gentity_t *attacker, gentity_t *targ, int damage );

void		G_ResetMapItems( void );

// g_sacrifice.c
qboolean	G_Sacrifice( void );
qboolean	G_SacrificeBotGoal( int clientNum, vec3_t out );
void		G_SacrificeObeliskSpot( gentity_t *ent, gitem_t *item );
void		G_SacrificeClear( void );
void		G_SacrificeInit( void );
void		G_SacrificeCheckRules( void );
void		G_SacrificeDropSoul( gentity_t *ent, qboolean thrown );
void		G_SacrificeKill( gentity_t *victim, gentity_t *attacker );
gentity_t	*G_SacrificeSpawnPoint( gentity_t *ent, vec3_t origin, vec3_t angles, qboolean isbot );

// g_roundduel.c
qboolean	G_RoundDuel( void );
qboolean	G_RoundDuelHolds( void );
int			G_RoundDuelChampion( gclient_t *client );
void		G_RoundDuelKill( gentity_t *victim );
void		G_RoundDuelCheckRules( void );
void		Cmd_Pick_f( gentity_t *ent );
void		Cmd_Next_f( gentity_t *ent );

// g_champions2.c
void		G_AbilityEyes( gentity_t *ent, vec3_t start, vec3_t forward, vec3_t right );
void		G_AbilityDone( gentity_t *ent );
qboolean	G_AbilityEnemy( gentity_t *owner, gentity_t *other );
qboolean	G_AbilityMarked( gentity_t *ent, int clientNum );
void		G_AbilityMark( gentity_t *ent, int clientNum );
gentity_t	*G_AbilityMissile( gentity_t *owner, const char *classname, vec3_t start, vec3_t dir, float speed, int trType, int weapon );
void		G_AbilityStop( gentity_t *ent, vec3_t point );
void		G_RegisterChampionCvars( void );
void		G_UpdateChampionCvars( void );
int			G_UndyingSave( gentity_t *targ, gentity_t *attacker, int take, int mod );
void		G_BankKick( gentity_t *ent );
void		G_LeapingKick( gentity_t *ent );
void		G_XaeroThink( gentity_t *ent );
void		G_MasterOfDeathStart( gentity_t *ent );
void		G_MasterOfDeathEnd( gentity_t *ent );
void		G_MasterOfDeathFrag( gentity_t *attacker );
void		G_ShellShock( gentity_t *ent );
void		G_SpineVolley( gentity_t *ent );
void		G_SupplyDrop( gentity_t *ent );
void		G_MarkPrey( gentity_t *ent );
void		G_Downpour( gentity_t *ent );
void		G_ReleaseHound( gentity_t *ent );
void		G_DropAnchor( gentity_t *ent );
void		G_RecallAnchor( gentity_t *ent );
void		G_ChampionClientThink( gentity_t *ent );
void		G_ChampionClientDied( gentity_t *ent );
int			G_ChampionDamageScale( gentity_t *targ, gentity_t *attacker, int damage, int dflags, int mod );
void		G_ChampionDamaged( gentity_t *targ, int take );
qboolean	G_ShellShockRecharge( gclient_t *client, int msec );

// g_rewind.c
//
void G_RewindInit( void );
void G_RewindReset( gentity_t *ent );
void G_RewindRecord( void );
void G_RewindPlayers( gentity_t *shooter );
void G_RestorePlayers( void );
void G_ShotTrace( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs,
	const vec3_t end, int passEntityNum, int contentmask );
void G_AbilityStarted( gentity_t *ent );
void G_AbilityEnded( gentity_t *ent );
void G_DroneEnd( gentity_t *ent, qboolean launch );
void G_DroneBodyPush( gentity_t *body, const vec3_t kvel );
#define DRONE_INPUT_WAIT	250		// msec of no input after the drone ends
float G_BarrierScale( gentity_t *targ, int mod );
void G_BarrierHit( gentity_t *targ, int damage );
qboolean G_DroneAbsorbDamage( gentity_t *ent, int take );
void G_DropStroyentCell( gentity_t *victim, gentity_t *attacker );
qboolean G_AbilityActive( gentity_t *ent, int active );

#define	G_ClassInfo( client )	( &bg_classInfo[BG_Champion( (client)->ps.stats[STAT_CHAMPION] )->bodyClass] )
int G_MaxArmor( gclient_t *client );
void Weapon_HookThink (gentity_t *ent);


//
// g_client.c
//
int TeamCount( int ignoreClientNum, team_t team );
int TeamLeader( int team );
team_t PickTeam( int ignoreClientNum );
void SetClientViewAngle( gentity_t *ent, vec3_t angle );
gentity_t *SelectSpawnPoint (vec3_t avoidPoint, vec3_t origin, vec3_t angles, qboolean isbot);
void CopyToBodyQue( gentity_t *ent );
void ClientRespawn(gentity_t *ent);
void BeginIntermission (void);
void InitBodyQue (void);
void ClientSpawn( gentity_t *ent );
void player_die (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);
void AddScore( gentity_t *ent, vec3_t origin, int score );
void CalculateRanks( void );
qboolean SpotWouldTelefrag( gentity_t *spot );

//
// g_svcmds.c
//
qboolean	ConsoleCommand( void );
void G_ProcessIPBans(void);
qboolean G_FilterPacket (char *from);

//
// g_weapon.c
//
void FireWeapon( gentity_t *ent );
#ifdef MISSIONPACK
void G_StartKamikaze( gentity_t *ent );
#endif

//
// g_cmds.c
//
void DeathmatchScoreboardMessage( gentity_t *ent );

//
// g_main.c
//
void MoveClientToIntermission( gentity_t *ent );
void FindIntermissionPoint( void );
void SetLeader(int team, int client);
void CheckTeamLeader( int team );
void G_RunThink (gentity_t *ent);
void AddTournamentQueue(gclient_t *client);
void QDECL G_LogPrintf( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
void SendScoreboardMessageToAllClients( void );
void QDECL G_Printf( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
void QDECL G_Error( const char *fmt, ... ) Q_NO_RETURN Q_PRINTF_FUNC(1, 2);

//
// g_client.c
//
char *ClientConnect( int clientNum, qboolean firstTime, qboolean isBot );
void ClientUserinfoChanged( int clientNum );
void ClientDisconnect( int clientNum );
void ClientBegin( int clientNum );
void ClientCommand( int clientNum );

//
// g_active.c
//
void ClientThink( int clientNum );
void ClientEndFrame( gentity_t *ent );
void G_RunClient( gentity_t *ent );

//
// g_team.c
//
qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 );
void Team_CheckDroppedItem( gentity_t *dropped );
qboolean CheckObeliskAttack( gentity_t *obelisk, gentity_t *attacker );

//
// g_mem.c
//
void *G_Alloc( int size );
void G_InitMemory( void );
void Svcmd_GameMem_f( void );

//
// g_session.c
//
void G_ReadSessionData( gclient_t *client );
void G_InitSessionData( gclient_t *client, char *userinfo );

void G_InitWorldSession( void );
void G_WriteSessionData( void );

//
// g_arenas.c
//
void UpdateTournamentInfo( void );
void SpawnModelsOnVictoryPads( void );
void Svcmd_AbortPodium_f( void );

//
// g_bot.c
//
void G_InitBots( qboolean restart );
char *G_GetBotInfoByNumber( int num );
char *G_GetBotInfoByName( const char *name );
void G_CheckBotSpawn( void );
void G_RemoveQueuedBotBegin( int clientNum );
qboolean G_BotConnect( int clientNum, qboolean restart );
void Svcmd_AddBot_f( void );
void Svcmd_BotList_f( void );
void BotInterbreedEndMatch( void );

// ai_main.c
#define MAX_FILEPATH			144

//bot settings
typedef struct bot_settings_s
{
	char characterfile[MAX_FILEPATH];
	float skill;
} bot_settings_t;

int BotAISetup( int restart );
int BotAIShutdown( int restart );
int BotAILoadMap( int restart );
int BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart);
int BotAIShutdownClient( int client, qboolean restart );
int BotAIStartFrame( int time );
void BotTestAAS(vec3_t origin);

#include "g_team.h" // teamplay specific stuff


extern	level_locals_t	level;
extern	gentity_t		g_entities[MAX_GENTITIES];

#define	FOFS(x) ((size_t)&(((gentity_t *)0)->x))

extern	vmCvar_t	g_gametype;
extern	vmCvar_t	g_dedicated;
extern	vmCvar_t	g_cheats;
extern	vmCvar_t	g_maxclients;			// allow this many total, including spectators
extern	vmCvar_t	g_maxGameClients;		// allow this many active
extern	vmCvar_t	g_restarted;
extern	vmCvar_t	g_overtime;
extern	vmCvar_t	g_duelReadyUp;
extern	vmCvar_t	g_readyUp;
extern	vmCvar_t	g_passiveRegen;
extern	vmCvar_t	g_selfDamage;
extern	vmCvar_t	g_damageOverTime;
extern	vmCvar_t	g_deathStun;
extern	vmCvar_t	g_warmupDamage;
extern	vmCvar_t	g_readyUpShare;
extern	vmCvar_t	g_warmupTime;
extern	vmCvar_t	g_duelFraglimit;
extern	vmCvar_t	g_tdmFraglimit;
extern	vmCvar_t	g_randomChampion;
extern	vmCvar_t	g_championDuplicates;
extern	vmCvar_t	g_instagib;
extern	vmCvar_t	g_unholyTrinity;
extern	vmCvar_t	g_spawnProtection;
extern	vmCvar_t	g_spawnProtectionScale;
extern	vmCvar_t	g_teamSize;
extern	vmCvar_t	g_voteTypes;
extern	vmCvar_t	g_mapVote;
extern	vmCvar_t	g_lobby;
extern	vmCvar_t	g_lobbyVoteTime;
extern	vmCvar_t	g_lobbyPickTime;
extern	vmCvar_t	g_mapVoteTime;
extern	vmCvar_t	g_mapVoteDelay;
extern	vmCvar_t	g_ffaFraglimit;
extern	vmCvar_t	g_ffaMaxPlayers;
extern	vmCvar_t	g_ctfHalfLength;
extern	vmCvar_t	g_ctfCapturelimit;
extern	vmCvar_t	g_clanArena;
extern	vmCvar_t	g_caRounds;
extern	vmCvar_t	g_caRoundTime;
extern	vmCvar_t	g_sacrifice;
extern	vmCvar_t	g_sacRounds;
extern	vmCvar_t	g_sacMeterTime;
extern	vmCvar_t	g_sacExtractTime;
extern	vmCvar_t	g_sacObeliskRadius;
extern	vmCvar_t	g_sacSoulReturn;
extern	vmCvar_t	g_roundDuel;
extern	vmCvar_t	g_rdRounds;
extern	vmCvar_t	g_rdRoundTime;

extern	vmCvar_t	g_dmflags;
extern	vmCvar_t	g_fraglimit;
extern	vmCvar_t	g_timelimit;
extern	vmCvar_t	g_capturelimit;
extern	vmCvar_t	g_friendlyFire;
extern	vmCvar_t	g_password;
extern	vmCvar_t	g_needpass;
extern	vmCvar_t	g_gravity;
extern	vmCvar_t	g_speed;
extern	vmCvar_t	g_injectionHeal;
extern	vmCvar_t	g_berserkDamage;
extern	vmCvar_t	g_berserkReach;
extern	vmCvar_t	g_berserkRadius;
extern	vmCvar_t	g_ghostWalkTelefrag;
extern	vmCvar_t	g_regenDelay;
extern	vmCvar_t	g_regenRate;
extern	vmCvar_t	g_selfDamageScale;
extern	vmCvar_t	g_splashResistScale;
extern	vmCvar_t	g_cooldownArmorLight;
extern	vmCvar_t	g_cooldownArmorHeavy;
extern	vmCvar_t	g_cooldownHealth;
extern	vmCvar_t	g_cooldownMega;
extern	vmCvar_t	g_cooldownAmmo;
extern	vmCvar_t	g_orbSpeed;
extern	vmCvar_t	g_orbLife;
extern	vmCvar_t	g_orbFuse;
extern	vmCvar_t	g_orbShotDamage;
extern	vmCvar_t	g_orbShotRadius;
extern	vmCvar_t	g_orbContactDamage;
extern	vmCvar_t	g_orbSplashDamage;
extern	vmCvar_t	g_orbSplashRadius;
extern	vmCvar_t	g_orbTelefragDamage;
extern	vmCvar_t	g_swarmSpeed;
extern	vmCvar_t	g_swarmFuse;
extern	vmCvar_t	g_swarmDamage;
extern	vmCvar_t	g_swarmRadius;
extern	vmCvar_t	g_fireballSpeed;
extern	vmCvar_t	g_fireballSpread;
extern	vmCvar_t	g_fireballDamage;
extern	vmCvar_t	g_fireballSplash;
extern	vmCvar_t	g_fireballRadius;
extern	vmCvar_t	g_flameConvergeDamage;
extern	vmCvar_t	g_flameConvergeRadius;
extern	vmCvar_t	g_burnDamage;
extern	vmCvar_t	g_burnTime;
extern	vmCvar_t	g_poisonDamage;
extern	vmCvar_t	g_poisonTime;
extern	vmCvar_t	g_acidBlobs;
extern	vmCvar_t	g_acidInterval;
extern	vmCvar_t	g_acidSpeed;
extern	vmCvar_t	g_acidDamage;
extern	vmCvar_t	g_acidPoolTime;
extern	vmCvar_t	g_acidPoolSize;
extern	vmCvar_t	g_totemSpeed;
extern	vmCvar_t	g_totemArmTime;
extern	vmCvar_t	g_totemHealth;
extern	vmCvar_t	g_totemHeal;
extern	vmCvar_t	g_totemDamage;
extern	vmCvar_t	g_totemHealRadius;
extern	vmCvar_t	g_totemHurtRadius;
extern	vmCvar_t	g_totemEnforcedHeal;
extern	vmCvar_t	g_totemEnforcedDamage;
extern	vmCvar_t	g_totemMax;
extern	vmCvar_t	g_turretHealth;
extern	vmCvar_t	g_turretDamage;
extern	vmCvar_t	g_turretInterval;
extern	vmCvar_t	g_turretRange;
extern	vmCvar_t	g_turretBoltSpeed;
extern	vmCvar_t	g_barrierScale;
extern	vmCvar_t	g_barrierHealth;
extern	vmCvar_t	g_laserTime;
extern	vmCvar_t	g_laserDelay;
extern	vmCvar_t	g_laserInterval;
extern	vmCvar_t	g_laserDamage;
extern	vmCvar_t	g_laserRange;
extern	vmCvar_t	g_trailSpacing;
extern	vmCvar_t	g_trailLife;
extern	vmCvar_t	g_trailDamage;
extern	vmCvar_t	g_trailBlast;
extern	vmCvar_t	g_trailRadius;
extern	vmCvar_t	g_droneHealth;
extern	vmCvar_t	g_droneDamage;
extern	vmCvar_t	g_droneLaunchDelay;
extern	vmCvar_t	g_droneRocketDamage;
extern	vmCvar_t	g_droneRocketRadius;
extern	vmCvar_t	g_stroyentHeal;
extern	vmCvar_t	g_stroyentLife;
extern	vmCvar_t	g_lagHaxMs;
extern	vmCvar_t	g_cylinderScale;
extern	vmCvar_t	g_rewindDebug;
extern	vmCvar_t	g_championModels;
extern	vmCvar_t	g_knockbackScale;
extern	vmCvar_t	g_protectionScale;
extern	vmCvar_t	g_shotgunFalloffStep;
extern	vmCvar_t	g_shotgunCloseFade;
extern	vmCvar_t	g_shotgunCloseBonus;
extern	vmCvar_t	g_shotgunCloseRange;
extern	vmCvar_t	g_rushDamage;
extern	vmCvar_t	g_rushSlowDamage;
extern	vmCvar_t	g_rushFastSpeed;
extern	vmCvar_t	g_swarmKnock;
extern	vmCvar_t	g_hmgZoomDamage;
extern	vmCvar_t	g_railSightDamage;
extern	vmCvar_t	g_hmgZoomSpread;
extern	vmCvar_t	g_ssgCloseBonus;
extern	vmCvar_t	g_ssgCloseRange;
extern	vmCvar_t	g_ssgCloseFade;
extern	vmCvar_t	g_triboltShots;
extern	vmCvar_t	g_triboltInterval;
extern	vmCvar_t	g_triboltFuse;
extern	vmCvar_t	g_respawnHealth;
extern	vmCvar_t	g_respawnHealthLarge;
extern	vmCvar_t	g_respawnMega;
extern	vmCvar_t	g_respawnArmor;
extern	vmCvar_t	g_respawnAmmo;
extern	vmCvar_t	g_shardArmor;
extern	vmCvar_t	g_shardDrops;
extern	vmCvar_t	g_shardLife;
extern	vmCvar_t	g_powerupFirstSpawn;
extern	vmCvar_t	g_powerupWarning;
extern	vmCvar_t	g_powerupAlternate;
extern	vmCvar_t	g_hookSpeed;
extern	vmCvar_t	g_hookRange;
extern	vmCvar_t	g_hookDamage;
extern	vmCvar_t	g_hookSwingDamage;
extern	vmCvar_t	g_hookSwingSpeed;
extern	vmCvar_t	g_hookSwingRepeat;
extern	vmCvar_t	g_rushDamageScale;
extern	vmCvar_t	g_hwSpeed;
extern	vmCvar_t	g_hwDamage;
extern	vmCvar_t	g_hwDamageCap;
extern	vmCvar_t	g_hwSpeed2;
extern	vmCvar_t	g_hwDamage2;
extern	vmCvar_t	g_hwDamagePer100;
extern	vmCvar_t	g_hwKnockback;
extern	vmCvar_t	g_hookKnockback;
extern	vmCvar_t	g_warmupMax;
extern	vmCvar_t	g_blindPick;
extern	vmCvar_t	g_specChatToPlayers;
void G_SendDuelHud( void );
extern	vmCvar_t	g_teleportKeepSpeed;
extern	vmCvar_t	g_injectionMaxGain;
extern	vmCvar_t	g_injectionMaxGainCap;
extern	vmCvar_t	g_berserkHitCooldown;
void G_BreakHook( gentity_t *ent );
void G_UpdateItemTimers( void );
void G_TimeoutFrame( int msec );
void Cmd_Timeout_f( gentity_t *ent );
void Cmd_Timein_f( gentity_t *ent );
void G_LogAccuracyHit( gclient_t *client, int weapon );
void G_BounceMissile( gentity_t *ent, trace_t *trace );
void G_MissileImpact( gentity_t *ent, trace_t *trace );
extern	vmCvar_t	g_hookBreakDamage;
extern	vmCvar_t	g_totemBlastRadius;
extern	vmCvar_t	g_turretDeployTime;
extern	vmCvar_t	g_turretTurnRate;
extern	vmCvar_t	g_swarmKnockSelf;
extern	vmCvar_t	g_reinforcedSelfScale;
extern	vmCvar_t	g_collisionRepeat;
extern	vmCvar_t	g_knockback;
extern	vmCvar_t	g_quadfactor;
extern	vmCvar_t	g_forcerespawn;
extern	vmCvar_t	g_inactivity;
extern	vmCvar_t	g_maxSpectators;
extern	vmCvar_t	g_debugMove;
extern	vmCvar_t	g_debugAlloc;
extern	vmCvar_t	g_debugDamage;
extern	vmCvar_t	g_weaponRespawn;
extern	vmCvar_t	g_weaponTeamRespawn;
extern	vmCvar_t	g_synchronousClients;
extern	vmCvar_t	g_motd;
extern	vmCvar_t	g_warmup;
extern	vmCvar_t	g_doWarmup;
extern	vmCvar_t	g_blood;
extern	vmCvar_t	g_allowVote;
extern	vmCvar_t	g_teamAutoJoin;
extern	vmCvar_t	g_teamForceBalance;
extern	vmCvar_t	g_banIPs;
extern	vmCvar_t	g_filterBan;
extern	vmCvar_t	g_obeliskHealth;
extern	vmCvar_t	g_obeliskRegenPeriod;
extern	vmCvar_t	g_obeliskRegenAmount;
extern	vmCvar_t	g_obeliskRespawnDelay;
extern	vmCvar_t	g_cubeTimeout;
extern	vmCvar_t	g_redteam;
extern	vmCvar_t	g_blueteam;
extern	vmCvar_t	g_smoothClients;
extern	vmCvar_t	pmove_fixed;
extern	vmCvar_t	pmove_msec;
extern	vmCvar_t	g_rankings;
extern	vmCvar_t	g_enableDust;
extern	vmCvar_t	g_enableBreath;
extern	vmCvar_t	g_singlePlayer;
extern	vmCvar_t	g_proxMineTimeout;
extern	vmCvar_t	g_localTeamPref;

void	trap_Print( const char *text );
void	trap_Error( const char *text ) Q_NO_RETURN;
int		trap_Milliseconds( void );
int	trap_RealTime( qtime_t *qtime );
int		trap_Argc( void );
void	trap_Argv( int n, char *buffer, int bufferLength );
void	trap_Args( char *buffer, int bufferLength );
int		trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void	trap_FS_Read( void *buffer, int len, fileHandle_t f );
void	trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void	trap_FS_FCloseFile( fileHandle_t f );
int		trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
int		trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t
void	trap_SendConsoleCommand( int exec_when, const char *text );
void	trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags );
void	trap_Cvar_Update( vmCvar_t *cvar );
void	trap_Cvar_Set( const char *var_name, const char *value );
int		trap_Cvar_VariableIntegerValue( const char *var_name );
float	trap_Cvar_VariableValue( const char *var_name );
void	trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
void	trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *gameClients, int sizeofGameClient );
void	trap_DropClient( int clientNum, const char *reason );
void	trap_SendServerCommand( int clientNum, const char *text );
void	trap_SetConfigstring( int num, const char *string );
void	trap_GetConfigstring( int num, char *buffer, int bufferSize );
void	trap_GetUserinfo( int num, char *buffer, int bufferSize );
void	trap_SetUserinfo( int num, const char *buffer );
void	trap_GetServerinfo( char *buffer, int bufferSize );
void	trap_SetBrushModel( gentity_t *ent, const char *name );
void	trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
int		trap_PointContents( const vec3_t point, int passEntityNum );
qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 );
qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 );
void	trap_AdjustAreaPortalState( gentity_t *ent, qboolean open );
qboolean trap_AreasConnected( int area1, int area2 );
void	trap_LinkEntity( gentity_t *ent );
void	trap_UnlinkEntity( gentity_t *ent );
int		trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount );
qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );
int		trap_BotAllocateClient( void );
void	trap_BotFreeClient( int clientNum );
void	trap_GetUsercmd( int clientNum, usercmd_t *cmd );
qboolean	trap_GetEntityToken( char *buffer, int bufferSize );

int		trap_DebugPolygonCreate(int color, int numPoints, vec3_t *points);
void	trap_DebugPolygonDelete(int id);

int		trap_BotLibSetup( void );
int		trap_BotLibShutdown( void );
int		trap_BotLibVarSet(char *var_name, char *value);
int		trap_BotLibVarGet(char *var_name, char *value, int size);
int		trap_BotLibDefine(char *string);
int		trap_BotLibStartFrame(float time);
int		trap_BotLibLoadMap(const char *mapname);
int		trap_BotLibUpdateEntity(int ent, void /* struct bot_updateentity_s */ *bue);
int		trap_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);

int		trap_BotGetSnapshotEntity( int clientNum, int sequence );
int		trap_BotGetServerCommand(int clientNum, char *message, int size);
void	trap_BotUserCommand(int client, usercmd_t *ucmd);

int		trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas);
int		trap_AAS_AreaInfo( int areanum, void /* struct aas_areainfo_s */ *info );
void	trap_AAS_EntityInfo(int entnum, void /* struct aas_entityinfo_s */ *info);

int		trap_AAS_Initialized(void);
void	trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs);
float	trap_AAS_Time(void);

int		trap_AAS_PointAreaNum(vec3_t point);
int		trap_AAS_PointReachabilityAreaIndex(vec3_t point);
int		trap_AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas);

int		trap_AAS_PointContents(vec3_t point);
int		trap_AAS_NextBSPEntity(int ent);
int		trap_AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size);
int		trap_AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v);
int		trap_AAS_FloatForBSPEpairKey(int ent, char *key, float *value);
int		trap_AAS_IntForBSPEpairKey(int ent, char *key, int *value);

int		trap_AAS_AreaReachability(int areanum);

int		trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags);
int		trap_AAS_EnableRoutingArea( int areanum, int enable );
int		trap_AAS_PredictRoute(void /*struct aas_predictroute_s*/ *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum);

int		trap_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags,
										void /*struct aas_altroutegoal_s*/ *altroutegoals, int maxaltroutegoals,
										int type);
int		trap_AAS_Swimming(vec3_t origin);
int		trap_AAS_PredictClientMovement(void /* aas_clientmove_s */ *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize);


void	trap_EA_Say(int client, char *str);
void	trap_EA_SayTeam(int client, char *str);
void	trap_EA_Command(int client, char *command);

void	trap_EA_Action(int client, int action);
void	trap_EA_Gesture(int client);
void	trap_EA_Talk(int client);
void	trap_EA_Attack(int client);
void	trap_EA_Use(int client);
void	trap_EA_Respawn(int client);
void	trap_EA_Crouch(int client);
void	trap_EA_MoveUp(int client);
void	trap_EA_MoveDown(int client);
void	trap_EA_MoveForward(int client);
void	trap_EA_MoveBack(int client);
void	trap_EA_MoveLeft(int client);
void	trap_EA_MoveRight(int client);
void	trap_EA_SelectWeapon(int client, int weapon);
void	trap_EA_Jump(int client);
void	trap_EA_DelayedJump(int client);
void	trap_EA_Move(int client, vec3_t dir, float speed);
void	trap_EA_View(int client, vec3_t viewangles);

void	trap_EA_EndRegular(int client, float thinktime);
void	trap_EA_GetInput(int client, float thinktime, void /* struct bot_input_s */ *input);
void	trap_EA_ResetInput(int client);


int		trap_BotLoadCharacter(char *charfile, float skill);
void	trap_BotFreeCharacter(int character);
float	trap_Characteristic_Float(int character, int index);
float	trap_Characteristic_BFloat(int character, int index, float min, float max);
int		trap_Characteristic_Integer(int character, int index);
int		trap_Characteristic_BInteger(int character, int index, int min, int max);
void	trap_Characteristic_String(int character, int index, char *buf, int size);

int		trap_BotAllocChatState(void);
void	trap_BotFreeChatState(int handle);
void	trap_BotQueueConsoleMessage(int chatstate, int type, char *message);
void	trap_BotRemoveConsoleMessage(int chatstate, int handle);
int		trap_BotNextConsoleMessage(int chatstate, void /* struct bot_consolemessage_s */ *cm);
int		trap_BotNumConsoleMessages(int chatstate);
void	trap_BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 );
int		trap_BotNumInitialChats(int chatstate, char *type);
int		trap_BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 );
int		trap_BotChatLength(int chatstate);
void	trap_BotEnterChat(int chatstate, int client, int sendto);
void	trap_BotGetChatMessage(int chatstate, char *buf, int size);
int		trap_StringContains(char *str1, char *str2, int casesensitive);
int		trap_BotFindMatch(char *str, void /* struct bot_match_s */ *match, unsigned long int context);
void	trap_BotMatchVariable(void /* struct bot_match_s */ *match, int variable, char *buf, int size);
void	trap_UnifyWhiteSpaces(char *string);
void	trap_BotReplaceSynonyms(char *string, unsigned long int context);
int		trap_BotLoadChatFile(int chatstate, char *chatfile, char *chatname);
void	trap_BotSetChatGender(int chatstate, int gender);
void	trap_BotSetChatName(int chatstate, char *name, int client);
void	trap_BotResetGoalState(int goalstate);
void	trap_BotRemoveFromAvoidGoals(int goalstate, int number);
void	trap_BotResetAvoidGoals(int goalstate);
void	trap_BotPushGoal(int goalstate, void /* struct bot_goal_s */ *goal);
void	trap_BotPopGoal(int goalstate);
void	trap_BotEmptyGoalStack(int goalstate);
void	trap_BotDumpAvoidGoals(int goalstate);
void	trap_BotDumpGoalStack(int goalstate);
void	trap_BotGoalName(int number, char *name, int size);
int		trap_BotGetTopGoal(int goalstate, void /* struct bot_goal_s */ *goal);
int		trap_BotGetSecondGoal(int goalstate, void /* struct bot_goal_s */ *goal);
int		trap_BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags);
int		trap_BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags, void /* struct bot_goal_s */ *ltg, float maxtime);
int		trap_BotTouchingGoal(vec3_t origin, void /* struct bot_goal_s */ *goal);
int		trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void /* struct bot_goal_s */ *goal);
int		trap_BotGetNextCampSpotGoal(int num, void /* struct bot_goal_s */ *goal);
int		trap_BotGetMapLocationGoal(char *name, void /* struct bot_goal_s */ *goal);
int		trap_BotGetLevelItemGoal(int index, char *classname, void /* struct bot_goal_s */ *goal);
float	trap_BotAvoidGoalTime(int goalstate, int number);
void	trap_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime);
void	trap_BotInitLevelItems(void);
void	trap_BotUpdateEntityItems(void);
int		trap_BotLoadItemWeights(int goalstate, char *filename);
void	trap_BotFreeItemWeights(int goalstate);
void	trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child);
void	trap_BotSaveGoalFuzzyLogic(int goalstate, char *filename);
void	trap_BotMutateGoalFuzzyLogic(int goalstate, float range);
int		trap_BotAllocGoalState(int state);
void	trap_BotFreeGoalState(int handle);

void	trap_BotResetMoveState(int movestate);
void	trap_BotMoveToGoal(void /* struct bot_moveresult_s */ *result, int movestate, void /* struct bot_goal_s */ *goal, int travelflags);
int		trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type);
void	trap_BotResetAvoidReach(int movestate);
void	trap_BotResetLastAvoidReach(int movestate);
int		trap_BotReachabilityArea(vec3_t origin, int testground);
int		trap_BotMovementViewTarget(int movestate, void /* struct bot_goal_s */ *goal, int travelflags, float lookahead, vec3_t target);
int		trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void /* struct bot_goal_s */ *goal, int travelflags, vec3_t target);
int		trap_BotAllocMoveState(void);
void	trap_BotFreeMoveState(int handle);
void	trap_BotInitMoveState(int handle, void /* struct bot_initmove_s */ *initmove);
void	trap_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type);

int		trap_BotChooseBestFightWeapon(int weaponstate, int *inventory);
void	trap_BotGetWeaponInfo(int weaponstate, int weapon, void /* struct weaponinfo_s */ *weaponinfo);
int		trap_BotLoadWeaponWeights(int weaponstate, char *filename);
int		trap_BotAllocWeaponState(void);
void	trap_BotFreeWeaponState(int weaponstate);
void	trap_BotResetWeaponState(int weaponstate);

int		trap_GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child);

void	trap_SnapVector( float *v );

