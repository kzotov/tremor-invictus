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
// bg_public.h -- definitions shared by both the server game and client game modules

// because games can change separately from the main system version, we need a
// second version that must match between game and cgame

#define	GAME_VERSION		BASEGAME "-1"

#define	DEFAULT_GRAVITY		800
#define	GIB_HEALTH			-40
#define	ARMOR_PROTECTION	0.66

#define	MAX_ITEMS			256

#define	RANK_TIED_FLAG		0x4000

#define DEFAULT_SHOTGUN_SPREAD	700
#define DEFAULT_SHOTGUN_COUNT	11

#define	ITEM_RADIUS			15		// item sizes are needed for client side pickup detection

#define	LIGHTNING_RANGE		768

#define	SCORE_NOT_PRESENT	-9999	// for the CS_SCORES[12] when only one player is present

#define	VOTE_TIME			30000	// 30 seconds before vote times out
#define	VOTE_CALL_GAP		30000	// one call per player per 30 seconds

#define PLAYER_WIDTH		18		// medium body; see bg_classInfo
#define	MINS_Z				-24
#define DEFAULT_HEIGHT		45		// 69 units standing
#define	MODEL_HEIGHT		56		// standing height the player models are built for
#define	DEFAULT_VIEWHEIGHT	38		// eye height of a player model scaled to the hull
#define CROUCH_HEIGHT		10.5f	// half the standing height
#define CROUCH_VIEWHEIGHT	19		// half the standing eye height
#define DEAD_HEIGHT			-8
#define	DEAD_VIEWHEIGHT		-16
#define INVUL_RADIUS		42

//
// config strings are a general means of communicating variable length strings
// from the server to all connected clients.
//

// CS_SERVERINFO and CS_SYSTEMINFO are defined in q_shared.h
#define	CS_MUSIC				2
#define	CS_MESSAGE				3		// from the map worldspawn's message field
#define	CS_MOTD					4		// g_motd string for server message of the day
#define	CS_WARMUP				5		// server time when the match will be restarted
#define	CS_SCORES1				6
#define	CS_SCORES2				7
#define CS_VOTE_TIME			8
#define CS_VOTE_STRING			9
#define	CS_VOTE_YES				10
#define	CS_VOTE_NO				11

#define CS_TEAMVOTE_TIME		12
#define CS_TEAMVOTE_STRING		14
#define	CS_TEAMVOTE_YES			16
#define	CS_TEAMVOTE_NO			18

#define	CS_GAME_VERSION			20
#define	CS_LEVEL_START_TIME		21		// so the timer only shows the current level
#define	CS_INTERMISSION			22		// when 1, fraglimit/timelimit has been hit and intermission will start in a second or two
#define CS_FLAGSTATUS			23		// string indicating flag status in CTF
#define CS_SHADERSTATE			24
#define CS_BOTINFO				25
#define CS_SACRIFICE			26		// "red blue soulState carrier extract meterTime": sacrifice HUD

#define	CS_ITEMS				27		// string of 0's and 1's that tell which items are present
#define	CS_ITEM_TIMERS			28		// "item respawnTime x y z ..." for major items, time 0 = up
#define	CS_TIMEOUT				29		// level.time a timeout ends, 0 when none
#define	CS_MAPVOTE				31		// "endTime map1 map2 map3 votes1 votes2 votes3", end-of-match map vote
#define	CS_MAPAUTHORS			30		// "\a1\<author>\a2\<author2>" from the map's worldspawn

// medals: the arena set beyond Quake III's own awards (excellent, impressive,
// humiliation, defend, assist, capture), sent to the earner as "medal <id> <count>"
typedef enum {
	MEDAL_FIRSTBLOOD,
	MEDAL_MIDAIR,
	MEDAL_AIRBORNE,
	MEDAL_COMBOKILL,
	MEDAL_COLLATERAL,
	MEDAL_DOUBLEWHAMMY,
	MEDAL_DENIED,
	MEDAL_REVENGE,
	MEDAL_PARTYBREAKER,
	MEDAL_PERFORATED,
	MEDAL_POINTBLANK,
	MEDAL_PRECISE,
	MEDAL_RINGOUT,
	MEDAL_STOPWATCH,
	MEDAL_TANK,
	MEDAL_DIEHARD,
	MEDAL_FROMTHEGRAVE,
	MEDAL_KAMIKAZE,
	MEDAL_SHOWSTOPPER,
	MEDAL_TRIPLEKILL,
	MEDAL_OVERKILL,
	MEDAL_FIGHTER,
	MEDAL_CRUSHER,
	MEDAL_EXTERMINATOR,
	MEDAL_DEMOLISHER,
	MEDAL_GRIMREAPER,
	MEDAL_AVATAR,
	MEDAL_SPREE,
	MEDAL_RAGE,
	MEDAL_FRENZY,
	MEDAL_RAMPAGE,
	MEDAL_DOMINATING,
	MEDAL_PUMASSACRE,
	MEDAL_GOD_GAUNTLET,
	MEDAL_GOD_MG,
	MEDAL_GOD_SG,
	MEDAL_GOD_NG,
	MEDAL_GOD_TB,
	MEDAL_GOD_RL,
	MEDAL_GOD_LG,
	MEDAL_GOD_RG,
	MEDAL_DAMAGEDEALER,
	MEDAL_NETMASTER,
	MEDAL_ASSISTANT,
	MEDAL_NUM
} medal_t;

typedef struct {
	int			name;			// string table
	const char	*key;			// sound/feedback/<key>.wav when the install has it, icon gfx/tremor/medals/<key>
} medalInfo_t;

extern const medalInfo_t	bg_medalInfo[MEDAL_NUM];

// loading-screen tips: general, for a mode, or for a champion
typedef enum {
	TIP_GENERAL,
	TIP_MODE,
	TIP_CHAMPION
} tipArea_t;

typedef struct {
	int			area;			// tipArea_t
	const char	*key;			// mode or champion name
	int			text;			// string table
} tipInfo_t;

extern const tipInfo_t	bg_tips[];
extern const int		bg_numTips;

// entityState generic1 of ability entities: which look to draw
typedef enum {
	AV_NONE,
	AV_SHOCKZONE,		// Wrack's Shell Shock zone
	AV_SPINE,			// Klesk's spines
	AV_SUPPLYDROP,		// Sarge's crate
	AV_STORM,			// Raine's Downpour
	AV_HOUND,			// the Beastmaster's hound
	AV_ANCHOR,			// Unumrey's Rift Anchor
	AV_ORB,				// the dire orb
	AV_FIREBALL,		// Flame Strike's fireballs
	AV_ACID,			// Acid Spit's blob
	AV_TRAIL,			// a piece of the plasma trail; otherEntityNum is its owner
	AV_NUM
} abilityVisual_t;

#define	CS_MODELS				32
#define	CS_SOUNDS				(CS_MODELS+MAX_MODELS)
#define	CS_PLAYERS				(CS_SOUNDS+MAX_SOUNDS)
#define CS_LOCATIONS			(CS_PLAYERS+MAX_CLIENTS)
#define CS_PARTICLES			(CS_LOCATIONS+MAX_LOCATIONS) 

#define CS_READYUP				(CS_PARTICLES+MAX_LOCATIONS)	// warm-up: "readyUp share ready players needed startTime"

#define CS_LOBBY				(CS_READYUP+1)	// the pre-match lobby (g_mapvote.c)

#define CS_MAX					(CS_LOBBY+1)

#if (CS_MAX) > MAX_CONFIGSTRINGS
#error overflow: (CS_MAX) > MAX_CONFIGSTRINGS
#endif

typedef enum {
	GT_FFA,				// free for all
	GT_TOURNAMENT,		// one on one tournament
	GT_SINGLE_PLAYER,	// single player ffa

	//-- team games go after this --

	GT_TEAM,			// team deathmatch
	GT_CTF,				// capture the flag
	GT_1FCTF,
	GT_OBELISK,
	GT_HARVESTER,
	GT_MAX_GAME_TYPE
} gametype_t;

typedef enum { GENDER_MALE, GENDER_FEMALE, GENDER_NEUTER } gender_t;

/*
===================================================================================

PMOVE MODULE

The pmove code takes a player_state_t and a usercmd_t and generates a new player_state_t
and some other output data.  Used for local prediction on the client game and true
movement on the server game.
===================================================================================
*/

typedef enum {
	PM_NORMAL,		// can accelerate and turn
	PM_NOCLIP,		// noclip movement
	PM_SPECTATOR,	// still run into walls
	PM_DEAD,		// no acceleration or turning, but free falling
	PM_FREEZE,		// stuck in place with no control
	PM_INTERMISSION,	// no movement or status bar
	PM_SPINTERMISSION	// no movement or status bar
} pmtype_t;

typedef enum {
	WEAPON_READY, 
	WEAPON_RAISING,
	WEAPON_DROPPING,
	WEAPON_FIRING
} weaponstate_t;

// pmove->pm_flags
#define	PMF_DUCKED			1
#define	PMF_JUMP_HELD		2
#define	PMF_JUMP_DOWN		4		// jump button was down on the previous frame
#define	PMF_BACKWARDS_JUMP	8		// go into backwards land
#define	PMF_BACKWARDS_RUN	16		// coast down to backwards run
#define	PMF_TIME_LAND		32		// pm_time is time before rejump
#define	PMF_TIME_KNOCKBACK	64		// pm_time is an air-accelerate only time
#define	PMF_TIME_WATERJUMP	256		// pm_time is waterjump
#define	PMF_RESPAWNED		512		// clear after attack and jump buttons come up
#define	PMF_USE_ITEM_HELD	1024
#define PMF_GRAPPLE_PULL	2048	// pull towards grapple location
#define PMF_FOLLOW			4096	// spectate following another player
#define PMF_SCOREBOARD		8192	// spectate as a scoreboard
#define PMF_INVULEXPAND		16384	// invulnerability sphere set to full size
#define	PMF_AIRMOVE_USED	128		// wall jump, double jump or air-dodge used this airtime
#define	PMF_ABILITY_HELD	32768	// ability button was down on the previous frame

#define	PMF_ALL_TIMES	(PMF_TIME_WATERJUMP|PMF_TIME_LAND|PMF_TIME_KNOCKBACK)

