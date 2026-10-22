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
/*
=======================================================================

SETTINGS

One screen with a tab per area. Every control is a row of a table:
label on the left, value on the right, and a line of help at the bottom
for the row under the cursor.

=======================================================================
*/

#include "ui_local.h"

enum {
	TAB_VIDEO,
	TAB_MOUSE,
	TAB_CONTROLS,
	TAB_GAME,
	TAB_HUD,
	TAB_SOUND,
	TAB_CHAMPIONS,
	NUM_TABS
};

typedef enum {
	ROW_SPIN,			// a list of values for one cvar
	ROW_SLIDER,
	ROW_FIELD,			// a number typed in
	ROW_RESOLUTION,		// desktop or one of the display's modes
	ROW_DISPLAY,		// fullscreen, windowed or borderless
	ROW_INVERT,			// sign of m_pitch
	ROW_BIT,			// one bit (slot) of a mask cvar, off or on
	ROW_READOUT,		// computed text, not a control
	ROW_DEVICE,			// a microphone from the sound system's list
	ROW_BIND			// a command and up to two keys
} rowType_t;

#define MAX_ROW_VALUES	12

static void Settings_SetBit( const char *cvar, int bit, int on );

typedef struct {
	rowType_t	type;
	int			label;
	int			help;
	const char	*cvar;				// or the command, for binds
	int			numValues;
	const char	*values[MAX_ROW_VALUES];
	int			names[MAX_ROW_VALUES];	// STR_ ids; 0 shows the value itself
	float		min, max;
	qboolean	restart;			// needs Apply
	int			slot;				// weapon slot number, for binds
} settingRow_t;

#define OFF_ON		2, { "0", "1" }, { STR_SET_OFF, STR_SET_ON }

static const settingRow_t videoRows[] = {
	{ ROW_SPIN, STR_SET_RENDERER, STR_SET_RENDERER_HELP, "cl_renderer", 3, { "metal", "opengl1", "opengl2" }, { STR_SET_METAL, STR_SET_OPENGL1, STR_SET_OPENGL2 }, 0, 0, qtrue },
	{ ROW_RESOLUTION, STR_SET_RESOLUTION, STR_SET_RESOLUTION_HELP, "r_mode", 0, { NULL }, { 0 }, 0, 0, qtrue },
	{ ROW_DISPLAY, STR_SET_DISPLAY, STR_SET_DISPLAY_HELP, "r_fullscreen", 3, { NULL }, { STR_SET_FULLSCREEN, STR_SET_WINDOWED, STR_SET_BORDERLESS }, 0, 0, qtrue },
	{ ROW_SPIN, STR_SET_RETINA, STR_SET_RETINA_HELP, "r_retina", OFF_ON, 0, 0, qtrue },
	{ ROW_SPIN, STR_SET_VSYNC, STR_SET_VSYNC_HELP, "r_swapInterval", OFF_ON, 0, 0, qtrue },
	{ ROW_SPIN, STR_SET_FPSCAP, STR_SET_FPSCAP_HELP, "com_maxfps", 9, { "60", "120", "125", "144", "165", "240", "250", "360", "0" },
		{ 0, 0, 0, 0, 0, 0, 0, 0, STR_SET_UNLIMITED } },
	{ ROW_SLIDER, STR_SET_FOV, STR_SET_FOV_HELP, "cg_fov", 0, { NULL }, { 0 }, 80, 130 },
	{ ROW_SLIDER, STR_SET_ZOOMFOV, STR_SET_ZOOMFOV_HELP, "cg_zoomfov", 0, { NULL }, { 0 }, 20, 80 },
	{ ROW_SLIDER, STR_SET_BRIGHTNESS, STR_SET_BRIGHTNESS_HELP, "r_gamma", 0, { NULL }, { 0 }, 0.5f, 2.0f },
	{ ROW_SLIDER, STR_SET_CONTRAST, STR_SET_CONTRAST_HELP, "r_contrast", 0, { NULL }, { 0 }, 0.5f, 2.0f },
	{ ROW_SPIN, STR_SET_PICMIP, STR_SET_PICMIP_HELP, "r_picmip", 4, { "0", "1", "2", "3" },
		{ STR_SET_HIGH, STR_SET_MEDIUM, STR_SET_LOW, STR_SET_LOWEST }, 0, 0, qtrue },
	{ ROW_SPIN, STR_SET_TEXFILTER, STR_SET_TEXFILTER_HELP, "r_textureMode", 2, { "GL_LINEAR_MIPMAP_NEAREST", "GL_LINEAR_MIPMAP_LINEAR" },
		{ STR_SET_BILINEAR, STR_SET_TRILINEAR } }
};

