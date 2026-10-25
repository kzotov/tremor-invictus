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
// cg_colors.c -- forced enemy and team colours: bright and sport skins are
// made at draw time by tinting the player's own model, head, upper and
// lower body separately

#include "cg_local.h"

typedef enum {
	TINT_NONE,
	TINT_BRIGHT,
	TINT_SPORT
} tintMode_t;

// what the next player part is drawn with, set by CG_PlayerColors
static int		tintMode;
static byte		tintColors[3][4];		// head, upper, lower
static int		tintPart;
static qboolean	tintWeapon;

static qboolean	outlineOn;			// the next parts get the enemy outline
static byte		outlineColor[4];

/*
================
CG_ParseColor

"0xRRGGBBAA" or "0xRRGGBB"; alpha is ignored, bodies are always opaque.
An invalid value falls back to the default with a warning.
================
*/
void CG_ParseColor( vmCvar_t *cv, const char *defaultValue, byte out[4] ) {
	const char	*s = cv->string;
	unsigned	value = 0;
	int			i, len, digit;

	for ( i = 0 ; i < 2 ; i++ ) {
		len = strlen( s );
		if ( ( len == 10 || len == 8 ) && s[0] == '0' && ( s[1] == 'x' || s[1] == 'X' ) ) {
			value = 0;
			for ( s += 2 ; *s ; s++ ) {
				if ( *s >= '0' && *s <= '9' ) {
					digit = *s - '0';
				} else if ( *s >= 'a' && *s <= 'f' ) {
					digit = *s - 'a' + 10;
				} else if ( *s >= 'A' && *s <= 'F' ) {
					digit = *s - 'A' + 10;
				} else {
					break;
				}
				value = ( value << 4 ) | digit;
			}
			if ( !*s ) {
				if ( len == 8 ) {
					value = ( value << 8 ) | 0xff;
				}
				out[0] = ( value >> 24 ) & 0xff;
				out[1] = ( value >> 16 ) & 0xff;
				out[2] = ( value >> 8 ) & 0xff;
				out[3] = 0xff;
				return;
			}
		}
		if ( i == 0 ) {
			CG_Printf( S_COLOR_YELLOW "%s: invalid colour \"%s\", using %s\n", cv->string, cv->string, defaultValue );
			s = defaultValue;
		}
	}
	out[0] = out[1] = out[2] = out[3] = 0xff;
}

/*
================
CG_SkinMode
================
*/
static int CG_SkinMode( const char *s ) {
	if ( !Q_stricmp( s, "bright" ) ) {
		return TINT_BRIGHT;
	}
	if ( !Q_stricmp( s, "sport" ) ) {
		return TINT_SPORT;
	}
	return TINT_NONE;
}

/*
================
CG_PlayerColors

Decides how a player is drawn this frame: enemy or team colours relative to
the player being viewed, red and blue colours for free spectators. Nothing
is forced on the viewed player's own model.
================
*/
void CG_PlayerColors( centity_t *cent, clientInfo_t *ci ) {
	playerState_t	*ps = &cg.snap->ps;
	int				viewTeam = ps->persistant[PERS_TEAM];
	qboolean		freeSpectator;
	int				i;

	tintMode = TINT_NONE;
	tintPart = 0;
	tintWeapon = qfalse;
	outlineOn = qfalse;

	// the enemy outline: a living enemy in plain sight (never through walls)
	if ( cg_enemyOutline.integer && cent->currentState.number != ps->clientNum
		&& !( cent->currentState.powerups & ( 1 << PW_INVIS ) ) && !CG_InGhostWalk( &cent->currentState )
		&& !( cent->currentState.eFlags & EF_DEAD )
		&& ( cgs.gametype < GT_TEAM || ci->team != viewTeam ) && viewTeam != TEAM_SPECTATOR ) {
		trace_t	tr;
		vec3_t	chest;

		VectorCopy( cent->lerpOrigin, chest );
		chest[2] += 16;
		CG_Trace( &tr, cg.refdef.vieworg, NULL, NULL, chest, ps->clientNum, MASK_SOLID );
		if ( tr.fraction == 1.0f ) {
			outlineOn = qtrue;
			CG_ParseColor( &cg_enemyOutlineColor, "0xFF2020FF", outlineColor );
		}
	}

	if ( cent->currentState.number == ps->clientNum ) {
		return;
	}
	if ( ( cent->currentState.powerups & ( 1 << PW_INVIS ) ) || CG_InGhostWalk( &cent->currentState ) ) {
		return;		// never make an invisible player visible
	}

	freeSpectator = viewTeam == TEAM_SPECTATOR && !( ps->pm_flags & PMF_FOLLOW );
	if ( freeSpectator && cgs.gametype >= GT_TEAM ) {
		if ( ci->team == TEAM_RED ) {
			tintMode = CG_SkinMode( cg_forceRedTeamSkin.string );
			CG_ParseColor( &cg_redTeamColor, "0xFF2020FF", tintColors[0] );
		} else if ( ci->team == TEAM_BLUE ) {
			tintMode = CG_SkinMode( cg_forceBlueTeamSkin.string );
			CG_ParseColor( &cg_blueTeamColor, "0x2040FFFF", tintColors[0] );
		}
		Byte4Copy( tintColors[0], tintColors[1] );
		Byte4Copy( tintColors[0], tintColors[2] );
		tintWeapon = tintMode != TINT_NONE;
		return;
	}

	if ( cgs.gametype >= GT_TEAM && ci->team == viewTeam ) {
		tintMode = CG_SkinMode( cg_forceTeamSkin.string );
		CG_ParseColor( &cg_teamHeadColor, "0x00FFFFFF", tintColors[0] );
		CG_ParseColor( &cg_teamUpperColor, "0x00FFFFFF", tintColors[1] );
		CG_ParseColor( &cg_teamLowerColor, "0x00FFFFFF", tintColors[2] );
		tintWeapon = tintMode != TINT_NONE && cg_forceTeamWeaponColor.integer;
	} else {
		tintMode = CG_SkinMode( cg_forceEnemySkin.string );
		CG_ParseColor( &cg_enemyHeadColor, "0x00FF00FF", tintColors[0] );
		CG_ParseColor( &cg_enemyUpperColor, "0x00FF00FF", tintColors[1] );
		CG_ParseColor( &cg_enemyLowerColor, "0x00FF00FF", tintColors[2] );
		tintWeapon = tintMode != TINT_NONE && cg_forceEnemyWeaponColor.integer;
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		tintColors[i][3] = 0xff;
	}
}