#define	MAXTOUCH	32
typedef struct {
	// state (in / out)
	playerState_t	*ps;

	// command (in)
	usercmd_t	cmd;
	int			tracemask;			// collide against these types of surfaces
	int			debugLevel;			// if set, diagnostic output will be printed
	qboolean	noFootsteps;		// if the game is setup for no footsteps by the server
	qboolean	gauntletHit;		// true if a gauntlet attack would actually hit something

	int			framecount;

	// results (out)
	int			numtouch;
	int			touchents[MAXTOUCH];

	vec3_t		mins, maxs;			// bounding box size

	int			watertype;
	int			waterlevel;

	float		xyspeed;

	// for fixed msec Pmove
	int			pmove_fixed;
	int			pmove_msec;

	qboolean	autoHop;			// holding jump jumps again on landing
	qboolean	movementOnly;		// warm-up: only movement abilities (hook, rush, orb, rift)

	// callbacks to test the world
	// these will be different functions during game and cgame
	void		(*trace)( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask );
	int			(*pointcontents)( const vec3_t point, int passEntityNum );
} pmove_t;

// if a full pmove isn't done on the client, you can just update the angles
void PM_UpdateViewAngles( playerState_t *ps, const usercmd_t *cmd );
void Pmove (pmove_t *pmove);

//===================================================================================

// user-facing text goes through BG_String so other languages can be added;
// only English exists for now

