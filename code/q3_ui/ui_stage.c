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
// ui_stage.c -- the champion stage: the selected champion on a pillar,
// drawn by the main menu and the Champions screen
//

#include "ui_local.h"

#define SINK_MSEC	150		// the old champion leaving
#define RISE_MSEC	200		// the new one landing
#define SWAP_HEIGHT	140		// units above the pillar the swap starts or ends

/*
=================
UI_StartWeapon

"mg", "sg" or "ng" to the weapon the champion holds
=================
*/
weapon_t UI_StartWeapon( const char *code ) {
	if ( !Q_stricmp( code, "sg" ) ) {
		return WP_SHOTGUN;
	}
	if ( !Q_stricmp( code, "ng" ) ) {
		return WP_NAILGUN;
	}
	return WP_MACHINEGUN;
}

static void UI_Stage_Load( playerInfo_t *pi, int champion, weapon_t weapon ) {
	vec3_t	angles;
	char	model[MAX_QPATH];

	memset( pi, 0, sizeof( *pi ) );
	BG_ChampionModel( champion, model, sizeof( model ) );
	UI_PlayerInfo_SetModel( pi, model );
	VectorClear( angles );
	angles[YAW] = 180;
	UI_PlayerInfo_SetInfo( pi, LEGS_IDLE, TORSO_STAND, angles, vec3_origin, weapon, qfalse );
	pi->onPillar = qtrue;
	pi->champion = champion;
}

/*
=================
UI_Stage_Draw

The champion (an index into bg_championList) holding weapon, turned yaw
degrees from facing the camera; a change of champion sinks the old one
into the pillar and raises the new one
=================
*/
void UI_Stage_Draw( uiStage_t *st, float x, float y, float w, float h, int champion, weapon_t weapon, float yaw ) {
	playerInfo_t	*pi = UI_Stage_Update( st, champion, weapon, yaw );

	if ( pi ) {
		pi->sharedScene = qfalse;
		pi->blueSide = qfalse;
		UI_DrawPlayer( x, y, w, h, pi, uis.realtime / 2 );
	}
}

/*
=================
UI_Stage_Update

The champion swap, gestures and turn of a stage; the player to draw (the
old one while it lifts off), NULL for no champion
=================
*/
playerInfo_t *UI_Stage_Update( uiStage_t *st, int champion, weapon_t weapon, float yaw ) {
	int				t;
	playerInfo_t	*pi;

	if ( champion < 0 || champion >= bg_numChampions ) {
		return NULL;
	}
	if ( st->champion != champion || !st->pi.legsModel ) {
		if ( st->pi.legsModel && st->champion >= 0 && st->champion != champion ) {
			st->old = st->pi;
			st->oldChampion = st->champion;
			st->swapTime = uis.realtime;
		} else {
			st->oldChampion = -1;
			st->swapTime = uis.realtime - SINK_MSEC;
		}
		UI_Stage_Load( &st->pi, champion, weapon );
		st->champion = champion;
		st->weapon = weapon;
		st->nextGesture = uis.realtime + 8000 + rand() % 4000;
	} else if ( st->weapon != weapon ) {
		vec3_t	angles;

		VectorSet( angles, 0, 180, 0 );
		UI_PlayerInfo_SetInfo( &st->pi, LEGS_IDLE, TORSO_STAND, angles, vec3_origin, weapon, qfalse );
		st->weapon = weapon;
	}

	// a gesture every 8 to 12 seconds
	if ( uis.realtime > st->nextGesture ) {
		UI_PlayerInfo_Gesture( &st->pi );
		st->nextGesture = uis.realtime + 8000 + rand() % 4000;
	}

	// the swap happens above the pillar, never through it: the old
	// champion lifts off out of the box, the new one drops onto the top
	t = uis.realtime - st->swapTime;
	if ( t < SINK_MSEC && st->oldChampion >= 0 ) {
		pi = &st->old;
		pi->rise = SWAP_HEIGHT * t / (float)SINK_MSEC;
		pi->flash = 0;
	} else {
		pi = &st->pi;
		t -= SINK_MSEC;
		if ( t < RISE_MSEC ) {
			float	f = 1.0f - t / (float)RISE_MSEC;

			pi->rise = SWAP_HEIGHT * f * f;		// eases onto the pillar
			pi->flash = f;
		} else {
			pi->rise = 0;
			pi->flash = 0;
		}
	}

	pi->viewAngles[YAW] = AngleMod( 180 + yaw );
	pi->legs.yawing = qtrue;
	pi->torso.yawing = qtrue;
	return pi;
}

/*
=================
UI_DrawFaceoff

menu-style.md 4.1: the duel's two champions in one full-screen scene, you
on the left and the opponent on the right, each turned 48 degrees toward
the other on a pillar 183 u away and 59 u to the side, the pillar tops at
70 % of the screen height. The vertical view is fixed at 40 degrees, so
wider screens see more at the sides; the pillars run 400 u down out of the
view. The page draws the names, the VS mark and the floor mist over it.
=================
*/
#define FACEOFF_FOV_Y		40
#define FACEOFF_DIST		183
#define FACEOFF_SIDE		59
#define FACEOFF_TURN		48
#define FACEOFF_STANCE		30		// the idle stance turns the body this far to its right: the left one turns more
#define FACEOFF_ARRIVE		500		// msec between the two champions' arrivals