/*
================
CG_OwnerColors

A deployable (Eisen's turret, the Peeker drone) drawn like its owner's
body: the owner's enemy or team colours and forced skin, without the
enemy outline
================
*/
void CG_OwnerColors( int owner ) {
	if ( owner < 0 || owner >= MAX_CLIENTS || !cgs.clientinfo[owner].infoValid ) {
		CG_ColorReset();
		return;
	}
	CG_PlayerColors( &cg_entities[owner], &cgs.clientinfo[owner] );
	outlineOn = qfalse;
	tintPart = 1;		// the upper body's colour
}

/*
================
CG_ColorReset

Nothing forced for whatever is drawn next
================
*/
void CG_ColorReset( void ) {
	outlineOn = qfalse;
	tintMode = TINT_NONE;
	tintWeapon = qfalse;
}

/*
================
CG_PlayerColorPart

Which part (0 head, 1 upper, 2 lower) the next refEntity is
================
*/
void CG_PlayerColorPart( int part ) {
	tintPart = part;
}

/*
================
CG_ColorBeforeDraw

Bright replaces the part's surface with flat colour
================
*/
void CG_ColorBeforeDraw( refEntity_t *ent ) {
	if ( tintMode == TINT_BRIGHT ) {
		ent->customShader = cgs.media.brightShader;
		Byte4Copy( tintColors[tintPart], ent->shaderRGBA );
	}
}

/*
================
CG_ColorAfterDraw

Sport adds a colour pass over the part as drawn
================
*/
void CG_ColorAfterDraw( refEntity_t *ent ) {
	refEntity_t	pass;

	if ( outlineOn ) {
		pass = *ent;
		pass.customShader = cgs.media.outlineShader;
		Byte4Copy( outlineColor, pass.shaderRGBA );
		trap_R_AddRefEntityToScene( &pass );
	}

	if ( tintMode != TINT_SPORT ) {
		return;
	}
	pass = *ent;
	pass.customShader = cgs.media.sportShader;
	Byte4Copy( tintColors[tintPart], pass.shaderRGBA );
	trap_R_AddRefEntityToScene( &pass );
}

/*
================
CG_ColorWeapon

The weapon a forced-colour player holds takes the upper body colour
================
*/
void CG_ColorWeapon( refEntity_t *gun ) {
	refEntity_t	pass;

	if ( !tintWeapon ) {
		return;
	}
	pass = *gun;
	pass.customShader = cgs.media.tintShader;
	Byte4Copy( tintColors[1], pass.shaderRGBA );
	trap_R_AddRefEntityToScene( &pass );
}

/*
================
CG_ColorGrenade
================
*/
void CG_ColorGrenade( refEntity_t *ent ) {
	refEntity_t	pass;

	pass = *ent;
	pass.customShader = cgs.media.tintShader;
	CG_ParseColor( &cg_weaponColor_grenade, "0xFCFC00FF", pass.shaderRGBA );
	trap_R_AddRefEntityToScene( &pass );
}

/*
================
CG_ColorCorpse

Corpses fade to the dead body colour over a second
================
*/
void CG_ColorCorpse( centity_t *cent, refEntity_t *ent ) {
	refEntity_t	pass;
	byte		dead[4];
	float		frac;
	int			i;

	if ( !cg_deadBodyDarken.integer || !( cent->currentState.eFlags & EF_DEAD ) ) {
		cent->deadTime = 0;
		return;
	}
	if ( !cent->deadTime ) {
		cent->deadTime = cg.time;
	}
	frac = ( cg.time - cent->deadTime ) / 1000.0f;
	if ( frac > 1 ) {
		frac = 1;
	}
	CG_ParseColor( &cg_deadBodyColor, "0x101010FF", dead );
	pass = *ent;
	pass.customShader = cgs.media.tintShader;
	for ( i = 0 ; i < 3 ; i++ ) {
		pass.shaderRGBA[i] = 255 - ( 255 - dead[i] ) * frac;
	}
	pass.shaderRGBA[3] = 0xff;
	trap_R_AddRefEntityToScene( &pass );
}
