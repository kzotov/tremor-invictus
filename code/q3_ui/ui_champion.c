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
/*
=======================================================================

CHAMPION SELECT

=======================================================================
*/

#include "ui_local.h"

#define GRID_COLUMNS		5
#define GRID_X				16
#define GRID_Y				64
#define CELL_SIZE			56		// portrait 52, gap 4
#define PORTRAIT_SIZE		52
#define MAX_MENU_CHAMPIONS	32

#define INFO_X				312		// left edge of the right side
#define TEXT_X				460
#define TEXT_WIDTH			40		// characters per line of ability text

#define ID_WEAPON			200
#define ID_SELECT			201
#define ID_BACK				202

static const char *weaponCodes[] = { "mg", "sg", "ng" };

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menuaction_s	cells[MAX_MENU_CHAMPIONS];
	menulist_s		weapon;
	menutext_s		select;
	menutext_s		back;

	const char		*weaponNames[4];
	qhandle_t		portraits[MAX_MENU_CHAMPIONS];
	int				count;
	int				shown;			// champion the right side shows
	char			current[MAX_QPATH];

	playerInfo_t	playerInfo;
	int				modelFor;		// champion the model is loaded for, or -1
} championMenu_t;

static championMenu_t	s_champion;


/*
=================
Champion_WeaponCvar

Each champion remembers its own starting weapon
=================
*/
static const char *Champion_WeaponCvar( int i ) {
	return va( "ui_startWeapon_%s", bg_championList[i].name );
}

/*
=================
Champion_Show
=================
*/
static void Champion_Show( int i ) {
	char	code[16];
	int		w;

	if ( i < 0 || i >= s_champion.count ) {
		return;
	}
	s_champion.shown = i;
	trap_Cvar_Register( NULL, Champion_WeaponCvar( i ), "mg", CVAR_ARCHIVE );
	trap_Cvar_VariableStringBuffer( Champion_WeaponCvar( i ), code, sizeof( code ) );
	s_champion.weapon.curvalue = 0;
	for ( w = 0; w < ARRAY_LEN( weaponCodes ); w++ ) {
		if ( !Q_stricmp( code, weaponCodes[w] ) ) {
			s_champion.weapon.curvalue = w;
		}
	}
}

/*
=================
Champion_Select
=================
*/
static void Champion_Select( void ) {
	int		i = s_champion.shown;

	trap_Cvar_Set( Champion_WeaponCvar( i ), weaponCodes[s_champion.weapon.curvalue] );
	trap_Cvar_Set( "cg_startWeapon", weaponCodes[s_champion.weapon.curvalue] );
	trap_Cvar_Set( "champion", bg_championList[i].name );
	Q_strncpyz( s_champion.current, bg_championList[i].name, sizeof( s_champion.current ) );

	if ( trap_Cvar_VariableValue( "sv_running" ) || UI_Cvar_VariableString( "cl_currentServerAddress" )[0] ) {
		UI_ForceMenuOff();
	}
}

/*
=================
Champion_Event
=================
*/
static void Champion_Event( void *ptr, int notification ) {
	int		id = ( (menucommon_s *)ptr )->id;

	if ( notification != QM_ACTIVATED ) {
		return;
	}

	switch ( id ) {
	case ID_BACK:
		UI_PopMenu();
		break;
	case ID_SELECT:
		Champion_Select();
		break;
	case ID_WEAPON:
		trap_Cvar_Set( Champion_WeaponCvar( s_champion.shown ), weaponCodes[s_champion.weapon.curvalue] );
		break;
	default:
		if ( id >= 0 && id < s_champion.count ) {
			Champion_Show( id );
			Champion_Select();
		}
		break;
	}
}

/*
=================
Champion_DrawCell
=================
*/
static void Champion_DrawCell( void *self ) {
	menuaction_s	*a = (menuaction_s *)self;
	int				i = a->generic.id;
	qboolean		focus = ( a->generic.parent->cursor == a->generic.menuPosition );
	int				x = a->generic.left, y = a->generic.top;

	UI_FillRect( x, y, PORTRAIT_SIZE, PORTRAIT_SIZE, focus ? listbar_color : colorBlack );
	if ( s_champion.portraits[i] ) {
		UI_DrawHandlePic( x + 2, y + 2, PORTRAIT_SIZE - 4, PORTRAIT_SIZE - 4, s_champion.portraits[i] );
	} else {
		char	shortName[8];

		// no portrait: the first letters of the name
		Q_strncpyz( shortName, BG_String( bg_championList[i].nameString ), sizeof( shortName ) - 1 );
		UI_DrawString( x + PORTRAIT_SIZE / 2, y + PORTRAIT_SIZE / 2 - 8, shortName, UI_CENTER | UI_SMALLFONT, text_color_normal );
	}
	if ( !Q_stricmp( s_champion.current, bg_championList[i].name ) ) {
		UI_DrawRect( x, y, PORTRAIT_SIZE, PORTRAIT_SIZE, color_white );
		UI_DrawString( x + PORTRAIT_SIZE - 2, y + 2, "*", UI_RIGHT | UI_SMALLFONT, color_white );
	}
	if ( focus ) {
		UI_DrawRect( x, y, PORTRAIT_SIZE, PORTRAIT_SIZE, color_yellow );
	}
}