static const settingRow_t mouseRows[] = {
	{ ROW_FIELD, STR_SET_SENS, STR_SET_SENS_HELP, "sensitivity" },
	{ ROW_FIELD, STR_SET_CPI, STR_SET_CPI_HELP, "m_cpi" },
	{ ROW_READOUT, STR_SET_TURN, STR_SET_TURN_HELP, NULL },
	{ ROW_SPIN, STR_SET_RAWMOUSE, STR_SET_RAWMOUSE_HELP, "in_rawMouse", OFF_ON, 0, 0, qtrue },
	{ ROW_FIELD, STR_SET_ACCEL, STR_SET_ACCEL_HELP, "cl_mouseAccel" },
	{ ROW_FIELD, STR_SET_ACCELOFFSET, STR_SET_ACCELOFFSET_HELP, "cl_mouseAccelOffset" },
	{ ROW_FIELD, STR_SET_ACCELPOWER, STR_SET_ACCELPOWER_HELP, "cl_mouseAccelPower" },
	{ ROW_FIELD, STR_SET_ACCELCAP, STR_SET_ACCELCAP_HELP, "cl_mouseSensCap" },
	{ ROW_INVERT, STR_SET_INVERT, STR_SET_INVERT_HELP, "m_pitch", 2, { NULL }, { STR_SET_OFF, STR_SET_ON } },
	{ ROW_SPIN, STR_SET_SMOOTH, STR_SET_SMOOTH_HELP, "m_filter", OFF_ON },
	{ ROW_SPIN, STR_SET_ZOOMSENS, STR_SET_ZOOMSENS_HELP, "cg_zoomSensitivity", 10,
		{ "0", "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "1" }, { STR_SET_AUTO } }
};

#define BIND( label, cmd )	{ ROW_BIND, label, STR_SET_BIND_HELP, cmd }
#define SLOT( n )			{ ROW_BIND, STR_SET_BIND_SLOT, STR_SET_BIND_HELP, "weapon " #n, 0, { NULL }, { 0 }, 0, 0, qfalse, n }

static const settingRow_t controlRows[] = {
	BIND( STR_SET_BIND_FORWARD, "+forward" ),
	BIND( STR_SET_BIND_BACK, "+back" ),
	BIND( STR_SET_BIND_LEFT, "+moveleft" ),
	BIND( STR_SET_BIND_RIGHT, "+moveright" ),
	BIND( STR_SET_BIND_JUMP, "+moveup" ),
	BIND( STR_SET_BIND_CROUCH, "+movedown" ),
	BIND( STR_SET_BIND_WALK, "+speed" ),
	BIND( STR_SET_BIND_ATTACK, "+attack" ),
	BIND( STR_SET_BIND_ZOOM, "+zoom" ),
	BIND( STR_SET_BIND_NEXTWEAP, "weapnext" ),
	BIND( STR_SET_BIND_PREVWEAP, "weapprev" ),
	SLOT( 1 ), SLOT( 2 ), SLOT( 3 ), SLOT( 4 ), SLOT( 5 ), SLOT( 6 ), SLOT( 7 ), SLOT( 8 ),
	BIND( STR_SET_BIND_ABILITY, "+button12" ),
	BIND( STR_SET_BIND_CHAT, "messagemode" ),
	BIND( STR_SET_BIND_TEAMCHAT, "messagemode2" ),
	BIND( STR_SET_BIND_CHATHISTORY, "+chathistory" ),
	BIND( STR_SET_BIND_VOICE, "+voiprecord" ),
	BIND( STR_SET_BIND_FOLLOWNEXT, "follownext" ),
	BIND( STR_SET_BIND_FOLLOWPREV, "followprev" ),
	BIND( STR_SET_BIND_SCORES, "+scores" ),
	BIND( STR_SET_BIND_SCREENSHOT, "screenshotJPEG" ),
	BIND( STR_SET_BIND_CONSOLE, "toggleconsole" )
};

static const settingRow_t gameRows[] = {
	{ ROW_SPIN, STR_SET_CONFIGPRESET, STR_SET_CONFIGPRESET_HELP, "ui_configPreset", 4, { "", "competitive", "casual", "default" },
		{ STR_SET_CUSTOM, STR_SET_COMPETITIVE, STR_SET_CASUAL, STR_SET_DEFAULTPRESET } },
	{ ROW_SPIN, STR_SET_AUTOSWITCH, STR_SET_AUTOSWITCH_HELP, "cg_autoswitch", OFF_ON },
	{ ROW_SPIN, STR_SET_AUTOHOP, STR_SET_AUTOHOP_HELP, "cg_autoHop", OFF_ON },
	{ ROW_SPIN, STR_SET_OUTLINE, STR_SET_OUTLINE_HELP, "cg_enemyOutline", OFF_ON },
	{ ROW_SPIN, STR_SET_OUTLINECOLOR, STR_SET_OUTLINECOLOR_HELP, "cg_enemyOutlineColor", 5,
		{ "0xFF2020FF", "0xFFFF00FF", "0x00FF00FF", "0xFF00FFFF", "0xFFFFFFFF" },
		{ STR_SET_RED, STR_SET_YELLOW, STR_SET_GREEN, STR_SET_PINK, STR_SET_WHITE } },
	{ ROW_SPIN, STR_SET_COLORBLIND, STR_SET_COLORBLIND_HELP, "cg_colorBlind", 4, { "0", "1", "2", "3" },
		{ STR_SET_OFF, STR_SET_DEUTERANOPIA, STR_SET_PROTANOPIA, STR_SET_TRITANOPIA } },
	{ ROW_SPIN, STR_SET_HITBEEP, STR_SET_HITBEEP_HELP, "cg_hitBeep", OFF_ON },
	{ ROW_SPIN, STR_SET_KILLBEEP, STR_SET_KILLBEEP_HELP, "cg_killBeep", OFF_ON },
	{ ROW_SPIN, STR_SET_DMGNUM, STR_SET_DMGNUM_HELP, "cg_damagePlum", OFF_ON },
	{ ROW_SPIN, STR_SET_ZOOMTOGGLE, STR_SET_ZOOMTOGGLE_HELP, "cg_zoomToggle", 2, { "0", "1" }, { STR_SET_HOLD, STR_SET_TOGGLE } },
	{ ROW_SPIN, STR_SET_TIPS, STR_SET_TIPS_HELP, "cg_loadingTips", OFF_ON },
	{ ROW_SPIN, STR_SET_GORE, STR_SET_GORE_HELP, "cg_gibs", OFF_ON },
	{ ROW_SPIN, STR_SET_RAILRINGS, STR_SET_RAILRINGS_HELP, "cg_oldRail", 2, { "1", "0" }, { STR_SET_OFF, STR_SET_ON } },
	{ ROW_SPIN, STR_SET_TIMENUDGE, STR_SET_TIMENUDGE_HELP, "cl_timeNudge", 5, { "-16", "-12", "-8", "-4", "0" } },
	{ ROW_SPIN, STR_SET_PROJNUDGE, STR_SET_PROJNUDGE_HELP, "cg_autoProjectileNudge", OFF_ON },
	{ ROW_SPIN, STR_SET_AUTOREC, STR_SET_AUTOREC_HELP, "cg_autoAction", 3, { "0", "1", "3" },
		{ STR_SET_OFF, STR_SET_AUTOREC_DEMO, STR_SET_AUTOREC_BOTH } }
};

static const settingRow_t hudRows[] = {
	{ ROW_SPIN, STR_SET_HUDSTYLE, STR_SET_HUDSTYLE_HELP, "cg_hudLayout", 2, { "modern", "classic" }, { STR_SET_HUD_NEW, STR_SET_HUD_CLASSIC } },
	{ ROW_SPIN, STR_SET_CROSSHAIR, STR_SET_CROSSHAIR_HELP, "cg_drawCrosshair", 10, { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" },
		{ STR_SET_OFF } },
	{ ROW_SLIDER, STR_SET_CROSSHAIRSIZE, STR_SET_CROSSHAIRSIZE_HELP, "cg_crosshairSize", 0, { NULL }, { 0 }, 12, 48 },
	{ ROW_SPIN, STR_SET_XHCOLOR, STR_SET_XHCOLOR_HELP, "cg_crosshairColor", 8, { "0", "1", "2", "3", "4", "5", "6", "7" },
		{ STR_SET_WHITE, STR_SET_RED, STR_SET_GREEN, STR_SET_YELLOW, STR_SET_BLUE, STR_SET_CYAN, STR_SET_PINK, STR_SET_ORANGE } },
	{ ROW_SPIN, STR_SET_ARC, STR_SET_ARC_HELP, "cg_drawCooldownArc", 6, { "0", "1", "2", "3", "4", "5" },
		{ STR_SET_OFF, STR_SET_AUTO, STR_SET_TOP, STR_SET_RIGHTSIDE, STR_SET_BOTTOM, STR_SET_LEFTSIDE } },
	{ ROW_SPIN, STR_SET_ABILRING, STR_SET_ABILRING_HELP, "cg_drawCooldown", OFF_ON },
	{ ROW_SPIN, STR_SET_PASSRING, STR_SET_PASSRING_HELP, "cg_drawPassive", OFF_ON },
	{ ROW_SPIN, STR_SET_MEDALS, STR_SET_MEDALS_HELP, "cg_medals", 3, { "0", "1", "2" }, { STR_SET_OFF, STR_SET_ON, STR_SET_SOUNDONLY } },
	{ ROW_SPIN, STR_SET_FRAGLINE, STR_SET_FRAGLINE_HELP, "cg_fragLine", OFF_ON },
	{ ROW_SPIN, STR_SET_EVENTLINE, STR_SET_EVENTLINE_HELP, "cg_eventLine", OFF_ON },
	{ ROW_SPIN, STR_SET_PICKUPS, STR_SET_PICKUPS_HELP, "cg_drawItemPickups", OFF_ON },
	{ ROW_SPIN, STR_SET_SYSMSG, STR_SET_SYSMSG_HELP, "cg_systemMessages", OFF_ON },
	{ ROW_SLIDER, STR_SET_CHATLINES, STR_SET_CHATLINES_HELP, "cg_chatHistoryLength", 0, { NULL }, { 0 }, 0, 24 },
	{ ROW_BIT, STR_SET_ST_POWERUPS, STR_SET_SPECTIMERS_HELP, "cg_specItemTimers", 2, { NULL }, { STR_SET_OFF, STR_SET_ON }, 0, 0, qfalse, 1 },
	{ ROW_BIT, STR_SET_ST_MEGA, STR_SET_SPECTIMERS_HELP, "cg_specItemTimers", 2, { NULL }, { STR_SET_OFF, STR_SET_ON }, 0, 0, qfalse, 2 },
	{ ROW_BIT, STR_SET_ST_HEAVY, STR_SET_SPECTIMERS_HELP, "cg_specItemTimers", 2, { NULL }, { STR_SET_OFF, STR_SET_ON }, 0, 0, qfalse, 4 },
	{ ROW_BIT, STR_SET_ST_LIGHT, STR_SET_SPECTIMERS_HELP, "cg_specItemTimers", 2, { NULL }, { STR_SET_OFF, STR_SET_ON }, 0, 0, qfalse, 8 },
	{ ROW_SPIN, STR_SET_SPEED, STR_SET_SPEED_HELP, "cg_drawSpeed", 3, { "0", "1", "2" }, { STR_SET_OFF, STR_SET_CROSSHAIRPOS, STR_SET_VITALS } },
	{ ROW_SPIN, STR_SET_WEAPONBAR, STR_SET_WEAPONBAR_HELP, "cg_weaponBar", 4, { "0", "1", "2", "3" },
		{ STR_SET_WB_COLUMN, STR_SET_WB_CENTRE, STR_SET_WB_HELD, STR_SET_WB_HIDDEN } },
	{ ROW_SPIN, STR_SET_KILLFEED, STR_SET_KILLFEED_HELP, "cg_obituaryRowSize", 6, { "0", "1", "2", "3", "4", "5" }, { STR_SET_OFF } },
	{ ROW_SPIN, STR_SET_DMGARC, STR_SET_DMGARC_HELP, "cg_damageArc", OFF_ON },
	{ ROW_SPIN, STR_SET_LOWHEALTH, STR_SET_LOWHEALTH_HELP, "cg_lowHealthWarning", OFF_ON },
	{ ROW_SPIN, STR_SET_ITEMTIMERS, STR_SET_ITEMTIMERS_HELP, "cg_worldItemTimers", OFF_ON },
	{ ROW_SPIN, STR_SET_TEAMOVERLAY, STR_SET_TEAMOVERLAY_HELP, "cg_drawTeamOverlay", OFF_ON },
	{ ROW_SPIN, STR_SET_FPS, STR_SET_FPS_HELP, "cg_drawFPS", OFF_ON },
	{ ROW_SPIN, STR_SET_LAGOMETER, STR_SET_LAGOMETER_HELP, "cg_lagometer", OFF_ON }
};

static const settingRow_t soundRows[] = {
	{ ROW_SLIDER, STR_SET_VOLUME, STR_SET_VOLUME_HELP, "s_volume", 0, { NULL }, { 0 }, 0, 1 },
	{ ROW_SLIDER, STR_SET_MUSIC, STR_SET_MUSIC_HELP, "s_musicvolume", 0, { NULL }, { 0 }, 0, 1 },
	{ ROW_SLIDER, STR_SET_ANNOUNCER, STR_SET_ANNOUNCER_HELP, "s_announcerVolume", 0, { NULL }, { 0 }, 0, 1 },
	{ ROW_SPIN, STR_SET_VOIP, STR_SET_VOIP_HELP, "cl_voip", OFF_ON },
	{ ROW_SLIDER, STR_SET_VOICEVOL, STR_SET_VOICEVOL_HELP, "cl_voipVolume", 0, { NULL }, { 0 }, 0, 2 },
	{ ROW_DEVICE, STR_SET_MIC, STR_SET_MIC_HELP, "s_alInputDevice", 0, { NULL }, { 0 }, 0, 0, qtrue },
	{ ROW_SLIDER, STR_SET_MICGAIN, STR_SET_MICGAIN_HELP, "cl_voipCaptureMult", 0, { NULL }, { 0 }, 0.5f, 5 },
	{ ROW_SPIN, STR_SET_OPENMIC, STR_SET_OPENMIC_HELP, "cl_voipUseVAD", OFF_ON }
};

static const settingRow_t championRows[] = {
	{ ROW_SPIN, STR_SET_TRAILGREY, STR_SET_TRAILGREY_HELP, "cg_trailGrey", 4, { "0", "0.3", "0.6", "1" },
		{ STR_SET_OFF, STR_SET_LOW, STR_SET_MEDIUM, STR_SET_HIGH } },
	{ ROW_SPIN, STR_SET_TRAILCOLOR, STR_SET_TRAILCOLOR_HELP, "cg_trailColor", 5, { "", "0xFF40C0FF", "0x40C0FFFF", "0xA040FFFF", "0x40FF60FF" },
		{ STR_SET_TEAMCOLOR, STR_SET_PINK2, STR_SET_CYAN, STR_SET_VIOLET, STR_SET_GREEN } },
	{ ROW_SPIN, STR_SET_TRAILENEMY, STR_SET_TRAILENEMY_HELP, "cg_trailEnemyColor", 5, { "", "0xFF40C0FF", "0x40C0FFFF", "0xA040FFFF", "0xFF8000FF" },
		{ STR_SET_TEAMCOLOR, STR_SET_PINK2, STR_SET_CYAN, STR_SET_VIOLET, STR_SET_ORANGE } },
	{ ROW_SPIN, STR_SET_ABILLIGHT, STR_SET_ABILLIGHT_HELP, "cg_abilityLights", OFF_ON },
	{ ROW_SPIN, STR_SET_BACKFLIP, STR_SET_BACKFLIP_HELP, "cg_backflips", OFF_ON }
};

static const struct {
	const settingRow_t	*rows;
	int					numRows;
	int					name;
} settingTabs[NUM_TABS] = {
	{ videoRows, ARRAY_LEN( videoRows ), STR_SET_TAB_VIDEO },
	{ mouseRows, ARRAY_LEN( mouseRows ), STR_SET_TAB_MOUSE },
	{ controlRows, ARRAY_LEN( controlRows ), STR_SET_TAB_CONTROLS },
	{ gameRows, ARRAY_LEN( gameRows ), STR_SET_TAB_GAME },
	{ hudRows, ARRAY_LEN( hudRows ), STR_SET_TAB_HUD },
	{ soundRows, ARRAY_LEN( soundRows ), STR_SET_TAB_SOUND },
	{ championRows, ARRAY_LEN( championRows ), STR_SET_TAB_CHAMPIONS }
};

#define MAX_ROWS			32
#define MAX_RESOLUTIONS		32

#define ID_TAB				100		// + tab
#define ID_DEFAULTS			110
#define ID_APPLY			111
#define ID_CREDITS			112
#define ID_BACK				113

#define ROW_HEIGHT			18
#define ROWS_TOP			80
#define SPLIT_X				330		// labels end left of it, values start right of it
#define HELP_Y				408

typedef struct {
	menulist_s		spin;
	menuslider_s	slider;
	menufield_s		field;
	menuaction_s	action;
	const char		*itemNames[MAX_RESOLUTIONS + 2];
	char			appliedField[MAX_EDIT_LINE];
} rowItems_t;

typedef struct {
	menuframework_s	menu;
	menutext_s		tabs[NUM_TABS];
	menutext_s		defaults;
	menutext_s		apply;
	menutext_s		credits;
	menutext_s		back;

	int				tab;
	const settingRow_t	*rows;
	int				numRows;
	rowItems_t		items[MAX_ROWS];

	qboolean		restartNeeded;
	int				waitingBind;		// row waiting for a key, or -1

	char			resolutionBuf[MAX_STRING_CHARS];
	const char		*resolutions[MAX_RESOLUTIONS + 2];
	int				numResolutions;
} settingsMenu_t;

static settingsMenu_t	s_settings;

static void Settings_SetTab( int tab );

/*
=================
Settings_FindValue

Index of the cvar's current value in a row's list, matched as numbers
when both are numbers
=================
*/
static int Settings_FindValue( const settingRow_t *row, const char *value ) {
	int		i;

	for ( i = 0; i < row->numValues; i++ ) {
		if ( !Q_stricmp( row->values[i], value ) ) {
			return i;
		}
	}
	for ( i = 0; i < row->numValues; i++ ) {
		if ( atof( row->values[i] ) == atof( value ) && ( atof( value ) != 0 || value[0] == '0' ) ) {
			return i;
		}
	}
	return 0;
}

/*
=================
Settings_GetResolutions
=================
*/
static void Settings_GetResolutions( void ) {
	char	*s;

	s_settings.numResolutions = 0;
	s_settings.resolutions[s_settings.numResolutions++] = BG_String( STR_SET_DESKTOP );
	trap_Cvar_VariableStringBuffer( "r_availableModes", s_settings.resolutionBuf, sizeof( s_settings.resolutionBuf ) );
	s = s_settings.resolutionBuf;
	while ( *s && s_settings.numResolutions < MAX_RESOLUTIONS ) {
		s_settings.resolutions[s_settings.numResolutions++] = s;
		s = strchr( s, ' ' );
		if ( !s ) {
			break;
		}
		*s++ = '\0';
	}
	s_settings.resolutions[s_settings.numResolutions] = NULL;
}

/*
=================
Settings_CurrentResolution
=================
*/
static int Settings_CurrentResolution( void ) {
	char	want[32];
	int		i;

	if ( (int)trap_Cvar_VariableValue( "r_mode" ) != -1 ) {
		return 0;
	}
	Com_sprintf( want, sizeof( want ), "%ix%i", (int)trap_Cvar_VariableValue( "r_customwidth" ),
		(int)trap_Cvar_VariableValue( "r_customheight" ) );
	for ( i = 1; i < s_settings.numResolutions; i++ ) {
		if ( !strcmp( s_settings.resolutions[i], want ) ) {
			return i;
		}
	}
	return 0;
}

/*
=================
Settings_RowEvent
=================
*/
static void Settings_RowEvent( void *ptr, int event ) {
	int					id = ( (menucommon_s *)ptr )->id;
	const settingRow_t	*row;
	rowItems_t			*it;
	int					v;

	if ( event != QM_ACTIVATED || id < 0 || id >= s_settings.numRows ) {
		return;
	}
	row = &s_settings.rows[id];
	it = &s_settings.items[id];

	switch ( row->type ) {
	case ROW_SPIN:
		trap_Cvar_Set( row->cvar, row->values[it->spin.curvalue] );
		// a config preset sets a group of options at once
		if ( !Q_stricmp( row->cvar, "ui_configPreset" ) && row->values[it->spin.curvalue][0] ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "exec configs/%s.cfg\n", row->values[it->spin.curvalue] ) );
			trap_Cmd_ExecuteText( EXEC_APPEND, "ui_settingsRefresh\n" );
		}
		break;
	case ROW_SLIDER:
		if ( row->max - row->min > 5 ) {
			trap_Cvar_SetValue( row->cvar, (int)( it->slider.curvalue + 0.5f ) );
		} else {
			trap_Cvar_SetValue( row->cvar, it->slider.curvalue );
		}
		break;
	case ROW_RESOLUTION:
		v = it->spin.curvalue;
		if ( v == 0 ) {
			trap_Cvar_Set( "r_mode", "-2" );
		} else {
			const char	*res = s_settings.resolutions[v];
			const char	*x = strchr( res, 'x' );

			trap_Cvar_Set( "r_mode", "-1" );
			trap_Cvar_SetValue( "r_customwidth", atoi( res ) );
			trap_Cvar_SetValue( "r_customheight", x ? atoi( x + 1 ) : 0 );
		}
		break;
	case ROW_DISPLAY:
		trap_Cvar_SetValue( "r_fullscreen", it->spin.curvalue == 0 );
		trap_Cvar_SetValue( "r_noborder", it->spin.curvalue == 2 );
		break;
	case ROW_INVERT:
		v = it->spin.curvalue;
		trap_Cvar_SetValue( "m_pitch", ( v ? -1 : 1 ) * fabs( trap_Cvar_VariableValue( "m_pitch" ) ) );
		break;
	case ROW_BIT:
		Settings_SetBit( row->cvar, row->slot, it->spin.curvalue );
		break;
	case ROW_BIND:
		s_settings.waitingBind = id;
		return;
	default:
		return;
	}

	if ( row->restart ) {
		s_settings.restartNeeded = qtrue;
		s_settings.apply.generic.flags &= ~( QMF_GRAYED | QMF_INACTIVE );
	}
}

/*
=================
Settings_ApplyFields

Typed numbers take effect as soon as they read as a number
=================
*/
static void Settings_ApplyFields( void ) {
	int		i;
	char	*buf;

	for ( i = 0; i < s_settings.numRows; i++ ) {
		if ( s_settings.rows[i].type != ROW_FIELD ) {
			continue;
		}
		buf = s_settings.items[i].field.field.buffer;
		if ( !buf[0] || !strcmp( buf, s_settings.items[i].appliedField ) ) {
			continue;
		}
		if ( buf[0] == '-' && !buf[1] ) {
			continue;
		}
		trap_Cvar_Set( s_settings.rows[i].cvar, buf );
		Q_strncpyz( s_settings.items[i].appliedField, buf, sizeof( s_settings.items[i].appliedField ) );
	}
}

/*
=================
Settings_KeysForCommand

Up to two keys bound to a command; -1 where there is none
=================
*/
static void Settings_KeysForCommand( const char *command, int *keys ) {
	char	b[256];
	int		i, n = 0;

	keys[0] = keys[1] = -1;
	for ( i = 0; i < MAX_KEYS && n < 2; i++ ) {
		trap_Key_GetBindingBuf( i, b, sizeof( b ) );
		if ( b[0] && !Q_stricmp( b, command ) ) {
			keys[n++] = i;
		}
	}
}

/*
=================
Settings_DrawBind
=================
*/
static void Settings_DrawBind( void *self ) {
	menuaction_s		*a = (menuaction_s *)self;
	const settingRow_t	*row = &s_settings.rows[a->generic.id];
	qboolean			focus = ( a->generic.parent->cursor == a->generic.menuPosition );
	float				*color = focus ? text_color_highlight : text_color_normal;
	int					keys[2];
	char				name[64], text[96];
	const char			*label = BG_String( row->label );

	if ( row->slot ) {
		Com_sprintf( text, sizeof( text ), label, row->slot );
		label = text;
	}
	if ( focus ) {
		UI_FillRect( a->generic.left, a->generic.top, a->generic.right - a->generic.left,
			a->generic.bottom - a->generic.top, listbar_color );
	}
	UI_DrawString( a->generic.x - 8, a->generic.y, label, UI_RIGHT | UI_SMALLFONT, color );

	if ( s_settings.waitingBind == a->generic.id ) {
		UI_DrawString( a->generic.x + 8, a->generic.y, BG_String( STR_SET_BIND_WAIT ), UI_LEFT | UI_SMALLFONT | UI_BLINK, color_yellow );
		return;
	}

	Settings_KeysForCommand( row->cvar, keys );
	text[0] = 0;
	if ( keys[0] != -1 ) {
		trap_Key_KeynumToStringBuf( keys[0], name, sizeof( name ) );
		Q_strupr( name );
		Q_strcat( text, sizeof( text ), name );
	}
	if ( keys[1] != -1 ) {
		trap_Key_KeynumToStringBuf( keys[1], name, sizeof( name ) );
		Q_strupr( name );
		Q_strcat( text, sizeof( text ), "  " );
		Q_strcat( text, sizeof( text ), name );
	}
	UI_DrawString( a->generic.x + 8, a->generic.y, text[0] ? text : "---", UI_LEFT | UI_SMALLFONT, color );
}

/*
=================
Settings_StatusBar
=================
*/
static void Settings_StatusBar( void *self ) {
	int		id = ( (menucommon_s *)self )->id;

	if ( id >= 0 && id < s_settings.numRows ) {
		UI_DrawString( 320, HELP_Y, BG_String( s_settings.rows[id].help ), UI_CENTER | UI_SMALLFONT, colorWhite );
	}
}

/*
=================
Settings_Draw
=================
*/
static void Settings_Draw( void ) {
	int		i;

	Settings_ApplyFields();

	Menu_Draw( &s_settings.menu );

	// computed rows
	for ( i = 0; i < s_settings.numRows; i++ ) {
		const settingRow_t	*row = &s_settings.rows[i];
		int					y = s_settings.items[i].action.generic.y;

		if ( row->type == ROW_SLIDER ) {
			float	v = trap_Cvar_VariableValue( row->cvar );

			UI_DrawString( s_settings.items[i].slider.generic.x + 112, y,
				row->max - row->min > 5 ? va( "%i", (int)v ) : va( "%.2f", v ), UI_LEFT | UI_SMALLFONT, text_color_normal );
			continue;
		}
		if ( row->type != ROW_READOUT ) {
			continue;
		}
		UI_DrawString( SPLIT_X - 8, y, BG_String( row->label ), UI_RIGHT | UI_SMALLFONT, text_color_normal );
		{
			float	cpi = trap_Cvar_VariableValue( "m_cpi" );
			float	sens = trap_Cvar_VariableValue( "sensitivity" );
			float	perCm;
			char	text[96];

			if ( cpi <= 0 || sens <= 0 ) {
				UI_DrawString( SPLIT_X + 8, y, BG_String( STR_SET_TURN_NEEDCPI ), UI_LEFT | UI_SMALLFONT, text_color_disabled );
				continue;
			}
			// with CPI set, sensitivity is already degrees per centimetre
			perCm = sens;
			Com_sprintf( text, sizeof( text ), BG_String( STR_SET_TURN_VALUE ), perCm, 360.0f / perCm );
			UI_DrawString( SPLIT_X + 8, y, text, UI_LEFT | UI_SMALLFONT, text_color_normal );
		}
	}

	if ( s_settings.restartNeeded ) {
		UI_DrawString( 320, HELP_Y + 18, BG_String( STR_SET_RESTART_NOTE ), UI_CENTER | UI_SMALLFONT, color_yellow );
	}
}

/*
=================
Settings_Key
=================
*/
static sfxHandle_t Settings_Key( int key ) {
	int		keys[2];
	const settingRow_t	*row;
	menucommon_s		*item;

	if ( s_settings.waitingBind >= 0 ) {
		row = &s_settings.rows[s_settings.waitingBind];
		if ( key == K_ESCAPE ) {
			s_settings.waitingBind = -1;
			return menu_out_sound;
		}
		if ( key == '`' || key == '~' ) {
			return menu_buzz_sound;		// the console key stays where it is
		}
		Settings_KeysForCommand( row->cvar, keys );
		if ( keys[1] != -1 ) {
			// both slots taken: start over with the new key
			trap_Key_SetBinding( keys[0], "" );
			trap_Key_SetBinding( keys[1], "" );
		}
		trap_Key_SetBinding( key, row->cvar );
		s_settings.waitingBind = -1;
		return menu_in_sound;
	}

	item = Menu_ItemAtCursor( &s_settings.menu );
	if ( item && item->id >= 0 && item->id < s_settings.numRows
		&& s_settings.rows[item->id].type == ROW_BIND ) {
		row = &s_settings.rows[item->id];
		if ( key == K_BACKSPACE || key == K_DEL ) {
			Settings_KeysForCommand( row->cvar, keys );
			if ( keys[0] != -1 ) {
				trap_Key_SetBinding( keys[0], "" );
			}
			if ( keys[1] != -1 ) {
				trap_Key_SetBinding( keys[1], "" );
			}
			return menu_out_sound;
		}
	}

	if ( key == K_MOUSE2 || key == K_ESCAPE ) {
		Settings_ApplyFields();
	}
	return Menu_DefaultKey( &s_settings.menu, key );
}

/*
=================
Settings_Defaults

Resets the shown tab's settings to their defaults
=================
*/
static void Settings_Defaults( void ) {
	int		i;
	const settingRow_t	*row;

	for ( i = 0; i < s_settings.numRows; i++ ) {
		row = &s_settings.rows[i];
		switch ( row->type ) {
		case ROW_BIND:
			break;
		case ROW_RESOLUTION:
			trap_Cmd_ExecuteText( EXEC_APPEND, "reset r_mode\n" );
			break;
		case ROW_DISPLAY:
			trap_Cmd_ExecuteText( EXEC_APPEND, "reset r_fullscreen; reset r_noborder\n" );
			break;
		case ROW_READOUT:
			break;
		default:
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "reset %s\n", (char *)row->cvar ) );
			break;
		}
		if ( row->restart ) {
			s_settings.restartNeeded = qtrue;
		}
	}
	if ( s_settings.tab == TAB_CONTROLS ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "unbindall; exec default.cfg\n" );
	}
	trap_Cmd_ExecuteText( EXEC_APPEND, "ui_settingsRefresh\n" );
}