typedef enum {
	STR_NONE,

	STR_CHAMPION_RANGER,
	STR_CHAMPION_ANARKI,
	STR_CHAMPION_ATHENA,
	STR_CHAMPION_NYX,
	STR_CHAMPION_SLASH,
	STR_CHAMPION_BJ,
	STR_CHAMPION_DEATHKNIGHT,
	STR_CHAMPION_DOOMSLAYER,
	STR_CHAMPION_EISEN,
	STR_CHAMPION_GALENA,
	STR_CHAMPION_STROGG,
	STR_CHAMPION_VISOR,
	STR_CHAMPION_CLUTCH,
	STR_CHAMPION_KEEL,
	STR_CHAMPION_SCALEBEARER,
	STR_CHAMPION_SORLAG,

	STR_CLASS_LIGHT,
	STR_CLASS_MEDIUM,
	STR_CLASS_HEAVY,

	STR_ABILITY_HOOK,
	STR_ABILITY_BULLRUSH,
	STR_ABILITY_INJECTION,
	STR_ABILITY_DUALWIELD,
	STR_ABILITY_BARRIER,
	STR_ABILITY_FLAMESTRIKE,
	STR_ABILITY_BERSERK,
	STR_ABILITY_TURRET,
	STR_ABILITY_TOTEM,
	STR_ABILITY_GRENADES,
	STR_ABILITY_GHOSTWALK,
	STR_ABILITY_ORB,
	STR_ABILITY_PLASMATRAIL,
	STR_ABILITY_ACIDSPIT,
	STR_ABILITY_DRONE,
	STR_ABILITY_PIERCINGSIGHT,
	STR_ABILITY_RECHARGE,			// "next in %s s"

	STR_MENU_CHAMPION,
	STR_MENU_CHAMPION_TITLE,
	STR_MENU_CHAMPION_NEXT_SPAWN,
	STR_MENU_BACK,
	STR_MENU_COLORS,
	STR_MENU_COLORS_TITLE,
	STR_MENU_ENEMY,
	STR_MENU_TEAM,
	STR_MENU_SKIN,
	STR_MENU_HEAD,
	STR_MENU_UPPER,
	STR_MENU_LOWER,
	STR_MENU_SAMEALL,
	STR_SKIN_OFF,
	STR_SKIN_BRIGHT,
	STR_SKIN_SPORT,
	STR_COLOR_GREEN,
	STR_COLOR_CYAN,
	STR_COLOR_RED,
	STR_COLOR_BLUE,
	STR_COLOR_YELLOW,
	STR_COLOR_ORANGE,
	STR_COLOR_MAGENTA,
	STR_COLOR_PURPLE,
	STR_COLOR_PINK,
	STR_COLOR_WHITE,
	STR_COLOR_LIME,
	STR_COLOR_TEAL,

	STR_POWERUP_WARNING,			// "%s in %i seconds"
	STR_OBIT_HEAVYWEIGHT,
	STR_OBIT_ORB,
	STR_OBIT_FIREBALL,
	STR_OBIT_BURN,
	STR_OBIT_ACID,
	STR_OBIT_TOTEM,
	STR_OBIT_TURRET,
	STR_OBIT_LASER,
	STR_OBIT_PLASMATRAIL,
	STR_OBIT_DRONE,

	STR_DUEL_READY,					// "%s is ready"
	STR_DUEL_NOT_READY,				// "%s is not ready"
	STR_DUEL_TYPE_READYUP,			// warmup hint for players not yet ready
	STR_DUEL_OVERTIME,				// "Overtime: %i minutes"
	STR_DUEL_SUDDEN_DEATH,
	STR_DUEL_ABORTED,
	STR_DUEL_CHAMPION_LOCKED,
	STR_CTF_HALF_TIME,				// "Half time: red %i, blue %i"
	STR_CTF_SECOND_HALF,
	STR_CA_ROUND,					// "Round %i"
	STR_CA_FIGHT,
	STR_CA_RED_WINS,
	STR_CA_BLUE_WINS,
	STR_CA_DRAW,
	STR_SAC_SOUL_SOON,				// "The soul appears in %i seconds"
	STR_SAC_SOUL_SPAWNED,
	STR_SAC_PLANTED,				// "%s planted the soul"
	STR_SAC_EXTRACTED,				// "%s extracted the soul"
	STR_SAC_DROPPED,
	STR_SAC_RETURNED,
	STR_SAC_RED_WINS,
	STR_SAC_BLUE_WINS,
	STR_SAC_SUDDEN_DEATH,
	STR_SAC_HUD,					// "Red %i%%   Blue %i%%"
	STR_SAC_SOUL_AT_SPAWN,
	STR_SAC_SOUL_CARRIED,			// "Soul: %s"
	STR_SAC_SOUL_GROUND,
	STR_SAC_SOUL_RED,
	STR_SAC_SOUL_BLUE,
	STR_SAC_EXTRACTING,				// "Extracting %i%%"
	STR_RD_YOUR_PICK,				// "Your pick: type pick <champion> (%i s)"
	STR_RD_PICKED,					// "%s picks %s"
	STR_RD_ROUND,					// "Round %i"
	STR_RD_WINS_ROUND,				// "%s wins the round"
	STR_RD_SUDDEN_DEATH,
	STR_RD_NEXT,					// "Next champion: type next <1-3>"

	STR_MENU_PLAY,
	STR_MENU_CHAMPIONS,
	STR_MENU_PLAYER_COLORS,
	STR_MENU_START_WEAPON,			// "STARTER: %s" (MG, SG or NG)
	STR_MENU_SETTINGS,
	STR_MENU_DEMOS,
	STR_MENU_QUIT,
	STR_MENU_QUIT_CONFIRM,
	STR_MENU_NAME,
	STR_MENU_FOOTER,
	STR_MENU_PRACTICE,
	STR_MENU_FIND_SERVERS,
	STR_MENU_CREATE_SERVER,
	STR_MENU_RESUME,
	STR_MENU_JOIN_TEAM,
	STR_MENU_VOTE,
	STR_MENU_ADD_BOTS,
	STR_MENU_REMOVE_BOTS,
	STR_MENU_LEAVE,
	STR_MODE_DUEL,
	STR_MODE_TDM,
	STR_MODE_CTF,
	STR_MODE_CA,
	STR_MODE_SACRIFICE,
	STR_MODE_ROUNDDUEL,
	STR_MODE_FFA,
	STR_MODE_INSTAGIB,
	STR_MODE_TRINITY,
	STR_LOADING,
	STR_MV_TITLE,
	STR_MV_HELP,
	STR_MV_ENDS,
	STR_MV_VOTES,
	STR_VOTE_WAIT,
	STR_VOTE_LIST,
	STR_VOTE_NOMAP,
	STR_VOTE_TEAMSONLY,
	STR_VOTE_MODES,
	STR_VOTE_TEAMFULL,
	STR_VOTE_HEADS,
	STR_VOTE_TAILS,
	STR_VOTE_NUMBER,
	STR_VOTE_LINE,
	STR_VOTE_ESC,
	STR_VOTE_KICKPLAYERS,
	STR_VOTE_KICKLIMIT,
	STR_ADMIN_MUTED,
	STR_ADMIN_UNMUTED,
	STR_ADMIN_BANNED,
	STR_ADMIN_NORIGHTS,
	STR_ADMIN_LOCKED,
	STR_MENU_TEAM_ORDERS,
	STR_IDLE_MOVED,
	STR_IDLE_WARNING,
	STR_SPECTATORS_FULL,
	STR_AWARD_ACCURACY,
	STR_AWARD_DAMAGE,
	STR_AWARD_NET,
	STR_AWARD_PERFECT,
	STR_AWARD_WEAPON,
	STR_SET_CONTRAST,
	STR_SET_CONTRAST_HELP,
	STR_SET_HITBEEP,
	STR_SET_HITBEEP_HELP,
	STR_SET_KILLBEEP,
	STR_SET_KILLBEEP_HELP,
	STR_SET_DMGNUM,
	STR_SET_DMGNUM_HELP,
	STR_SET_ITEMTIMERS,
	STR_SET_ITEMTIMERS_HELP,
	STR_SET_DMGARC,
	STR_SET_DMGARC_HELP,
	STR_SET_LOWHEALTH,
	STR_SET_LOWHEALTH_HELP,
	STR_SET_ZOOMTOGGLE,
	STR_SET_ZOOMTOGGLE_HELP,
	STR_SET_HOLD,
	STR_SET_TOGGLE,
	STR_SET_TIPS,
	STR_SET_TIPS_HELP,
	STR_SET_ANNOUNCER,
	STR_SET_ANNOUNCER_HELP,
	STR_SET_CONFIGPRESET,
	STR_SET_CONFIGPRESET_HELP,
	STR_SET_CUSTOM,
	STR_SET_COMPETITIVE,
	STR_SET_CASUAL,
	STR_SET_DEFAULTPRESET,
	STR_SET_GORE,
	STR_SET_GORE_HELP,
	STR_SET_RAILRINGS,
	STR_SET_RAILRINGS_HELP,
	STR_BIO_RANGER,
	STR_BIO_ANARKI,
	STR_BIO_ATHENA,
	STR_BIO_NYX,
	STR_BIO_SLASH,
	STR_BIO_BJ,
	STR_BIO_DEATHKNIGHT,
	STR_BIO_DOOMSLAYER,
	STR_BIO_EISEN,
	STR_BIO_GALENA,
	STR_BIO_STROGG,
	STR_BIO_VISOR,
	STR_BIO_CLUTCH,
	STR_BIO_KEEL,
	STR_BIO_SCALEBEARER,
	STR_BIO_SORLAG,
	STR_BIO_WRACK,
	STR_BIO_SARGE,
	STR_BIO_KLESK,
	STR_BIO_HUNTER,
	STR_BIO_XAERO,
	STR_BIO_RAINE,
	STR_BIO_BEASTMASTER,
	STR_BIO_UNUMREY,
	STR_TIP,
	STR_TIP_GEN_1,
	STR_TIP_GEN_2,
	STR_TIP_GEN_3,
	STR_TIP_GEN_4,
	STR_TIP_GEN_5,
	STR_TIP_GEN_6,
	STR_TIP_GEN_7,
	STR_TIP_GEN_8,
	STR_TIP_GEN_9,
	STR_TIP_GEN_10,
	STR_TIP_GEN_11,
	STR_TIP_GEN_12,
	STR_TIP_GEN_13,
	STR_TIP_MODE_DUEL_1,
	STR_TIP_MODE_DUEL_2,
	STR_TIP_MODE_TDM_1,
	STR_TIP_MODE_TDM_2,
	STR_TIP_MODE_CTF_1,
	STR_TIP_MODE_CTF_2,
	STR_TIP_MODE_CA_1,
	STR_TIP_MODE_CA_2,
	STR_TIP_MODE_SACRIFICE_1,
	STR_TIP_MODE_SACRIFICE_2,
	STR_TIP_MODE_ROUNDDUEL_1,
	STR_TIP_MODE_ROUNDDUEL_2,
	STR_TIP_CH_RANGER_1,
	STR_TIP_CH_ANARKI_1,
	STR_TIP_CH_ATHENA_1,
	STR_TIP_CH_ATHENA_2,
	STR_TIP_CH_NYX_1,
	STR_TIP_CH_NYX_2,
	STR_TIP_CH_SLASH_1,
	STR_TIP_CH_SLASH_2,
	STR_TIP_CH_BJ_1,
	STR_TIP_CH_DEATHKNIGHT_1,
	STR_TIP_CH_DOOMSLAYER_1,
	STR_TIP_CH_DOOMSLAYER_2,
	STR_TIP_CH_EISEN_1,
	STR_TIP_CH_GALENA_1,
	STR_TIP_CH_STROGG_1,
	STR_TIP_CH_VISOR_1,
	STR_TIP_CH_CLUTCH_1,
	STR_TIP_CH_CLUTCH_2,
	STR_TIP_CH_KEEL_1,
	STR_TIP_CH_SCALEBEARER_1,
	STR_TIP_CH_SORLAG_1,
	STR_TIP_CH_SORLAG_2,
	STR_TIP_CH_WRACK_1,
	STR_TIP_CH_SARGE_1,
	STR_TIP_CH_KLESK_1,
	STR_TIP_CH_HUNTER_1,
	STR_TIP_CH_XAERO_1,
	STR_TIP_CH_RAINE_1,
	STR_TIP_CH_BEASTMASTER_1,
	STR_TIP_CH_UNUMREY_1,
	STR_MEDAL_FIRSTBLOOD,
	STR_MEDAL_MIDAIR,
	STR_MEDAL_AIRBORNE,
	STR_MEDAL_COMBOKILL,
	STR_MEDAL_COLLATERAL,
	STR_MEDAL_DOUBLEWHAMMY,
	STR_MEDAL_DENIED,
	STR_MEDAL_REVENGE,
	STR_MEDAL_PARTYBREAKER,
	STR_MEDAL_PERFORATED,
	STR_MEDAL_POINTBLANK,
	STR_MEDAL_PRECISE,
	STR_MEDAL_RINGOUT,
	STR_MEDAL_STOPWATCH,
	STR_MEDAL_TANK,
	STR_MEDAL_DIEHARD,
	STR_MEDAL_FROMTHEGRAVE,
	STR_MEDAL_KAMIKAZE,
	STR_MEDAL_SHOWSTOPPER,
	STR_MEDAL_TRIPLEKILL,
	STR_MEDAL_OVERKILL,
	STR_MEDAL_FIGHTER,
	STR_MEDAL_CRUSHER,
	STR_MEDAL_EXTERMINATOR,
	STR_MEDAL_DEMOLISHER,
	STR_MEDAL_GRIMREAPER,
	STR_MEDAL_AVATAR,
	STR_MEDAL_SPREE,
	STR_MEDAL_RAGE,
	STR_MEDAL_FRENZY,
	STR_MEDAL_RAMPAGE,
	STR_MEDAL_DOMINATING,
	STR_MEDAL_PUMASSACRE,
	STR_MEDAL_GOD_GAUNTLET,
	STR_MEDAL_GOD_MG,
	STR_MEDAL_GOD_SG,
	STR_MEDAL_GOD_NG,
	STR_MEDAL_GOD_TB,
	STR_MEDAL_GOD_RL,
	STR_MEDAL_GOD_LG,
	STR_MEDAL_GOD_RG,
	STR_MEDAL_DAMAGEDEALER,
	STR_MEDAL_NETMASTER,
	STR_MEDAL_ASSISTANT,
	STR_DEMO_PAUSED,
	STR_DEMO_HELP,
	STR_DEMO_REC,
	STR_CHAMPION_WRACK,
	STR_CHAMPION_KLESK,
	STR_CHAMPION_XAERO,
	STR_CHAMPION_SARGE,
	STR_CHAMPION_HUNTER,
	STR_CHAMPION_RAINE,
	STR_CHAMPION_BEASTMASTER,
	STR_CHAMPION_UNUMREY,
	STR_ABILITY_SHELLSHOCK,
	STR_ABILITY_SPINEVOLLEY,
	STR_ABILITY_MASTEROFDEATH,
	STR_ABILITY_SUPPLYDROP,
	STR_ABILITY_MARKPREY,
	STR_ABILITY_DOWNPOUR,
	STR_ABILITY_HOUND,
	STR_ABILITY_RIFTANCHOR,
	STR_OBIT_SPINES,
	STR_OBIT_STORM,
	STR_OBIT_HOUND,
	STR_SET_TAB_VIDEO,
	STR_SET_TAB_MOUSE,
	STR_SET_TAB_CONTROLS,
	STR_SET_TAB_GAME,
	STR_SET_TAB_HUD,
	STR_SET_TAB_SOUND,
	STR_SET_DEFAULTS,
	STR_SET_APPLY,
	STR_SET_CREDITS,
	STR_SET_BACK,
	STR_SET_RESTART_NOTE,
	STR_SET_OFF,
	STR_SET_ON,
	STR_SET_UNLIMITED,
	STR_SET_DESKTOP,
	STR_SET_RENDERER,
	STR_SET_RENDERER_HELP,
	STR_SET_RESOLUTION,
	STR_SET_RESOLUTION_HELP,
	STR_SET_DISPLAY,
	STR_SET_DISPLAY_HELP,
	STR_SET_FULLSCREEN,
	STR_SET_WINDOWED,
	STR_SET_BORDERLESS,
	STR_SET_VSYNC,
	STR_SET_VSYNC_HELP,
	STR_SET_FPSCAP,
	STR_SET_FPSCAP_HELP,
	STR_SET_FOV,
	STR_SET_FOV_HELP,
	STR_SET_ZOOMFOV,
	STR_SET_ZOOMFOV_HELP,
	STR_SET_BRIGHTNESS,
	STR_SET_BRIGHTNESS_HELP,
	STR_SET_PICMIP,
	STR_SET_PICMIP_HELP,
	STR_SET_HIGH,
	STR_SET_MEDIUM,
	STR_SET_LOW,
	STR_SET_LOWEST,
	STR_SET_TEXFILTER,
	STR_SET_TEXFILTER_HELP,
	STR_SET_BILINEAR,
	STR_SET_TRILINEAR,
	STR_SET_SENS,
	STR_SET_SENS_HELP,
	STR_SET_CPI,
	STR_SET_CPI_HELP,
	STR_SET_TURN,
	STR_SET_TURN_HELP,
	STR_SET_TURN_VALUE,
	STR_SET_TURN_NEEDCPI,
	STR_SET_RAWMOUSE,
	STR_SET_RAWMOUSE_HELP,
	STR_SET_ACCEL,
	STR_SET_ACCEL_HELP,
	STR_SET_ACCELOFFSET,
	STR_SET_ACCELOFFSET_HELP,
	STR_SET_ACCELPOWER,
	STR_SET_ACCELPOWER_HELP,
	STR_SET_ACCELCAP,
	STR_SET_ACCELCAP_HELP,
	STR_SET_INVERT,
	STR_SET_INVERT_HELP,
	STR_SET_SMOOTH,
	STR_SET_SMOOTH_HELP,
	STR_SET_BIND_HELP,
	STR_SET_BIND_WAIT,
	STR_SET_BIND_FORWARD,
	STR_SET_BIND_BACK,
	STR_SET_BIND_LEFT,
	STR_SET_BIND_RIGHT,
	STR_SET_BIND_JUMP,
	STR_SET_BIND_CROUCH,
	STR_SET_BIND_WALK,
	STR_SET_BIND_ATTACK,
	STR_SET_BIND_ZOOM,
	STR_SET_BIND_NEXTWEAP,
	STR_SET_BIND_PREVWEAP,
	STR_SET_BIND_SLOT,
	STR_SET_BIND_ABILITY,
	STR_SET_BIND_CHAT,
	STR_SET_BIND_TEAMCHAT,
	STR_SET_BIND_VOICE,
	STR_SET_BIND_FOLLOWNEXT,
	STR_SET_BIND_FOLLOWPREV,
	STR_SET_BIND_SCORES,
	STR_SET_BIND_SCREENSHOT,
	STR_SET_BIND_CONSOLE,
	STR_SET_BIND_CHATHISTORY,
	STR_SET_AUTOSWITCH,
	STR_SET_AUTOSWITCH_HELP,
	STR_SET_AUTOHOP,
	STR_SET_AUTOHOP_HELP,
	STR_SET_CROSSHAIR,
	STR_SET_CROSSHAIR_HELP,
	STR_SET_CROSSHAIRSIZE,
	STR_SET_CROSSHAIRSIZE_HELP,
	STR_SET_ARC,
	STR_SET_ARC_HELP,
	STR_SET_TOP,
	STR_SET_BOTTOM,
	STR_SET_LEFTSIDE,
	STR_SET_RIGHTSIDE,
	STR_SET_WEAPONBAR,
	STR_SET_WEAPONBAR_HELP,
	STR_SET_WB_COLUMN,
	STR_SET_WB_CENTRE,
	STR_SET_WB_HELD,
	STR_SET_WB_HIDDEN,
	STR_SET_MEDALS,
	STR_SET_MEDALS_HELP,
	STR_SET_SOUNDONLY,
	STR_SET_FRAGLINE,
	STR_SET_FRAGLINE_HELP,
	STR_SET_EVENTLINE,
	STR_SET_EVENTLINE_HELP,
	STR_SET_PICKUPS,
	STR_SET_PICKUPS_HELP,
	STR_SET_SYSMSG,
	STR_SET_SYSMSG_HELP,
	STR_SET_CHATLINES,
	STR_SET_CHATLINES_HELP,
	STR_SET_SPECTIMERS,
	STR_SET_SPECTIMERS_HELP,
	STR_SET_ST_POWERUPS,
	STR_SET_ST_MEGA,
	STR_SET_ST_HEAVY,
	STR_SET_ST_LIGHT,
	STR_SET_ABILRING,
	STR_SET_ABILRING_HELP,
	STR_SET_PASSRING,
	STR_SET_PASSRING_HELP,
	STR_SET_SPEED,
	STR_SET_SPEED_HELP,
	STR_SET_CROSSHAIRPOS,
	STR_SET_VITALS,
	STR_SET_HUDSTYLE,
	STR_SET_HUDSTYLE_HELP,
	STR_SET_HUD_CLASSIC,
	STR_SET_HUD_NEW,
	STR_SET_KILLFEED,
	STR_SET_KILLFEED_HELP,
	STR_SET_TEAMOVERLAY,
	STR_SET_TEAMOVERLAY_HELP,
	STR_SET_FPS,
	STR_SET_FPS_HELP,
	STR_SET_LAGOMETER,
	STR_SET_LAGOMETER_HELP,
	STR_SET_TIMENUDGE,
	STR_SET_TIMENUDGE_HELP,
	STR_SET_PROJNUDGE,
	STR_SET_PROJNUDGE_HELP,
	STR_SET_AUTOREC,
	STR_SET_AUTOREC_HELP,
	STR_SET_AUTOREC_DEMO,
	STR_SET_AUTOREC_BOTH,
	STR_SET_VOLUME,
	STR_SET_VOLUME_HELP,
	STR_SET_MUSIC,
	STR_SET_MUSIC_HELP,
	STR_SET_VOIP,
	STR_SET_VOIP_HELP,
	STR_SET_METAL,
	STR_SET_OPENGL1,
	STR_SET_OPENGL2,
	STR_ABILDESC_HOOK,
	STR_ABILDESC_BULLRUSH,
	STR_ABILDESC_INJECTION,
	STR_ABILDESC_DUALWIELD,
	STR_ABILDESC_BARRIER,
	STR_ABILDESC_FLAMESTRIKE,
	STR_ABILDESC_BERSERK,
	STR_ABILDESC_TURRET,
	STR_ABILDESC_TOTEM,
	STR_ABILDESC_GRENADES,
	STR_ABILDESC_GHOSTWALK,
	STR_ABILDESC_ORB,
	STR_ABILDESC_PLASMATRAIL,
	STR_ABILDESC_ACIDSPIT,
	STR_ABILDESC_DRONE,
	STR_ABILDESC_PIERCINGSIGHT,
	STR_ABILDESC_SHELLSHOCK,
	STR_ABILDESC_SPINEVOLLEY,
	STR_ABILDESC_MASTEROFDEATH,
	STR_ABILDESC_SUPPLYDROP,
	STR_ABILDESC_MARKPREY,
	STR_ABILDESC_DOWNPOUR,
	STR_ABILDESC_HOUND,
	STR_ABILDESC_RIFTANCHOR,
	STR_PASSIVE_REGENERATION,
	STR_PASSIVE_HASTE,
	STR_PASSIVE_SPLASHRESIST,
	STR_PASSIVE_FIREPROOF,
	STR_PASSIVE_ACIDPROOF,
	STR_PASSIVE_ARMORCOOLDOWN,
	STR_PASSIVE_HEALTHCOOLDOWN,
	STR_PASSIVE_AMMOCOOLDOWN,
	STR_PASSIVE_FULLSPRINT,
	STR_PASSIVE_CHARRING,
	STR_PASSIVE_STROYENT,
	STR_PASSIVE_DECORATEDAIRMAN,
	STR_PASSIVE_HIVEREGEN,
	STR_PASSIVE_LEAPINGKICK,
	STR_PASSIVE_DRILLSERGEANT,
	STR_PASSIVE_LIGHTFEET,
	STR_PASSIVE_SLIPSTREAM,
	STR_PASSIVE_THICKHIDE,
	STR_PASSIVE_ECHO,
	STR_PASSIVE_ARMORREGEN,
	STR_PASSIVE_SLOWDECAY,
	STR_CH_HEALTH,
	STR_CH_ARMOR,
	STR_CH_SPEED,
	STR_CH_MOVEMENT,
	STR_CH_ABILITY,
	STR_CH_PASSIVE,
	STR_CH_WEAPON,
	STR_CH_SELECT,
	STR_CH_CURRENT,
	STR_CH_EVERY,
	STR_CH_EVERY1,
	STR_CH_STATS,
	STR_CH_MOVE_STANDARD,
	STR_CH_MOVE_AIRCONTROL,
	STR_CH_MOVE_SLIDE,
	STR_CH_MOVE_WALLJUMP,
	STR_CH_MOVE_DOUBLEJUMP,
	STR_CH_MOVE_DODGE,
	STR_CH_MOVE_ACCEL,
	STR_CH_MOVE_HEAVY,
	STR_CH_MOVE_RAMP,
	STR_CH_MOVE_BUNNY,
	STR_CH_MOVE_EASYSTRAFE,
	STR_CH_MOVE_ISOAIR,
	STR_CH_WEAPON_MG,
	STR_CH_WEAPON_SG,
	STR_CH_WEAPON_NG,
	STR_PR_MODE,
	STR_PR_OPPONENTS,
	STR_PR_TEAMMATES,
	STR_PR_SKILL,
	STR_PR_TIMELIMIT,
	STR_PR_FRAGLIMIT,
	STR_PR_HALF,
	STR_PR_CAPLIMIT,
	STR_PR_ROUNDS,
	STR_PR_START,
	STR_PR_PAGE,
	STR_PR_NOMAPS,
	STR_PR_OFF_HINT,
	STR_DM_TITLE,
	STR_DM_PLAY,
	STR_DM_RENAME,
	STR_DM_DELETE,
	STR_DM_OK,
	STR_DM_DATE,
	STR_DM_MAP,
	STR_DM_MODE,
	STR_DM_LENGTH,
	STR_DM_PLAYERS,
	STR_DM_NONE,
	STR_DM_OLD,
	STR_DM_DELETE_ASK,
	STR_DM_NEWNAME,
	STR_DM_RENAME_HINT,
	STR_DM_COUNT,
	STR_CR_TITLE,
	STR_CR_GAME,
	STR_CR_MADEBY,
	STR_CR_SITE,
	STR_CR_ENGINE,
	STR_CR_IDCOPY,
	STR_CR_GPL1,
	STR_CR_GPL2,
	STR_CR_FONT,
	STR_CR_THIRDPARTY,
	STR_CR_LIB1,
	STR_CR_LIB2,
	STR_CR_LIB3,
	STR_CR_LIB4,
	STR_CR_IOQ3,
	STR_CR_NEXT,
	STR_CR_LAST,
	STR_SB_SCORE,
	STR_SB_DEATHS,
	STR_SB_DAMAGE,
	STR_SB_PING,
	STR_SB_CAPS,
	STR_SB_ASSISTS,
	STR_SB_DEFENDS,
	STR_PR_RANDOM,
	STR_PR_BOTCHAMP,
	STR_SB_NAME,
	STR_SB_PLANTS,
	STR_SB_EXTRACTS,
	STR_SB_ROUNDS,
	STR_HUD_NEXT_SPAWN,
	STR_WARMUP,
	STR_WARMUP_READY,
	STR_WARMUP_WAITING,
	STR_WARMUP_STARTS,
	STR_WARMUP_ANYWAY,
	STR_DEATH_KILLEDBY,
	STR_DEATH_LEFT,
	STR_DEATH_DAMAGE,
	STR_DEATH_RESPAWN_IN,
	STR_DEATH_RESPAWN,
	STR_SET_TAB_CHAMPIONS,
	STR_SET_TRAILGREY,
	STR_SET_TRAILGREY_HELP,
	STR_SET_TRAILCOLOR,
	STR_SET_TRAILCOLOR_HELP,
	STR_SET_TRAILENEMY,
	STR_SET_TRAILENEMY_HELP,
	STR_SET_ABILLIGHT,
	STR_SET_ABILLIGHT_HELP,
	STR_SET_BACKFLIP,
	STR_SET_BACKFLIP_HELP,
	STR_SET_TEAMCOLOR,
	STR_SET_PINK2,
	STR_SET_VIOLET,
	STR_SET_OUTLINE,
	STR_SET_OUTLINE_HELP,
	STR_SET_OUTLINECOLOR,
	STR_SET_OUTLINECOLOR_HELP,
	STR_SET_COLORBLIND,
	STR_SET_COLORBLIND_HELP,
	STR_SET_DEUTERANOPIA,
	STR_SET_PROTANOPIA,
	STR_SET_TRITANOPIA,
	STR_ACC_TITLE,
	STR_TO_CALLED,
	STR_TO_OVER,
	STR_TO_NOTNOW,
	STR_TO_USED,
	STR_TO_ONLYMODES,
	STR_TO_LEFT,
	STR_DB_DAMAGE,
	STR_DB_TAKEN,
	STR_DB_MEGA,
	STR_DB_HEAVY,
	STR_DB_LIGHT,
	STR_DB_ACCURACY,
	STR_HUD_SLIDE,
	STR_FS_ALL,
	STR_FS_TITLE,
	STR_FS_SOURCE,
	STR_FS_INTERNET,
	STR_FS_LAN,
	STR_FS_FAVORITES,
	STR_FS_MODE,
	STR_FS_SEARCH,
	STR_FS_HIDEEMPTY,
	STR_FS_HIDEFULL,
	STR_FS_HIDEPASS,
	STR_FS_MAXPING,
	STR_FS_ANY,
	STR_FS_SERVER,
	STR_FS_MAP,
	STR_FS_PLAYERS,
	STR_FS_PING,
	STR_FS_REFRESH,
	STR_FS_CONNECT,
	STR_FS_ADDFAV,
	STR_FS_REMOVEFAV,
	STR_FS_SCANNING,
	STR_FS_COUNT,
	STR_FS_NOMASTER,
	STR_FS_NONE,
	STR_SET_RETINA,
	STR_SET_RETINA_HELP,
	STR_SET_ZOOMSENS,
	STR_SET_ZOOMSENS_HELP,
	STR_SET_AUTO,
	STR_SET_XHCOLOR,
	STR_SET_XHCOLOR_HELP,
	STR_SET_WHITE,
	STR_SET_RED,
	STR_SET_GREEN,
	STR_SET_YELLOW,
	STR_SET_BLUE,
	STR_SET_CYAN,
	STR_SET_PINK,
	STR_SET_ORANGE,
	STR_SET_SPEEDO,
	STR_SET_SPEEDO_HELP,
	STR_SET_VOICEVOL,
	STR_SET_VOICEVOL_HELP,
	STR_SET_MIC,
	STR_SET_MIC_HELP,
	STR_SET_MICGAIN,
	STR_SET_MICGAIN_HELP,
	STR_SET_OPENMIC,
	STR_SET_OPENMIC_HELP,
	STR_SET_DEFAULTDEV,
	STR_SCORE_MUTE,
	STR_SCORE_UNMUTE,
	STR_LINEUP_READY,
	STR_RECR_WRACK,
	STR_RECR_XAERO,
	STR_RECR_HUNTER,
	STR_RECR_KLESK,
	STR_RECR_SARGE,
	STR_RECR_RAINE,
	STR_RECR_BEASTMASTER,
	STR_RECR_UNUMREY,
	STR_RECR_DISCUSS,
	STR_MSG_FRAGGED,
	STR_MSG_PLACE,
	STR_MSG_YOUFRAGGED,
	STR_MSG_PLACE_ONE,				// "%s place with 1 frag"
	STR_HUD_RED,
	STR_HUD_BLUE,
	STR_HUD_TIED,
	STR_EV_TAKEN_LEAD,				// event line, cgame only
	STR_EV_TIED_LEAD,
	STR_EV_LOST_LEAD,
	STR_EV_5_MINUTES,
	STR_EV_1_MINUTE,
	STR_EV_QUAD_UP,
	STR_EV_TAKEN_LEAD_OTHER,		// "%s^7 HAS TAKEN THE LEAD": the followed player
	STR_EV_TIED_LEAD_OTHER,
	STR_EV_LOST_LEAD_OTHER,
	STR_EV_QUAD_UP_OTHER,
	STR_EV_OBELISK_ATTACK,
	STR_CHAT_TEAM,					// "(TEAM)"
	STR_CHAT_SPEC,
	STR_DEATH_BIND_HINT,				// "Bind the weapon keys in Controls"
	STR_BD_PLAYER,
	STR_BD_SCORE,
	STR_BD_DEATHS,
	STR_BD_KD,
	STR_BD_DAMAGE,
	STR_BD_ACC,
	STR_BD_PING,
	STR_BD_CAPS,
	STR_BD_ASSISTS,
	STR_BD_DEFENDS,
	STR_BD_PLANTS,
	STR_BD_EXTRACTS,
	STR_BD_BOT,
	STR_BD_LEAD,
	STR_BD_TRAIL,
	STR_BD_TIED,
	STR_BD_ROUND,
	STR_BD_SPECTATORS,
	STR_BD_TEAM_DAMAGE,
	STR_JOIN_RED,					// "%s^7 joined the red team"
	STR_JOIN_BLUE,
	STR_JOIN_SPECTATORS,
	STR_JOIN_GAME,
	STR_SYS_CONNECTED,
	STR_SYS_ENTERED,
	STR_SYS_RENAMED,
	STR_SYS_LEFT,
	STR_SYS_TIMEDOUT,
	STR_SYS_KICKED,
	STR_SYS_FORFEIT,
	STR_SYS_TIMELIMIT,
	STR_SYS_FRAGLIMIT,
	STR_SYS_RED_FRAGLIMIT,
	STR_SYS_BLUE_FRAGLIMIT,
	STR_SYS_RED_CAPLIMIT,
	STR_SYS_BLUE_CAPLIMIT,
	STR_SYS_VOTE_PASSED,
	STR_SYS_VOTE_FAILED,
	STR_SYS_TEAMVOTE_PASSED,
	STR_SYS_TEAMVOTE_FAILED,
	STR_SYS_SHUFFLED,
	STR_SYS_TEAMSWITCH_WAIT,
	STR_SYS_VOTE_OFF,
	STR_SYS_VOTE_RUNNING,
	STR_SYS_KICK_HOST,
	STR_SYS_NEXTMAP_UNSET,
	STR_SYS_CVAR_CHANGED,
	STR_SYS_NOITEM,
	STR_SYS_USEITEM,
	STR_SYS_VOTE_CALLED,
	STR_SYS_RED_FULL,
	STR_SYS_BLUE_FULL,
	STR_SYS_TEAMVOTE_CALLED,
	STR_EV_RED_FLAG_TAKEN,			// event line, priority 2: first of the objectives
	STR_EV_BLUE_FLAG_TAKEN,
	STR_EV_RED_FLAG_DROPPED,
	STR_EV_BLUE_FLAG_DROPPED,
	STR_EV_RED_FLAG_RETURNED,
	STR_EV_BLUE_FLAG_RETURNED,
	STR_EV_RED_FLAG_HOME,
	STR_EV_BLUE_FLAG_HOME,
	STR_EV_RED_FLAG_CAPTURED,
	STR_EV_BLUE_FLAG_CAPTURED,		// last of the objectives
	STR_EV_QUAD_SOON,				// event line, priority 3: first of the power-ups
	STR_EV_PROTECTION_SOON,
	STR_EV_QUAD_SPAWN,
	STR_EV_PROTECTION_SPAWN,
	STR_EV_HAS_QUAD,
	STR_EV_HAS_PROTECTION,			// last of the power-ups
	STR_SYS_FIGHT,				// keep last of the system-message range

	STR_NUM_STRINGS
} stringId_t;