/*
=================
Champion_DrawWrapped

Draws text in lines of at most `width` characters, broken at spaces;
returns the y below the last line
=================
*/
static int Champion_DrawWrapped( int x, int y, const char *text, int width, float *color ) {
	char	line[64];
	int		len, cut;

	while ( *text ) {
		len = strlen( text );
		if ( len <= width ) {
			cut = len;
		} else {
			for ( cut = width; cut > 0 && text[cut] != ' '; cut-- ) {
			}
			if ( cut == 0 ) {
				cut = width;
			}
		}
		if ( cut >= sizeof( line ) ) {
			cut = sizeof( line ) - 1;
		}
		Q_strncpyz( line, text, cut + 1 );
		UI_DrawString( x, y, line, UI_LEFT | UI_SMALLFONT, color );
		y += SMALLCHAR_HEIGHT + 2;
		text += cut;
		while ( *text == ' ' ) {
			text++;
		}
	}
	return y;
}

/*
=================
Champion_MovementText
=================
*/
static void Champion_MovementText( const champion_t *c, char *out, int size ) {
	static const int	extraBits[] = { MOVEABIL_WALLJUMP, MOVEABIL_DOUBLEJUMP, MOVEABIL_AIRDODGE, MOVEABIL_ACCELERATION,
		MOVEABIL_HEAVYWEIGHT, MOVEABIL_RAMPJUMP, MOVEABIL_BUNNYHOP, MOVEABIL_EASYSTRAFE, MOVEABIL_ISOAIR };
	static const int	extraNames[] = { STR_CH_MOVE_WALLJUMP, STR_CH_MOVE_DOUBLEJUMP, STR_CH_MOVE_DODGE, STR_CH_MOVE_ACCEL,
		STR_CH_MOVE_HEAVY, STR_CH_MOVE_RAMP, STR_CH_MOVE_BUNNY, STR_CH_MOVE_EASYSTRAFE, STR_CH_MOVE_ISOAIR };
	int		i;

	Q_strncpyz( out, BG_String( c->moveStyle == MOVE_AIRCONTROL ? STR_CH_MOVE_AIRCONTROL
		: c->moveStyle == MOVE_SLIDE ? STR_CH_MOVE_SLIDE : STR_CH_MOVE_STANDARD ), size );
	for ( i = 0; i < ARRAY_LEN( extraBits ); i++ ) {
		if ( c->abilities & extraBits[i] ) {
			Q_strcat( out, size, ", " );
			Q_strcat( out, size, BG_String( extraNames[i] ) );
		}
	}
}

/*
=================
Champion_AbilityNumber

The server's number when connected, else the default
=================
*/
static int Champion_AbilityNumber( int active, int param, const char *paramName ) {
	char	buf[32];

	trap_Cvar_VariableStringBuffer( va( "ab_%s_%s", bg_abilityInfo[active].cvarName, (char *)paramName ), buf, sizeof( buf ) );
	if ( buf[0] ) {
		return atoi( buf );
	}
	return atoi( bg_abilityInfo[active].defaults[param] );
}