/*
=================
Settings_Event
=================
*/
static void Settings_Event( void *ptr, int event ) {
	int		id = ( (menucommon_s *)ptr )->id;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	if ( id >= ID_TAB && id < ID_TAB + NUM_TABS ) {
		Settings_ApplyFields();
		Settings_SetTab( id - ID_TAB );
		return;
	}
	switch ( id ) {
	case ID_DEFAULTS:
		Settings_Defaults();
		break;
	case ID_APPLY:
		Settings_ApplyFields();
		trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
		break;
	case ID_CREDITS:
		UI_CreditMenu();
		break;
	case ID_BACK:
		Settings_ApplyFields();
		UI_PopMenu();
		break;
	}
}

/*
=================
Settings_InitText
=================
*/
static void Settings_InitText( menutext_s *t, int id, int string, int x, int y, int style ) {
	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_PULSEIFFOCUS;
	t->generic.id		= id;
	t->generic.callback	= Settings_Event;
	t->generic.x		= x;
	t->generic.y		= y;
	t->string			= (char *)BG_String( string );
	t->style			= style | UI_SMALLFONT;
	t->color			= color_red;
	Menu_AddItem( &s_settings.menu, t );
}

/*
=================
Settings_AddRow
=================
*/
static void Settings_AddRow( int i, int x, int y ) {
	const settingRow_t	*row = &s_settings.rows[i];
	rowItems_t			*it = &s_settings.items[i];
	menucommon_s		*g;
	int					j;
	char				value[MAX_EDIT_LINE];

	memset( it, 0, sizeof( *it ) );
	it->action.generic.y = y;

	switch ( row->type ) {
	case ROW_SPIN:
	case ROW_DISPLAY:
	case ROW_INVERT:
	case ROW_BIT:
	case ROW_RESOLUTION:
		g = &it->spin.generic;
		g->type = MTYPE_SPINCONTROL;
		if ( row->type == ROW_RESOLUTION ) {
			for ( j = 0; j < s_settings.numResolutions; j++ ) {
				it->itemNames[j] = s_settings.resolutions[j];
			}
			it->spin.curvalue = Settings_CurrentResolution();
		} else {
			for ( j = 0; j < row->numValues; j++ ) {
				it->itemNames[j] = row->names[j] ? BG_String( row->names[j] ) : row->values[j];
			}
			if ( row->type == ROW_DISPLAY ) {
				it->spin.curvalue = trap_Cvar_VariableValue( "r_fullscreen" ) ? 0
					: trap_Cvar_VariableValue( "r_noborder" ) ? 2 : 1;
			} else if ( row->type == ROW_INVERT ) {
				it->spin.curvalue = trap_Cvar_VariableValue( "m_pitch" ) < 0;
			} else if ( row->type == ROW_BIT ) {
				it->spin.curvalue = ( (int)trap_Cvar_VariableValue( row->cvar ) & row->slot ) != 0;
			} else {
				trap_Cvar_VariableStringBuffer( row->cvar, value, sizeof( value ) );
				it->spin.curvalue = Settings_FindValue( row, value );
			}
		}
		it->spin.itemnames = it->itemNames;
		break;

	case ROW_SLIDER:
		g = &it->slider.generic;
		g->type = MTYPE_SLIDER;
		it->slider.minvalue = row->min;
		it->slider.maxvalue = row->max;
		it->slider.curvalue = trap_Cvar_VariableValue( row->cvar );
		break;

	case ROW_FIELD:
		g = &it->field.generic;
		g->type = MTYPE_FIELD;
		g->flags = QMF_NUMBERSONLY;
		it->field.field.widthInChars = 8;
		it->field.field.maxchars = 8;
		break;

	case ROW_BIND:
		g = &it->action.generic;
		g->type = MTYPE_ACTION;
		g->flags = QMF_NODEFAULTINIT;
		g->ownerdraw = Settings_DrawBind;
		g->left = x - 150;
		g->right = x + 150;
		g->top = y;
		g->bottom = y + SMALLCHAR_HEIGHT;
		break;

	default:
		return;		// drawn by Settings_Draw
	}

	g->name = BG_String( row->label );
	g->flags |= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	g->id = i;
	g->x = x;
	g->y = y;
	g->callback = Settings_RowEvent;
	g->statusbar = Settings_StatusBar;
	Menu_AddItem( &s_settings.menu, g );

	// the field is cleared when added
	if ( row->type == ROW_FIELD ) {
		trap_Cvar_VariableStringBuffer( row->cvar, it->field.field.buffer, sizeof( it->field.field.buffer ) );
		it->field.field.cursor = strlen( it->field.field.buffer );
		Q_strncpyz( it->appliedField, it->field.field.buffer, sizeof( it->appliedField ) );
	}
}