const char	*BG_String( int id );

// champions

typedef enum {
	CLASS_LIGHT,
	CLASS_MEDIUM,
	CLASS_HEAVY,

	CLASS_NUM_CLASSES
} championClass_t;

typedef enum {
	MOVE_STANDARD,					// plain VQ3
	MOVE_AIRCONTROL,				// strafe turning and forward-key air control
	MOVE_SLIDE,						// crouch slide charged by falling

	MOVE_NUM_STYLES
} moveStyle_t;

// passive movement abilities on top of the style, champion_t.abilities bits
#define	MOVEABIL_WALLJUMP		1
#define	MOVEABIL_DOUBLEJUMP		2
#define	MOVEABIL_AIRDODGE		4	// double-tap a direction in the air
#define	MOVEABIL_ACCELERATION	8	// running straight raises run speed
#define	MOVEABIL_HEAVYWEIGHT	16	// own speed becomes collision damage
#define	MOVEABIL_RAMPJUMP		32	// jumping off a climb goes higher
#define	MOVEABIL_BUNNYHOP		64	// each jump adds a share of the speed, up to a cap
#define	MOVEABIL_EASYSTRAFE		128	// extra air acceleration up to a speed
#define	MOVEABIL_ISOAIR			256	// steers in the air in every direction; hard speed clamp in air