/*
=================
Champion_Draw
=================
*/
static void Champion_Draw( void ) {
	const champion_t	*c;
	const classInfo_t	*ci;
	vec3_t				angles;
	char				text[256];
	int					y, bit, charges, recharge;

	Menu_Draw( &s_champion.menu );

	// the right side follows the cursor, mouse or keys
	{
		menucommon_s	*item = Menu_ItemAtCursor( &s_champion.menu );

		if ( item && item->id >= 0 && item->id < s_champion.count && item->id != s_champion.shown ) {
			Champion_Show( item->id );
		}
	}

	if ( s_champion.shown < 0 || s_champion.shown >= s_champion.count ) {
		return;
	}
	c = &bg_championList[s_champion.shown];
	ci = &bg_classInfo[c->bodyClass];

	// the model, turning slowly
	if ( s_champion.modelFor != s_champion.shown ) {
		char	model[MAX_QPATH];

		memset( &s_champion.playerInfo, 0, sizeof( s_champion.playerInfo ) );
		BG_ChampionModel( s_champion.shown, model, sizeof( model ) );
		UI_PlayerInfo_SetModel( &s_champion.playerInfo, model );
		VectorClear( angles );
		angles[YAW] = 180 - 30;
		UI_PlayerInfo_SetInfo( &s_champion.playerInfo, LEGS_IDLE, TORSO_STAND, angles, vec3_origin, WP_MACHINEGUN, qfalse );
		s_champion.modelFor = s_champion.shown;
	}
	s_champion.playerInfo.viewAngles[YAW] = AngleMod( 180 + uis.realtime * 0.03f );
	s_champion.playerInfo.legs.yawing = qtrue;
	s_champion.playerInfo.torso.yawing = qtrue;
	UI_DrawPlayer( INFO_X, 56, 140, 230, &s_champion.playerInfo, uis.realtime / 2 );

	// numbers
	y = 64;
	UI_DrawProportionalString( TEXT_X, y, BG_String( c->nameString ), UI_LEFT, color_white );
	y += 30;
	UI_DrawString( TEXT_X, y, BG_String( STR_CLASS_LIGHT + c->bodyClass ), UI_LEFT | UI_SMALLFONT, color_orange );
	y += 24;
	UI_DrawString( TEXT_X, y, BG_String( STR_CH_HEALTH ), UI_LEFT | UI_SMALLFONT, text_color_normal );
	y += 18;
	Com_sprintf( text, sizeof( text ), BG_String( STR_CH_STATS ), 100, 100 + ci->maxHealthBonus );
	UI_DrawString( TEXT_X + 8, y, text, UI_LEFT | UI_SMALLFONT, color_white );
	y += 20;
	UI_DrawString( TEXT_X, y, BG_String( STR_CH_ARMOR ), UI_LEFT | UI_SMALLFONT, text_color_normal );
	y += 18;
	Com_sprintf( text, sizeof( text ), BG_String( STR_CH_STATS ), ci->baseArmor, ci->maxArmor );
	UI_DrawString( TEXT_X + 8, y, text, UI_LEFT | UI_SMALLFONT, color_white );
	y += 20;
	UI_DrawString( TEXT_X, y, BG_String( STR_CH_SPEED ), UI_LEFT | UI_SMALLFONT, text_color_normal );
	y += 18;
	UI_DrawString( TEXT_X + 8, y, va( "%i", c->runSpeed ), UI_LEFT | UI_SMALLFONT, color_white );
	y += 20;
	UI_DrawString( TEXT_X, y, BG_String( STR_CH_MOVEMENT ), UI_LEFT | UI_SMALLFONT, text_color_normal );
	y += 18;
	Champion_MovementText( c, text, sizeof( text ) );
	Champion_DrawWrapped( TEXT_X + 8, y, text, 20, color_white );

	// ability and passives under the model
	y = 296;
	if ( c->active > ACTIVE_NONE && c->active < ACTIVE_NUM_ABILITIES ) {
		charges = Champion_AbilityNumber( c->active, AP_CHARGES, "charges" );
		recharge = Champion_AbilityNumber( c->active, AP_RECHARGE, "recharge" ) / 1000;
		if ( charges > 1 ) {
			Com_sprintf( text, sizeof( text ), BG_String( STR_CH_EVERY ), charges, recharge );
		} else {
			Com_sprintf( text, sizeof( text ), BG_String( STR_CH_EVERY1 ), recharge );
		}
		UI_DrawString( INFO_X, y, BG_String( bg_abilityInfo[c->active].nameString ), UI_LEFT | UI_SMALLFONT, color_orange );
		UI_DrawString( 628, y, text, UI_RIGHT | UI_SMALLFONT, text_color_normal );
		y += 20;
		y = Champion_DrawWrapped( INFO_X, y, BG_String( STR_ABILDESC_HOOK + c->active - ACTIVE_HOOK ), TEXT_WIDTH, color_white );
		y += 6;
	}
	for ( bit = 0; bit < NUM_PASSIVES; bit++ ) {
		if ( c->passives & ( 1 << bit ) ) {
			UI_DrawString( INFO_X, y, BG_String( STR_CH_PASSIVE ), UI_LEFT | UI_SMALLFONT, color_orange );
			y += 18;
			y = Champion_DrawWrapped( INFO_X, y, BG_String( STR_PASSIVE_REGENERATION + bit ), TEXT_WIDTH, color_white );
			y += 4;
		}
	}

	// who they are, under the grid
	if ( s_champion.shown >= 0 && s_champion.shown < STR_BIO_UNUMREY - STR_BIO_RANGER + 1 ) {
		Champion_DrawWrapped( GRID_X + 4, 384, BG_String( STR_BIO_RANGER + s_champion.shown ), 38, text_color_disabled );
	}

	UI_DrawString( 320, 458, BG_String( STR_MENU_CHAMPION_NEXT_SPAWN ), UI_CENTER | UI_SMALLFONT, text_color_disabled );
}