/*
=================
Settings_SetTab
=================
*/
static void Settings_SetTab( int tab ) {
	int		i, x, y, perColumn;

	s_settings.menu.nitems = 0;
	s_settings.menu.cursor = 0;
	s_settings.menu.cursor_prev = 0;
	s_settings.waitingBind = -1;

	s_settings.tab = tab;
	s_settings.rows = settingTabs[tab].rows;
	s_settings.numRows = settingTabs[tab].numRows;

	// tabs along the top
	for ( i = 0; i < NUM_TABS; i++ ) {
		Settings_InitText( &s_settings.tabs[i], ID_TAB + i, settingTabs[i].name, 64 + i * 128, 48, UI_CENTER );
		s_settings.tabs[i].color = ( i == tab ) ? color_white : color_orange;
	}

	// rows: one column, or two for long lists
	perColumn = ( HELP_Y - 8 - ROWS_TOP ) / ROW_HEIGHT;
	for ( i = 0; i < s_settings.numRows && i < MAX_ROWS; i++ ) {
		if ( s_settings.numRows > perColumn ) {
			x = ( i < ( s_settings.numRows + 1 ) / 2 ) ? 170 : 490;
			y = ROWS_TOP + ( i % ( ( s_settings.numRows + 1 ) / 2 ) ) * ROW_HEIGHT;
		} else {
			x = SPLIT_X;
			y = ROWS_TOP + i * ROW_HEIGHT;
		}
		Settings_AddRow( i, x, y );
	}

	Settings_InitText( &s_settings.back, ID_BACK, STR_SET_BACK, 40, 448, UI_LEFT );
	Settings_InitText( &s_settings.defaults, ID_DEFAULTS, STR_SET_DEFAULTS, 220, 448, UI_CENTER );
	Settings_InitText( &s_settings.credits, ID_CREDITS, STR_SET_CREDITS, 420, 448, UI_CENTER );
	Settings_InitText( &s_settings.apply, ID_APPLY, STR_SET_APPLY, 600, 448, UI_RIGHT );
	if ( !s_settings.restartNeeded ) {
		s_settings.apply.generic.flags |= QMF_GRAYED;
	}

	// start on the first row
	for ( i = 0; i < s_settings.menu.nitems; i++ ) {
		if ( ( (menucommon_s *)s_settings.menu.items[i] )->id == 0 ) {
			s_settings.menu.cursor = i;
			break;
		}
	}
}

