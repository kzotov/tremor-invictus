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
		memset( &s_champion.playerInfo, 0, sizeof( s_champion.playerInfo ) );
		UI_PlayerInfo_SetModel( &s_champion.playerInfo, c->model );
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
		const char		*model = bg_championList[i].model;
		const char		*skin = strchr( model, '/' );

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