/*
=================
Champion_Key

Arrows move through the grid
=================
*/
static sfxHandle_t Champion_Key( int key ) {
	menucommon_s	*item = Menu_ItemAtCursor( &s_champion.menu );
	int				i, next = -1;

	if ( item && item->id >= 0 && item->id < s_champion.count ) {
		i = item->id;
		switch ( key ) {
		case K_LEFTARROW: case K_KP_LEFTARROW:		next = i - 1; break;
		case K_RIGHTARROW: case K_KP_RIGHTARROW:	next = i + 1; break;
		case K_UPARROW: case K_KP_UPARROW:			next = i - GRID_COLUMNS; break;
		case K_DOWNARROW: case K_KP_DOWNARROW:
			next = i + GRID_COLUMNS;
			if ( next >= s_champion.count ) {
				Menu_SetCursorToItem( &s_champion.menu, &s_champion.weapon );
				return menu_move_sound;
			}
			break;
		}
		if ( next != -1 ) {
			if ( next >= 0 && next < s_champion.count ) {
				Menu_SetCursorToItem( &s_champion.menu, &s_champion.cells[next] );
				Champion_Show( next );
				return menu_move_sound;
			}
			return menu_buzz_sound;
		}
	}
	return Menu_DefaultKey( &s_champion.menu, key );
}

/*
=================
Champion_InitText
=================
*/
static void Champion_InitText( menutext_s *t, int id, int string, int x, int y, int style ) {
	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_PULSEIFFOCUS;
	t->generic.id		= id;
	t->generic.callback	= Champion_Event;
	t->generic.x		= x;
	t->generic.y		= y;
	t->string			= (char *)BG_String( string );
	t->style			= style;
	t->color			= color_red;
	Menu_AddItem( &s_champion.menu, t );
}

/*
=================
Champion_MenuInit
=================
*/
static void Champion_MenuInit( void ) {
	int		i;

	memset( &s_champion, 0, sizeof( s_champion ) );
	s_champion.modelFor = -1;

	s_champion.menu.wrapAround = qtrue;
	s_champion.menu.fullscreen = qtrue;
	s_champion.menu.draw = Champion_Draw;
	s_champion.menu.key = Champion_Key;

	s_champion.banner.generic.type		= MTYPE_BTEXT;
	s_champion.banner.generic.x			= 320;
	s_champion.banner.generic.y			= 16;
	s_champion.banner.string			= (char *)BG_String( STR_MENU_CHAMPION_TITLE );
	s_champion.banner.color				= color_white;
	s_champion.banner.style				= UI_CENTER;
	Menu_AddItem( &s_champion.menu, &s_champion.banner );

	trap_Cvar_VariableStringBuffer( "champion", s_champion.current, sizeof( s_champion.current ) );

	s_champion.count = bg_numChampions < MAX_MENU_CHAMPIONS ? bg_numChampions : MAX_MENU_CHAMPIONS;
	s_champion.shown = 0;
	for ( i = 0 ; i < s_champion.count ; i++ ) {
		menuaction_s	*cell = &s_champion.cells[i];
		char			model[MAX_QPATH];
		const char		*skin;

		BG_ChampionModel( i, model, sizeof( model ) );
		skin = strchr( model, '/' );

		if ( skin ) {
			char	base[MAX_QPATH];

			Q_strncpyz( base, model, skin - model + 1 );
			s_champion.portraits[i] = trap_R_RegisterShaderNoMip( va( "models/players/%s/icon_%s", base, (char *)skin + 1 ) );
		} else {
			s_champion.portraits[i] = trap_R_RegisterShaderNoMip( va( "models/players/%s/icon_default", (char *)model ) );
		}

		cell->generic.type		= MTYPE_ACTION;
		cell->generic.flags		= QMF_NODEFAULTINIT;
		cell->generic.id		= i;
		cell->generic.callback	= Champion_Event;
		cell->generic.ownerdraw	= Champion_DrawCell;
		cell->generic.left		= GRID_X + ( i % GRID_COLUMNS ) * CELL_SIZE;
		cell->generic.top		= GRID_Y + ( i / GRID_COLUMNS ) * CELL_SIZE;
		cell->generic.right		= cell->generic.left + PORTRAIT_SIZE;
		cell->generic.bottom	= cell->generic.top + PORTRAIT_SIZE;
		cell->generic.x			= cell->generic.left;
		cell->generic.y			= cell->generic.top;
		Menu_AddItem( &s_champion.menu, cell );

		if ( !Q_stricmp( s_champion.current, bg_championList[i].name ) ) {
			s_champion.shown = i;
		}
	}

	s_champion.weaponNames[0] = BG_String( STR_CH_WEAPON_MG );
	s_champion.weaponNames[1] = BG_String( STR_CH_WEAPON_SG );
	s_champion.weaponNames[2] = BG_String( STR_CH_WEAPON_NG );
	s_champion.weaponNames[3] = NULL;
	s_champion.weapon.generic.type		= MTYPE_SPINCONTROL;
	s_champion.weapon.generic.flags		= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_champion.weapon.generic.name		= BG_String( STR_CH_WEAPON );
	s_champion.weapon.generic.id		= ID_WEAPON;
	s_champion.weapon.generic.callback	= Champion_Event;
	s_champion.weapon.generic.x			= 160;
	s_champion.weapon.generic.y			= GRID_Y + ( ( s_champion.count + GRID_COLUMNS - 1 ) / GRID_COLUMNS ) * CELL_SIZE + 12;
	s_champion.weapon.itemnames			= s_champion.weaponNames;
	Menu_AddItem( &s_champion.menu, &s_champion.weapon );

	Champion_InitText( &s_champion.back, ID_BACK, STR_MENU_BACK, 40, 430, UI_LEFT | UI_SMALLFONT );
	Champion_InitText( &s_champion.select, ID_SELECT, STR_CH_SELECT, 600, 430, UI_RIGHT );

	Champion_Show( s_champion.shown );
}