/*
=================
UI_SettingsRefresh

Re-reads every value of the shown tab (after Defaults)
=================
*/
void UI_SettingsRefresh( void ) {
	if ( uis.activemenu == &s_settings.menu ) {
		Settings_SetTab( s_settings.tab );
	}
}

/*
=================
UI_SettingsTab
=================
*/
void UI_SettingsTab( int tab ) {
	if ( tab >= 0 && tab < NUM_TABS && uis.activemenu == &s_settings.menu ) {
		Settings_SetTab( tab );
	}
}

/*
=================
UI_SettingsMenu
=================
*/
/*
=================
Settings_RegisterGameCvars

The game's own settings exist only once a game has run; register them with
their defaults so the menu shows the right values before that
=================
*/
static void Settings_RegisterGameCvars( void ) {
	static const char	*defaults[][2] = {
		{ "cg_hitBeep", "1" }, { "cg_killBeep", "1" }, { "cg_damagePlum", "1" }, { "cg_damageArc", "1" },
		{ "cg_lowHealthWarning", "1" }, { "cg_worldItemTimers", "1" }, { "cg_zoomToggle", "0" },
		{ "cg_loadingTips", "1" }, { "r_contrast", "1" }, { "s_announcerVolume", "1" }, { "cg_gibs", "1" },
		{ "cg_oldRail", "0" }
	};
	int		i;

	for ( i = 0 ; i < ARRAY_LEN( defaults ) ; i++ ) {
		trap_Cvar_Register( NULL, defaults[i][0], defaults[i][1], CVAR_ARCHIVE );
	}
}