// active abilities, on BUTTON_ABILITY
typedef enum {
	ACTIVE_NONE,
	ACTIVE_HOOK,					// grappling hook with charges
	ACTIVE_BULLRUSH,				// forward charge
	ACTIVE_INJECTION,				// heal and a speed boost
	ACTIVE_DUALWIELD,				// faster firing
	ACTIVE_BARRIER,					// damage shield, then a laser
	ACTIVE_FLAMESTRIKE,				// three fireballs
	ACTIVE_BERSERK,					// fists only, fast and hard-hitting
	ACTIVE_TURRET,					// placed sentry
	ACTIVE_TOTEM,					// thrown heal / trap totem
	ACTIVE_GRENADES,				// stored grenades
	ACTIVE_GHOSTWALK,				// invisible and untouchable
	ACTIVE_ORB,						// thrown orb, teleport on re-press
	ACTIVE_PLASMATRAIL,				// damaging trail, detonate on re-press
	ACTIVE_ACIDSPIT,				// acid blobs and pools
	ACTIVE_DRONE,					// remote drone
	ACTIVE_PIERCINGSIGHT,			// see enemies through walls
	ACTIVE_SHELLSHOCK,				// zone that cancels and blocks enemy abilities
	ACTIVE_SPINEVOLLEY,				// fan of slowing spines
	ACTIVE_MASTEROFDEATH,			// below 50 health: cheat death for a few seconds
	ACTIVE_SUPPLYDROP,				// crate of armour and ammo for the team
	ACTIVE_MARKPREY,				// dart that marks an enemy
	ACTIVE_DOWNPOUR,				// storm column at a point
	ACTIVE_HOUND,					// a hound that hunts enemies
	ACTIVE_RIFTANCHOR,				// anchor now, recall on re-press

	ACTIVE_NUM_ABILITIES
} activeAbility_t;

// per-ability timing, all cvars named ab_<name>_<param>, sent to clients
typedef enum {
	AP_CHARGES,						// charges when full
	AP_RECHARGE,					// msec to recharge one charge
	AP_DELAY,						// msec before a second press works (the effect itself is at once)
	AP_DURATION,					// msec the ability stays active, 0 = instant
	AP_LOCKOUT,						// msec the weapon is disabled

	AP_NUM_PARAMS
} abilityParam_t;

typedef struct {
	char		*cvarName;			// ab_<cvarName>_<param>
	int			nameString;			// stringId_t
	int			lockoutAtEnd;		// qtrue: weapon lockout when it ends, else when it starts
	char		*defaults[AP_NUM_PARAMS];
} abilityInfo_t;

extern	const abilityInfo_t	bg_abilityInfo[ACTIVE_NUM_ABILITIES];

float	BG_AbilityParam( int active, int param );

// weapon numbers, all cvars named wp_<name>_<param>; the ones prediction
// needs (refire, pellets, spread) are sent to clients
typedef enum {
	WPP_DAMAGE,						// per hit, bullet, pellet or tick
	WPP_REFIRE,						// msec between shots
	WPP_RANGE,						// instant-hit reach
	WPP_SPREAD,						// sine of the largest angle off the aim line
	WPP_PELLETS,					// shotgun pellets per shot
	WPP_KNOCK,						// knockback on others standing
	WPP_KNOCKAIR,					// knockback on others in the air
	WPP_KNOCKSELF,					// knockback on yourself
	WPP_SPLASH,						// splash damage at the centre
	WPP_SPLASHMIN,					// splash damage at the edge
	WPP_RADIUS,						// splash radius
	WPP_SPEED,						// projectile speed
	WPP_AMMOSTART,					// ammo with the weapon
	WPP_AMMOMAX,					// ammo cap

	WPP_NUM_PARAMS
} weaponParam_t;

typedef struct {
	int			weapon;				// weapon_t
	char		*cvarName;			// wp_<cvarName>_<param>
	char		*defaults[WPP_NUM_PARAMS];
} weaponNumbers_t;

extern	const weaponNumbers_t	bg_weaponInfo[];
extern	const int			bg_numWeaponInfo;

float	BG_WeaponParam( int weapon, int param );
qboolean BG_WeaponTuned( int weapon );
void	BG_ShotgunPelletOffset( int weapon, int index, int count, int *seed, float *right, float *up );

