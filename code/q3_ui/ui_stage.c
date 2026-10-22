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

#define SINK_MSEC	150
#define RISE_MSEC	200
#define SINK_DEPTH	64

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

	memset( pi, 0, sizeof( *pi ) );
	UI_PlayerInfo_SetModel( pi, bg_championList[champion].model );
	VectorClear( angles );
	angles[YAW] = 180;
	UI_PlayerInfo_SetInfo( pi, LEGS_IDLE, TORSO_STAND, angles, vec3_origin, weapon, qfalse );
	pi->onPillar = qtrue;
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
	int				t;
	playerInfo_t	*pi;

	if ( champion < 0 || champion >= bg_numChampions ) {
		return;
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

	t = uis.realtime - st->swapTime;
	if ( t < SINK_MSEC && st->oldChampion >= 0 ) {
		pi = &st->old;
		pi->rise = -SINK_DEPTH * t / (float)SINK_MSEC;
		pi->flash = 0;
	} else {
		pi = &st->pi;
		t -= SINK_MSEC;
		if ( t < RISE_MSEC ) {
			pi->rise = -SINK_DEPTH * ( 1.0f - t / (float)RISE_MSEC );
			pi->flash = 1.0f - t / (float)RISE_MSEC;
		} else {
			pi->rise = 0;
			pi->flash = 0;
		}
	}

	pi->viewAngles[YAW] = AngleMod( 180 + yaw );
	pi->legs.yawing = qtrue;
	pi->torso.yawing = qtrue;
	UI_DrawPlayer( x, y, w, h, pi, uis.realtime / 2 );
}