void UI_SettingsMenu( void ) {
	Settings_RegisterGameCvars();
	memset( &s_settings, 0, sizeof( s_settings ) );
	s_settings.menu.wrapAround = qtrue;
	s_settings.menu.fullscreen = qtrue;
	s_settings.menu.draw = Settings_Draw;
	s_settings.menu.key = Settings_Key;
	s_settings.waitingBind = -1;

	Settings_GetResolutions();
	Settings_SetTab( TAB_VIDEO );

	UI_PushMenu( &s_settings.menu );
}

/*
===============================================================================

Settings in RmlUi

The document is written from the row tables above to ui-gen/settings.rml
in the home folder each time the page opens, so the tables stay the one
place a setting is described. Its commands:
  ui_setTab <n>				show a tab (ui_setTab cvar)
  ui_setBind <tab> <row>	wait for a key for a bind row (rml_capture)
  ui_setBindKey <tab> <row> <key>
  ui_setInvert <0|1>
  ui_setDefaults <tab>
  ui_setPreset <name>		a config preset, then the page again

===============================================================================
*/

/*
=================
Settings_SetBit

Sets or clears one bit of a mask cvar, and the ui_bit_<cvar>_<bit> helper
the page reads it from
=================
*/
static void Settings_SetBit( const char *cvar, int bit, int on ) {
	int		mask = (int)trap_Cvar_VariableValue( cvar );

	mask = on ? ( mask | bit ) : ( mask & ~bit );
	trap_Cvar_SetValue( cvar, mask );
	trap_Cvar_Set( va( "ui_bit_%s_%i", cvar, bit ), on ? "1" : "0" );
}

#define RML_PAGE_SIZE	65536

static char		rmlPage[RML_PAGE_SIZE];

// text for the document: & < > escaped, [ ] kept away from the string keys
const char *UI_RmlText( const char *s ) {
	static char	out[2][512];
	static int	which;
	char		*o = out[which ^= 1];
	int			n = 0;

	for ( ; *s && n < (int)sizeof( out[0] ) - 8; s++ ) {
		const char	*rep = *s == '&' ? "&amp;" : *s == '<' ? "&lt;" : *s == '>' ? "&gt;" : *s == '[' ? "(" : *s == ']' ? ")" : NULL;

		if ( rep ) {
			strcpy( o + n, rep );
			n += strlen( rep );
		} else {
			o[n++] = *s;
		}
	}
	o[n] = 0;
	return o;
}

static void Rml_Add( const char *text ) {
	Q_strcat( rmlPage, sizeof( rmlPage ), text );
}