void UI_DrawFaceoff( uiStage_t stages[2], const int champions[2], const weapon_t weapons[2], int startTime ) {
	refdef_t		refdef;
	playerInfo_t	*pi;
	vec3_t			light;
	float			top = -tan( DEG2RAD( FACEOFF_FOV_Y * 0.5f ) ) * FACEOFF_DIST * 0.4f;	// 20 % of the height under the middle
	int				i;

	memset( &refdef, 0, sizeof( refdef ) );
	refdef.rdflags = RDF_NOWORLDMODEL;
	AxisClear( refdef.viewaxis );
	refdef.x = 0;
	refdef.y = 0;
	refdef.width = uis.glconfig.vidWidth;
	refdef.height = uis.glconfig.vidHeight;
	refdef.fov_y = FACEOFF_FOV_Y;
	refdef.fov_x = 2 * RAD2DEG( atan2( tan( DEG2RAD( FACEOFF_FOV_Y * 0.5f ) ) * refdef.width, refdef.height ) );
	refdef.time = uis.realtime / 2;

	trap_R_ClearScene();
	for ( i = 0 ; i < 2 ; i++ ) {
		// each arrives in turn, with its entrance sound: the stage's own swap
		// drops it onto its pillar
		if ( uis.realtime - startTime < i * FACEOFF_ARRIVE ) {
			continue;
		}
		if ( stages[i].champion != champions[i] && champions[i] >= 0 && champions[i] < bg_numChampions ) {
			char			model[MAX_QPATH], *slash;
			sfxHandle_t		sfx;

			BG_ChampionModel( champions[i], model, sizeof( model ) );
			slash = strchr( model, '/' );
			if ( slash ) {
				*slash = 0;
			}
			sfx = trap_S_RegisterSound( va( "sound/player/%s/taunt.wav", model ), qfalse );
			if ( sfx ) {
				trap_S_StartLocalSound( sfx, CHAN_LOCAL_SOUND );
			}
		}
		pi = UI_Stage_Update( &stages[i], champions[i], weapons[i], i ? -FACEOFF_TURN + FACEOFF_STANCE : FACEOFF_TURN + FACEOFF_STANCE );
		if ( !pi ) {
			continue;
		}
		pi->sharedScene = qtrue;
		pi->blueSide = !i;
		VectorSet( pi->stageTop, FACEOFF_DIST, i ? -FACEOFF_SIDE : FACEOFF_SIDE, top );
		UI_DrawPlayer( 0, 0, 0, 0, pi, uis.realtime / 2 );
	}
	// a key light from above in front of each, a rim light behind in the
	// side's colour
	for ( i = 0 ; i < 2 ; i++ ) {
		float	y = i ? -FACEOFF_SIDE : FACEOFF_SIDE;

		VectorSet( light, FACEOFF_DIST - 120, y, top + 160 );
		trap_R_AddLightToScene( light, 400, 1.0f, 0.95f, 0.9f );
		VectorSet( light, FACEOFF_DIST + 60, y, top + 140 );
		if ( i ) {
			trap_R_AddLightToScene( light, 300, 1.0f, 0.27f, 0.27f );
		} else {
			trap_R_AddLightToScene( light, 300, 0.27f, 0.84f, 1.0f );
		}
	}
	trap_R_RenderScene( &refdef );
}

/*
=================
UI_Stage_Tint

Draw the champion as the player colours settings show an enemy ("enemy")
or a teammate ("team"); anything else draws it plain
=================
*/
static void UI_ParseColor( const char *cvar, byte out[4] ) {
	char			text[32];
	const char		*s = text;
	unsigned int	value = 0;
	int				digits = 0;

	trap_Cvar_VariableStringBuffer( cvar, text, sizeof( text ) );
	if ( s[0] == '0' && ( s[1] == 'x' || s[1] == 'X' ) ) {
		s += 2;
	}
	for ( ; *s && digits < 8; s++, digits++ ) {
		int d = ( *s >= '0' && *s <= '9' ) ? *s - '0' : ( *s >= 'a' && *s <= 'f' ) ? *s - 'a' + 10
			: ( *s >= 'A' && *s <= 'F' ) ? *s - 'A' + 10 : -1;

		if ( d < 0 ) {
			break;
		}
		value = ( value << 4 ) | d;
	}
	if ( digits == 6 ) {
		value = ( value << 8 ) | 0xff;
	}
	out[0] = ( value >> 24 ) & 0xff;
	out[1] = ( value >> 16 ) & 0xff;
	out[2] = ( value >> 8 ) & 0xff;
	out[3] = 0xff;
}

void UI_Stage_Tint( uiStage_t *st, const char *side ) {
	static const char	*parts[3] = { "Head", "Upper", "Lower" };
	char				skin[16];
	int					i, mode = 0;

	if ( !Q_stricmp( side, "enemy" ) || !Q_stricmp( side, "team" ) ) {
		trap_Cvar_VariableStringBuffer( !Q_stricmp( side, "enemy" ) ? "cg_forceEnemySkin" : "cg_forceTeamSkin", skin, sizeof( skin ) );
		mode = !Q_stricmp( skin, "bright" ) ? 1 : !Q_stricmp( skin, "sport" ) ? 2 : 0;
		for ( i = 0; i < 3; i++ ) {
			UI_ParseColor( va( "cg_%s%sColor", side, parts[i] ), st->pi.tint[i] );
			Byte4Copy( st->pi.tint[i], st->old.tint[i] );
		}
	}
	st->pi.tintMode = st->old.tintMode = mode;
}