/*
=================
Champion_Cache
=================
*/
void Champion_Cache( void ) {
}


/*
=================
UI_ChampionMenu
=================
*/
void UI_ChampionMenu( void ) {
	Champion_MenuInit();
	UI_PushMenu( &s_champion.menu );
	// start on the current champion (pushing moves the cursor to the top)
	Menu_SetCursorToItem( &s_champion.menu, &s_champion.cells[s_champion.shown] );
}

/*
===============================================================================

The Champions screen in RmlUi (ui/champions.rml)

The document shows cvars this module fills: ui_chShown is the champion on
view (the stage draws it), ui_ch* its details. Commands from the document:
ui_champPreview <name>, ui_champWeapon <mg|sg|ng>, ui_champSelect.

===============================================================================
*/

#define SELECTED_MSEC	900		// "Selected" shows this long before the menu goes back

static int	selectedTime;		// uis.realtime of the last Select, 0 when none pending

/*
=================
Champion_Locked

Connected to a match that fixes champions: a duel after its countdown, or
a round duel (picked in its draft)
=================
*/
static qboolean Champion_Locked( void ) {
	char	info[MAX_INFO_STRING], warmup[32];

	if ( !UI_Cvar_VariableString( "cl_currentServerAddress" )[0] && !trap_Cvar_VariableValue( "sv_running" ) ) {
		return qfalse;
	}
	trap_GetConfigString( CS_SERVERINFO, info, sizeof( info ) );
	trap_GetConfigString( CS_WARMUP, warmup, sizeof( warmup ) );
	if ( atoi( Info_ValueForKey( info, "g_roundDuel" ) ) ) {
		return qtrue;
	}
	return atoi( Info_ValueForKey( info, "g_gametype" ) ) == GT_TOURNAMENT && !warmup[0];
}

static void Champion_SetText( const char *cvar, const char *text ) {
	char	value[256];

	Q_strncpyz( value, text, sizeof( value ) );
	trap_Cvar_Set( cvar, value );
}

// the recreated section, in the order of their notes (STR_RECR_*)
static const char *recreatedChampions[] = { "wrack", "xaero", "hunter", "klesk", "sarge", "raine", "beastmaster", "unumrey" };

/*
=================
Champion_Bars

Health, armour and speed as 0-10 segments against the roster's highest
(ui_chHpSeg, ui_chArSeg, ui_chSpSeg)
=================
*/
static void Champion_Bars( const champion_t *c ) {
	int		i, bestHp = 1, bestAr = 1, bestSp = 1;

	for ( i = 0; i < bg_numChampions; i++ ) {
		const classInfo_t	*ci = &bg_classInfo[bg_championList[i].bodyClass];

		bestHp = MAX( bestHp, 100 + ci->maxHealthBonus );
		bestAr = MAX( bestAr, ci->maxArmor );
		bestSp = MAX( bestSp, bg_championList[i].runSpeed );
	}
	trap_Cvar_SetValue( "ui_chHpSeg", ( 10 * ( 100 + bg_classInfo[c->bodyClass].maxHealthBonus ) + bestHp / 2 ) / bestHp );
	trap_Cvar_SetValue( "ui_chArSeg", ( 10 * bg_classInfo[c->bodyClass].maxArmor + bestAr / 2 ) / bestAr );
	trap_Cvar_SetValue( "ui_chSpSeg", ( 10 * c->runSpeed + bestSp / 2 ) / bestSp );
}