static void Rml_BindText( int tab, int r ) {
	const settingRow_t	*row = &settingTabs[tab].rows[r];
	char				name[64], text[96];
	int					keys[2];

	Settings_KeysForCommand( row->cvar, keys );
	text[0] = 0;
	if ( keys[0] != -1 ) {
		trap_Key_KeynumToStringBuf( keys[0], name, sizeof( name ) );
		Q_strupr( name );
		Q_strcat( text, sizeof( text ), name );
	}
	if ( keys[1] != -1 ) {
		trap_Key_KeynumToStringBuf( keys[1], name, sizeof( name ) );
		Q_strupr( name );
		Q_strcat( text, sizeof( text ), "   " );
		Q_strcat( text, sizeof( text ), name );
	}
	trap_Cvar_Set( va( "ui_bind%i_%i", tab, r ), text[0] ? text : "---" );
}

static void Rml_Option( const settingRow_t *row, const char *condition, const char *command, const char *label ) {
	Rml_Add( va( "<button class=\"opt\" data-class=\"on:%s\" onclick=\"%s%s\">%s</button>", condition, command,
		row->restart ? "; set ui_setRestart 1" : "", UI_RmlText( label ) ) );
}

/*
=================
Settings_Device

The n-th microphone the sound system lists (0 is the system default),
copied to out; false past the end of the list
=================
*/
static qboolean Settings_Device( int n, char *out, int size ) {
	char		list[4096];
	const char	*s;
	int			i, len;

	out[0] = 0;
	if ( n == 0 ) {
		return qtrue;
	}
	trap_Cvar_VariableStringBuffer( "s_alAvailableInputDevices", list, sizeof( list ) );
	s = list;
	for ( i = 1; *s; i++ ) {
		for ( len = 0; s[len] && s[len] != '\n'; len++ ) {
		}
		if ( i == n ) {
			if ( len >= size ) {
				len = size - 1;
			}
			memcpy( out, s, len );
			out[len] = 0;
			return qtrue;
		}
		s += len;
		if ( *s ) {
			s++;
		}
	}
	return qfalse;
}

/*
=================
Rml_Devices

A button per microphone; ui_setMic holds the shown one's number, as
device names don't fit a condition
=================
*/
static void Rml_Devices( const settingRow_t *row ) {
	char	name[256], current[256], cond[32], command[32];
	int		i, chosen = 0;

	trap_Cvar_VariableStringBuffer( row->cvar, current, sizeof( current ) );
	for ( i = 0; i < 16 && Settings_Device( i, name, sizeof( name ) ); i++ ) {
		if ( i > 0 && !strcmp( name, current ) ) {
			chosen = i;
		}
		Com_sprintf( cond, sizeof( cond ), "ui_setMic=%i", i );
		Com_sprintf( command, sizeof( command ), "ui_setMicPick %i", i );
		Rml_Option( row, cond, command, i == 0 ? BG_String( STR_SET_DEFAULTDEV ) : name );
	}
	trap_Cvar_SetValue( "ui_setMic", chosen );
}

static void Rml_Row( int tab, int r ) {
	const settingRow_t	*row = &settingTabs[tab].rows[r];
	char				label[96];
	int					i;

	Q_strncpyz( label, BG_String( row->label ), sizeof( label ) );
	if ( row->slot ) {
		Com_sprintf( label, sizeof( label ), BG_String( row->label ), row->slot );
	}
	Rml_Add( va( "<div class=\"srow\"><span class=\"slabel\">%s</span><span class=\"svalue\">", UI_RmlText( label ) ) );

	switch ( row->type ) {
	case ROW_SPIN:
		for ( i = 0; i < row->numValues; i++ ) {
			const char	*name = row->names[i] ? BG_String( row->names[i] ) : row->values[i];
			const char	*cmd = !Q_stricmp( row->cvar, "ui_configPreset" )
				? va( "ui_setPreset %s", row->values[i][0] ? row->values[i] : "\"\"" )
				: va( "set %s %s", row->cvar, row->values[i][0] ? row->values[i] : "\"\"" );
			char		cond[128], command[128];

			Com_sprintf( cond, sizeof( cond ), "%s=%s", row->cvar, row->values[i] );
			Q_strncpyz( command, cmd, sizeof( command ) );
			Rml_Option( row, cond, command, name );
		}
		break;
	case ROW_SLIDER:
		Rml_Add( va( "<input type=\"range\" min=\"%g\" max=\"%g\" step=\"%g\" data-value-cvar=\"%s\" onchange=\"set %s {value}\"/>",
			row->min, row->max, row->max - row->min > 5 ? 1.0f : 0.05f, row->cvar, row->cvar ) );
		Rml_Add( va( "<span class=\"sread\" data-text-cvar=\"%s\"></span>", row->cvar ) );
		break;
	case ROW_FIELD:
		Rml_Add( va( "<input type=\"text\" size=\"10\" maxlength=\"12\" data-value-cvar=\"%s\" onchange=\"set %s {value}\"/>",
			row->cvar, row->cvar ) );
		break;
	case ROW_RESOLUTION:
		Settings_GetResolutions();
		Rml_Option( row, "r_mode=-2", "set r_mode -2", BG_String( STR_SET_DESKTOP ) );
		for ( i = 1; i < s_settings.numResolutions; i++ ) {
			const char	*res = s_settings.resolutions[i];
			const char	*x = strchr( res, 'x' );
			char		cond[96], command[128];

			if ( !x ) {
				continue;
			}
			Com_sprintf( cond, sizeof( cond ), "r_mode=-1&r_customwidth=%i&r_customheight=%i", atoi( res ), atoi( x + 1 ) );
			Com_sprintf( command, sizeof( command ), "set r_mode -1; set r_customwidth %i; set r_customheight %i", atoi( res ), atoi( x + 1 ) );
			Rml_Option( row, cond, command, res );
		}
		break;
	case ROW_DISPLAY:
		Rml_Option( row, "r_fullscreen=1", "set r_fullscreen 1; set r_noborder 0", BG_String( row->names[0] ) );
		Rml_Option( row, "r_fullscreen=0&r_noborder=0", "set r_fullscreen 0; set r_noborder 0", BG_String( row->names[1] ) );
		Rml_Option( row, "r_fullscreen=0&r_noborder=1", "set r_fullscreen 0; set r_noborder 1", BG_String( row->names[2] ) );
		break;
	case ROW_INVERT:
		Rml_Option( row, "m_pitch>0", "ui_setInvert 0", BG_String( row->names[0] ) );
		Rml_Option( row, "m_pitch<0", "ui_setInvert 1", BG_String( row->names[1] ) );
		break;
	case ROW_BIT:
		// a helper cvar mirrors the bit so the page can show it
		Settings_SetBit( row->cvar, row->slot, ( (int)trap_Cvar_VariableValue( row->cvar ) & row->slot ) != 0 );
		Rml_Option( row, va( "ui_bit_%s_%i=0", row->cvar, row->slot ), va( "ui_setBit %s %i 0", row->cvar, row->slot ),
			BG_String( row->names[0] ) );
		Rml_Option( row, va( "ui_bit_%s_%i=1", row->cvar, row->slot ), va( "ui_setBit %s %i 1", row->cvar, row->slot ),
			BG_String( row->names[1] ) );
		break;
	case ROW_READOUT:
		Rml_Add( "<span class=\"sread\" data-text-cvar=\"ui_setTurn\"></span>" );
		break;
	case ROW_DEVICE:
		Rml_Devices( row );
		break;
	case ROW_BIND:
		Rml_BindText( tab, r );
		Rml_Add( va( "<button class=\"opt bind\" data-when=\"ui_setWait!=%i_%i\" onclick=\"ui_setBind %i %i\"><span data-text-cvar=\"ui_bind%i_%i\"></span></button>",
			tab, r, tab, r, tab, r ) );
		Rml_Add( va( "<span class=\"sread wait\" data-when=\"ui_setWait=%i_%i\">%s</span>", tab, r, UI_RmlText( BG_String( STR_SET_BIND_WAIT ) ) ) );
		break;
	}
	Rml_Add( va( "</span><div class=\"shelp\">%s</div></div>\n", UI_RmlText( BG_String( row->help ) ) ) );
}