// passive champion traits, champion_t.passives bits
#define	PASSIVE_REGENERATION	1		// refill to the top of the 25 HP block
#define	PASSIVE_HASTE			2		// Ranger: +10% run speed for 3 s after each teleport
#define	PASSIVE_SPLASHRESIST	4		// less splash damage
#define	PASSIVE_FIREPROOF		8		// immune to fire and lava
#define	PASSIVE_ACIDPROOF		16		// immune to acid and slime
#define	PASSIVE_ARMORCOOLDOWN	32		// armor pickups cut the cooldown
#define	PASSIVE_HEALTHCOOLDOWN	64		// health pickups cut the cooldown
#define	PASSIVE_AMMOCOOLDOWN	128		// ammo pickups cut the cooldown
#define	PASSIVE_FULLSPRINT		256		// higher air acceleration
#define	PASSIVE_CHARRING		512		// melee hits set the target burning
#define	PASSIVE_STROYENT		1024	// kills drop cells that heal this champion
#define	PASSIVE_DECORATEDAIRMAN	2048	// names Wrack's isotropic air control (MOVEABIL_ISOAIR); no effect of its own
#define	PASSIVE_HIVEREGEN		4096	// regains health to base after a quiet while
#define	PASSIVE_LEAPINGKICK		8192	// Xaero: medals bank kicks (a leap through enemies)
#define	PASSIVE_DRILLSERGEANT	16384	// faster weapon switch
#define	PASSIVE_LIGHTFEET		32768	// no falling damage, silent steps and landings
#define	PASSIVE_SLIPSTREAM		65536	// more air acceleration
#define	PASSIVE_THICKHIDE		131072	// less splash damage from others
#define	PASSIVE_ECHO			262144	// part of each hit heals back
#define	PASSIVE_ARMORREGEN		524288	// armour regrows to a floor after a quiet while
#define	PASSIVE_SLOWDECAY		1048576	// health over max falls at half speed
#define	NUM_PASSIVES			21

typedef struct {
	char		*name;				// userinfo "champion" value
	int			bodyClass;			// championClass_t
	int			runSpeed;
	int			jumpSpeedCap;		// horizontal speed cap (acceleration and air decay), 0 = none
	int			moveStyle;			// moveStyle_t
	int			abilities;			// MOVEABIL_* bits
	int			active;				// activeAbility_t
	int			passives;			// PASSIVE_* bits
	char		*model;				// player model, "name" or "name/skin"
	int			nameString;			// stringId_t of the display name
} champion_t;

extern	const champion_t	bg_championList[];
extern	const int			bg_numChampions;
typedef struct {
	float		halfWidth;
	int			baseArmor;			// armor above this counts down
	int			maxHealthBonus;		// mega health may go this far over max health
	int			maxArmor;			// armor pickups stop here
	int			spawnArmor;
	int			spawnArmorDuel;
} classInfo_t;

extern	const classInfo_t	bg_classInfo[CLASS_NUM_CLASSES];

#define	DEFAULT_CHAMPION	"ranger"

int					BG_ChampionForName( const char *name );
void				BG_ChampionModel( int champion, char *out, int size );
#define	MAX_CHAMPIONS		32
int					BG_ChampionForModel( const char *model );
const champion_t	*BG_Champion( int index );
qboolean			BG_MovementAbility( int active );

// movement tunables, shared with the client through CVAR_SYSTEMINFO cvars
typedef struct {
	float	jumpVelocity;
	float	accelerate;
	float	airAccelerate;
	float	friction;
	float	stopSpeed;
	float	crouchSpeed;
	float	speedLimit;				// non-zero: the champions' speed caps apply
	float	jumpSilentTime;

	float	acFriction;				// air-control style
	float	acStrafeAccel;
	float	acStrafeSpeed;
	float	acBrakeAccel;
	float	acAirControl;
	float	hopAccel;
	float	hopMaxSpeed;
	float	easyStrafeAccel;
	float	easyStrafeMaxSpeed;
	float	isoAirAccel;			// air acceleration scale
	float	isoAirControl;			// turn strength, any direction
	float	isoAirClamp;			// how fast speed over the cap bleeds off in air (per second)

	float	chainTime;				// msec after a jump that a new jump goes higher
	float	chainVelocity;			// air-control style: added upward speed
	float	stepJumpVelocity;		// added to a jump made just after a step up
	float	stepJumpTime;			// msec after a step up that still counts
	float	chainScale;				// others: up to this share more, fading over the chain time
	float	crouchJumpScale;		// a jump from a crouch (sliders jump fully)
	float	crouchWalkScale;		// crouch walking, times run speed (sliders: pm_slideCrouchScale)
	float	airSpeedDecay;			// u/s each second lost above the speed cap in the air
	float	airSpeedDecayWrack;
	float	airSpeedDecayClutch;
	float	circleFriction;			// air-control style, forward and strafe held on the ground

	float	slideFriction;
	float	slideFrictionStrogg;
	float	slideStopSpeed;
	float	slideChargeRate;
	float	slideChargeMax;
	float	slideDrain;
	float	slideAirGrace;			// msec in the air that don't end a slide or count as a landing
	float	slideChargeMaxStrogg;	// Strogg's shorter slide
	float	slideAirGraceStrogg;
	float	slideCrouchScale;		// crouch walking without charge, times run speed

	float	wallJumpVelocity;
	float	wallJumpReach;			// how close the wall must be
	float	wallJumpMaxSpeed;		// horizontal speed a wall jump can't go past
	float	wallJumpPush;
	float	wallJumpNormal;

	float	doubleJumpVelocity;

	float	dodgeTapWindow;
	float	dodgeSpeed;
	float	dodgeUp;				// share of the dash speed added upward (air)
	float	dodgeGroundSpeed;
	float	dodgeGroundUp;

	float	accelAngle;				// degrees between aim and velocity still "straight"

	float	rushSpeed;				// top speed of the rush
	float	rushTurnRate;			// degrees per second
	float	rushStartSpeed;			// bull rush speed at the start
	float	rushAccel;				// speed gained per second, up to rushSpeed
	float	rushMinSpeed;			// the rush ends when forward speed drops below this
	float	rushGravity;			// gravity scale during the rush

	float	hookImpulse;
	float	hookPullStart;
	float	hookPullAccel;
	float	hookPullMax;
	float	hookDeadZone;
	float	hookSwingMax;

	float	rampJumpScale;
	float	rampJumpMax;

	float	airAccelLight;			// class air acceleration, scaled by airAccelerate
	float	airAccelMedium;
	float	airAccelHeavy;
	float	airAccelFullSprint;

	float	injectionSpeed;			// run speed multiplier
	float	dualWieldRate;
	float	dualWieldRateHeavy;		// tri-bolt and rocket launcher
	float	trailSpeed;				// Slash's run speed while laying the trail			// fire rate multiplier
	float	berserkSpeed;			// run speed while berserk
	float	berserkLunge;			// Berserk jump strike speed toward the aim

	float	hmgZoomRefire;			// heavy machinegun refire while zoomed
	float	droneSpeed;				// drone flying speed
	float	droneSize;				// drone half-size
	float	droneFireInterval;		// msec between drone shots
} pmoveSettings_t;

extern	pmoveSettings_t	bg_pmoveSettings;

void	BG_RegisterPmoveCvars( int flags );
void	BG_UpdatePmoveCvars( void );

//===================================================================================


// player_state->stats[] indexes
// NOTE: may not have more than 16
typedef enum {
	STAT_HEALTH,
	STAT_HOLDABLE_ITEM,
#ifdef MISSIONPACK
	STAT_PERSISTANT_POWERUP,
#endif
	STAT_WEAPONS,					// 16 bit fields
	STAT_ARMOR,				
	STAT_DEAD_YAW,					// look this direction when dead (FIXME: get rid of?)
	STAT_CLIENTS_READY,				// bit mask of clients wishing to exit the intermission (FIXME: configstring?)
	STAT_MAX_HEALTH,				// health / armor limit, changeable by handicap
	STAT_CHAMPION,					// index into bg_championList
	STAT_MOVE_STATE,				// per-champion movement state, see below; slide: BG_SLIDE_* packing
	STAT_JUMP_TIME,					// msec since the last jump, capped
	STAT_LAND_TIME,					// msec since the last landing, capped
	STAT_TAPS,						// air-dodge double-tap tracking, TAP_* fields
	STAT_ABILITY,					// active ability: charges | state << ABILITY_STATE_SHIFT
	STAT_ABILITY_TIME,				// msec, meaning depends on the ability state
	STAT_ABILITY_RECHARGE			// tenths of a second until the next charge, 0 when full
} statIndex_t;

// Xaero's Leaping Kick in STAT_TAPS (free for champions without air-dodge):
// kicks banked in bits 0-1, air control left in 100 ms units in bits 2-7,
// bit 8 while the kick passes through players
#define	STAT_KICK				STAT_TAPS
#define	KICK_COUNT( ps )		( (ps)->stats[STAT_KICK] & 3 )
#define	KICK_AIR( ps )			( ( (ps)->stats[STAT_KICK] >> 2 ) & 63 )
#define	KICK_PASSING			256
#define	KICK_MAX				3

// crouch slide state in STAT_MOVE_STATE (stats travel as 16 bits): the charge
// in 8 ms units in the low 10 bits, time in the air in 64 ms units above
#define BG_SLIDE_CHARGE_UNIT	8
#define BG_SLIDE_AIR_UNIT		64
#define BG_SLIDE_CHARGE_BITS	10
#define BG_SlideCharge( ps )	( ( (ps)->stats[STAT_MOVE_STATE] & ( ( 1 << BG_SLIDE_CHARGE_BITS ) - 1 ) ) * BG_SLIDE_CHARGE_UNIT )
#define BG_SlideAirTime( ps )	( ( ( (ps)->stats[STAT_MOVE_STATE] >> BG_SLIDE_CHARGE_BITS ) & 31 ) * BG_SLIDE_AIR_UNIT )
#define BG_SlideState( charge, air )	( ( (charge) / BG_SLIDE_CHARGE_UNIT ) | ( ( (air) / BG_SLIDE_AIR_UNIT ) << BG_SLIDE_CHARGE_BITS ) )


// STAT_MOVE_STATE holds one champion-specific value:
//   slide style		msec of slide charge left
//   acceleration		run speed bonus in tenths of u/s (low 11 bits) and
//						time held at the current step in ACCEL_HOLD_UNIT ms
//   ramp jump			upward speed climbed on the last ground frame (u/s),
//						or the locked rope length while swinging on the hook

#define	ACCEL_BONUS_MASK	0x07ff
#define	ACCEL_HOLD_SHIFT	11
#define	ACCEL_HOLD_UNIT		125

