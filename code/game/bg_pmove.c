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
// bg_pmove.c -- both games player movement code
// takes a playerstate and a usercmd as input and returns a modifed playerstate

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"

pmove_t		*pm;
pml_t		pml;

// movement parameters
float	pm_swimScale = 0.50f;

float	pm_wateraccelerate = 4.0f;
float	pm_flyaccelerate = 8.0f;

float	pm_waterfriction = 1.0f;
float	pm_flightfriction = 3.0f;
float	pm_flyfriction = 5.0f;		// drone air brake
float	pm_spectatorfriction = 5.0f;

int		c_pmove = 0;

pmoveSettings_t	bg_pmoveSettings;

static qboolean	PM_AbilityActive( int active );
static qboolean	PM_DroneActive( void );
static float	PM_WeaponTimeScale( void );
static float	PM_ClassAirAccelerate( void );

typedef struct {
	vmCvar_t	vmCvar;
	char		*name;
	char		*defaultString;
	float		*value;
} pmoveCvar_t;

// the server sends these to clients as systeminfo so prediction matches
static pmoveCvar_t pmoveCvars[] = {
	{ {0}, "pm_jumpVelocity",		"275",	&bg_pmoveSettings.jumpVelocity },
	{ {0}, "pm_accelerate",			"10",	&bg_pmoveSettings.accelerate },
	{ {0}, "pm_airAccelerate",		"1",	&bg_pmoveSettings.airAccelerate },
	{ {0}, "pm_friction",			"6",	&bg_pmoveSettings.friction },
	{ {0}, "pm_stopSpeed",			"100",	&bg_pmoveSettings.stopSpeed },
	{ {0}, "pm_crouchSpeed",		"160",	&bg_pmoveSettings.crouchSpeed },
	{ {0}, "pm_speedLimit",			"1",	&bg_pmoveSettings.speedLimit },
	{ {0}, "pm_jumpSilentTime",		"100",	&bg_pmoveSettings.jumpSilentTime },

	{ {0}, "pm_acFriction",			"6",	&bg_pmoveSettings.acFriction },	// ground friction, as everyone
	{ {0}, "pm_acStrafeAccel",		"70",	&bg_pmoveSettings.acStrafeAccel },
	{ {0}, "pm_acStrafeSpeed",		"30",	&bg_pmoveSettings.acStrafeSpeed },
	{ {0}, "pm_acBrakeAccel",		"2.5",	&bg_pmoveSettings.acBrakeAccel },
	{ {0}, "pm_acAirControl",		"150",	&bg_pmoveSettings.acAirControl },
	{ {0}, "pm_hopAccel",			"240",	&bg_pmoveSettings.hopAccel },
	{ {0}, "pm_hopMaxSpeed",		"500",	&bg_pmoveSettings.hopMaxSpeed },
	{ {0}, "pm_easyStrafeAccel",	"0.08",	&bg_pmoveSettings.easyStrafeAccel },
	{ {0}, "pm_easyStrafeMaxSpeed",	"400",	&bg_pmoveSettings.easyStrafeMaxSpeed },
	{ {0}, "pm_isoAirAccel",		"2.5",	&bg_pmoveSettings.isoAirAccel },
	{ {0}, "pm_isoAirControl",		"1",	&bg_pmoveSettings.isoAirControl },
	{ {0}, "pm_isoAirClamp",		"10",	&bg_pmoveSettings.isoAirClamp },

	{ {0}, "pm_chainTime",			"500",	&bg_pmoveSettings.chainTime },
	{ {0}, "pm_chainVelocity",		"110",	&bg_pmoveSettings.chainVelocity },
	{ {0}, "pm_stepJumpVelocity",	"48",	&bg_pmoveSettings.stepJumpVelocity },
	{ {0}, "pm_stepJumpTime",		"100",	&bg_pmoveSettings.stepJumpTime },
	{ {0}, "pm_chainScale",			"0.4",	&bg_pmoveSettings.chainScale },
	{ {0}, "pm_crouchJumpScale",	"0.6",	&bg_pmoveSettings.crouchJumpScale },
	{ {0}, "pm_crouchWalkScale",	"0.5",	&bg_pmoveSettings.crouchWalkScale },
	{ {0}, "pm_airSpeedDecay",		"52",	&bg_pmoveSettings.airSpeedDecay },
	{ {0}, "pm_airSpeedDecayWrack",	"400",	&bg_pmoveSettings.airSpeedDecayWrack },
	{ {0}, "pm_airSpeedDecayClutch",	"600",	&bg_pmoveSettings.airSpeedDecayClutch },
	{ {0}, "pm_circleFriction",		"8",	&bg_pmoveSettings.circleFriction },

	{ {0}, "pm_slideFriction",		"1.2",	&bg_pmoveSettings.slideFriction },
	{ {0}, "pm_slideFrictionStrogg",	"2",	&bg_pmoveSettings.slideFrictionStrogg },
	{ {0}, "pm_slideStopSpeed",		"1",	&bg_pmoveSettings.slideStopSpeed },
	{ {0}, "pm_slideChargeRate",	"5",	&bg_pmoveSettings.slideChargeRate },
	{ {0}, "pm_slideChargeMax",		"6000",	&bg_pmoveSettings.slideChargeMax },
	{ {0}, "pm_slideDrain",			"1",	&bg_pmoveSettings.slideDrain },
	{ {0}, "pm_slideAirGrace",		"200",	&bg_pmoveSettings.slideAirGrace },
	{ {0}, "pm_slideChargeMaxStrogg",	"1000",	&bg_pmoveSettings.slideChargeMaxStrogg },
	{ {0}, "pm_slideAirGraceStrogg",	"200",	&bg_pmoveSettings.slideAirGraceStrogg },
	{ {0}, "pm_slideCrouchScale",	"0.85",	&bg_pmoveSettings.slideCrouchScale },

	{ {0}, "pm_wallJumpVelocity",	"400",	&bg_pmoveSettings.wallJumpVelocity },
	{ {0}, "pm_wallJumpReach",		"18",	&bg_pmoveSettings.wallJumpReach },
	{ {0}, "pm_wallJumpMaxSpeed",	"680",	&bg_pmoveSettings.wallJumpMaxSpeed },
	{ {0}, "pm_wallJumpPush",		"14",	&bg_pmoveSettings.wallJumpPush },
	{ {0}, "pm_wallJumpNormal",		"0.3",	&bg_pmoveSettings.wallJumpNormal },

	{ {0}, "pm_doubleJumpVelocity",	"275",	&bg_pmoveSettings.doubleJumpVelocity },

	{ {0}, "pm_dodgeTapWindow",		"250",	&bg_pmoveSettings.dodgeTapWindow },
	{ {0}, "pm_dodgeSpeed",			"320",	&bg_pmoveSettings.dodgeSpeed },
	{ {0}, "pm_dodgeUp",			"0.25",	&bg_pmoveSettings.dodgeUp },
	{ {0}, "pm_dodgeGroundSpeed",	"480",	&bg_pmoveSettings.dodgeGroundSpeed },
	{ {0}, "pm_dodgeGroundUp",		"0.15",	&bg_pmoveSettings.dodgeGroundUp },

	{ {0}, "pm_accelAngle",			"15",	&bg_pmoveSettings.accelAngle },

	{ {0}, "pm_rushSpeed",			"700",	&bg_pmoveSettings.rushSpeed },
	{ {0}, "pm_rushTurnRate",		"120",	&bg_pmoveSettings.rushTurnRate },
	{ {0}, "pm_rushStartSpeed",	"450",	&bg_pmoveSettings.rushStartSpeed },
	{ {0}, "pm_rushAccel",			"150",	&bg_pmoveSettings.rushAccel },
	{ {0}, "pm_rushMinSpeed",		"150",	&bg_pmoveSettings.rushMinSpeed },
	{ {0}, "pm_rushGravity",		"2",	&bg_pmoveSettings.rushGravity },

	{ {0}, "pm_hookImpulse",		"400",	&bg_pmoveSettings.hookImpulse },
	{ {0}, "pm_hookPullStart",		"400",	&bg_pmoveSettings.hookPullStart },
	{ {0}, "pm_hookPullAccel",		"400",	&bg_pmoveSettings.hookPullAccel },
	{ {0}, "pm_hookPullMax",		"700",	&bg_pmoveSettings.hookPullMax },
	{ {0}, "pm_hookDeadZone",		"40",	&bg_pmoveSettings.hookDeadZone },
	{ {0}, "pm_hookSwingMax",		"1000",	&bg_pmoveSettings.hookSwingMax },

	{ {0}, "pm_rampJumpScale",		"1",	&bg_pmoveSettings.rampJumpScale },
	{ {0}, "pm_rampJumpMax",		"600",	&bg_pmoveSettings.rampJumpMax },

	{ {0}, "pm_airAccelLight",		"1",	&bg_pmoveSettings.airAccelLight },	// 1.0 for every class
	{ {0}, "pm_airAccelMedium",		"1",	&bg_pmoveSettings.airAccelMedium },
	{ {0}, "pm_airAccelHeavy",		"1",	&bg_pmoveSettings.airAccelHeavy },
	{ {0}, "pm_airAccelFullSprint",	"1.012",	&bg_pmoveSettings.airAccelFullSprint },

	{ {0}, "pm_injectionSpeed",		"1.1",	&bg_pmoveSettings.injectionSpeed },
	{ {0}, "pm_dualWieldRate",		"1.3",	&bg_pmoveSettings.dualWieldRate },
	{ {0}, "pm_dualWieldRateHeavy",	"1.5",	&bg_pmoveSettings.dualWieldRateHeavy },
	{ {0}, "pm_trailSpeed",			"1.1",	&bg_pmoveSettings.trailSpeed },
	{ {0}, "pm_berserkSpeed",		"500",	&bg_pmoveSettings.berserkSpeed },
	{ {0}, "pm_berserkLunge",		"666",	&bg_pmoveSettings.berserkLunge },

	{ {0}, "pm_hmgZoomRefire",		"150",	&bg_pmoveSettings.hmgZoomRefire },
	{ {0}, "pm_droneSpeed",			"320",	&bg_pmoveSettings.droneSpeed },
	{ {0}, "pm_droneSize",			"10",	&bg_pmoveSettings.droneSize },
	{ {0}, "pm_droneFireInterval",	"125",	&bg_pmoveSettings.droneFireInterval }
};

static vmCvar_t	abilityCvars[ACTIVE_NUM_ABILITIES][AP_NUM_PARAMS];
static vmCvar_t	weaponCvars[16][WPP_NUM_PARAMS];
static char		*weaponParamNames[WPP_NUM_PARAMS] = { "damage", "refire", "range", "spread", "pellets",
	"knock", "knockair", "knockself", "splash", "splashmin", "radius", "speed", "ammostart", "ammomax" };

/*
===============
BG_ShotgunPelletOffset

Where a shotgun pellet goes, as offsets per unit of distance along the
right and up directions: evenly spaced around a ring at the full spread,
with a little seeded jitter so the server and clients agree
===============
*/
void BG_ShotgunPelletOffset( int weapon, int index, int count, int *seed, float *right, float *up ) {
	float	spread, angle, radius;

	spread = BG_WeaponParam( weapon, WPP_SPREAD );
	if ( count < 1 ) {
		count = 1;
	}
	angle = ( index + 0.25f * Q_crandom( seed ) ) * 2 * M_PI / count;
	radius = spread * ( 0.75f + 0.25f * Q_random( seed ) );
	*right = cos( angle ) * radius;
	*up = sin( angle ) * radius;
}