/*
=================
Champion_Pictures

Each tile's picture (ui_pic_<champion>): a portrait pack's
gfx/champions/<champion>, else the model's icon, else our generated badge
=================
*/
static void Champion_Pictures( void ) {
	char	model[MAX_QPATH], path[MAX_QPATH], cvar[64], *skin;
	int		i;

	for ( i = 0; i < bg_numChampions; i++ ) {
		const champion_t	*c = &bg_championList[i];

		Com_sprintf( path, sizeof( path ), "gfx/champions/%s", c->name );
		if ( !trap_R_RegisterShaderNoMip( path ) ) {
			BG_ChampionModel( i, model, sizeof( model ) );
			skin = strchr( model, '/' );
			if ( skin ) {
				*skin++ = 0;
			}
			Com_sprintf( path, sizeof( path ), "models/players/%s/icon_%s", model, skin ? skin : "default" );
			if ( !trap_R_RegisterShaderNoMip( path ) ) {
				Com_sprintf( path, sizeof( path ), "gfx/champions-generated/%s", c->name );
			}
		}
		Com_sprintf( cvar, sizeof( cvar ), "ui_pic_%s", c->name );
		trap_Cvar_Set( cvar, va( "/%s", path ) );
		// the class mark in the tile's corner
		Com_sprintf( cvar, sizeof( cvar ), "ui_cls_%s", c->name );
		trap_Cvar_SetValue( cvar, c->bodyClass );
	}
}

/*
=================
Champion_Publish

The details of champion i for the document
=================
*/
static void Champion_Publish( int i ) {
	const champion_t	*c = &bg_championList[i];
	const classInfo_t	*ci = &bg_classInfo[c->bodyClass];
	char				text[256], weapon[16];
	int					bit, charges, recharge, n;

	trap_Cvar_Register( NULL, Champion_WeaponCvar( i ), "mg", CVAR_ARCHIVE );
	trap_Cvar_VariableStringBuffer( Champion_WeaponCvar( i ), weapon, sizeof( weapon ) );

	trap_Cvar_Set( "ui_chShown", c->name );
	trap_Cvar_Set( "ui_chWeapon", weapon );
	Champion_SetText( "ui_chName", BG_String( c->nameString ) );
	Champion_SetText( "ui_chClass", BG_String( STR_CLASS_LIGHT + c->bodyClass ) );
	Champion_SetText( "ui_chHealth", va( (char *)BG_String( STR_CH_STATS ), 100, 100 + ci->maxHealthBonus ) );
	Champion_SetText( "ui_chArmor", va( (char *)BG_String( STR_CH_STATS ), ci->baseArmor, ci->maxArmor ) );
	Champion_SetText( "ui_chSpeed", va( "%i", c->runSpeed ) );
	Champion_MovementText( c, text, sizeof( text ) );
	Champion_SetText( "ui_chMove", text );

	text[0] = 0;
	if ( c->active > ACTIVE_NONE && c->active < ACTIVE_NUM_ABILITIES ) {
		charges = Champion_AbilityNumber( c->active, AP_CHARGES, "charges" );
		recharge = Champion_AbilityNumber( c->active, AP_RECHARGE, "recharge" ) / 1000;
		if ( charges > 1 ) {
			Com_sprintf( text, sizeof( text ), BG_String( STR_CH_EVERY ), charges, recharge );
		} else {
			Com_sprintf( text, sizeof( text ), BG_String( STR_CH_EVERY1 ), recharge );
		}
		Champion_SetText( "ui_chAbility", BG_String( bg_abilityInfo[c->active].nameString ) );
		Champion_SetText( "ui_chAbilityDesc", BG_String( STR_ABILDESC_HOOK + c->active - ACTIVE_HOOK ) );
	} else {
		trap_Cvar_Set( "ui_chAbility", "" );
		trap_Cvar_Set( "ui_chAbilityDesc", "" );
	}
	trap_Cvar_Set( "ui_chAbilityEvery", text );

	// up to three passives, a line each
	n = 0;
	for ( bit = 0; bit < NUM_PASSIVES && n < 3; bit++ ) {
		if ( c->passives & ( 1 << bit ) ) {
			Champion_SetText( va( "ui_chPassive%i", n++ ), BG_String( STR_PASSIVE_REGENERATION + bit ) );
		}
	}
	trap_Cvar_Set( "ui_chPassive", n ? "1" : "" );
	for ( ; n < 3; n++ ) {
		trap_Cvar_Set( va( "ui_chPassive%i", n ), "" );
	}

	// the stat bars, ten segments against the best of the roster
	Champion_Bars( c );

	// recreated champions: where each part came from
	trap_Cvar_Set( "ui_chNote", "" );
	trap_Cvar_Set( "ui_chNoteEnd", "" );
	for ( n = 0; n < ARRAY_LEN( recreatedChampions ); n++ ) {
		if ( !Q_stricmp( c->name, recreatedChampions[n] ) ) {
			Champion_SetText( "ui_chNote", BG_String( STR_RECR_WRACK + n ) );
			Champion_SetText( "ui_chNoteEnd", BG_String( STR_RECR_DISCUSS ) );
		}
	}
	if ( i < STR_BIO_UNUMREY - STR_BIO_RANGER + 1 ) {
		Champion_SetText( "ui_chBio", BG_String( STR_BIO_RANGER + i ) );
	} else {
		trap_Cvar_Set( "ui_chBio", "" );
	}
}