// STAT_TAPS fields; read through & 0xffff, the network sign-extends bit 15
#define	TAP_HELD_MASK		0x000f	// movement directions held last frame
#define	TAP_DIR_SHIFT		4		// last tapped direction + 1, 3 bits
#define	TAP_DIR_MASK		0x0070
#define	TAP_TIME_SHIFT		7		// msec since that tap, 9 bits
#define	TAP_TIME_MAX		511

// STAT_ABILITY
#define	ABILITY_CHARGES_MASK	0x000f
#define	ABILITY_STATE_SHIFT		4

typedef enum {
	ABILITY_IDLE,
	ABILITY_WINDUP,					// STAT_ABILITY_TIME counts down to the throw
	ABILITY_HOOK_FLYING,			// hook thrown, not attached yet
	ABILITY_HOOK_PULL,				// attached; STAT_ABILITY_TIME counts up
	ABILITY_RUSH,					// STAT_ABILITY_TIME counts down to the end
	ABILITY_ACTIVE					// generic timed ability; STAT_ABILITY_TIME counts down
} abilityState_t;

#define	BG_AbilityCharges( ps )		( (ps)->stats[STAT_ABILITY] & ABILITY_CHARGES_MASK )
#define	BG_AbilityState( ps )		( ( (ps)->stats[STAT_ABILITY] & 0xffff ) >> ABILITY_STATE_SHIFT )
void	BG_SetAbility( playerState_t *ps, int state, int charges );

#define	STAT_TIME_CAP		30000	// timer stats stop counting here (16 bit)


// player_state->persistant[] indexes
// these fields are the only part of player_state that isn't
// cleared on respawn
// NOTE: may not have more than 16
typedef enum {
	PERS_SCORE,						// !!! MUST NOT CHANGE, SERVER AND GAME BOTH REFERENCE !!!
	PERS_HITS,						// total points damage inflicted so damage beeps can sound on change
	PERS_RANK,						// player rank or team rank
	PERS_TEAM,						// player team
	PERS_SPAWN_COUNT,				// incremented every respawn
	PERS_PLAYEREVENTS,				// 16 bits that can be flipped for events
	PERS_ATTACKER,					// clientnum of last damage inflicter
	PERS_ATTACKEE_ARMOR,			// health/armor of last person we attacked
	PERS_KILLED,					// count of the number of times you died
	// player awards tracking
	PERS_IMPRESSIVE_COUNT,			// two railgun hits in a row
	PERS_EXCELLENT_COUNT,			// two successive kills in a short amount of time
	PERS_DEFEND_COUNT,				// defend awards
	PERS_ASSIST_COUNT,				// assist awards
	PERS_GAUNTLET_FRAG_COUNT,		// kills with the guantlet
	PERS_CAPTURES,					// captures
	PERS_STEP_TIME					// msec since the last step up, capped (step jump boost)
} persEnum_t;


// entityState_t->eFlags
#define	EF_DEAD				0x00000001		// don't draw a foe marker over players with EF_DEAD
#ifdef MISSIONPACK
#define EF_TICKING			0x00000002		// used to make players play the prox mine ticking sound
#endif
#define	EF_TELEPORT_BIT		0x00000004		// toggled every time the origin abruptly changes
#define	EF_AWARD_EXCELLENT	0x00000008		// draw an excellent sprite
#define EF_PLAYER_EVENT		0x00000010
#define	EF_BOUNCE			0x00000010		// for missiles
#define	EF_BOUNCE_HALF		0x00000020		// for missiles
#define	EF_AWARD_GAUNTLET	0x00000040		// draw a gauntlet sprite
#define	EF_NODRAW			0x00000080		// may have an event, but no model (unspawned items)
#define	EF_FIRING			0x00000100		// for lightning gun
#define	EF_KAMIKAZE			0x00000200
#define	EF_MOVER_STOP		0x00000400		// will push otherwise
#define EF_AWARD_CAP		0x00000800		// draw the capture sprite
#define	EF_TALK				0x00001000		// draw a talk balloon
#define	EF_CONNECTION		0x00002000		// draw a connection trouble sprite
#define	EF_VOTED			0x00004000		// already cast a vote
#define	EF_AWARD_IMPRESSIVE	0x00008000		// draw an impressive sprite
#define	EF_AWARD_DEFEND		0x00010000		// draw a defend sprite
#define	EF_AWARD_ASSIST		0x00020000		// draw a assist sprite
#define EF_AWARD_DENIED		0x00040000		// denied
#define EF_TEAMVOTED		0x00080000		// already cast a team vote

// player-only reuses of missile and mover bits (the playerstate sends 16 bits)
#define	EF_BARRIER			EF_BOUNCE_HALF	// shield up around the player
#define	EF_DRONE			EF_MOVER_STOP	// the player is flying the drone
#define	EF_MARKED			0x00000002		// marked or caught in a storm: shown through walls to enemies (the base game leaves this bit free)

// shell-shocked until this time (a powerup slot the base game doesn't use)
#define	PW_SHELLSHOCK		PW_SCOUT

// NOTE: may not have more than 16
typedef enum {
	PW_NONE,

	PW_QUAD,
	PW_BATTLESUIT,
	PW_HASTE,
	PW_INVIS,
	PW_REGEN,
	PW_FLIGHT,

	PW_REDFLAG,
	PW_BLUEFLAG,
	PW_NEUTRALFLAG,

	PW_SCOUT,
	PW_GUARD,
	PW_DOUBLER,
	PW_AMMOREGEN,
	PW_INVULNERABILITY,

	PW_NUM_POWERUPS

} powerup_t;

typedef enum {
	HI_NONE,

	HI_TELEPORTER,
	HI_MEDKIT,
	HI_KAMIKAZE,
	HI_PORTAL,
	HI_INVULNERABILITY,

	HI_NUM_HOLDABLE
} holdable_t;


typedef enum {
	WP_NONE,

	WP_GAUNTLET,
	WP_MACHINEGUN,
	WP_SHOTGUN,
	WP_GRENADE_LAUNCHER,
	WP_ROCKET_LAUNCHER,
	WP_LIGHTNING,
	WP_RAILGUN,
	WP_PLASMAGUN,
	WP_BFG,
	WP_GRAPPLING_HOOK,
#ifdef MISSIONPACK
	WP_NAILGUN,
	WP_PROX_LAUNCHER,
	WP_CHAINGUN,
#endif

	WP_HEAVY_MACHINEGUN,			// shares the machinegun's slot and ammo
	WP_SUPER_SHOTGUN,				// shares the shotgun's slot and ammo
#ifndef MISSIONPACK
	WP_NAILGUN,						// a starting weapon
#endif

	WP_NUM_WEAPONS
} weapon_t;

// classic weapon slots given new jobs
#define	WP_TRIBOLT			WP_GRENADE_LAUNCHER	// three sticky bolts per trigger pull
#define	WP_SUPER_NAILGUN	WP_PLASMAGUN		// shares the nailgun's slot and ammo

int		BG_AmmoPartner( int weapon );
int		BG_WeaponVisual( int weapon );
int		BG_WeaponStarter( int weapon );
int		BG_WeaponSlot( int weapon );
qboolean BG_StartingWeapon( int weapon );


// reward sounds (stored in ps->persistant[PERS_PLAYEREVENTS])
#define	PLAYEREVENT_DENIEDREWARD		0x0001
#define	PLAYEREVENT_GAUNTLETREWARD		0x0002
#define PLAYEREVENT_HOLYSHIT			0x0004

// entityState_t->event values
// entity events are for effects that take place relative
// to an existing entities origin.  Very network efficient.

// two bits at the top of the entityState->event field
// will be incremented with each change in the event so
// that an identical event started twice in a row can
// be distinguished.  And off the value with ~EV_EVENT_BITS
// to retrieve the actual event number
#define	EV_EVENT_BIT1		0x00000100
#define	EV_EVENT_BIT2		0x00000200
#define	EV_EVENT_BITS		(EV_EVENT_BIT1|EV_EVENT_BIT2)

#define	EVENT_VALID_MSEC	300

typedef enum {
	EV_NONE,

	EV_FOOTSTEP,
	EV_FOOTSTEP_METAL,
	EV_FOOTSPLASH,
	EV_FOOTWADE,
	EV_SWIM,

	EV_STEP_4,
	EV_STEP_8,
	EV_STEP_12,
	EV_STEP_16,

	EV_FALL_SHORT,
	EV_FALL_MEDIUM,
	EV_FALL_FAR,

	EV_JUMP_PAD,			// boing sound at origin, jump sound on player

	EV_JUMP,
	EV_WATER_TOUCH,	// foot touches
	EV_WATER_LEAVE,	// foot leaves
	EV_WATER_UNDER,	// head touches
	EV_WATER_CLEAR,	// head leaves

	EV_ITEM_PICKUP,			// normal item pickups are predictable
	EV_GLOBAL_ITEM_PICKUP,	// powerup / team sounds are broadcast to everyone

	EV_NOAMMO,
	EV_CHANGE_WEAPON,
	EV_FIRE_WEAPON,

	EV_USE_ITEM0,
	EV_USE_ITEM1,
	EV_USE_ITEM2,
	EV_USE_ITEM3,
	EV_USE_ITEM4,
	EV_USE_ITEM5,
	EV_USE_ITEM6,
	EV_USE_ITEM7,
	EV_USE_ITEM8,
	EV_USE_ITEM9,
	EV_USE_ITEM10,
	EV_USE_ITEM11,
	EV_USE_ITEM12,
	EV_USE_ITEM13,
	EV_USE_ITEM14,
	EV_USE_ITEM15,

	EV_ITEM_RESPAWN,
	EV_ITEM_POP,
	EV_PLAYER_TELEPORT_IN,
	EV_PLAYER_TELEPORT_OUT,

	EV_GRENADE_BOUNCE,		// eventParm will be the soundindex

	EV_GENERAL_SOUND,
	EV_GLOBAL_SOUND,		// no attenuation
	EV_GLOBAL_TEAM_SOUND,

	EV_BULLET_HIT_FLESH,
	EV_BULLET_HIT_WALL,

	EV_MISSILE_HIT,
	EV_MISSILE_MISS,
	EV_MISSILE_MISS_METAL,
	EV_RAILTRAIL,
	EV_SHOTGUN,
	EV_BULLET,				// otherEntity is the shooter

	EV_PAIN,
	EV_DEATH1,
	EV_DEATH2,
	EV_DEATH3,
	EV_OBITUARY,

	EV_POWERUP_QUAD,
	EV_POWERUP_BATTLESUIT,
	EV_POWERUP_REGEN,

	EV_GIB_PLAYER,			// gib a previously living player
	EV_SCOREPLUM,			// score plum

//#ifdef MISSIONPACK
	EV_PROXIMITY_MINE_STICK,
	EV_PROXIMITY_MINE_TRIGGER,
	EV_KAMIKAZE,			// kamikaze explodes
	EV_OBELISKEXPLODE,		// obelisk explodes
	EV_OBELISKPAIN,			// obelisk is in pain
	EV_INVUL_IMPACT,		// invulnerability sphere impact
	EV_JUICED,				// invulnerability juiced effect
	EV_LIGHTNINGBOLT,		// lightning bolt bounced of invulnerability sphere
//#endif

	EV_DEBUG_LINE,
	EV_STOPLOOPINGSOUND,
	EV_TAUNT,
	EV_TAUNT_YES,
	EV_TAUNT_NO,
	EV_TAUNT_FOLLOWME,
	EV_TAUNT_GETFLAG,
	EV_TAUNT_GUARDBASE,
	EV_TAUNT_PATROL,

	EV_USE_ABILITY,					// active ability takes effect (game side)
	EV_ABILITY_REPRESS,				// ability pressed again while active
	EV_DAMAGEPLUM					// damage dealt, a number over the target for the attacker only
} entity_event_t;