/*
===============
BG_WeaponTuned

True if the weapon has numbers in the weapon table
===============
*/
qboolean BG_WeaponTuned( int weapon ) {
	int		i;

	for ( i = 0 ; i < bg_numWeaponInfo ; i++ ) {
		if ( bg_weaponInfo[i].weapon == weapon ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
===============
BG_WeaponParam
===============
*/
float BG_WeaponParam( int weapon, int param ) {
	int		i;

	if ( param < 0 || param >= WPP_NUM_PARAMS ) {
		return 0;
	}
	for ( i = 0 ; i < bg_numWeaponInfo ; i++ ) {
		if ( bg_weaponInfo[i].weapon == weapon ) {
			return weaponCvars[i][param].value;
		}
	}
	return 0;
}
static char		*abilityParamNames[AP_NUM_PARAMS] = { "charges", "recharge", "delay", "duration", "lockout" };

/*
===============
BG_AbilityParam
===============
*/
float BG_AbilityParam( int active, int param ) {
	if ( active <= ACTIVE_NONE || active >= ACTIVE_NUM_ABILITIES || param < 0 || param >= AP_NUM_PARAMS ) {
		return 0;
	}
	// a duration rides in a 16-bit stat: longer would break prediction
	if ( param == AP_DURATION && abilityCvars[active][param].value > STAT_TIME_CAP ) {
		return STAT_TIME_CAP;
	}
	return abilityCvars[active][param].value;
}

static const int pmoveCvarCount = ARRAY_LEN( pmoveCvars );

void	trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void	trap_Cvar_Update( vmCvar_t *vmCvar );

/*
===============
BG_RegisterPmoveCvars

The game registers with CVAR_SYSTEMINFO so clients receive the values,
cgame registers with CVAR_SYSTEMINFO so it accepts them
===============
*/
void BG_RegisterPmoveCvars( int flags ) {
	int		i, j;
	char	name[MAX_CVAR_VALUE_STRING];

	for ( i = 0 ; i < pmoveCvarCount ; i++ ) {
		trap_Cvar_Register( &pmoveCvars[i].vmCvar, pmoveCvars[i].name,
			pmoveCvars[i].defaultString, flags );
		*pmoveCvars[i].value = pmoveCvars[i].vmCvar.value;
	}

	for ( i = ACTIVE_NONE + 1 ; i < ACTIVE_NUM_ABILITIES ; i++ ) {
		for ( j = 0 ; j < AP_NUM_PARAMS ; j++ ) {
			Com_sprintf( name, sizeof( name ), "ab_%s_%s", bg_abilityInfo[i].cvarName, abilityParamNames[j] );
			trap_Cvar_Register( &abilityCvars[i][j], name, bg_abilityInfo[i].defaults[j], flags );
		}
	}

	// only what prediction needs goes to clients
	for ( i = 0 ; i < bg_numWeaponInfo ; i++ ) {
		for ( j = 0 ; j < WPP_NUM_PARAMS ; j++ ) {
			Com_sprintf( name, sizeof( name ), "wp_%s_%s", bg_weaponInfo[i].cvarName, weaponParamNames[j] );
			trap_Cvar_Register( &weaponCvars[i][j], name, bg_weaponInfo[i].defaults[j],
				( j == WPP_REFIRE || j == WPP_PELLETS || j == WPP_SPREAD || j == WPP_RANGE ) ? flags : ( flags & ~CVAR_SYSTEMINFO ) );
		}
	}
}

/*
===============
BG_UpdatePmoveCvars
===============
*/
void BG_UpdatePmoveCvars( void ) {
	int		i, j;

	for ( i = 0 ; i < pmoveCvarCount ; i++ ) {
		trap_Cvar_Update( &pmoveCvars[i].vmCvar );
		*pmoveCvars[i].value = pmoveCvars[i].vmCvar.value;
	}

	for ( i = ACTIVE_NONE + 1 ; i < ACTIVE_NUM_ABILITIES ; i++ ) {
		for ( j = 0 ; j < AP_NUM_PARAMS ; j++ ) {
			trap_Cvar_Update( &abilityCvars[i][j] );
		}
	}
	for ( i = 0 ; i < bg_numWeaponInfo ; i++ ) {
		for ( j = 0 ; j < WPP_NUM_PARAMS ; j++ ) {
			trap_Cvar_Update( &weaponCvars[i][j] );
		}
	}
}


/*
===============
PM_AddEvent

===============
*/
void PM_AddEvent( int newEvent ) {
	BG_AddPredictableEventToPlayerstate( newEvent, 0, pm->ps );
}

/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt( int entityNum ) {
	int		i;

	if ( entityNum == ENTITYNUM_WORLD ) {
		return;
	}
	if ( pm->numtouch == MAXTOUCH ) {
		return;
	}

	// see if it is already added
	for ( i = 0 ; i < pm->numtouch ; i++ ) {
		if ( pm->touchents[ i ] == entityNum ) {
			return;
		}
	}

	// add it
	pm->touchents[pm->numtouch] = entityNum;
	pm->numtouch++;
}

/*
===================
PM_StartTorsoAnim
===================
*/
static void PM_StartTorsoAnim( int anim ) {
	if ( pm->ps->pm_type >= PM_DEAD ) {
		return;
	}
	pm->ps->torsoAnim = ( ( pm->ps->torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT )
		| anim;
}
static void PM_StartLegsAnim( int anim ) {
	if ( pm->ps->pm_type >= PM_DEAD ) {
		return;
	}
	if ( pm->ps->legsTimer > 0 ) {
		return;		// a high priority animation is running
	}
	pm->ps->legsAnim = ( ( pm->ps->legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT )
		| anim;
}

static void PM_ContinueLegsAnim( int anim ) {
	if ( ( pm->ps->legsAnim & ~ANIM_TOGGLEBIT ) == anim ) {
		return;
	}
	if ( pm->ps->legsTimer > 0 ) {
		return;		// a high priority animation is running
	}
	PM_StartLegsAnim( anim );
}

static void PM_ContinueTorsoAnim( int anim ) {
	if ( ( pm->ps->torsoAnim & ~ANIM_TOGGLEBIT ) == anim ) {
		return;
	}
	if ( pm->ps->torsoTimer > 0 ) {
		return;		// a high priority animation is running
	}
	PM_StartTorsoAnim( anim );
}

static void PM_ForceLegsAnim( int anim ) {
	pm->ps->legsTimer = 0;
	PM_StartLegsAnim( anim );
}


/*
==================
PM_ClipVelocity

Slide off of the impacting surface
==================
*/
void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce ) {
	float	backoff;
	float	change;
	int		i;
	
	backoff = DotProduct (in, normal);
	
	if ( backoff < 0 ) {
		backoff *= overbounce;
	} else {
		backoff /= overbounce;
	}

	for ( i=0 ; i<3 ; i++ ) {
		change = normal[i]*backoff;
		out[i] = in[i] - change;
	}
}


/*
==================
PM_IsSlash

Slash's slide differs from Strogg's
==================
*/
static qboolean PM_IsSlash( void ) {
	return !Q_stricmp( pml.champion->name, "slash" );
}

static float PM_SlideChargeMax( void ) {
	return PM_IsSlash() ? bg_pmoveSettings.slideChargeMax : bg_pmoveSettings.slideChargeMaxStrogg;
}

static float PM_SlideAirGrace( void ) {
	return PM_IsSlash() ? bg_pmoveSettings.slideAirGrace : bg_pmoveSettings.slideAirGraceStrogg;
}

/*
==================
PM_IsSliding

Slide style: crouching on the ground with slide charge left
==================
*/
static qboolean PM_IsSliding( void ) {
	return pml.champion->moveStyle == MOVE_SLIDE && pml.walking
		&& ( pm->ps->pm_flags & PMF_DUCKED )
		&& BG_SlideCharge( pm->ps ) > 0;
}

/*
==================
PM_GroundFriction
==================
*/
static float PM_GroundFriction( void ) {
	if ( PM_IsSliding() ) {
		return PM_IsSlash() ? bg_pmoveSettings.slideFriction : bg_pmoveSettings.slideFrictionStrogg;
	}
	if ( pml.champion->moveStyle == MOVE_AIRCONTROL ) {
		// circling: forward and strafe both held
		return pm->cmd.forwardmove && pm->cmd.rightmove ? bg_pmoveSettings.circleFriction : bg_pmoveSettings.acFriction;
	}
	return bg_pmoveSettings.friction;
}


/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
static void PM_Friction( void ) {
	vec3_t	vec;
	float	*vel;
	float	speed, newspeed, control;
	float	drop;
	
	vel = pm->ps->velocity;
	
	VectorCopy( vel, vec );
	if ( pml.walking ) {
		vec[2] = 0;	// ignore slope movement
	}

	speed = VectorLength(vec);
	if (speed < 1) {
		vel[0] = 0;
		vel[1] = 0;		// allow sinking underwater
		// FIXME: still have z friction underwater?
		return;
	}

	drop = 0;

	// apply ground friction
	if ( pm->waterlevel <= 1 ) {
		if ( pml.walking && !(pml.groundTrace.surfaceFlags & SURF_SLICK) ) {
			// if getting knocked back, no friction
			if ( ! (pm->ps->pm_flags & PMF_TIME_KNOCKBACK) ) {
				// sliding slows smoothly all the way down, with no sudden stop
				float	stop = PM_IsSliding() ? bg_pmoveSettings.slideStopSpeed : bg_pmoveSettings.stopSpeed;

				control = speed < stop ? stop : speed;
				drop += control*PM_GroundFriction()*pml.frametime;
			}
		}
	}

	// water below the waist counts as dry ground
	if ( pm->waterlevel > 1 ) {
		drop += speed*pm_waterfriction*pm->waterlevel*pml.frametime;
	}

	// apply flying friction
	if ( pm->ps->powerups[PW_FLIGHT]) {
		drop += speed*pm_flightfriction*pml.frametime;
	}

	if ( pm->ps->pm_type == PM_SPECTATOR) {
		drop += speed*pm_spectatorfriction*pml.frametime;
	}

	// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0) {
		newspeed = 0;
	}
	newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}


/*
==============
PM_HorizontalSpeed
==============
*/
static float PM_HorizontalSpeed( void ) {
	return sqrt( pm->ps->velocity[0] * pm->ps->velocity[0] + pm->ps->velocity[1] * pm->ps->velocity[1] );
}

/*
==============
PM_SlowMoving

Crouched (not sliding) or holding walk: a fixed slow speed
==============
*/
static qboolean PM_SlowMoving( void ) {
	if ( pm->ps->pm_flags & PMF_DUCKED ) {
		return !PM_IsSliding();
	}
	return ( pm->cmd.buttons & BUTTON_WALKING ) != 0;
}

/*
==============
PM_SlowSpeed

The crouch or walk speed
==============
*/
static float PM_SlowSpeed( void ) {
	if ( ( pm->ps->pm_flags & PMF_DUCKED ) && pml.champion->moveStyle == MOVE_SLIDE && PM_IsSlash() ) {
		return pml.runSpeed * bg_pmoveSettings.slideCrouchScale;
	}
	return pml.runSpeed * bg_pmoveSettings.crouchWalkScale;
}

/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
static float PM_Accelerate( vec3_t wishdir, float wishspeed, float accel ) {
#if 1
	// q2 style
	int			i;
	float		addspeed, accelspeed, currentspeed, before, after, limit;

	before = PM_HorizontalSpeed();
	currentspeed = DotProduct (pm->ps->velocity, wishdir);
	if ( PM_SlowMoving() ) {
		currentspeed = before;		// crouched or walking: no strafe gain
	}
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0) {
		return 0;
	}
	accelspeed = accel*pml.frametime*wishspeed;
	if (accelspeed > addspeed) {
		accelspeed = addspeed;
	}
	
	for (i=0 ; i<3 ; i++) {
		pm->ps->velocity[i] += accelspeed*wishdir[i];	
	}

	// never past the cap, or past the speed already held: above it,
	// strafing only turns
	if ( bg_pmoveSettings.speedLimit && pml.champion->jumpSpeedCap > 0 ) {
		limit = before > pml.champion->jumpSpeedCap ? before : pml.champion->jumpSpeedCap;
		after = PM_HorizontalSpeed();
		if ( after > limit ) {
			pm->ps->velocity[0] *= limit / after;
			pm->ps->velocity[1] *= limit / after;
		}
	}
	return accelspeed;
#else
	// proper way (avoids strafe jump maxspeed bug), but feels bad
	vec3_t		wishVelocity;
	vec3_t		pushDir;
	float		pushLen;
	float		canPush;

	VectorScale( wishdir, wishspeed, wishVelocity );
	VectorSubtract( wishVelocity, pm->ps->velocity, pushDir );
	pushLen = VectorNormalize( pushDir );

	canPush = accel*pml.frametime*wishspeed;
	if (canPush > pushLen) {
		canPush = pushLen;
	}

	VectorMA( pm->ps->velocity, canPush, pushDir, pm->ps->velocity );
	return canPush;
#endif
}



/*
============
PM_CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
static float PM_CmdScale( usercmd_t *cmd ) {
	int		max;
	float	total;
	float	scale;

	max = abs( cmd->forwardmove );
	if ( abs( cmd->rightmove ) > max ) {
		max = abs( cmd->rightmove );
	}
	if ( abs( cmd->upmove ) > max ) {
		max = abs( cmd->upmove );
	}
	if ( !max ) {
		return 0;
	}

	total = sqrt( cmd->forwardmove * cmd->forwardmove
		+ cmd->rightmove * cmd->rightmove + cmd->upmove * cmd->upmove );
	scale = pml.runSpeed * max / ( 127.0 * total );

	return scale;
}


/*
================
PM_SetMovementDir

Determine the rotation of the legs relative
to the facing dir
================
*/
static void PM_SetMovementDir( void ) {
	if ( pm->cmd.forwardmove || pm->cmd.rightmove ) {
		if ( pm->cmd.rightmove == 0 && pm->cmd.forwardmove > 0 ) {
			pm->ps->movementDir = 0;
		} else if ( pm->cmd.rightmove < 0 && pm->cmd.forwardmove > 0 ) {
			pm->ps->movementDir = 1;
		} else if ( pm->cmd.rightmove < 0 && pm->cmd.forwardmove == 0 ) {
			pm->ps->movementDir = 2;
		} else if ( pm->cmd.rightmove < 0 && pm->cmd.forwardmove < 0 ) {
			pm->ps->movementDir = 3;
		} else if ( pm->cmd.rightmove == 0 && pm->cmd.forwardmove < 0 ) {
			pm->ps->movementDir = 4;
		} else if ( pm->cmd.rightmove > 0 && pm->cmd.forwardmove < 0 ) {
			pm->ps->movementDir = 5;
		} else if ( pm->cmd.rightmove > 0 && pm->cmd.forwardmove == 0 ) {
			pm->ps->movementDir = 6;
		} else if ( pm->cmd.rightmove > 0 && pm->cmd.forwardmove > 0 ) {
			pm->ps->movementDir = 7;
		}
	} else {
		// if they aren't actively going directly sideways,
		// change the animation to the diagonal so they
		// don't stop too crooked
		if ( pm->ps->movementDir == 2 ) {
			pm->ps->movementDir = 1;
		} else if ( pm->ps->movementDir == 6 ) {
			pm->ps->movementDir = 7;
		} 
	}
}


/*
=============
PM_CheckJump
=============
*/
#define CHAIN_FULL_MSEC		300		// air control: full chain boost this soon after a jump
#define JUMP_REPEAT_MSEC	50		// holding jump hops again after this long

static qboolean PM_CheckJump( void ) {
	qboolean	silent;
	int			style, jt;
	float		speed;

	if ( pm->ps->pm_flags & PMF_RESPAWNED ) {
		return qfalse;		// don't allow jump until all buttons are up
	}

	if ( pm->cmd.upmove < 10 ) {
		// not holding jump
		return qfalse;
	}

	// holding jump hops, a short while after the last jump
	if ( pm->ps->stats[STAT_JUMP_TIME] < JUMP_REPEAT_MSEC ) {
		return qfalse;
	}

	// a crouched or walking jump keeps no more than the slow speed's worth
	// of what it carries above it, halved
	if ( PM_SlowMoving() && !( pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) ) {
		float	h = PM_HorizontalSpeed(), slow = PM_SlowSpeed();

		if ( h > slow ) {
			float	nh = slow + ( h - slow ) * 0.5f;

			pm->ps->velocity[0] *= nh / h;
			pm->ps->velocity[1] *= nh / h;
		}
	}

	// landing and sliding jumps are silent
	silent = ( pm->ps->stats[STAT_LAND_TIME] >= 0 && pm->ps->stats[STAT_LAND_TIME] < bg_pmoveSettings.jumpSilentTime )
		|| pml.wasSliding;

	pml.groundPlane = qfalse;		// jumping away
	pml.walking = qfalse;
	pml.jumped = qtrue;
	pm->ps->pm_flags |= PMF_JUMP_HELD;

	pm->ps->groundEntityNum = ENTITYNUM_NONE;

	// ramp jump: jumping while climbing (up a ramp, or just after a step up)
	// keeps the upward speed and adds the jump, up to a cap
	speed = 0;
	if ( pml.champion->abilities & MOVEABIL_RAMPJUMP ) {
		speed = pm->ps->stats[STAT_MOVE_STATE];
		if ( pm->ps->velocity[2] > speed ) {
			speed = pm->ps->velocity[2];
		}
	}
	pm->ps->velocity[2] = bg_pmoveSettings.jumpVelocity;

	style = pml.champion->moveStyle;

	// a jump from a crouch is lower, except for sliders
	if ( ( pm->ps->pm_flags & PMF_DUCKED ) && style != MOVE_SLIDE ) {
		pm->ps->velocity[2] *= bg_pmoveSettings.crouchJumpScale;
	}

	// one bonus per jump.
	// Air control: under 300 ms after the last jump +110 (+48 on a stair
	// jump), then +48 fading to +16 at the chain time.
	// Everyone else: a chained jump scales up, fading over the chain time.
	jt = pm->ps->stats[STAT_JUMP_TIME];
	if ( style == MOVE_AIRCONTROL ) {
		if ( jt < CHAIN_FULL_MSEC ) {
			pm->ps->velocity[2] += pml.stairJump ? bg_pmoveSettings.stepJumpVelocity : bg_pmoveSettings.chainVelocity;
		} else if ( jt < bg_pmoveSettings.chainTime ) {
			pm->ps->velocity[2] += bg_pmoveSettings.stepJumpVelocity * ( 2.0f - jt / (float)CHAIN_FULL_MSEC );
		}
	} else if ( jt < bg_pmoveSettings.chainTime ) {
		pm->ps->velocity[2] *= 1.0f + bg_pmoveSettings.chainScale
			* ( bg_pmoveSettings.chainTime - jt ) / bg_pmoveSettings.chainTime;
	}
	// ramp jump: the climb adds to the jump (chain bonus included), up to a cap
	if ( speed > 0 ) {
		pm->ps->velocity[2] += bg_pmoveSettings.rampJumpScale * speed;
		if ( pm->ps->velocity[2] > bg_pmoveSettings.rampJumpMax ) {
			pm->ps->velocity[2] = bg_pmoveSettings.rampJumpMax;
		}
	}
	pm->ps->persistant[PERS_STEP_TIME] = STAT_TIME_CAP;
	if ( style == MOVE_SLIDE ) {
		pm->ps->stats[STAT_MOVE_STATE] = 0;
	}

	pm->ps->stats[STAT_JUMP_TIME] = 0;

	if ( !silent ) {
		PM_AddEvent( EV_JUMP );
	}

	if ( pm->cmd.forwardmove >= 0 ) {
		PM_ForceLegsAnim( LEGS_JUMP );
		pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
	} else {
		PM_ForceLegsAnim( LEGS_JUMPB );
		pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
	}

	return qtrue;
}

/*
=============
PM_CheckWaterJump
=============
*/
static qboolean	PM_CheckWaterJump( void ) {
	vec3_t	spot;
	int		cont;
	vec3_t	flatforward;

	if (pm->ps->pm_time) {
		return qfalse;
	}

	// check for water jump
	if ( pm->waterlevel != 2 ) {
		return qfalse;
	}

	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	VectorMA (pm->ps->origin, 30, flatforward, spot);
	spot[2] += 4;
	cont = pm->pointcontents (spot, pm->ps->clientNum );
	if ( !(cont & CONTENTS_SOLID) ) {
		return qfalse;
	}

	spot[2] += 16;
	cont = pm->pointcontents (spot, pm->ps->clientNum );
	if ( cont & (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY) ) {
		return qfalse;
	}

	// jump out of water
	VectorScale (pml.forward, 200, pm->ps->velocity);
	pm->ps->velocity[2] = 350;

	pm->ps->pm_flags |= PMF_TIME_WATERJUMP;
	pm->ps->pm_time = 2000;

	return qtrue;
}

//============================================================================


/*
===================
PM_WaterJumpMove

Flying out of the water
===================
*/
static void PM_WaterJumpMove( void ) {
	// waterjump has no control, but falls

	PM_StepSlideMove( qtrue );

	pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
	if (pm->ps->velocity[2] < 0) {
		// cancel as soon as we are falling down again
		pm->ps->pm_flags &= ~PMF_ALL_TIMES;
		pm->ps->pm_time = 0;
	}
}

/*
===================
PM_WaterMove

===================
*/
static void PM_WaterMove( void ) {
	int		i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;
	float	vel;

	if ( PM_CheckWaterJump() ) {
		PM_WaterJumpMove();
		return;
	}
#if 0
	// jump = head for surface
	if ( pm->cmd.upmove >= 10 ) {
		if (pm->ps->velocity[2] > -300) {
			if ( pm->watertype & CONTENTS_WATER ) {
				pm->ps->velocity[2] = 100;
			} else if ( pm->watertype & CONTENTS_SLIME ) {
				pm->ps->velocity[2] = 80;
			} else {
				pm->ps->velocity[2] = 50;
			}
		}
	}
#endif
	PM_Friction ();

	scale = PM_CmdScale( &pm->cmd );
	//
	// user intentions
	//
	if ( !scale ) {
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = -60;		// sink towards bottom
	} else {
		for (i=0 ; i<3 ; i++)
			wishvel[i] = scale * pml.forward[i]*pm->cmd.forwardmove + scale * pml.right[i]*pm->cmd.rightmove;

		wishvel[2] += scale * pm->cmd.upmove;
	}

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if ( wishspeed > pm->ps->speed * pm_swimScale ) {
		wishspeed = pm->ps->speed * pm_swimScale;
	}

	PM_Accelerate (wishdir, wishspeed, pm_wateraccelerate);

	// make sure we can go up slopes easily under water
	if ( pml.groundPlane && DotProduct( pm->ps->velocity, pml.groundTrace.plane.normal ) < 0 ) {
		vel = VectorLength(pm->ps->velocity);
		// slide along the ground plane
		PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
			pm->ps->velocity, OVERCLIP );

		VectorNormalize(pm->ps->velocity);
		VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
	}

	PM_SlideMove( qfalse );
}

#ifdef MISSIONPACK
/*
===================
PM_InvulnerabilityMove

Only with the invulnerability powerup
===================
*/
static void PM_InvulnerabilityMove( void ) {
	pm->cmd.forwardmove = 0;
	pm->cmd.rightmove = 0;
	pm->cmd.upmove = 0;
	VectorClear(pm->ps->velocity);
}
#endif

/*
===================
PM_FlyMove

Only with the flight powerup
===================
*/
static void PM_FlyMove( void ) {
	int		i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;

	// normal slowdown
	PM_Friction ();

	scale = PM_CmdScale( &pm->cmd );
	//
	// user intentions
	//
	if ( !scale ) {
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = 0;
	} else {
		for (i=0 ; i<3 ; i++) {
			wishvel[i] = scale * pml.forward[i]*pm->cmd.forwardmove + scale * pml.right[i]*pm->cmd.rightmove;
		}

		wishvel[2] += scale * pm->cmd.upmove;
	}

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	PM_Accelerate (wishdir, wishspeed, pm_flyaccelerate);

	PM_StepSlideMove( qfalse );
}


/*
===================
PM_WallTrace

Looks for a wall to jump off in one horizontal direction
===================
*/
static qboolean PM_WallTrace( const vec3_t dir, trace_t *trace ) {
	vec3_t		end;

	VectorMA( pm->ps->origin, pm->maxs[0] + bg_pmoveSettings.wallJumpReach, dir, end );
	pm->trace( trace, pm->ps->origin, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask );

	if ( trace->allsolid || trace->fraction == 1.0f ) {
		return qfalse;
	}
	if ( fabs( trace->plane.normal[2] ) >= bg_pmoveSettings.wallJumpNormal ) {
		return qfalse;		// floor, ceiling or slope, not a wall
	}
	if ( trace->surfaceFlags & ( SURF_SKY | SURF_NOWALLJUMP ) ) {
		return qfalse;
	}
	if ( trace->entityNum < MAX_CLIENTS ) {
		return qfalse;		// other players
	}
	return qtrue;
}

/*
===================
PM_CheckWallJump

A fresh jump press in the air next to a wall, once per airtime
===================
*/
static qboolean PM_CheckWallJump( const vec3_t wishdir ) {
	trace_t		trace;
	vec3_t		dir, normal;
	float		before, limit, speed;
	int			i;
	qboolean	found;

	if ( !( pml.champion->abilities & MOVEABIL_WALLJUMP ) ) {
		return qfalse;
	}
	if ( pm->ps->pm_flags & ( PMF_AIRMOVE_USED | PMF_RESPAWNED ) ) {
		return qfalse;
	}
	if ( pm->cmd.upmove < 10 || pml.jumpWasDown || pml.jumped ) {
		return qfalse;
	}
	if ( pm->ps->groundEntityNum != ENTITYNUM_NONE ) {
		return qfalse;
	}

	// movement input direction first, then all around
	found = qfalse;
	if ( VectorLength( wishdir ) > 0 ) {
		found = PM_WallTrace( wishdir, &trace );
	}
	for ( i = 0 ; i < 8 && !found ; i++ ) {
		dir[0] = cos( DEG2RAD( i * 45 ) );
		dir[1] = sin( DEG2RAD( i * 45 ) );
		dir[2] = 0;
		found = PM_WallTrace( dir, &trace );
	}
	if ( !found ) {
		return qfalse;
	}

	// a nudge off the wall and straight up; no faster than the speed held
	// before or the cap, whichever is more
	before = VectorLength( pm->ps->velocity );
	VectorSet( normal, trace.plane.normal[0], trace.plane.normal[1], 0 );
	VectorNormalize( normal );
	VectorMA( pm->ps->velocity, bg_pmoveSettings.wallJumpPush, normal, pm->ps->velocity );
	pm->ps->velocity[2] = bg_pmoveSettings.wallJumpVelocity;
	limit = before > bg_pmoveSettings.wallJumpMaxSpeed ? before : bg_pmoveSettings.wallJumpMaxSpeed;
	speed = VectorLength( pm->ps->velocity );
	if ( speed > limit ) {
		VectorScale( pm->ps->velocity, limit / speed, pm->ps->velocity );
	}

	pm->ps->pm_flags |= PMF_AIRMOVE_USED | PMF_JUMP_HELD;
	pml.jumped = qtrue;
	PM_AddEvent( EV_JUMP );
	PM_ForceLegsAnim( LEGS_JUMP );
	pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;

	return qtrue;
}

/*
===================
PM_CheckDoubleJump

A fresh jump press in the air jumps again, once per airtime
===================
*/
static qboolean PM_CheckDoubleJump( void ) {
	if ( !( pml.champion->abilities & MOVEABIL_DOUBLEJUMP ) ) {
		return qfalse;
	}
	if ( pm->ps->pm_flags & ( PMF_AIRMOVE_USED | PMF_RESPAWNED ) ) {
		return qfalse;
	}
	if ( pm->cmd.upmove < 10 || pml.jumpWasDown || pml.jumped ) {
		return qfalse;
	}
	if ( pm->ps->groundEntityNum != ENTITYNUM_NONE ) {
		return qfalse;
	}
	if ( pm->ps->stats[STAT_JUMP_TIME] < 100 ) {
		return qfalse;		// at least 0.1 s after the first jump
	}

	pm->ps->velocity[2] = bg_pmoveSettings.doubleJumpVelocity;
	pm->ps->pm_flags |= PMF_AIRMOVE_USED | PMF_JUMP_HELD;
	pm->ps->stats[STAT_JUMP_TIME] = 0;
	pml.jumped = qtrue;
	PM_AddEvent( EV_JUMP );
	if ( pm->cmd.forwardmove >= 0 ) {
		PM_ForceLegsAnim( LEGS_JUMP );
		pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
	} else {
		PM_ForceLegsAnim( LEGS_JUMPB );
		pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
	}
	return qtrue;
}

/*
===================
PM_AirControl

Air-control style: with only forward or back held, turn the velocity
toward the wish direction without changing speed
===================
*/
static void PM_AirControl( const vec3_t wishdir, float wishspeed, float strength ) {
	vec3_t		dir;
	float		zspeed, speed, dot, k;

	if ( wishspeed == 0 || strength <= 0 ) {
		return;
	}

	zspeed = pm->ps->velocity[2];
	VectorSet( dir, pm->ps->velocity[0], pm->ps->velocity[1], 0 );
	speed = VectorNormalize( dir );
	if ( speed < 1 ) {
		return;
	}

	dot = DotProduct( dir, wishdir );
	if ( dot <= 0 ) {
		return;		// no turning while slowing down
	}

	// turn rate grows with the square of how well aim and velocity agree
	k = strength * 32 * dot * dot * pml.frametime;
	VectorScale( dir, speed, dir );
	VectorMA( dir, k, wishdir, dir );
	VectorNormalize( dir );

	pm->ps->velocity[0] = dir[0] * speed;
	pm->ps->velocity[1] = dir[1] * speed;
	pm->ps->velocity[2] = zspeed;
}

/*
===================
PM_AirSpeedDecay

In the air, speed above the cap (the crouch or walk speed while moving
slowly) bleeds off at a fixed rate, except after a jump pad until landing
===================
*/
static void PM_AirSpeedDecay( void ) {
	float	cap, h, rate, nh;

	if ( !bg_pmoveSettings.speedLimit || pm->ps->stats[STAT_LAND_TIME] < 0 ) {
		return;		// off, or launched
	}
	cap = PM_SlowMoving() ? PM_SlowSpeed() : pml.champion->jumpSpeedCap;
	if ( cap <= 0 ) {
		return;
	}
	h = PM_HorizontalSpeed();
	if ( h <= cap ) {
		return;
	}
	rate = bg_pmoveSettings.airSpeedDecay;
	if ( !Q_stricmp( pml.champion->name, "wrack" ) ) {
		rate = bg_pmoveSettings.airSpeedDecayWrack;
	} else if ( !Q_stricmp( pml.champion->name, "clutch" ) ) {
		rate = bg_pmoveSettings.airSpeedDecayClutch;
	}
	nh = h - rate * pml.frametime;
	if ( nh < cap ) {
		nh = cap;
	}
	pm->ps->velocity[0] *= nh / h;
	pm->ps->velocity[1] *= nh / h;
}

/*
===================
PM_AirMove

===================
*/
static void PM_AirMove( void ) {
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;
	usercmd_t	cmd;
	float		accelerate, accelspeed, added;

	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

	// set the movementDir so clients can rotate the legs for strafing
	PM_SetMovementDir();

	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2] = 0;
	VectorNormalize (pml.forward);
	VectorNormalize (pml.right);

	for ( i = 0 ; i < 2 ; i++ ) {
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	}
	wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	wishspeed *= scale;

	if ( !PM_CheckWallJump( wishdir ) ) {
		PM_CheckDoubleJump();
	}

	// not on ground, so little effect on velocity
	accelerate = PM_ClassAirAccelerate();
	accelspeed = wishspeed;
	if ( pml.champion->moveStyle == MOVE_AIRCONTROL ) {
		if ( fmove == 0 && smove != 0 ) {
			// pure side strafe turns sharply
			accelerate = bg_pmoveSettings.acStrafeAccel;
			if ( accelspeed > bg_pmoveSettings.acStrafeSpeed ) {
				accelspeed = bg_pmoveSettings.acStrafeSpeed;
			}
		} else if ( DotProduct( pm->ps->velocity, wishdir ) < 0 ) {
			accelerate = bg_pmoveSettings.acBrakeAccel;
		}
	}
	added = PM_Accelerate (wishdir, accelspeed, accelerate);

	// with forward alone held and nothing gained the normal way:
	if ( fmove > 0 && smove == 0 && added <= 0 && wishspeed > 0 ) {
		if ( pml.champion->abilities & MOVEABIL_EASYSTRAFE ) {
			// easy strafe: accelerate as if the wish speed were higher
			PM_Accelerate( wishdir, bg_pmoveSettings.easyStrafeMaxSpeed, bg_pmoveSettings.easyStrafeAccel );
		} else if ( pml.champion->abilities & MOVEABIL_BUNNYHOP ) {
			// bunny hop: gain up to the run speed, less and less above it
			float	h = PM_HorizontalSpeed(), gain = bg_pmoveSettings.hopAccel * pml.frametime;

			if ( h > pml.runSpeed ) {
				gain *= ( bg_pmoveSettings.hopMaxSpeed - h ) / pml.runSpeed;
			}
			if ( gain > 0 ) {
				float	before = h, limit;

				VectorMA( pm->ps->velocity, gain, wishdir, pm->ps->velocity );
				limit = before > pml.champion->jumpSpeedCap ? before : pml.champion->jumpSpeedCap;
				h = PM_HorizontalSpeed();
				if ( pml.champion->jumpSpeedCap > 0 && h > limit ) {
					pm->ps->velocity[0] *= limit / h;
					pm->ps->velocity[1] *= limit / h;
				}
			}
		}
	}

	if ( pml.champion->moveStyle == MOVE_AIRCONTROL && fmove != 0 && smove == 0 ) {
		PM_AirControl( wishdir, wishspeed, bg_pmoveSettings.acAirControl );
	} else if ( ( pml.champion->passives & PASSIVE_LEAPINGKICK ) && KICK_AIR( pm->ps ) > 0 ) {
		// after a Leaping Kick: air control for a while
		PM_AirControl( wishdir, wishspeed, bg_pmoveSettings.acAirControl );
	}

	// isotropic: steering works with any keys
	if ( pml.champion->abilities & MOVEABIL_ISOAIR ) {
		PM_AirControl( wishdir, wishspeed, bg_pmoveSettings.isoAirControl );
	}

	PM_AirSpeedDecay();

	// we may have a ground plane that is very steep, even
	// though we don't have a groundentity
	// slide along the steep plane
	if ( pml.groundPlane ) {
		PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
			pm->ps->velocity, OVERCLIP );
	}