/*
=================
UI_Champion_PublishCurrent

The selected champion's name and ability for the main menu
=================
*/
void UI_Champion_PublishCurrent( void ) {
	char	name[MAX_QPATH];
	int		i;

	trap_Cvar_Register( NULL, "champion", DEFAULT_CHAMPION, CVAR_USERINFO | CVAR_ARCHIVE );
	trap_Cvar_VariableStringBuffer( "champion", name, sizeof( name ) );
	i = BG_ChampionForName( name );
	if ( i < 0 ) {
		i = 0;
		trap_Cvar_Set( "champion", bg_championList[0].name );
	}
	trap_Cvar_VariableStringBuffer( "name", name, sizeof( name ) );
	Q_CleanStr( name );
	trap_Cvar_Set( "ui_playerName", name );
	Champion_SetText( "ui_chCurrentName", BG_String( bg_championList[i].nameString ) );
	Champion_SetText( "ui_chCurrentInfo", va( "%s  -  %s", BG_String( STR_CLASS_LIGHT + bg_championList[i].bodyClass ),
		bg_championList[i].active > ACTIVE_NONE ? BG_String( bg_abilityInfo[bg_championList[i].active].nameString ) : "" ) );
}

/*
=================
UI_ChampionsRml

Opens the RmlUi Champions screen on the selected champion
=================
*/
void UI_ChampionsRml( void ) {
	UI_ChampionsRmlFor( qfalse );
}

/*
=================
UI_ChampionsRmlFor

The same screen, or as Create server's second page (Back returns there,
Start selects the champion and starts the server)
=================
*/
void UI_ChampionsRmlFor( qboolean createServer ) {
	char	name[MAX_QPATH];
	int		i;

	trap_Cvar_Set( "ui_chFromCs", createServer ? "1" : "0" );
	UI_Champion_PublishCurrent();
	Champion_Pictures();
	trap_Cvar_VariableStringBuffer( "champion", name, sizeof( name ) );
	i = BG_ChampionForName( name );
	Champion_Publish( i >= 0 ? i : 0 );
	trap_Cvar_Set( "ui_chPicked", bg_championList[i >= 0 ? i : 0].name );
	trap_Cvar_Set( "ui_chFlash", "0" );
	trap_Cvar_Set( "ui_chLocked", Champion_Locked() ? "1" : "0" );
	selectedTime = 0;
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/champions.rml\n" );
}