/*
=================
UI_SettingsRml
=================
*/
void UI_SettingsRml( void ) {
	fileHandle_t	f;
	int				tab, r;

	// the game module's settings shown here, saved even before a game has run
	trap_Cvar_Register( NULL, "cg_trailGrey", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "cg_trailColor", "", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "cg_trailEnemyColor", "", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "cg_abilityLights", "1", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "cg_backflips", "1", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "cg_enemyOutline", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "cg_enemyOutlineColor", "0xFF2020FF", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "cg_colorBlind", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_setTab", "0", CVAR_TEMP );
	trap_Cvar_Register( NULL, "ui_setRestart", "0", CVAR_TEMP );
	trap_Cvar_Set( "ui_setWait", "" );
	UI_Settings_Frame();

	rmlPage[0] = 0;
	Rml_Add( "<rml>\n<head>\n<title>Settings</title>\n<link type=\"text/rcss\" href=\"/ui/tremor.rcss\"/>\n</head>\n" );
	Rml_Add( "<body data-escape=\"rml_show ui/main.rml\">\n<div class=\"settings\">\n" );
	Rml_Add( va( "<h1>%s</h1>\n<div class=\"tabs\">", UI_RmlText( BG_String( STR_MENU_SETTINGS ) ) ) );
	for ( tab = 0; tab < NUM_TABS; tab++ ) {
		Rml_Add( va( "<button class=\"tab\" data-class=\"on:ui_setTab=%i\" onclick=\"ui_setTab %i\">%s</button>", tab, tab,
			UI_RmlText( BG_String( settingTabs[tab].name ) ) ) );
	}
	Rml_Add( "</div>\n" );
	for ( tab = 0; tab < NUM_TABS; tab++ ) {
		Rml_Add( va( "<div class=\"spage\" data-when=\"ui_setTab=%i\">\n", tab ) );
		for ( r = 0; r < settingTabs[tab].numRows; r++ ) {
			Rml_Row( tab, r );
		}
		Rml_Add( "</div>\n" );
	}
	Rml_Add( "<div class=\"sactions\">" );
	Rml_Add( "<button onclick=\"rml_show ui/main.rml\">[menu_back]</button>" );
	Rml_Add( va( "<button onclick=\"ui_setDefaults\">%s</button>", UI_RmlText( BG_String( STR_SET_DEFAULTS ) ) ) );
	Rml_Add( va( "<button class=\"apply\" data-when=\"ui_setRestart=1\" onclick=\"set ui_setRestart 0; vid_restart\">%s</button>",
		UI_RmlText( BG_String( STR_SET_APPLY ) ) ) );
	Rml_Add( va( "<button onclick=\"ui_credits\">%s</button>", UI_RmlText( BG_String( STR_SET_CREDITS ) ) ) );
	Rml_Add( "</div>\n</div>\n</body>\n</rml>\n" );

	trap_FS_FOpenFile( "ui-gen/settings.rml", &f, FS_WRITE );
	if ( !f ) {
		UI_SettingsMenu();
		return;
	}
	trap_FS_Write( rmlPage, strlen( rmlPage ), f );
	trap_FS_FCloseFile( f );
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui-gen/settings.rml\n" );
}

/*
=================
UI_Settings_Frame

The turn readout for the page
=================
*/
void UI_Settings_Frame( void ) {
	float	cpi = trap_Cvar_VariableValue( "m_cpi" );
	float	sens = trap_Cvar_VariableValue( "sensitivity" );

	if ( cpi <= 0 || sens <= 0 ) {
		trap_Cvar_Set( "ui_setTurn", BG_String( STR_SET_TURN_NEEDCPI ) );
	} else {
		trap_Cvar_Set( "ui_setTurn", va( (char *)BG_String( STR_SET_TURN_VALUE ), sens, 360.0f / sens ) );
	}
}

/*
=================
UI_SettingsCommand
=================
*/
qboolean UI_SettingsCommand( const char *cmd ) {
	int		tab, r, key, keys[2];

	if ( !Q_stricmp( cmd, "ui_setTab" ) ) {
		trap_Cvar_Set( "ui_setTab", UI_Argv( 1 ) );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_setMicPick" ) ) {
		char	name[256];

		if ( Settings_Device( atoi( UI_Argv( 1 ) ), name, sizeof( name ) ) ) {
			trap_Cvar_Set( "s_alInputDevice", name );
			trap_Cvar_SetValue( "ui_setMic", atoi( UI_Argv( 1 ) ) );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_setBit" ) ) {
		Settings_SetBit( UI_Argv( 1 ), atoi( UI_Argv( 2 ) ), atoi( UI_Argv( 3 ) ) );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_setInvert" ) ) {
		trap_Cvar_SetValue( "m_pitch", ( atoi( UI_Argv( 1 ) ) ? -1 : 1 ) * fabs( trap_Cvar_VariableValue( "m_pitch" ) ) );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_setPreset" ) ) {
		trap_Cvar_Set( "ui_configPreset", UI_Argv( 1 ) );
		if ( UI_Argv( 1 )[0] ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "exec configs/%s.cfg\nui_settings\n", UI_Argv( 1 ) ) );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_setDefaults" ) ) {
		tab = (int)trap_Cvar_VariableValue( "ui_setTab" );
		for ( r = 0; tab >= 0 && tab < NUM_TABS && r < settingTabs[tab].numRows; r++ ) {
			const settingRow_t	*row = &settingTabs[tab].rows[r];

			if ( row->type == ROW_RESOLUTION ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, "reset r_mode\n" );
			} else if ( row->type == ROW_DISPLAY ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, "reset r_fullscreen; reset r_noborder\n" );
			} else if ( row->type == ROW_DEVICE ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, "reset s_alInputDevice\nset ui_setMic 0\n" );
			} else if ( row->type != ROW_BIND && row->type != ROW_READOUT ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "reset %s\n", (char *)row->cvar ) );
			}
			if ( row->restart ) {
				trap_Cvar_Set( "ui_setRestart", "1" );
			}
		}
		trap_Cmd_ExecuteText( EXEC_APPEND, "ui_settings\n" );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_setBind" ) ) {
		tab = atoi( UI_Argv( 1 ) );
		r = atoi( UI_Argv( 2 ) );
		trap_Cvar_Set( "ui_setWait", va( "%i_%i", tab, r ) );
		trap_Cmd_ExecuteText( EXEC_NOW, va( "rml_capture ui_setBindKey %i %i\n", tab, r ) );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_setBindKey" ) ) {
		tab = atoi( UI_Argv( 1 ) );
		r = atoi( UI_Argv( 2 ) );
		key = atoi( UI_Argv( 3 ) );
		trap_Cvar_Set( "ui_setWait", "" );
		if ( tab < 0 || tab >= NUM_TABS || r < 0 || r >= settingTabs[tab].numRows || settingTabs[tab].rows[r].type != ROW_BIND ) {
			return qtrue;
		}
		if ( key == K_BACKSPACE || key == K_DEL ) {
			// clear the row's keys
			Settings_KeysForCommand( settingTabs[tab].rows[r].cvar, keys );
			if ( keys[0] != -1 ) trap_Key_SetBinding( keys[0], "" );
			if ( keys[1] != -1 ) trap_Key_SetBinding( keys[1], "" );
		} else if ( key > 0 && key != K_ESCAPE ) {
			// a third key replaces the first
			Settings_KeysForCommand( settingTabs[tab].rows[r].cvar, keys );
			if ( keys[1] != -1 && key != keys[0] && key != keys[1] ) {
				trap_Key_SetBinding( keys[0], "" );
			}
			trap_Key_SetBinding( key, settingTabs[tab].rows[r].cvar );
		}
		// any row may have lost a key
		for ( r = 0; r < settingTabs[TAB_CONTROLS].numRows; r++ ) {
			Rml_BindText( TAB_CONTROLS, r );
		}
		return qtrue;
	}
	return qfalse;
}