#if 0
	//ZOID:  If we are on the grapple, try stair-stepping
	//this allows a player to use the grapple to pull himself
	//over a ledge
	if (pm->ps->pm_flags & PMF_GRAPPLE_PULL)
		PM_StepSlideMove ( qtrue );
	else
		PM_SlideMove ( qtrue );
#endif

	PM_StepSlideMove ( qtrue );
}

/*
===================
PM_GrappleMove

===================
*/
static void PM_GrappleMove( void ) {
	vec3_t vel, v;
	float vlen;

	VectorScale(pml.forward, -16, v);
	VectorAdd(pm->ps->grapplePoint, v, v);
	VectorSubtract(v, pm->ps->origin, vel);
	vlen = VectorLength(vel);
	VectorNormalize( vel );

	if (vlen <= 100)
		VectorScale(vel, 10 * vlen, vel);
	else
		VectorScale(vel, 800, vel);

	VectorCopy(vel, pm->ps->velocity);

	pml.groundPlane = qfalse;
}

/*
===================
PM_WalkMove

===================
*/
static void PM_WalkMove( void ) {
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;
	usercmd_t	cmd;
	float		accelerate;
	float		vel;

	if ( pm->waterlevel > 2 && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 ) {
		// begin swimming
		PM_WaterMove();
		return;
	}


	if ( PM_CheckJump () ) {
		// jumped away
		if ( pm->waterlevel > 1 ) {
			PM_WaterMove();
		} else {
			PM_AirMove();
		}
		return;
	}

	PM_Friction ();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

	// set the movementDir so clients can rotate the legs for strafing
	PM_SetMovementDir();

	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2] = 0;

	// project the forward and right directions onto the ground plane
	PM_ClipVelocity (pml.forward, pml.groundTrace.plane.normal, pml.forward, OVERCLIP );
	PM_ClipVelocity (pml.right, pml.groundTrace.plane.normal, pml.right, OVERCLIP );
	//
	VectorNormalize (pml.forward);
	VectorNormalize (pml.right);

	for ( i = 0 ; i < 3 ; i++ ) {
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	}
	// when going up or down slopes the wish velocity should Not be zero