typedef enum {
	GTS_RED_CAPTURE,
	GTS_BLUE_CAPTURE,
	GTS_RED_RETURN,
	GTS_BLUE_RETURN,
	GTS_RED_TAKEN,
	GTS_BLUE_TAKEN,
	GTS_REDOBELISK_ATTACKED,
	GTS_BLUEOBELISK_ATTACKED,
	GTS_REDTEAM_SCORED,
	GTS_BLUETEAM_SCORED,
	GTS_REDTEAM_TOOK_LEAD,
	GTS_BLUETEAM_TOOK_LEAD,
	GTS_TEAMS_ARE_TIED,
	GTS_KAMIKAZE
} global_team_sound_t;

// animations
typedef enum {
	BOTH_DEATH1,
	BOTH_DEAD1,
	BOTH_DEATH2,
	BOTH_DEAD2,
	BOTH_DEATH3,
	BOTH_DEAD3,

	TORSO_GESTURE,

	TORSO_ATTACK,
	TORSO_ATTACK2,

	TORSO_DROP,
	TORSO_RAISE,

	TORSO_STAND,
	TORSO_STAND2,

	LEGS_WALKCR,
	LEGS_WALK,
	LEGS_RUN,
	LEGS_BACK,
	LEGS_SWIM,

	LEGS_JUMP,
	LEGS_LAND,

	LEGS_JUMPB,
	LEGS_LANDB,

	LEGS_IDLE,
	LEGS_IDLECR,

	LEGS_TURN,

	TORSO_GETFLAG,
	TORSO_GUARDBASE,
	TORSO_PATROL,
	TORSO_FOLLOWME,
	TORSO_AFFIRMATIVE,
	TORSO_NEGATIVE,

	MAX_ANIMATIONS,

	LEGS_BACKCR,
	LEGS_BACKWALK,
	FLAG_RUN,
	FLAG_STAND,
	FLAG_STAND2RUN,

	MAX_TOTALANIMATIONS
} animNumber_t;


typedef struct animation_s {
	int		firstFrame;
	int		numFrames;
	int		loopFrames;			// 0 to numFrames
	int		frameLerp;			// msec between frames
	int		initialLerp;		// msec to get to first frame
	int		reversed;			// true if animation is reversed
	int		flipflop;			// true if animation should flipflop back to base
} animation_t;


// flip the togglebit every time an animation
// changes so a restart of the same anim can be detected
#define	ANIM_TOGGLEBIT		128


typedef enum {
	TEAM_FREE,
	TEAM_RED,
	TEAM_BLUE,
	TEAM_SPECTATOR,

	TEAM_NUM_TEAMS
} team_t;

// Time between location updates
#define TEAM_LOCATION_UPDATE_TIME		1000

// How many players on the overlay
#define TEAM_MAXOVERLAY		32

//team task
typedef enum {
	TEAMTASK_NONE,
	TEAMTASK_OFFENSE, 
	TEAMTASK_DEFENSE,
	TEAMTASK_PATROL,
	TEAMTASK_FOLLOW,
	TEAMTASK_RETRIEVE,
	TEAMTASK_ESCORT,
	TEAMTASK_CAMP
} teamtask_t;

// means of death
typedef enum {
	MOD_UNKNOWN,
	MOD_SHOTGUN,
	MOD_GAUNTLET,
	MOD_MACHINEGUN,
	MOD_GRENADE,
	MOD_GRENADE_SPLASH,
	MOD_ROCKET,
	MOD_ROCKET_SPLASH,
	MOD_PLASMA,
	MOD_PLASMA_SPLASH,
	MOD_RAILGUN,
	MOD_LIGHTNING,
	MOD_BFG,
	MOD_BFG_SPLASH,
	MOD_WATER,
	MOD_SLIME,
	MOD_LAVA,
	MOD_CRUSH,
	MOD_TELEFRAG,
	MOD_FALLING,
	MOD_SUICIDE,
	MOD_TARGET_LASER,
	MOD_TRIGGER_HURT,
#ifdef MISSIONPACK
	MOD_NAIL,
	MOD_CHAINGUN,
	MOD_PROXIMITY_MINE,
	MOD_KAMIKAZE,
	MOD_JUICED,
#endif
	MOD_GRAPPLE,
	MOD_HEAVYWEIGHT,
	MOD_ORB,
	MOD_FIREBALL,
	MOD_BURN,
	MOD_ACID,
	MOD_TOTEM,
	MOD_TURRET,
	MOD_LASER,
	MOD_PLASMATRAIL,
	MOD_DRONE,
	MOD_SPINES,
	MOD_STORM,
	MOD_HOUND
} meansOfDeath_t;


//---------------------------------------------------------

// gitem_t->type
typedef enum {
	IT_BAD,
	IT_WEAPON,				// EFX: rotate + upscale + minlight
	IT_AMMO,				// EFX: rotate
	IT_ARMOR,				// EFX: rotate + minlight
	IT_HEALTH,				// EFX: static external sphere + rotating internal
	IT_POWERUP,				// instant on, timer based
							// EFX: rotate + external ring that rotates
	IT_HOLDABLE,			// single use, holdable item
							// EFX: rotate + bob
	IT_PERSISTANT_POWERUP,
	IT_TEAM,
	IT_HOURGLASS			// takes time off the ability's wait
} itemType_t;

#define MAX_ITEM_MODELS 4

typedef struct gitem_s {
	char		*classname;	// spawning name
	char		*pickup_sound;
	char		*world_model[MAX_ITEM_MODELS];

	char		*icon;
	char		*pickup_name;	// for printing on pickup

	int			quantity;		// for ammo how much, or duration of powerup
	itemType_t  giType;			// IT_* flags

	int			giTag;

	char		*precaches;		// string of all models and images this item will use
	char		*sounds;		// string of all sounds this item will use
} gitem_t;

// included in both the game dll and the client
extern	gitem_t	bg_itemlist[];
extern	int		bg_numItems;

gitem_t	*BG_FindItem( const char *pickupName );
gitem_t	*BG_FindItemForWeapon( weapon_t weapon );
gitem_t	*BG_FindItemForPowerup( powerup_t pw );
gitem_t	*BG_FindItemForHoldable( holdable_t pw );
#define	ITEM_INDEX(x) ((x)-bg_itemlist)

int			BG_AmmoBoxAmount( int weapon );
int			BG_ItemTimerClass( const gitem_t *item );
qboolean	BG_AmmoBoxUseful( const playerState_t *ps );
qboolean	BG_CanItemBeGrabbed( int gametype, const entityState_t *ent, const playerState_t *ps );


// g_dmflags->integer flags
#define	DF_NO_FALLING			8
#define DF_FIXED_FOV			16
#define	DF_NO_FOOTSTEPS			32

// content masks
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(CONTENTS_SOLID)
#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY)
#define	MASK_DEADSOLID			(CONTENTS_SOLID|CONTENTS_PLAYERCLIP)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE)


//
// entityState_t->eType
//
typedef enum {
	ET_GENERAL,
	ET_PLAYER,
	ET_ITEM,
	ET_MISSILE,
	ET_MOVER,
	ET_BEAM,
	ET_PORTAL,
	ET_SPEAKER,
	ET_PUSH_TRIGGER,
	ET_TELEPORT_TRIGGER,
	ET_INVISIBLE,
	ET_GRAPPLE,				// grapple hooked on wall
	ET_TEAM,

	ET_EVENTS				// any of the EV_* events can be added freestanding
							// by setting eType to ET_EVENTS + eventNum
							// this avoids having to set eFlags and eventNum
} entityType_t;



void	BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result );
void	BG_EvaluateTrajectoryDelta( const trajectory_t *tr, int atTime, vec3_t result );

void	BG_AddPredictableEventToPlayerstate( int newEvent, int eventParm, playerState_t *ps );

void	BG_TouchJumpPad( playerState_t *ps, entityState_t *jumppad );

void	BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s, qboolean snap );
void	BG_PlayerStateToEntityStateExtraPolate( playerState_t *ps, entityState_t *s, int time, qboolean snap );

#define	ITEM_PICKUP_RADIUS	70		// how far across an item is picked up from (centre to centre)
qboolean	BG_PlayerTouchesItem( playerState_t *ps, entityState_t *item, int atTime );


#define ARENAS_PER_TIER		4
#define MAX_ARENAS			1024
#define	MAX_ARENAS_TEXT		8192

#define MAX_BOTS			1024
#define MAX_BOTS_TEXT		8192


// Kamikaze

// 1st shockwave times
#define KAMI_SHOCKWAVE_STARTTIME		0
#define KAMI_SHOCKWAVEFADE_STARTTIME	1500
#define KAMI_SHOCKWAVE_ENDTIME			2000
// explosion/implosion times
#define KAMI_EXPLODE_STARTTIME			250
#define KAMI_IMPLODE_STARTTIME			2000
#define KAMI_IMPLODE_ENDTIME			2250
// 2nd shockwave times
#define KAMI_SHOCKWAVE2_STARTTIME		2000
#define KAMI_SHOCKWAVE2FADE_STARTTIME	2500
#define KAMI_SHOCKWAVE2_ENDTIME			3000
// radius of the models without scaling
#define KAMI_SHOCKWAVEMODEL_RADIUS		88
#define KAMI_BOOMSPHEREMODEL_RADIUS		72
// maximum radius of the models during the effect
#define KAMI_SHOCKWAVE_MAXRADIUS		1320
#define KAMI_BOOMSPHERE_MAXRADIUS		720
#define KAMI_SHOCKWAVE2_MAXRADIUS		704