/*
=================
UI_ChampionCommand

The document's commands; qtrue when cmd was one of them
=================
*/
qboolean UI_ChampionCommand( const char *cmd ) {
	char	name[MAX_QPATH];
	int		i;

	if ( !Q_stricmp( cmd, "ui_champPreview" ) ) {
		// a click: shown until another is clicked
		i = BG_ChampionForName( UI_Argv( 1 ) );
		if ( i >= 0 ) {
			Champion_Publish( i );
			trap_Cvar_Set( "ui_chPicked", bg_championList[i].name );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_champHover" ) ) {
		// hovering previews without picking
		i = BG_ChampionForName( UI_Argv( 1 ) );
		if ( i >= 0 && Q_stricmp( UI_Cvar_VariableString( "ui_chShown" ), bg_championList[i].name ) ) {
			Champion_Publish( i );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_champHoverEnd" ) ) {
		i = BG_ChampionForName( UI_Cvar_VariableString( "ui_chPicked" ) );
		if ( i >= 0 && Q_stricmp( UI_Cvar_VariableString( "ui_chShown" ), bg_championList[i].name ) ) {
			Champion_Publish( i );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_champWeapon" ) ) {
		trap_Cvar_VariableStringBuffer( "ui_chShown", name, sizeof( name ) );
		i = BG_ChampionForName( name );
		if ( i >= 0 ) {
			trap_Cvar_Set( Champion_WeaponCvar( i ), UI_Argv( 1 ) );
			trap_Cvar_Set( "ui_chWeapon", UI_Argv( 1 ) );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_champBack" ) ) {
		if ( trap_Cvar_VariableValue( "ui_chFromCs" ) ) {
			trap_Cvar_Set( "ui_chFromCs", "0" );
			trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/createserver.rml\n" );
		} else {
			trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/main.rml\n" );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_champStart" ) ) {
		// Create server's Start: the shown champion is the one played
		if ( Champion_Locked() ) {
			trap_Cvar_Set( "ui_chLocked", "1" );
			return qtrue;
		}
		trap_Cvar_VariableStringBuffer( "ui_chShown", name, sizeof( name ) );
		i = BG_ChampionForName( name );
		if ( i >= 0 ) {
			trap_Cvar_Set( "cg_startWeapon", UI_Cvar_VariableString( Champion_WeaponCvar( i ) ) );
			trap_Cvar_Set( "champion", bg_championList[i].name );
		}
		trap_Cmd_ExecuteText( EXEC_NOW, "ui_csStart\n" );		// (clears ui_chFromCs once it starts)
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_champSelect" ) ) {
		if ( Champion_Locked() ) {
			trap_Cvar_Set( "ui_chLocked", "1" );
			return qtrue;
		}
		trap_Cvar_VariableStringBuffer( "ui_chShown", name, sizeof( name ) );
		i = BG_ChampionForName( name );
		if ( i < 0 ) {
			return qtrue;
		}
		trap_Cvar_Set( "cg_startWeapon", UI_Cvar_VariableString( Champion_WeaponCvar( i ) ) );
		trap_Cvar_Set( "champion", bg_championList[i].name );	// a userinfo cvar: a connected server hears at once
		UI_Champion_PublishCurrent();
		trap_Cvar_Set( "ui_chFlash", "1" );
		trap_S_StartLocalSound( menu_in_sound, CHAN_LOCAL_SOUND );
		selectedTime = uis.realtime ? uis.realtime : 1;
		return qtrue;
	}
	return qfalse;
}

/*
=================
UI_Champion_Frame

After Select has shown "Selected": back to the main menu, or into the game
=================
*/
void UI_Champion_Frame( void ) {
	if ( !selectedTime || uis.realtime - selectedTime < SELECTED_MSEC ) {
		return;
	}
	selectedTime = 0;
	trap_Cvar_Set( "ui_chFlash", "0" );
	if ( trap_Cvar_VariableValue( "sv_running" ) || UI_Cvar_VariableString( "cl_currentServerAddress" )[0] ) {
		trap_Cmd_ExecuteText( EXEC_NOW, "rml_hide\n" );
		UI_ForceMenuOff();
	} else if ( !Q_stricmp( UI_Cvar_VariableString( "mm_state" ), "found" ) ) {
		trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/matchfound.rml\n" );	// back to the match about to start
	} else {
		trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/main.rml\n" );
	}
}

static void UI_DrawOneStage( uiStage_t *stage, const char *suffix, qboolean turns ) {
	char	box[128], cvar[64], name[MAX_QPATH], *p;
	float	x, y, w, h, yaw;
	int		i;

	trap_Cvar_VariableStringBuffer( va( "rml_stage%s", suffix ), box, sizeof( box ) );
	if ( !box[0] ) {
		return;
	}
	p = box;
	x = atof( COM_Parse( &p ) );
	y = atof( COM_Parse( &p ) );
	w = atof( COM_Parse( &p ) );
	h = atof( COM_Parse( &p ) );
	if ( w <= 0 || h <= 0 ) {
		return;
	}
	// screen pixels to the 640x480 frame the drawing code adjusts from
	x = ( x - uis.bias ) / uis.xscale;
	y /= uis.yscale;
	w /= uis.xscale;
	h /= uis.yscale;

	trap_Cvar_VariableStringBuffer( va( "rml_stageCvar%s", suffix ), cvar, sizeof( cvar ) );
	trap_Cvar_VariableStringBuffer( cvar[0] ? cvar : "champion", name, sizeof( name ) );
	i = BG_ChampionForName( name );
	if ( i < 0 ) {
		i = 0;
	}
	// the match-found lineup: the left half turned to the right and the right
	// half to the left, a duel's two squarely facing each other
	yaw = turns ? trap_Cvar_VariableValue( "rml_stageYaw" ) : 0;
	if ( !Q_strncmp( cvar, "mm_p", 4 ) ) {
		int		slot = atoi( cvar + 4 ), players = (int)trap_Cvar_VariableValue( "mm_pcount" );
		float	turn = players <= 2 ? 40 : 15;

		yaw = slot * 2 < players ? turn : -turn;
	}
	trap_Cvar_VariableStringBuffer( va( "rml_stageTint%s", suffix ), cvar, sizeof( cvar ) );
	UI_Stage_Tint( stage, UI_Cvar_VariableString( cvar ) );
	UI_Stage_Draw( stage, x, y, w, h, i, UI_StartWeapon( UI_Cvar_VariableString( Champion_WeaponCvar( i ) ) ), yaw );
}

/*
=================
UI_DrawRmlStage

The champions into the boxes the RmlUi document reserved for them
=================
*/
void UI_DrawRmlStage( void ) {
	static uiStage_t	stages[8];
	static qboolean		started;
	int					i;

	if ( !started ) {
		memset( stages, 0, sizeof( stages ) );
		for ( i = 0; i < 8; i++ ) {
			stages[i].champion = stages[i].oldChampion = -1;
		}
		started = qtrue;
	}
	if ( !trap_Cvar_VariableValue( "rml_active" ) ) {
		return;
	}
	for ( i = 0; i < 8; i++ ) {
		char	suffix[4];

		suffix[0] = i ? '0' + i : 0;
		suffix[1] = 0;
		UI_DrawOneStage( &stages[i], suffix, i == 0 );
	}
}