//	wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	wishspeed *= scale;

	// crouching is a fixed speed (for sliders a share of run speed), except while sliding
	if ( ( pm->ps->pm_flags & PMF_DUCKED ) && !PM_IsSliding() ) {
		float	crouch = pml.runSpeed * ( pml.champion->moveStyle == MOVE_SLIDE && PM_IsSlash()
			? bg_pmoveSettings.slideCrouchScale : bg_pmoveSettings.crouchWalkScale );

		if ( wishspeed > crouch ) {
			wishspeed = crouch;
		}
	}

	// clamp the speed lower if walking on the bottom; below the waist is dry
	if ( pm->waterlevel > 1 ) {
		float	waterScale;

		waterScale = pm->waterlevel / 3.0;
		waterScale = 1.0 - ( 1.0 - pm_swimScale ) * waterScale;
		if ( wishspeed > pm->ps->speed * waterScale ) {
			wishspeed = pm->ps->speed * waterScale;
		}
	}

	// when a player gets hit, they temporarily lose
	// full control, which allows them to be moved a bit
	if ( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) {
		accelerate = bg_pmoveSettings.airAccelerate;
	} else {
		accelerate = bg_pmoveSettings.accelerate;
	}

	PM_Accelerate (wishdir, wishspeed, accelerate);

	//Com_Printf("velocity = %1.1f %1.1f %1.1f\n", pm->ps->velocity[0], pm->ps->velocity[1], pm->ps->velocity[2]);
	//Com_Printf("velocity1 = %1.1f\n", VectorLength(pm->ps->velocity));

	if ( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) {
		pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
	} else {
		// don't reset the z velocity for slopes
//		pm->ps->velocity[2] = 0;
	}

	vel = VectorLength(pm->ps->velocity);

	// slide along the ground plane
	PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
		pm->ps->velocity, OVERCLIP );

	// don't decrease velocity when going up or down a slope
	VectorNormalize(pm->ps->velocity);
	VectorScale(pm->ps->velocity, vel, pm->ps->velocity);

	// don't do anything if standing still
	if (!pm->ps->velocity[0] && !pm->ps->velocity[1]) {
		return;
	}

	PM_StepSlideMove( qfalse );

	//Com_Printf("velocity2 = %1.1f\n", VectorLength(pm->ps->velocity));

}


/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove( void ) {
	float	forward;

	if ( !pml.walking ) {
		return;
	}

	// extra friction

	forward = VectorLength (pm->ps->velocity);
	forward -= 20;
	if ( forward <= 0 ) {
		VectorClear (pm->ps->velocity);
	} else {
		VectorNormalize (pm->ps->velocity);
		VectorScale (pm->ps->velocity, forward, pm->ps->velocity);
	}
}


/*
===============
PM_NoclipMove
===============
*/
static void PM_NoclipMove( void ) {
	float	speed, drop, friction, control, newspeed;
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;

	pm->ps->viewheight = DEFAULT_VIEWHEIGHT;

	// friction

	speed = VectorLength (pm->ps->velocity);
	if (speed < 1)
	{
		VectorCopy (vec3_origin, pm->ps->velocity);
	}
	else
	{
		drop = 0;

		friction = bg_pmoveSettings.friction*1.5;	// extra friction
		control = speed < bg_pmoveSettings.stopSpeed ? bg_pmoveSettings.stopSpeed : speed;
		drop += control*friction*pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;

		VectorScale (pm->ps->velocity, newspeed, pm->ps->velocity);
	}

	// accelerate
	scale = PM_CmdScale( &pm->cmd );

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;
	
	for (i=0 ; i<3 ; i++)
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	wishvel[2] += pm->cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	wishspeed *= scale;

	PM_Accelerate( wishdir, wishspeed, bg_pmoveSettings.accelerate );

	// move
	VectorMA (pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin);
}

//============================================================================

/*
================
PM_FootstepForSurface

Returns an event number appropriate for the groundsurface
================
*/
static int PM_FootstepForSurface( void ) {
	if ( pml.groundTrace.surfaceFlags & SURF_NOSTEPS ) {
		return 0;
	}
	if ( pml.champion->passives & PASSIVE_LIGHTFEET ) {
		return 0;		// silent steps
	}
	if ( pml.groundTrace.surfaceFlags & SURF_METALSTEPS ) {
		return EV_FOOTSTEP_METAL;
	}
	return EV_FOOTSTEP;
}


/*
=================
PM_CrashLand

Check for hard landings that generate sound events
=================
*/
static void PM_CrashLand( void ) {
	float		delta;
	float		dist;
	float		vel, acc;
	float		t;
	float		a, b, c, den;

	// decide which landing animation to use
	if ( pm->ps->pm_flags & PMF_BACKWARDS_JUMP ) {
		PM_ForceLegsAnim( LEGS_LANDB );
	} else {
		PM_ForceLegsAnim( LEGS_LAND );
	}

	pm->ps->legsTimer = TIMER_LAND;

	// calculate the exact velocity on landing
	dist = pm->ps->origin[2] - pml.previous_origin[2];
	vel = pml.previous_velocity[2];
	acc = -pm->ps->gravity;

	a = acc / 2;
	b = vel;
	c = -dist;

	den =  b * b - 4 * a * c;
	if ( den < 0 ) {
		return;
	}
	t = (-b - sqrt( den ) ) / ( 2 * a );

	delta = vel + t * acc;
	delta = delta*delta * 0.0001;

	// ducking while falling doubles damage
	if ( pm->ps->pm_flags & PMF_DUCKED ) {
		delta *= 2;
	}

	// never take falling damage if completely underwater
	if ( pm->waterlevel == 3 ) {
		return;
	}

	// reduce falling damage if there is standing water
	if ( pm->waterlevel == 2 ) {
		delta *= 0.25;
	}
	if ( pm->waterlevel == 1 ) {
		delta *= 0.5;
	}

	if ( delta < 1 ) {
		return;
	}

	// create a local entity event to play the sound

	// SURF_NODAMAGE is used for bounce pads where you don't ever
	// want to take damage or play a crunch sound
	if ( !(pml.groundTrace.surfaceFlags & SURF_NODAMAGE) && !( pml.champion->passives & PASSIVE_LIGHTFEET ) )  {
		if ( delta > 60 ) {
			PM_AddEvent( EV_FALL_FAR );
		} else if ( delta > 40 ) {
			// this is a pain grunt, so don't play it if dead
			if ( pm->ps->stats[STAT_HEALTH] > 0 ) {
				PM_AddEvent( EV_FALL_MEDIUM );
			}
		} else if ( delta > 7 ) {
			PM_AddEvent( EV_FALL_SHORT );
		} else {
			PM_AddEvent( PM_FootstepForSurface() );
		}
	}

	// start footstep cycle over
	pm->ps->bobCycle = 0;
}

/*
=============
PM_CheckStuck
=============
*/
/*
void PM_CheckStuck(void) {
	trace_t trace;

	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);
	if (trace.allsolid) {
		//int shit = qtrue;
	}
}
*/

/*
=============
PM_CorrectAllSolid
=============
*/
static int PM_CorrectAllSolid( trace_t *trace ) {
	int			i, j, k;
	vec3_t		point;

	if ( pm->debugLevel ) {
		Com_Printf("%i:allsolid\n", c_pmove);
	}

	// jitter around
	for (i = -1; i <= 1; i++) {
		for (j = -1; j <= 1; j++) {
			for (k = -1; k <= 1; k++) {
				VectorCopy(pm->ps->origin, point);
				point[0] += (float) i;
				point[1] += (float) j;
				point[2] += (float) k;
				pm->trace (trace, point, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
				if ( !trace->allsolid ) {
					point[0] = pm->ps->origin[0];
					point[1] = pm->ps->origin[1];
					point[2] = pm->ps->origin[2] - 0.25;

					pm->trace (trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
					pml.groundTrace = *trace;
					return qtrue;
				}
			}
		}
	}

	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pml.groundPlane = qfalse;
	pml.walking = qfalse;

	return qfalse;
}


/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
static void PM_GroundTraceMissed( void ) {
	trace_t		trace;
	vec3_t		point;

	if ( pm->ps->groundEntityNum != ENTITYNUM_NONE ) {
		// we just transitioned into freefall
		if ( pm->debugLevel ) {
			Com_Printf("%i:lift\n", c_pmove);
		}

		// if they aren't in a jumping animation and the ground is a ways away, force into it
		// if we didn't do the trace, the player would be backflipping down staircases
		VectorCopy( pm->ps->origin, point );
		point[2] -= 64;

		pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
		if ( trace.fraction == 1.0 ) {
			if ( pm->cmd.forwardmove >= 0 ) {
				PM_ForceLegsAnim( LEGS_JUMP );
				pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
			} else {
				PM_ForceLegsAnim( LEGS_JUMPB );
				pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
			}
		}
	}

	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pml.groundPlane = qfalse;
	pml.walking = qfalse;
}


/*
=============
PM_OverbounceLanding

A fall that ends in an upward bounce without the hull reaching the floor
counts as a landing. Only bounces that clipping against the floor can't
explain are caught, so ramp jumps still throw the player up.
=============
*/
static qboolean PM_OverbounceLanding( trace_t *result ) {
	trace_t		tr;
	trace_t		*trace = &tr;
	vec3_t		point, clipped;
	float		dist;

	if ( pml.jumped || pml.previous_velocity[2] >= 0 || pm->ps->velocity[2] < 0 ) {
		return qfalse;
	}

	dist = -pml.previous_velocity[2] * pml.frametime + 0.25f;
	VectorCopy( pm->ps->origin, point );
	point[2] -= dist;
	pm->trace( trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );
	if ( trace->allsolid || trace->fraction == 1.0f || trace->plane.normal[2] < MIN_WALK_NORMAL ) {
		return qfalse;
	}

	// sliding along this floor would not send the player up
	PM_ClipVelocity( pml.previous_velocity, trace->plane.normal, clipped, 1.0f );
	if ( clipped[2] > 1.0f ) {
		return qfalse;
	}

	if ( pm->debugLevel ) {
		Com_Printf("%i:overbounce\n", c_pmove);
	}
	VectorCopy( trace->endpos, pm->ps->origin );
	PM_ClipVelocity( pm->ps->velocity, trace->plane.normal, pm->ps->velocity, 1.0f );
	if ( pm->ps->velocity[2] > 0 ) {
		pm->ps->velocity[2] = 0;
	}
	*result = tr;
	return qtrue;
}

/*
=============
PM_GroundTrace
=============
*/
static void PM_GroundTrace( void ) {
	vec3_t		point;
	trace_t		trace;

	point[0] = pm->ps->origin[0];
	point[1] = pm->ps->origin[1];
	point[2] = pm->ps->origin[2] - 0.25;

	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
	pml.groundTrace = trace;

	// do something corrective if the trace starts in a solid...
	if ( trace.allsolid ) {
		if ( !PM_CorrectAllSolid(&trace) )
			return;
	}

	// a fall that bounced off the floor without touching it lands
	if ( ( trace.fraction == 1.0 || pm->ps->velocity[2] > 0 ) && PM_OverbounceLanding( &trace ) ) {
		pml.groundTrace = trace;
	}

	// if the trace didn't hit anything, we are in free fall
	if ( trace.fraction == 1.0 ) {
		PM_GroundTraceMissed();
		pml.groundPlane = qfalse;
		pml.walking = qfalse;
		return;
	}

	// check if getting thrown off the ground
	if ( pm->ps->velocity[2] > 0 && DotProduct( pm->ps->velocity, trace.plane.normal ) > 10 ) {
		if ( pm->debugLevel ) {
			Com_Printf("%i:kickoff\n", c_pmove);
		}
		// go into jump animation
		if ( pm->cmd.forwardmove >= 0 ) {
			PM_ForceLegsAnim( LEGS_JUMP );
			pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
		} else {
			PM_ForceLegsAnim( LEGS_JUMPB );
			pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
		}

		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane = qfalse;
		pml.walking = qfalse;
		return;
	}
	
	// slopes that are too steep will not be considered onground
	if ( trace.plane.normal[2] < MIN_WALK_NORMAL ) {
		if ( pm->debugLevel ) {
			Com_Printf("%i:steep\n", c_pmove);
		}
		// FIXME: if they can't slide down the slope, let them
		// walk (sharp crevices)
		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane = qtrue;
		pml.walking = qfalse;
		return;
	}

	pml.groundPlane = qtrue;
	pml.walking = qtrue;
	pm->ps->pm_flags &= ~PMF_AIRMOVE_USED;

	// hitting solid ground will end a waterjump
	if (pm->ps->pm_flags & PMF_TIME_WATERJUMP)
	{
		pm->ps->pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND);
		pm->ps->pm_time = 0;
	}

	if ( pm->ps->groundEntityNum == ENTITYNUM_NONE ) {
		// just hit the ground
		if ( pm->debugLevel ) {
			Com_Printf("%i:Land\n", c_pmove);
		}
		
		PM_CrashLand();
		pm->ps->stats[STAT_LAND_TIME] = 0;

		// don't do landing time if we were just going down a slope
		if ( pml.previous_velocity[2] < -200 ) {
			// don't allow another jump for a little while
			pm->ps->pm_flags |= PMF_TIME_LAND;
			pm->ps->pm_time = 250;
		}
	}

	pm->ps->groundEntityNum = trace.entityNum;

	// don't reset the z velocity for slopes
//	pm->ps->velocity[2] = 0;

	PM_AddTouchEnt( trace.entityNum );
}


/*
=============
PM_SetWaterLevel	FIXME: avoid this twice?  certainly if not moving
=============
*/
static void PM_SetWaterLevel( void ) {
	vec3_t		point;
	int			cont;
	int			sample1;
	int			sample2;

	//
	// get waterlevel, accounting for ducking
	//
	pm->waterlevel = 0;
	pm->watertype = 0;

	point[0] = pm->ps->origin[0];
	point[1] = pm->ps->origin[1];
	point[2] = pm->ps->origin[2] + MINS_Z + 1;	
	cont = pm->pointcontents( point, pm->ps->clientNum );

	if ( cont & MASK_WATER ) {
		sample2 = pm->ps->viewheight - MINS_Z;
		sample1 = sample2 / 2;

		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pm->ps->origin[2] + MINS_Z + sample1;
		cont = pm->pointcontents (point, pm->ps->clientNum );
		if ( cont & MASK_WATER ) {
			pm->waterlevel = 2;
			point[2] = pm->ps->origin[2] + MINS_Z + sample2;
			cont = pm->pointcontents (point, pm->ps->clientNum );
			if ( cont & MASK_WATER ){
				pm->waterlevel = 3;
			}
		}
	}

}

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->ps->viewheight
==============
*/
static void PM_CheckDuck (void)
{
	trace_t	trace;
	float	width;

	if ( pm->ps->powerups[PW_INVULNERABILITY] ) {
		if ( pm->ps->pm_flags & PMF_INVULEXPAND ) {
			// invulnerability sphere has a 42 units radius
			VectorSet( pm->mins, -INVUL_RADIUS, -INVUL_RADIUS, -INVUL_RADIUS );
			VectorSet( pm->maxs, INVUL_RADIUS, INVUL_RADIUS, INVUL_RADIUS );
		}
		else {
			VectorSet( pm->mins, -PLAYER_WIDTH, -PLAYER_WIDTH, MINS_Z );
			VectorSet( pm->maxs, PLAYER_WIDTH, PLAYER_WIDTH, 16 );
		}
		pm->ps->pm_flags |= PMF_DUCKED;
		pm->ps->viewheight = CROUCH_VIEWHEIGHT;
		return;
	}
	pm->ps->pm_flags &= ~PMF_INVULEXPAND;

	if ( PM_DroneActive() ) {
		width = bg_pmoveSettings.droneSize;
		VectorSet( pm->mins, -width, -width, -width );
		VectorSet( pm->maxs, width, width, width );
		pm->ps->pm_flags &= ~PMF_DUCKED;
		pm->ps->viewheight = 0;
		return;
	}

	width = bg_classInfo[pml.champion->bodyClass].halfWidth;
	pm->mins[0] = -width;
	pm->mins[1] = -width;

	pm->maxs[0] = width;
	pm->maxs[1] = width;

	pm->mins[2] = MINS_Z;

	if (pm->ps->pm_type == PM_DEAD)
	{
		pm->maxs[2] = DEAD_HEIGHT;
		pm->ps->viewheight = DEAD_VIEWHEIGHT;
		return;
	}

	if (pm->cmd.upmove < 0)
	{	// duck
		pm->ps->pm_flags |= PMF_DUCKED;
	}
	else
	{	// stand up if possible
		if (pm->ps->pm_flags & PMF_DUCKED)
		{
			// try to stand up
			pm->maxs[2] = DEFAULT_HEIGHT;
			pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask );
			if (!trace.allsolid)
				pm->ps->pm_flags &= ~PMF_DUCKED;
		}
	}

	if (pm->ps->pm_flags & PMF_DUCKED)
	{
		pm->maxs[2] = CROUCH_HEIGHT;
		pm->ps->viewheight = CROUCH_VIEWHEIGHT;

		// the eye sits above the crouched hull: keep it under a low ceiling
		{
			vec3_t	eye, small = { -4, -4, 0 }, smallMax = { 4, 4, 4 };

			VectorCopy( pm->ps->origin, eye );
			eye[2] += CROUCH_VIEWHEIGHT;
			pm->trace( &trace, pm->ps->origin, small, smallMax, eye, pm->ps->clientNum, MASK_SOLID );
			if ( trace.fraction < 1.0f ) {
				pm->ps->viewheight = (int)( trace.endpos[2] - pm->ps->origin[2] );
				if ( pm->ps->viewheight < 0 ) {
					pm->ps->viewheight = 0;
				}
			}
		}
	}
	else
	{
		pm->maxs[2] = DEFAULT_HEIGHT;
		pm->ps->viewheight = DEFAULT_VIEWHEIGHT;
	}
}



//===================================================================


/*
===============
PM_Footsteps
===============
*/
static void PM_Footsteps( void ) {
	float		bobmove;
	int			old;
	qboolean	footstep;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	pm->xyspeed = sqrt( pm->ps->velocity[0] * pm->ps->velocity[0]
		+  pm->ps->velocity[1] * pm->ps->velocity[1] );

	if ( pm->ps->groundEntityNum == ENTITYNUM_NONE ) {

		if ( pm->ps->powerups[PW_INVULNERABILITY] ) {
			PM_ContinueLegsAnim( LEGS_IDLECR );
		}
		// airborne leaves position in cycle intact, but doesn't advance
		if ( pm->waterlevel > 1 ) {
			PM_ContinueLegsAnim( LEGS_SWIM );
		}
		return;
	}

	// if not trying to move
	if ( !pm->cmd.forwardmove && !pm->cmd.rightmove ) {
		if (  pm->xyspeed < 5 ) {
			pm->ps->bobCycle = 0;	// start at beginning of cycle again
			if ( pm->ps->pm_flags & PMF_DUCKED ) {
				PM_ContinueLegsAnim( LEGS_IDLECR );
			} else {
				PM_ContinueLegsAnim( LEGS_IDLE );
			}
		}
		return;
	}
	

	footstep = qfalse;

	if ( pm->ps->pm_flags & PMF_DUCKED ) {
		bobmove = 0.5;	// ducked characters bob much faster
		if ( pm->ps->pm_flags & PMF_BACKWARDS_RUN ) {
			PM_ContinueLegsAnim( LEGS_BACKCR );
		}
		else {
			PM_ContinueLegsAnim( LEGS_WALKCR );
		}
		// ducked characters never play footsteps
	/*
	} else 	if ( pm->ps->pm_flags & PMF_BACKWARDS_RUN ) {
		if ( !( pm->cmd.buttons & BUTTON_WALKING ) ) {
			bobmove = 0.4;	// faster speeds bob faster
			footstep = qtrue;
		} else {
			bobmove = 0.3;
		}
		PM_ContinueLegsAnim( LEGS_BACK );
	*/
	} else {
		if ( !( pm->cmd.buttons & BUTTON_WALKING ) ) {
			bobmove = 0.4f;	// faster speeds bob faster
			if ( pm->ps->pm_flags & PMF_BACKWARDS_RUN ) {
				PM_ContinueLegsAnim( LEGS_BACK );
			}
			else {
				PM_ContinueLegsAnim( LEGS_RUN );
			}
			footstep = qtrue;
		} else {
			bobmove = 0.3f;	// walking bobs slow
			if ( pm->ps->pm_flags & PMF_BACKWARDS_RUN ) {
				PM_ContinueLegsAnim( LEGS_BACKWALK );
			}
			else {
				PM_ContinueLegsAnim( LEGS_WALK );
			}
		}
	}

	// check for footstep / splash sounds
	old = pm->ps->bobCycle;
	pm->ps->bobCycle = (int)( old + bobmove * pml.msec ) & 255;

	// if we just crossed a cycle boundary, play an appropriate footstep event
	if ( ( ( old + 64 ) ^ ( pm->ps->bobCycle + 64 ) ) & 128 ) {
		if ( pm->waterlevel == 0 ) {
			// on ground will only play sounds if running
			if ( footstep && !pm->noFootsteps ) {
				PM_AddEvent( PM_FootstepForSurface() );
			}
		} else if ( pm->waterlevel == 1 ) {
			// splashing
			PM_AddEvent( EV_FOOTSPLASH );
		} else if ( pm->waterlevel == 2 ) {
			// wading / swimming at surface
			PM_AddEvent( EV_SWIM );
		} else if ( pm->waterlevel == 3 ) {
			// no sound when completely underwater

		}
	}
}

/*
==============
PM_WaterEvents

Generate sound events for entering and leaving water
==============
*/
static void PM_WaterEvents( void ) {		// FIXME?
	//
	// if just entered a water volume, play a sound
	//
	if (!pml.previous_waterlevel && pm->waterlevel) {
		PM_AddEvent( EV_WATER_TOUCH );
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (pml.previous_waterlevel && !pm->waterlevel) {
		PM_AddEvent( EV_WATER_LEAVE );
	}

	//
	// check for head just going under water
	//
	if (pml.previous_waterlevel != 3 && pm->waterlevel == 3) {
		PM_AddEvent( EV_WATER_UNDER );
	}

	//
	// check for head just coming out of water
	//
	if (pml.previous_waterlevel == 3 && pm->waterlevel != 3) {
		PM_AddEvent( EV_WATER_CLEAR );
	}
}


/*
===============
PM_SwitchTime

Milliseconds to lower or raise a weapon
===============
*/
static int PM_SwitchTime( void ) {
	return ( pml.champion->passives & PASSIVE_DRILLSERGEANT ) ? 100 : 150;	// 150 to lower, 150 to raise
}

/*
===============
PM_BeginWeaponChange
===============
*/
static void PM_BeginWeaponChange( int weapon ) {
	if ( weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS ) {
		return;
	}

	if ( !( pm->ps->stats[STAT_WEAPONS] & ( 1 << weapon ) ) ) {
		return;
	}
	
	if ( pm->ps->weaponstate == WEAPON_DROPPING ) {
		return;
	}

	PM_AddEvent( EV_CHANGE_WEAPON );
	pm->ps->weaponstate = WEAPON_DROPPING;
	pm->ps->weaponTime += PM_SwitchTime() * PM_WeaponTimeScale();
	PM_StartTorsoAnim( TORSO_DROP );
}


/*
===============
PM_FinishWeaponChange
===============
*/
static void PM_FinishWeaponChange( void ) {
	int		weapon;

	weapon = pm->cmd.weapon;
	if ( weapon < WP_NONE || weapon >= WP_NUM_WEAPONS ) {
		weapon = WP_NONE;
	}

	if ( !( pm->ps->stats[STAT_WEAPONS] & ( 1 << weapon ) ) ) {
		weapon = WP_NONE;
	}

	pm->ps->weapon = weapon;
	pm->ps->weaponstate = WEAPON_RAISING;
	pm->ps->weaponTime += PM_SwitchTime() * PM_WeaponTimeScale();		// raise as long as the lower
	PM_StartTorsoAnim( TORSO_RAISE );
}


/*
==============
PM_TorsoAnimation

==============
*/
static void PM_TorsoAnimation( void ) {
	if ( pm->ps->weaponstate == WEAPON_READY ) {
		if ( pm->ps->weapon == WP_GAUNTLET ) {
			PM_ContinueTorsoAnim( TORSO_STAND2 );
		} else {
			PM_ContinueTorsoAnim( TORSO_STAND );
		}
		return;
	}
}


/*
==============
PM_Weapon

Generates weapon events and modifes the weapon counter
==============
*/
static void PM_Weapon( void ) {
	int		addTime;

	// no firing or switching during a bull rush or ghost walk
	if ( BG_AbilityState( pm->ps ) == ABILITY_RUSH || PM_AbilityActive( ACTIVE_GHOSTWALK ) ) {
		return;
	}

	// the drone has its own gun: machinegun rate set by pm_droneFireInterval, no ammo
	if ( PM_DroneActive() ) {
		pm->cmd.weapon = WP_MACHINEGUN;
		if ( pm->ps->ammo[WP_MACHINEGUN] < 1 ) {
			pm->ps->ammo[WP_MACHINEGUN] = 1;
		}
	}

	// berserk is fists only
	if ( pml.champion->active == ACTIVE_BERSERK
		&& ( PM_AbilityActive( ACTIVE_BERSERK ) || BG_AbilityState( pm->ps ) == ABILITY_WINDUP ) ) {
		pm->cmd.weapon = WP_GAUNTLET;

		// the jump strike: a punch in the air lunges where the player looks,
		// once per time off the ground
		if ( PM_AbilityActive( ACTIVE_BERSERK ) && pm->ps->groundEntityNum == ENTITYNUM_NONE
			&& ( pm->cmd.buttons & BUTTON_ATTACK ) && pm->ps->weaponTime <= 0
			&& !( pm->ps->pm_flags & PMF_AIRMOVE_USED ) ) {
			vec3_t	forward;

			AngleVectors( pm->ps->viewangles, forward, NULL, NULL );
			VectorScale( forward, bg_pmoveSettings.berserkLunge, pm->ps->velocity );
			pm->ps->pm_flags |= PMF_AIRMOVE_USED;
		}
	}

	// don't allow attack until all buttons are up
	if ( pm->ps->pm_flags & PMF_RESPAWNED ) {
		return;
	}

	// ignore if spectator
	if ( pm->ps->persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return;
	}

	// check for dead player
	if ( pm->ps->stats[STAT_HEALTH] <= 0 ) {
		pm->ps->weapon = WP_NONE;
		return;
	}

	// check for item using
	if ( pm->cmd.buttons & BUTTON_USE_HOLDABLE ) {
		if ( ! ( pm->ps->pm_flags & PMF_USE_ITEM_HELD ) ) {
			if ( bg_itemlist[pm->ps->stats[STAT_HOLDABLE_ITEM]].giTag == HI_MEDKIT
				&& pm->ps->stats[STAT_HEALTH] >= (pm->ps->stats[STAT_MAX_HEALTH] + 25) ) {
				// don't use medkit if at max health
			} else {
				pm->ps->pm_flags |= PMF_USE_ITEM_HELD;
				PM_AddEvent( EV_USE_ITEM0 + bg_itemlist[pm->ps->stats[STAT_HOLDABLE_ITEM]].giTag );
				pm->ps->stats[STAT_HOLDABLE_ITEM] = 0;
			}
			return;
		}
	} else {
		pm->ps->pm_flags &= ~PMF_USE_ITEM_HELD;
	}


	// make weapon function
	if ( pm->ps->weaponTime > 0 ) {
		pm->ps->weaponTime -= pml.msec;
	}

	// check for weapon change
	// can't change if weapon is firing, but can change
	// again if lowering or raising
	if ( pm->ps->weaponTime <= 0 || pm->ps->weaponstate != WEAPON_FIRING ) {
		if ( pm->ps->weapon != pm->cmd.weapon ) {
			PM_BeginWeaponChange( pm->cmd.weapon );
		}
	}

	if ( pm->ps->weaponTime > 0 ) {
		return;
	}

	// change weapon if time
	if ( pm->ps->weaponstate == WEAPON_DROPPING ) {
		PM_FinishWeaponChange();
		return;
	}

	if ( pm->ps->weaponstate == WEAPON_RAISING ) {
		pm->ps->weaponstate = WEAPON_READY;
		if ( pm->ps->weapon == WP_GAUNTLET ) {
			PM_StartTorsoAnim( TORSO_STAND2 );
		} else {
			PM_StartTorsoAnim( TORSO_STAND );
		}
		return;
	}

	// check for fire
	if ( ! (pm->cmd.buttons & BUTTON_ATTACK) ) {
		pm->ps->weaponTime = 0;
		pm->ps->weaponstate = WEAPON_READY;
		return;
	}

	// start the animation even if out of ammo
	if ( pm->ps->weapon == WP_GAUNTLET ) {
		// the guantlet only "fires" when it actually hits something
		if ( !pm->gauntletHit ) {
			pm->ps->weaponTime = 0;
			pm->ps->weaponstate = WEAPON_READY;
			return;
		}
		PM_StartTorsoAnim( TORSO_ATTACK2 );
	} else {
		PM_StartTorsoAnim( TORSO_ATTACK );
	}

	pm->ps->weaponstate = WEAPON_FIRING;

	// check for out of ammo
	if ( ! pm->ps->ammo[ pm->ps->weapon ] ) {
		PM_AddEvent( EV_NOAMMO );
		pm->ps->weaponTime += 500;
		return;
	}

	// take an ammo away if not infinite; paired weapons share the pool
	if ( pm->ps->ammo[ pm->ps->weapon ] != -1 && !PM_DroneActive() ) {
		pm->ps->ammo[ pm->ps->weapon ]--;
		if ( BG_AmmoPartner( pm->ps->weapon ) != WP_NONE ) {
			pm->ps->ammo[ BG_AmmoPartner( pm->ps->weapon ) ] = pm->ps->ammo[ pm->ps->weapon ];
		}
	}

	// fire weapon
	PM_AddEvent( EV_FIRE_WEAPON );

	switch( pm->ps->weapon ) {
	default:
	case WP_GAUNTLET:
		addTime = 400;
		break;
	case WP_LIGHTNING:
		addTime = 50;
		break;
	case WP_SHOTGUN:
		addTime = 1000;
		break;
	case WP_MACHINEGUN:
		addTime = PM_DroneActive() ? bg_pmoveSettings.droneFireInterval : 100;
		break;
	case WP_GRENADE_LAUNCHER:
		addTime = 800;
		break;
	case WP_ROCKET_LAUNCHER:
		addTime = 800;
		break;
	case WP_PLASMAGUN:
		addTime = 100;
		break;
	case WP_RAILGUN:
		addTime = 1500;
		break;
	case WP_BFG:
		addTime = 200;
		break;
	case WP_GRAPPLING_HOOK:
		addTime = 400;
		break;
#ifdef MISSIONPACK
	case WP_NAILGUN:
		addTime = 1000;
		break;
	case WP_PROX_LAUNCHER:
		addTime = 800;
		break;
	case WP_CHAINGUN:
		addTime = 30;
		break;
#endif
	}

	// tuned weapons take their refire from the weapon table
	if ( BG_WeaponTuned( pm->ps->weapon ) && !PM_DroneActive() ) {
		addTime = BG_WeaponParam( pm->ps->weapon, WPP_REFIRE );
		if ( pm->ps->weapon == WP_HEAVY_MACHINEGUN && ( pm->cmd.buttons & BUTTON_ZOOM ) ) {
			addTime = bg_pmoveSettings.hmgZoomRefire;
		}
	}

#ifdef MISSIONPACK
	if( bg_itemlist[pm->ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_SCOUT ) {
		addTime /= 1.5;
	}
	else
	if( bg_itemlist[pm->ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_AMMOREGEN ) {
		addTime /= 1.3;
	}
	else
#endif
	if ( pm->ps->powerups[PW_HASTE] ) {
		addTime /= 1.3;
	}

	pm->ps->weaponTime += addTime * PM_WeaponTimeScale();
}

/*
================
PM_Animate
================
*/

static void PM_Animate( void ) {
	if ( pm->cmd.buttons & BUTTON_GESTURE ) {
		if ( pm->ps->torsoTimer == 0 ) {
			PM_StartTorsoAnim( TORSO_GESTURE );
			pm->ps->torsoTimer = TIMER_GESTURE;
			PM_AddEvent( EV_TAUNT );
		}
#ifdef MISSIONPACK
	} else if ( pm->cmd.buttons & BUTTON_GETFLAG ) {
		if ( pm->ps->torsoTimer == 0 ) {
			PM_StartTorsoAnim( TORSO_GETFLAG );
			pm->ps->torsoTimer = 600;	//TIMER_GESTURE;
		}
	} else if ( pm->cmd.buttons & BUTTON_GUARDBASE ) {
		if ( pm->ps->torsoTimer == 0 ) {
			PM_StartTorsoAnim( TORSO_GUARDBASE );
			pm->ps->torsoTimer = 600;	//TIMER_GESTURE;
		}
	} else if ( pm->cmd.buttons & BUTTON_PATROL ) {
		if ( pm->ps->torsoTimer == 0 ) {
			PM_StartTorsoAnim( TORSO_PATROL );
			pm->ps->torsoTimer = 600;	//TIMER_GESTURE;
		}
	} else if ( pm->cmd.buttons & BUTTON_FOLLOWME ) {
		if ( pm->ps->torsoTimer == 0 ) {
			PM_StartTorsoAnim( TORSO_FOLLOWME );
			pm->ps->torsoTimer = 600;	//TIMER_GESTURE;
		}
	} else if ( pm->cmd.buttons & BUTTON_AFFIRMATIVE ) {
		if ( pm->ps->torsoTimer == 0 ) {
			PM_StartTorsoAnim( TORSO_AFFIRMATIVE);
			pm->ps->torsoTimer = 600;	//TIMER_GESTURE;
		}
	} else if ( pm->cmd.buttons & BUTTON_NEGATIVE ) {
		if ( pm->ps->torsoTimer == 0 ) {
			PM_StartTorsoAnim( TORSO_NEGATIVE );
			pm->ps->torsoTimer = 600;	//TIMER_GESTURE;
		}
#endif
	}
}


/*
================
PM_MoveTimers

Jump, landing and slide charge timers kept in stats
================
*/
static void PM_MoveTimers( void ) {
	int		charge, air;

	pm->ps->stats[STAT_JUMP_TIME] += pml.msec;
	if ( pm->ps->stats[STAT_JUMP_TIME] > STAT_TIME_CAP ) {
		pm->ps->stats[STAT_JUMP_TIME] = STAT_TIME_CAP;
	}
	if ( pm->ps->stats[STAT_LAND_TIME] >= 0 ) {		// below 0: launched by a jump pad
		pm->ps->stats[STAT_LAND_TIME] += pml.msec;
	}
	if ( pm->ps->stats[STAT_LAND_TIME] > STAT_TIME_CAP ) {
		pm->ps->stats[STAT_LAND_TIME] = STAT_TIME_CAP;
	}
	pm->ps->persistant[PERS_STEP_TIME] += pml.msec;
	if ( pm->ps->persistant[PERS_STEP_TIME] > STAT_TIME_CAP ) {
		pm->ps->persistant[PERS_STEP_TIME] = STAT_TIME_CAP;
	}

	if ( pml.champion->moveStyle != MOVE_SLIDE ) {
		return;		// STAT_MOVE_STATE belongs to another mechanic
	}

	charge = BG_SlideCharge( pm->ps );
	air = BG_SlideAirTime( pm->ps );
	if ( pm->ps->groundEntityNum == ENTITYNUM_NONE ) {
		// in the air: falling without holding crouch charges the slide
		if ( pm->ps->velocity[2] < 0 && pm->cmd.upmove >= 0 ) {
			charge += bg_pmoveSettings.slideChargeRate * pml.msec;
		}
		air += pml.msec;
		// a slide that stays in the air too long is over (a bump or small drop isn't)
		if ( air > PM_SlideAirGrace() && air - pml.msec <= PM_SlideAirGrace()
			&& pm->cmd.upmove < 0 ) {
			charge = 0;
		}
	} else {
		// on the ground without crouch held the charge is gone: no slide
		// starts, and letting go of crouch ends one for good
		if ( pm->cmd.upmove >= 0 ) {
			charge = 0;
		}
		air = 0;
		charge -= bg_pmoveSettings.slideDrain * pml.msec;
	}
	if ( charge > PM_SlideChargeMax() ) {
		charge = PM_SlideChargeMax();
	}
	if ( charge < 0 ) {
		charge = 0;
	}
	if ( air > 31 * BG_SLIDE_AIR_UNIT ) {
		air = 31 * BG_SLIDE_AIR_UNIT;
	}
	pm->ps->stats[STAT_MOVE_STATE] = BG_SlideState( charge, air );
}

/*
================
PM_FlatDirection

Horizontal unit vector for a movement direction relative to the view yaw:
0 forward, 1 back, 2 left, 3 right
================
*/
static void PM_FlatDirection( int which, vec3_t dir ) {
	vec3_t	angles, forward, right;

	VectorSet( angles, 0, pm->ps->viewangles[YAW], 0 );
	AngleVectors( angles, forward, right, NULL );
	switch ( which ) {
	case 0:		VectorCopy( forward, dir ); break;
	case 1:		VectorNegate( forward, dir ); break;
	case 2:		VectorNegate( right, dir ); break;
	default:	VectorCopy( right, dir ); break;
	}
	dir[2] = 0;
	VectorNormalize( dir );
}

/*
================
PM_AirDodge

Double-tapping a movement direction dashes that way while airborne,
once per airtime
================
*/
#define DODGE_STAMINA_UNIT	40		// one dodge; refills at 10 units a second
#define DODGE_STAMINA_MAX	120

static void PM_AirDodge( void ) {
	int		taps, held, last, time, fresh, i, stamina;
	vec3_t	dir;
	float	along;

	if ( !( pml.champion->abilities & MOVEABIL_AIRDODGE ) ) {
		return;
	}

	// stamina: up to 3 dodges, a quarter charge back each second (whole
	// steps of 100 ms, so prediction and server always agree)
	stamina = pm->ps->generic1 + ( pm->cmd.serverTime / 100 - pm->ps->commandTime / 100 );
	if ( stamina > DODGE_STAMINA_MAX ) {
		stamina = DODGE_STAMINA_MAX;
	}
	pm->ps->generic1 = stamina;

	taps = pm->ps->stats[STAT_TAPS] & 0xffff;
	last = ( taps & TAP_DIR_MASK ) >> TAP_DIR_SHIFT;
	time = taps >> TAP_TIME_SHIFT;
	time += pml.msec;
	if ( time > TAP_TIME_MAX ) {
		time = TAP_TIME_MAX;
	}

	held = 0;
	if ( pm->cmd.forwardmove > 0 ) held |= 1;
	if ( pm->cmd.forwardmove < 0 ) held |= 2;
	if ( pm->cmd.rightmove < 0 ) held |= 4;
	if ( pm->cmd.rightmove > 0 ) held |= 8;
	fresh = held & ~( taps & TAP_HELD_MASK );

	for ( i = 0 ; i < 4 ; i++ ) {
		if ( !( fresh & ( 1 << i ) ) ) {
			continue;
		}
		if ( last == i + 1 && time <= bg_pmoveSettings.dodgeTapWindow && time >= 90
			&& pm->ps->generic1 >= DODGE_STAMINA_UNIT ) {
			// dash on the ground or in the air, for a charge of stamina
			float	speed = pm->ps->groundEntityNum == ENTITYNUM_NONE
				? bg_pmoveSettings.dodgeSpeed : bg_pmoveSettings.dodgeGroundSpeed;
			float	up = pm->ps->groundEntityNum == ENTITYNUM_NONE
				? bg_pmoveSettings.dodgeUp : bg_pmoveSettings.dodgeGroundUp;

			PM_FlatDirection( i, dir );
			along = DotProduct( pm->ps->velocity, dir );
			if ( along < speed ) {
				VectorMA( pm->ps->velocity, speed - along, dir, pm->ps->velocity );
			}
			if ( up > 0 && pm->ps->velocity[2] < speed * up ) {
				pm->ps->velocity[2] = speed * up;
			}
			pm->ps->groundEntityNum = ENTITYNUM_NONE;
			pm->ps->generic1 -= DODGE_STAMINA_UNIT;
			PM_AddEvent( EV_JUMP );
			last = 0;
		} else {
			last = i + 1;
			time = 0;
		}
	}

	pm->ps->stats[STAT_TAPS] = held | ( last << TAP_DIR_SHIFT ) | ( time << TAP_TIME_SHIFT );
}

/*
================
PM_Acceleration

Running forward in a straight line slowly raises the run speed;
turning, strafing or slowing down on the ground loses the bonus
================
*/
static void PM_Acceleration( void ) {
	// speed steps: each step's top speed, acceleration toward it (u/s per
	// second) and how long the previous top speed must be held first (ms)
	static const int	clutchTop[] = { 350, 400, 450 };
	static const int	clutchAccel[] = { 50, 40, 30 };
	static const int	clutchHold[] = { 0, 1250, 1000 };
	static const int	heavyTop[] = { 450 };
	static const int	heavyAccel[] = { 50 };
	static const int	heavyHold[] = { 0 };
	const int	*top, *accel, *hold;
	int		steps, state, bonus, held, i, maxBonus;
	vec3_t	flatvel, forward;
	float	speed, run;

	if ( !( pml.champion->abilities & MOVEABIL_ACCELERATION ) ) {
		return;
	}
	if ( pml.champion->abilities & MOVEABIL_HEAVYWEIGHT ) {
		top = heavyTop; accel = heavyAccel; hold = heavyHold; steps = 1;
	} else {
		top = clutchTop; accel = clutchAccel; hold = clutchHold; steps = 3;
	}

	state = pm->ps->stats[STAT_MOVE_STATE] & 0xffff;
	bonus = state & ACCEL_BONUS_MASK;
	held = state >> ACCEL_HOLD_SHIFT;

	if ( pml.walking ) {
		VectorSet( flatvel, pm->ps->velocity[0], pm->ps->velocity[1], 0 );
		speed = VectorNormalize( flatvel );
		PM_FlatDirection( 0, forward );
		if ( pm->cmd.forwardmove > 0 && pm->cmd.rightmove == 0
			&& speed >= pm->ps->speed * 0.9f
			&& DotProduct( flatvel, forward ) >= cos( DEG2RAD( bg_pmoveSettings.accelAngle ) ) ) {
			run = pm->ps->speed + bonus * 0.1f;
			for ( i = 0; i < steps - 1 && run >= top[i]; i++ ) {
			}
			if ( run < top[i] && ( i == 0 || held * ACCEL_HOLD_UNIT >= hold[i] ) ) {
				// climbing toward the next step
				bonus += (int)( accel[i] * 10 * pml.frametime + 0.5f );
				if ( pm->ps->speed + bonus * 0.1f > top[i] ) {
					bonus = ( top[i] - pm->ps->speed ) * 10;
				}
				held = 0;
			} else if ( run < top[i] ) {
				// holding the previous step's top speed (whole steps of
				// time, so prediction and server agree)
				held += pm->cmd.serverTime / ACCEL_HOLD_UNIT - pm->ps->commandTime / ACCEL_HOLD_UNIT;
				if ( held > 15 ) {
					held = 15;
				}
			}
		} else {
			bonus = 0;
			held = 0;
		}
	}

	maxBonus = ( top[steps - 1] - pm->ps->speed ) * 10;
	if ( bonus > maxBonus ) {
		bonus = maxBonus;
	}
	if ( bonus < 0 ) {
		bonus = 0;
	}
	pm->ps->stats[STAT_MOVE_STATE] = bonus | ( held << ACCEL_HOLD_SHIFT );
	pml.runSpeed = pm->ps->speed + bonus * 0.1f;
}

/*
================
PM_WeaponLockout
================
*/
static void PM_WeaponLockout( int active ) {
	int		lockout = BG_AbilityParam( active, AP_LOCKOUT );

	if ( pm->ps->weaponTime < lockout ) {
		pm->ps->weaponTime = lockout;
	}
}

/*
================
PM_EndAbility
================
*/
static void PM_EndAbility( void ) {
	int		active = pml.champion->active;

	BG_SetAbility( pm->ps, ABILITY_IDLE, BG_AbilityCharges( pm->ps ) );
	pm->ps->stats[STAT_ABILITY_TIME] = 0;
	pm->ps->pm_flags &= ~PMF_GRAPPLE_PULL;
	if ( bg_abilityInfo[active].lockoutAtEnd ) {
		PM_WeaponLockout( active );
	}
}

// the hook counts as blocked at a surface near its point when, after this
// long, Athena is this close and moving toward it slower than this
#define HOOK_BLOCKED_MSEC		200
#define HOOK_BLOCKED_RANGE		128
#define HOOK_BLOCKED_SPEED		50
#define HOOK_MAX_ROPE			3000	// let go beyond this much rope

/*
================
PM_StartAbility

The press delay is over: the ability takes effect
================
*/
static void PM_StartAbility( int *state, int *time ) {
	int		active = pml.champion->active;

	switch ( active ) {
	case ACTIVE_HOOK:
		*state = ABILITY_HOOK_FLYING;
		*time = 0;
		PM_AddEvent( EV_USE_ABILITY );
		break;
	case ACTIVE_BULLRUSH:
		*state = ABILITY_RUSH;
		*time = BG_AbilityParam( active, AP_DURATION );
		break;
	default:
		PM_AddEvent( EV_USE_ABILITY );
		*time = BG_AbilityParam( active, AP_DURATION );
		*state = *time > 0 ? ABILITY_ACTIVE : ABILITY_IDLE;
		if ( active == ACTIVE_DRONE ) {
			// the drone takes off from eye height; the game leaves the body below
			pm->ps->origin[2] += DEFAULT_VIEWHEIGHT;
			VectorClear( pm->ps->velocity );
		}
		break;
	}
}

/*
================
PM_Ability

Active ability on BUTTON_ABILITY. Charges are spent here and given back by
the game; effects outside movement (projectiles, heals, the hook entity)
happen in the game on EV_USE_ABILITY and EV_ABILITY_REPRESS.
================
*/
#define HOOK_THROW_GAP		1000	// msec between hook throws
#define GRENADE_THROW_GAP	400		// msec between Keel's grenades
#define BERSERK_MIN_TIME	2000	// Berserk can be ended after this long

static void PM_Ability( void ) {
	int			state, charges, time, active, gap;
	qboolean	pressed;
	vec3_t		dir;

	pml.abilityHeld = ( pm->cmd.buttons & BUTTON_ABILITY ) != 0;
	pressed = pml.abilityHeld && !( pm->ps->pm_flags & PMF_ABILITY_HELD );
	if ( pml.abilityHeld ) {
		pm->ps->pm_flags |= PMF_ABILITY_HELD;
	} else {
		pm->ps->pm_flags &= ~PMF_ABILITY_HELD;
	}

	active = pml.champion->active;
	if ( active == ACTIVE_NONE || pm->ps->pm_type != PM_NORMAL ) {
		return;
	}
	if ( pm->movementOnly && !BG_MovementAbility( active ) ) {
		return;
	}

	state = BG_AbilityState( pm->ps );
	charges = BG_AbilityCharges( pm->ps );
	time = pm->ps->stats[STAT_ABILITY_TIME];

	if ( state == ABILITY_IDLE ) {
		// some abilities wait a moment between uses; the idle time counts up
		gap = active == ACTIVE_HOOK ? HOOK_THROW_GAP : active == ACTIVE_GRENADES ? GRENADE_THROW_GAP : 0;
		if ( gap && time < gap ) {
			time += pml.msec;
			pm->ps->stats[STAT_ABILITY_TIME] = time;
		}
		if ( !pressed || charges <= 0 || ( pm->ps->pm_flags & PMF_RESPAWNED ) || time < gap ) {
			return;
		}
		// shell-shocked: no ability can be started
		if ( pm->ps->powerups[PW_SHELLSHOCK] > pm->cmd.serverTime ) {
			return;
		}
		// a flag carrier must stay visible: no ghost walk, no leaving the body for the drone
		if ( ( active == ACTIVE_GHOSTWALK || active == ACTIVE_DRONE )
			&& ( pm->ps->powerups[PW_REDFLAG] || pm->ps->powerups[PW_BLUEFLAG] || pm->ps->powerups[PW_NEUTRALFLAG] ) ) {
			return;
		}
		charges--;
		if ( !bg_abilityInfo[active].lockoutAtEnd ) {
			PM_WeaponLockout( active );
		}
		PM_StartAbility( &state, &time );
	} else if ( state == ABILITY_WINDUP ) {
		time -= pml.msec;
		if ( time <= 0 ) {
			PM_StartAbility( &state, &time );
		}
	} else if ( state == ABILITY_HOOK_FLYING ) {
		if ( pm->ps->pm_flags & PMF_GRAPPLE_PULL ) {
			// just attached: initial impulse toward the hook
			state = ABILITY_HOOK_PULL;
			time = 0;
			pm->ps->stats[STAT_MOVE_STATE] = 0;
			VectorSubtract( pm->ps->grapplePoint, pm->ps->origin, dir );
			VectorNormalize( dir );
			VectorMA( pm->ps->velocity, bg_pmoveSettings.hookImpulse, dir, pm->ps->velocity );
		} else if ( pressed ) {
			state = ABILITY_IDLE;
		}
	} else if ( state == ABILITY_HOOK_PULL ) {
		qboolean	arrived = qfalse;

		// pulled in (not swinging): let go at the dead zone, or when a surface
		// near the point stops the pull, so no second press is needed
		if ( !pml.abilityHeld ) {
			float	dist, radial;

			VectorSubtract( pm->ps->grapplePoint, pm->ps->origin, dir );
			dist = VectorNormalize( dir );
			radial = DotProduct( pm->ps->velocity, dir );
			if ( dist <= bg_pmoveSettings.hookDeadZone || dist > HOOK_MAX_ROPE
				|| ( time > HOOK_BLOCKED_MSEC && dist < HOOK_BLOCKED_RANGE && radial < HOOK_BLOCKED_SPEED ) ) {
				arrived = qtrue;
			}
		}
		if ( pressed || arrived || !( pm->ps->pm_flags & PMF_GRAPPLE_PULL ) ) {
			// let go and keep the momentum
			BG_SetAbility( pm->ps, ABILITY_IDLE, charges );
			PM_EndAbility();
			return;
		}
		time += pml.msec;
		if ( time > STAT_TIME_CAP ) {
			time = STAT_TIME_CAP;
		}
	} else if ( state == ABILITY_RUSH || state == ABILITY_ACTIVE ) {
		if ( pressed && state == ABILITY_ACTIVE ) {
			// Berserk can be ended early once it has run a while
			if ( active == ACTIVE_BERSERK && BG_AbilityParam( active, AP_DURATION ) - time >= BERSERK_MIN_TIME ) {
				BG_SetAbility( pm->ps, ABILITY_IDLE, charges );
				PM_EndAbility();
				return;
			}
			// a second press works once the ability has run its gate time
			if ( BG_AbilityParam( active, AP_DURATION ) - time >= BG_AbilityParam( active, AP_DELAY ) ) {
				PM_AddEvent( EV_ABILITY_REPRESS );
			}
		}
		time -= pml.msec;
		if ( time <= 0 ) {
			BG_SetAbility( pm->ps, ABILITY_IDLE, charges );
			PM_EndAbility();
			return;
		}
	}

	BG_SetAbility( pm->ps, state, charges );
	pm->ps->stats[STAT_ABILITY_TIME] = time;
}

/*
================
PM_AbilityActive

True while the given active ability is running
================
*/
static qboolean PM_AbilityActive( int active ) {
	return pml.champion->active == active && BG_AbilityState( pm->ps ) == ABILITY_ACTIVE;
}

/*
================
PM_AbilitySpeed

Run speed changes from timed abilities
================
*/
static void PM_AbilitySpeed( void ) {
	if ( PM_AbilityActive( ACTIVE_INJECTION ) ) {
		pml.runSpeed *= bg_pmoveSettings.injectionSpeed;
	} else if ( PM_AbilityActive( ACTIVE_BERSERK ) ) {
		pml.runSpeed = bg_pmoveSettings.berserkSpeed;
	} else if ( PM_AbilityActive( ACTIVE_PLASMATRAIL ) ) {
		pml.runSpeed *= bg_pmoveSettings.trailSpeed;
	} else if ( PM_AbilityActive( ACTIVE_PIERCINGSIGHT ) ) {
		pml.runSpeed *= 1.1f;		// Visor runs a little faster while looking
	}
}

/*
================
PM_DroneActive
================
*/
static qboolean PM_DroneActive( void ) {
	return PM_AbilityActive( ACTIVE_DRONE );
}

/*
================
PM_DroneMove

The drone flies where the player looks, without gravity
================
*/
static void PM_DroneMove( void ) {
	vec3_t	wishvel, wishdir;
	float	wishspeed, scale, speed, newspeed;
	int		i;

	// air brake like a spectator, so it stops when let go
	speed = VectorLength( pm->ps->velocity );
	if ( speed > 0 ) {
		newspeed = speed - speed * pm_flyfriction * pml.frametime;
		if ( newspeed < 0 ) {
			newspeed = 0;
		}
		VectorScale( pm->ps->velocity, newspeed / speed, pm->ps->velocity );
	}

	scale = PM_CmdScale( &pm->cmd );
	if ( scale ) {
		scale *= bg_pmoveSettings.droneSpeed / pml.runSpeed;
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
	}
	wishvel[2] += scale * pm->cmd.upmove;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	PM_Accelerate( wishdir, wishspeed, pm_flyaccelerate );
	PM_SlideMove( qfalse );
}

/*
================
PM_ClassAirAccelerate

Air acceleration for the champion's body class, scaled by pm_airAccelerate
================
*/
static float PM_ClassAirAccelerate( void ) {
	float	accel;

	if ( pml.champion->passives & PASSIVE_FULLSPRINT ) {
		accel = bg_pmoveSettings.airAccelFullSprint;
	} else if ( pml.champion->passives & PASSIVE_SLIPSTREAM ) {
		accel = 1.3f;
	} else if ( pml.champion->bodyClass == CLASS_LIGHT ) {
		accel = bg_pmoveSettings.airAccelLight;
	} else if ( pml.champion->bodyClass == CLASS_HEAVY ) {
		accel = bg_pmoveSettings.airAccelHeavy;
	} else {
		accel = bg_pmoveSettings.airAccelMedium;
	}
	if ( pml.champion->abilities & MOVEABIL_ISOAIR ) {
		accel *= bg_pmoveSettings.isoAirAccel;
	}
	return accel * bg_pmoveSettings.airAccelerate;
}

/*
================
PM_WeaponTimeScale

Dual wield fires and switches faster
================
*/
static float PM_WeaponTimeScale( void ) {
	if ( PM_AbilityActive( ACTIVE_DUALWIELD ) && bg_pmoveSettings.dualWieldRate > 0 ) {
		if ( pm->ps->weapon == WP_TRIBOLT || pm->ps->weapon == WP_ROCKET_LAUNCHER ) {
			return 1.0f / bg_pmoveSettings.dualWieldRateHeavy;
		}
		return 1.0f / bg_pmoveSettings.dualWieldRate;
	}
	return 1.0f;
}

/*
================
PM_RushTurn

Bull rush turns slowly: limit how far the yaw may move this frame
================
*/
static void PM_RushTurn( void ) {
	short	temp;
	float	want, diff, maxTurn;

	temp = pm->cmd.angles[YAW] + pm->ps->delta_angles[YAW];
	want = SHORT2ANGLE( temp );
	diff = AngleSubtract( want, pm->ps->viewangles[YAW] );
	maxTurn = bg_pmoveSettings.rushTurnRate * pml.frametime;
	if ( diff > maxTurn ) {
		diff = maxTurn;
	} else if ( diff < -maxTurn ) {
		diff = -maxTurn;
	}
	pm->ps->delta_angles[YAW] = ANGLE2SHORT( pm->ps->viewangles[YAW] + diff ) - pm->cmd.angles[YAW];
}

/*
================
PM_RushMove

Drives the player forward: the start speed, gaining speed each second up to
the rush speed
================
*/
static void PM_RushMove( void ) {
	vec3_t	forward;
	float	elapsed;

	elapsed = ( BG_AbilityParam( ACTIVE_BULLRUSH, AP_DURATION ) - pm->ps->stats[STAT_ABILITY_TIME] ) * 0.001f;
	if ( elapsed < 0 ) {
		elapsed = 0;
	}
	pml.rushSpeed = bg_pmoveSettings.rushStartSpeed + bg_pmoveSettings.rushAccel * elapsed;
	if ( pml.rushSpeed > bg_pmoveSettings.rushSpeed ) {
		pml.rushSpeed = bg_pmoveSettings.rushSpeed;
	}

	PM_FlatDirection( 0, forward );
	pm->ps->velocity[0] = forward[0] * pml.rushSpeed;
	pm->ps->velocity[1] = forward[1] * pml.rushSpeed;

	pm->cmd.forwardmove = 127;
	pm->cmd.rightmove = 0;
}

/*
================
PM_CheckRushBlocked

A wall or object that slows the rush below the minimum speed ends it; players are left to the game,
which ends the rush only if the player survives the hit
================
*/
static void PM_CheckRushBlocked( void ) {
	vec3_t	forward;
	int		i;

	if ( pml.rushSpeed <= 0 || BG_AbilityState( pm->ps ) != ABILITY_RUSH ) {
		return;
	}
	for ( i = 0 ; i < pm->numtouch ; i++ ) {
		if ( pm->touchents[i] < MAX_CLIENTS ) {
			return;
		}
	}
	PM_FlatDirection( 0, forward );
	if ( DotProduct( pm->ps->velocity, forward ) < bg_pmoveSettings.rushMinSpeed ) {
		PM_EndAbility();
	}
}

/*
================
PM_HookMove

Athena's hook. Holding the ability button locks the rope and swings;
otherwise the hook pulls, speeding up to the top pull speed
================
*/
static void PM_HookMove( void ) {
	vec3_t	dir;
	float	dist, rope, radial, pull, speed;

	VectorSubtract( pm->ps->grapplePoint, pm->ps->origin, dir );
	dist = VectorNormalize( dir );
	radial = DotProduct( pm->ps->velocity, dir );

	if ( pml.abilityHeld ) {
		rope = pm->ps->stats[STAT_MOVE_STATE];
		if ( rope <= 0 ) {
			rope = dist < 1 ? 1 : dist;
			pm->ps->stats[STAT_MOVE_STATE] = rope;
		}
		if ( dist >= rope ) {
			// taut rope: no moving away, and take up any slack
			if ( radial < 0 ) {
				VectorMA( pm->ps->velocity, -radial, dir, pm->ps->velocity );
			}
			VectorMA( pm->ps->velocity, ( dist - rope ) * 10, dir, pm->ps->velocity );
		}
		speed = VectorLength( pm->ps->velocity );
		if ( speed > bg_pmoveSettings.hookSwingMax ) {
			VectorScale( pm->ps->velocity, bg_pmoveSettings.hookSwingMax / speed, pm->ps->velocity );
		}
	} else {
		pm->ps->stats[STAT_MOVE_STATE] = 0;
		if ( dist > bg_pmoveSettings.hookDeadZone ) {
			// straight at the hook, at least the start speed, gaining each frame
			speed = VectorLength( pm->ps->velocity );
			pull = ( speed > bg_pmoveSettings.hookPullStart ? speed : bg_pmoveSettings.hookPullStart )
				+ bg_pmoveSettings.hookPullAccel * pml.frametime;
			if ( pull > bg_pmoveSettings.hookPullMax ) {
				pull = bg_pmoveSettings.hookPullMax;
			}
			VectorScale( dir, pull, pm->ps->velocity );
		}
	}

	pml.groundPlane = qfalse;
}

#define RAMP_CLIMB_FADE		4000	// u/s of remembered climb lost per second

/*
================
PM_TrackClimb

Ramp jump: remember how fast the player climbed on the ground this frame
================
*/
static void PM_TrackClimb( qboolean wasWalking ) {
	float	climb;

	if ( !( pml.champion->abilities & MOVEABIL_RAMPJUMP ) ) {
		return;
	}
	if ( BG_AbilityState( pm->ps ) == ABILITY_HOOK_PULL ) {
		return;		// STAT_MOVE_STATE holds the rope length
	}
	climb = 0;
	if ( wasWalking && pml.walking ) {
		climb = ( pm->ps->origin[2] - pml.previous_origin[2] ) / pml.frametime;
		if ( climb < 0 ) {
			climb = 0;
		} else if ( climb > bg_pmoveSettings.rampJumpMax - bg_pmoveSettings.jumpVelocity ) {
			// no more than can count toward the cap
			climb = bg_pmoveSettings.rampJumpMax - bg_pmoveSettings.jumpVelocity;
		}
	}
	// a step up is one frame of climbing; let it fade over a short while so a
	// jump just after it still counts
	if ( climb < pm->ps->stats[STAT_MOVE_STATE] - RAMP_CLIMB_FADE * pml.frametime ) {
		climb = pm->ps->stats[STAT_MOVE_STATE] - RAMP_CLIMB_FADE * pml.frametime;
	}
	if ( !pml.walking && !wasWalking ) {
		climb = 0;		// in the air: no climb to keep
	}
	pm->ps->stats[STAT_MOVE_STATE] = climb;
}

/*
================
PM_DropTimers
================
*/
static void PM_DropTimers( void ) {
	// drop misc timing counter
	if ( pm->ps->pm_time ) {
		if ( pml.msec >= pm->ps->pm_time ) {
			pm->ps->pm_flags &= ~PMF_ALL_TIMES;
			pm->ps->pm_time = 0;
		} else {
			pm->ps->pm_time -= pml.msec;
		}
	}

	// drop animation counter
	if ( pm->ps->legsTimer > 0 ) {
		pm->ps->legsTimer -= pml.msec;
		if ( pm->ps->legsTimer < 0 ) {
			pm->ps->legsTimer = 0;
		}
	}

	if ( pm->ps->torsoTimer > 0 ) {
		pm->ps->torsoTimer -= pml.msec;
		if ( pm->ps->torsoTimer < 0 ) {
			pm->ps->torsoTimer = 0;
		}
	}
}

/*
================
PM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated instead of a full move
================
*/
void PM_UpdateViewAngles( playerState_t *ps, const usercmd_t *cmd ) {
	short		temp;
	int		i;

	if ( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPINTERMISSION) {
		return;		// no view changes at all
	}

	if ( ps->pm_type != PM_SPECTATOR && ps->stats[STAT_HEALTH] <= 0 ) {
		return;		// no view changes at all
	}

	// circularly clamp the angles with deltas
	for (i=0 ; i<3 ; i++) {
		temp = cmd->angles[i] + ps->delta_angles[i];
		if ( i == PITCH ) {
			// don't let the player look up or down more than 90 degrees
			if ( temp > 16000 ) {
				ps->delta_angles[i] = 16000 - cmd->angles[i];
				temp = 16000;
			} else if ( temp < -16000 ) {
				ps->delta_angles[i] = -16000 - cmd->angles[i];
				temp = -16000;
			}
		}
		ps->viewangles[i] = SHORT2ANGLE(temp);
	}

}


/*
================
PmoveSingle

================
*/
void trap_SnapVector( float *v );

void PmoveSingle (pmove_t *pmove) {
	qboolean	wasWalking;

	pm = pmove;

	// this counter lets us debug movement problems with a journal
	// by setting a conditional breakpoint fot the previous frame
	c_pmove++;

	// clear results
	pm->numtouch = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;

	if ( pm->ps->stats[STAT_HEALTH] <= 0 ) {
		pm->tracemask &= ~CONTENTS_BODY;	// corpses can fly through bodies
	}

	// make sure walking button is clear if they are running, to avoid
	// proxy no-footsteps cheats
	if ( abs( pm->cmd.forwardmove ) > 64 || abs( pm->cmd.rightmove ) > 64 ) {
		pm->cmd.buttons &= ~BUTTON_WALKING;
	}

	// set the talk balloon flag
	if ( pm->cmd.buttons & BUTTON_TALK ) {
		pm->ps->eFlags |= EF_TALK;
	} else {
		pm->ps->eFlags &= ~EF_TALK;
	}

	// set the firing flag for continuous beam weapons
	if ( !(pm->ps->pm_flags & PMF_RESPAWNED) && pm->ps->pm_type != PM_INTERMISSION && pm->ps->pm_type != PM_NOCLIP
		&& ( pm->cmd.buttons & BUTTON_ATTACK ) && pm->ps->ammo[ pm->ps->weapon ]
		&& BG_AbilityState( pm->ps ) != ABILITY_RUSH
		&& !( BG_AbilityState( pm->ps ) == ABILITY_ACTIVE && pml.champion->active == ACTIVE_GHOSTWALK ) ) {
		pm->ps->eFlags |= EF_FIRING;
	} else {
		pm->ps->eFlags &= ~EF_FIRING;
	}

	// clear the respawned flag if attack and use are cleared
	if ( pm->ps->stats[STAT_HEALTH] > 0 && 
		!( pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE) ) ) {
		pm->ps->pm_flags &= ~PMF_RESPAWNED;
	}

	// if talk button is down, dissallow all other input
	// this is to prevent any possible intercept proxy from
	// adding fake talk balloons
	if ( pmove->cmd.buttons & BUTTON_TALK ) {
		// keep the talk button set tho for when the cmd.serverTime > 66 msec
		// and the same cmd is used multiple times in Pmove
		pmove->cmd.buttons = BUTTON_TALK;
		pmove->cmd.forwardmove = 0;
		pmove->cmd.rightmove = 0;
		pmove->cmd.upmove = 0;
	}

	// clear all pmove local vars
	memset (&pml, 0, sizeof(pml));

	pml.champion = BG_Champion( pm->ps->stats[STAT_CHAMPION] );
	pml.runSpeed = pm->ps->speed;

	// determine the time
	pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;
	if ( pml.msec < 1 ) {
		pml.msec = 1;
	} else if ( pml.msec > 200 ) {
		pml.msec = 200;
	}
	pm->ps->commandTime = pmove->cmd.serverTime;

	// save old org in case we get stuck
	VectorCopy (pm->ps->origin, pml.previous_origin);

	// save old velocity for crashlanding
	VectorCopy (pm->ps->velocity, pml.previous_velocity);

	pml.frametime = pml.msec * 0.001;

	// update the viewangles
	if ( BG_AbilityState( pm->ps ) == ABILITY_RUSH && pm->ps->pm_type == PM_NORMAL ) {
		PM_RushTurn();
	}
	PM_UpdateViewAngles( pm->ps, &pm->cmd );

	AngleVectors (pm->ps->viewangles, pml.forward, pml.right, pml.up);

	if ( pm->cmd.upmove < 10 ) {
		// not holding jump
		pm->ps->pm_flags &= ~PMF_JUMP_HELD;
	}

	// remember whether jump was down so a press can be told from a hold
	pml.jumpWasDown = ( pm->ps->pm_flags & PMF_JUMP_DOWN ) != 0;
	if ( pm->cmd.upmove >= 10 ) {
		pm->ps->pm_flags |= PMF_JUMP_DOWN;
	} else {
		pm->ps->pm_flags &= ~PMF_JUMP_DOWN;
	}

	// decide if backpedaling animations should be used
	if ( pm->cmd.forwardmove < 0 ) {
		pm->ps->pm_flags |= PMF_BACKWARDS_RUN;
	} else if ( pm->cmd.forwardmove > 0 || ( pm->cmd.forwardmove == 0 && pm->cmd.rightmove ) ) {
		pm->ps->pm_flags &= ~PMF_BACKWARDS_RUN;
	}

	if ( pm->ps->pm_type >= PM_DEAD ) {
		pm->cmd.forwardmove = 0;
		pm->cmd.rightmove = 0;
		pm->cmd.upmove = 0;
	}

	if ( pm->ps->pm_type == PM_SPECTATOR ) {
		PM_CheckDuck ();
		PM_FlyMove ();
		PM_DropTimers ();
		return;
	}

	if ( pm->ps->pm_type == PM_NOCLIP ) {
		PM_NoclipMove ();
		PM_DropTimers ();
		return;
	}

	if (pm->ps->pm_type == PM_FREEZE) {
		return;		// no movement at all
	}

	if ( pm->ps->pm_type == PM_INTERMISSION || pm->ps->pm_type == PM_SPINTERMISSION) {
		return;		// no movement at all
	}

	// set watertype, and waterlevel
	PM_SetWaterLevel();
	pml.previous_waterlevel = pmove->waterlevel;

	// releasing crouch to jump stands up first, so note a slide beforehand
	pml.wasSliding = pml.champion->moveStyle == MOVE_SLIDE
		&& pm->ps->groundEntityNum != ENTITYNUM_NONE
		&& ( pm->ps->pm_flags & PMF_DUCKED )
		&& pm->ps->stats[STAT_MOVE_STATE] > 0;

	// set mins, maxs, and viewheight
	PM_CheckDuck ();

	// set groundentity
	PM_GroundTrace();

	if ( pm->ps->pm_type == PM_DEAD ) {
		PM_DeadMove ();
	}

	PM_DropTimers();
	PM_MoveTimers();
	PM_Acceleration();
	PM_Ability();
	PM_AbilitySpeed();
	if ( PM_DroneActive() ) {
		pm->ps->eFlags |= EF_DRONE;
	} else {
		pm->ps->eFlags &= ~EF_DRONE;
	}
	PM_AirDodge();
	if ( BG_AbilityState( pm->ps ) == ABILITY_RUSH && pm->ps->pm_type == PM_NORMAL ) {
		PM_RushMove();
	}
	wasWalking = pml.walking;

#ifdef MISSIONPACK
	if ( pm->ps->powerups[PW_INVULNERABILITY] ) {
		PM_InvulnerabilityMove();
	} else
#endif
	if ( pm->ps->powerups[PW_FLIGHT] ) {
		// flight powerup doesn't allow jump and has different friction
		PM_FlyMove();
	} else if ( PM_DroneActive() ) {
		PM_DroneMove();
	} else if ( BG_AbilityState( pm->ps ) == ABILITY_HOOK_PULL ) {
		PM_HookMove();
		PM_AirMove();
	} else if (pm->ps->pm_flags & PMF_GRAPPLE_PULL) {
		PM_GrappleMove();
		// We can wiggle a bit
		PM_AirMove();
	} else if (pm->ps->pm_flags & PMF_TIME_WATERJUMP) {
		PM_WaterJumpMove();
	} else if ( pm->waterlevel > 1 ) {
		// swimming
		PM_WaterMove();
	} else if ( pml.walking ) {
		// walking on ground
		PM_WalkMove();
	} else {
		// airborne
		PM_AirMove();
	}

	PM_Animate();

	// set groundentity, watertype, and waterlevel
	PM_GroundTrace();
	PM_SetWaterLevel();

	// stair jump: a step up with jump held jumps at once
	pml.stairJump = qfalse;
	if ( pm->ps->persistant[PERS_STEP_TIME] == 0 && pm->ps->pm_type == PM_NORMAL && pm->waterlevel <= 1
		&& !PM_DroneActive() && BG_AbilityState( pm->ps ) != ABILITY_HOOK_PULL ) {
		pml.stairJump = qtrue;
		PM_CheckJump();
		pml.stairJump = qfalse;
	}

	PM_CheckRushBlocked();
	PM_TrackClimb( wasWalking );

	// weapons
	PM_Weapon();

	// torso animation
	PM_TorsoAnimation();

	// footstep events / legs animations
	PM_Footsteps();

	// entering / leaving water splashes
	PM_WaterEvents();

	// snap some parts of playerstate to save network bandwidth
	trap_SnapVector( pm->ps->velocity );
}


/*
================
Pmove

Can be called by either the server or the client
================
*/
void Pmove (pmove_t *pmove) {
	int			finalTime;

	finalTime = pmove->cmd.serverTime;

	if ( finalTime < pmove->ps->commandTime ) {
		return;	// should not happen
	}

	if ( finalTime > pmove->ps->commandTime + 1000 ) {
		pmove->ps->commandTime = finalTime - 1000;
	}

	pmove->ps->pmove_framecount = (pmove->ps->pmove_framecount+1) & ((1<<PS_PMOVEFRAMECOUNTBITS)-1);

	// chop the move up if it is too long, to prevent framerate
	// dependent behavior
	while ( pmove->ps->commandTime != finalTime ) {
		int		msec;

		msec = finalTime - pmove->ps->commandTime;

		if ( pmove->pmove_fixed ) {
			if ( msec > pmove->pmove_msec ) {
				msec = pmove->pmove_msec;
			}
		}
		else {
			if ( msec > 66 ) {
				msec = 66;
			}
		}
		pmove->cmd.serverTime = pmove->ps->commandTime + msec;
		if ( BG_AbilityState( pmove->ps ) == ABILITY_RUSH ) {
			// the bull rush falls faster
			int		gravity = pmove->ps->gravity;

			pmove->ps->gravity = (int)( gravity * bg_pmoveSettings.rushGravity );
			PmoveSingle( pmove );
			pmove->ps->gravity = gravity;
		} else {
			PmoveSingle( pmove );
		}

		if ( pmove->ps->pm_flags & PMF_JUMP_HELD ) {
			pmove->cmd.upmove = 20;
		}
	}

	//PM_CheckStuck();

}

