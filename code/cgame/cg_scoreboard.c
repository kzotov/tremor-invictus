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
// cg_scoreboard -- draw the scoreboard on top of the game screen
#include "cg_local.h"

static qboolean CG_FlagBoard( void );
static int CG_ModeBoard( void );


#define	SCOREBOARD_X		(0)

#define SB_HEADER			86
#define SB_TOP				(SB_HEADER+32)

// Where the status bar starts, so we don't overwrite it
#define SB_STATUSBAR		420

#define SB_NORMAL_HEIGHT	40
#define SB_INTER_HEIGHT		16 // interleaved height

#define SB_MAXCLIENTS_NORMAL  ((SB_STATUSBAR - SB_TOP) / SB_NORMAL_HEIGHT)
#define SB_MAXCLIENTS_INTER   ((SB_STATUSBAR - SB_TOP) / SB_INTER_HEIGHT - 1)

// Used when interleaved



#define SB_LEFT_BOTICON_X	(SCOREBOARD_X+0)
#define SB_LEFT_HEAD_X		(SCOREBOARD_X+32)
#define SB_RIGHT_BOTICON_X	(SCOREBOARD_X+64)
#define SB_RIGHT_HEAD_X		(SCOREBOARD_X+96)
// Normal
#define SB_BOTICON_X		(SCOREBOARD_X+32)
#define SB_HEAD_X			(SCOREBOARD_X+64)

#define SB_SCORELINE_X		112

#define SB_RATING_WIDTH	    (6 * BIGCHAR_WIDTH) // width 6
#define SB_SCORE_X			(SB_SCORELINE_X + BIGCHAR_WIDTH) // width 6
#define SB_RATING_X			(SB_SCORELINE_X + 6 * BIGCHAR_WIDTH) // width 6
#define SB_PING_X			(SB_SCORELINE_X + 12 * BIGCHAR_WIDTH + 8) // width 5
#define SB_TIME_X			(SB_SCORELINE_X + 17 * BIGCHAR_WIDTH + 8) // width 5
#define SB_NAME_X			(SB_SCORELINE_X + 22 * BIGCHAR_WIDTH) // width 15

// The new and improved score board
//
// In cases where the number of clients is high, the score board heads are interleaved
// here's the layout

//
//	0   32   80  112  144   240  320  400   <-- pixel position
//  bot head bot head score ping time name
//  
//  wins/losses are drawn on bot icon now

static qboolean localClient; // true if local client has been displayed


							 /*
=================
CG_DrawScoreboard
=================
*/
static void CG_DrawClientScore( int y, score_t *score, float *color, float fade, qboolean largeFormat ) {
	char	string[1024];
	vec3_t	headAngles;
	clientInfo_t	*ci;
	int iconx, headx;
	qhandle_t	portrait;

	if ( score->client < 0 || score->client >= cgs.maxclients ) {
		Com_Printf( "Bad score->client: %i\n", score->client );
		return;
	}
	
	ci = &cgs.clientinfo[score->client];

	iconx = SB_BOTICON_X + (SB_RATING_WIDTH / 2);
	headx = SB_HEAD_X + (SB_RATING_WIDTH / 2);

	// draw the handicap or bot skill marker (unless player has flag)
	if ( ci->powerups & ( 1 << PW_NEUTRALFLAG ) ) {
		if( largeFormat ) {
			CG_DrawFlagModel( iconx, y - ( 32 - BIGCHAR_HEIGHT ) / 2, 32, 32, TEAM_FREE, qfalse );
		}
		else {
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_FREE, qfalse );
		}
	} else if ( ci->powerups & ( 1 << PW_REDFLAG ) ) {
		if( largeFormat ) {
			CG_DrawFlagModel( iconx, y - ( 32 - BIGCHAR_HEIGHT ) / 2, 32, 32, TEAM_RED, qfalse );
		}
		else {
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_RED, qfalse );
		}
	} else if ( ci->powerups & ( 1 << PW_BLUEFLAG ) ) {
		if( largeFormat ) {
			CG_DrawFlagModel( iconx, y - ( 32 - BIGCHAR_HEIGHT ) / 2, 32, 32, TEAM_BLUE, qfalse );
		}
		else {
			CG_DrawFlagModel( iconx, y, 16, 16, TEAM_BLUE, qfalse );
		}
	} else {
		if ( ci->botSkill > 0 && ci->botSkill <= 5 ) {
			if ( cg_drawIcons.integer ) {
				if( largeFormat ) {
					CG_DrawPic( iconx, y - ( 32 - BIGCHAR_HEIGHT ) / 2, 32, 32, cgs.media.botSkillShaders[ ci->botSkill - 1 ] );
				}
				else {
					CG_DrawPic( iconx, y, 16, 16, cgs.media.botSkillShaders[ ci->botSkill - 1 ] );
				}
			}
		} else if ( ci->handicap < 100 ) {
			Com_sprintf( string, sizeof( string ), "%i", ci->handicap );
			if ( cgs.gametype == GT_TOURNAMENT )
				CG_DrawSmallStringColor( iconx, y - SMALLCHAR_HEIGHT/2, string, color );
			else
				CG_DrawSmallStringColor( iconx, y, string, color );
		}

		// draw the wins / losses
		if ( cgs.gametype == GT_TOURNAMENT ) {
			Com_sprintf( string, sizeof( string ), "%i/%i", ci->wins, ci->losses );
			if( ci->handicap < 100 && !ci->botSkill ) {
				CG_DrawSmallStringColor( iconx, y + SMALLCHAR_HEIGHT/2, string, color );
			}
			else {
				CG_DrawSmallStringColor( iconx, y, string, color );
			}
		}
	}

	// the champion's portrait, or the model's face when there is none
	portrait = CG_ChampionPortrait( BG_ChampionForModel( ci->modelName ), !largeFormat );
	VectorClear( headAngles );
	headAngles[YAW] = 180;
	if( largeFormat ) {
		if ( portrait ) {
			CG_DrawPic( headx, y - ( ICON_SIZE - BIGCHAR_HEIGHT ) / 2, ICON_SIZE, ICON_SIZE, portrait );
		} else {
			CG_DrawHead( headx, y - ( ICON_SIZE - BIGCHAR_HEIGHT ) / 2, ICON_SIZE, ICON_SIZE,
				score->client, headAngles );
		}
	}
	else if ( portrait ) {
		CG_DrawPic( headx, y, 16, 16, portrait );
	} else {
		CG_DrawHead( headx, y, 16, 16, score->client, headAngles );
	}
	// voice chat: talking or muted, at the face's corner
	if ( largeFormat ) {
		CG_DrawVoip( headx + ICON_SIZE - 14, y - ( ICON_SIZE - BIGCHAR_HEIGHT ) / 2 + ICON_SIZE - 14, 16, score->client );
	} else {
		CG_DrawVoip( headx + 10, y + 8, 10, score->client );
	}

#ifdef MISSIONPACK
	// draw the team task
	if ( ci->teamTask != TEAMTASK_NONE ) {
		if ( ci->teamTask == TEAMTASK_OFFENSE ) {
			CG_DrawPic( headx + 48, y, 16, 16, cgs.media.assaultShader );
		}
		else if ( ci->teamTask == TEAMTASK_DEFENSE ) {
			CG_DrawPic( headx + 48, y, 16, 16, cgs.media.defendShader );
		}
	}
#endif
	// draw the score line
	if ( score->ping == -1 ) {
		Com_sprintf(string, sizeof(string),
			" connecting    %s", ci->name);
	} else if ( ci->team == TEAM_SPECTATOR ) {
		Com_sprintf(string, sizeof(string),
			" SPECT %3i %4i %s", score->ping, score->time, ci->name);
	} else if ( CG_ModeBoard() == 1 ) {
		// sacrifice: plants and extracts
		Com_sprintf(string, sizeof(string),
			"%4i %3i %5i %2i %2i %3i %s", score->score, score->deaths, score->damage, score->modeStat[0],
			score->modeStat[1], score->ping, ci->name);
	} else if ( CG_ModeBoard() == 2 ) {
		// clan arena: rounds survived
		Com_sprintf(string, sizeof(string),
			"%4i %3i %5i %2i %3i %s", score->score, score->deaths, score->damage, score->modeStat[0],
			score->ping, ci->name);
	} else if ( CG_FlagBoard() ) {
		// capture the flag: captures, assists and defends instead of damage
		Com_sprintf(string, sizeof(string),
			"%4i %3i %2i %2i %2i %3i %s", score->score, score->deaths, score->captures, score->assistCount,
			score->defendCount, score->ping, ci->name);
	} else {
		Com_sprintf(string, sizeof(string),
			"%4i %3i %5i %3i %s", score->score, score->deaths, score->damage, score->ping, ci->name);
	}

	// highlight your position
	if ( score->client == cg.snap->ps.clientNum ) {
		float	hcolor[4];
		int		rank;

		localClient = qtrue;

		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR 
			|| cgs.gametype >= GT_TEAM ) {
			rank = -1;
		} else {
			rank = cg.snap->ps.persistant[PERS_RANK] & ~RANK_TIED_FLAG;
		}
		if ( rank == 0 ) {
			hcolor[0] = 0;
			hcolor[1] = 0;
			hcolor[2] = 0.7f;
		} else if ( rank == 1 ) {
			hcolor[0] = 0.7f;
			hcolor[1] = 0;
			hcolor[2] = 0;
		} else if ( rank == 2 ) {
			hcolor[0] = 0.7f;
			hcolor[1] = 0.7f;
			hcolor[2] = 0;
		} else {
			hcolor[0] = 0.7f;
			hcolor[1] = 0.7f;
			hcolor[2] = 0.7f;
		}

		hcolor[3] = fade * 0.7;
		CG_FillRect( SB_SCORELINE_X + BIGCHAR_WIDTH + (SB_RATING_WIDTH / 2), y, 
			640 - SB_SCORELINE_X - BIGCHAR_WIDTH, BIGCHAR_HEIGHT+1, hcolor );
	}

	CG_DrawBigString( SB_SCORELINE_X + (SB_RATING_WIDTH / 2), y, string, fade );

	// the champion after the name, small, where it fits
	if ( score->ping != -1 && ci->team != TEAM_SPECTATOR ) {
		int			champ = BG_ChampionForModel( ci->modelName );
		const char	*champName = champ >= 0 ? BG_String( BG_Champion( champ )->nameString ) : "";
		int			x = SB_SCORELINE_X + ( SB_RATING_WIDTH / 2 ) + CG_DrawStrlen( string ) * BIGCHAR_WIDTH + 8;
		vec4_t		gray = { 0.6f, 0.6f, 0.6f, 1 };

		gray[3] = fade;
		if ( champName[0] && x + CG_DrawStrlen( champName ) * SMALLCHAR_WIDTH <= 640 ) {
			CG_DrawSmallStringColor( x, y + ( BIGCHAR_HEIGHT - SMALLCHAR_HEIGHT ) / 2, champName, gray );
		}
	}

	// add the "ready" marker for intermission exiting, or in warm-up
	if ( ( cg.warmup || cg.snap->ps.pm_type == PM_INTERMISSION )
		&& ( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << score->client ) ) ) {
		CG_DrawBigStringColor( iconx, y, "READY", color );
	}
}

/*
=================
CG_DrawDuelBoard

Both duel players: frags, damage given and taken, major pickups, and
accuracy for every weapon either fired
=================
*/
void CG_DrawDuelBoard( void ) {
	static const vec4_t	back = { 0, 0, 0, 0.6f };
	static const int	rowLabels[] = { STR_DB_DAMAGE, STR_DB_TAKEN, STR_DB_MEGA, STR_DB_HEAVY, STR_DB_LIGHT };
	int			p, w, y, i, value, col[2] = { 300, 460 };
	clientInfo_t	*ci;

	if ( cg.time - cg.duelStatsTime > 1000 || cg.duelStatsTime > cg.time ) {
		cg.duelStatsTime = cg.time;
		trap_SendClientCommand( "dstats" );
	}

	CG_FillRect( 60, 60, 520, 360, back );
	if ( cg.numDuelStats < 1 ) {
		return;
	}

	// names, champions and frags
	for ( p = 0 ; p < cg.numDuelStats ; p++ ) {
		ci = &cgs.clientinfo[cg.duelStats[p].client];
		if ( ci->modelIcon ) {
			CG_DrawPic( col[p] + 24, 70, 32, 32, ci->modelIcon );
		}
		CG_DrawStringExt( col[p] + 40 - CG_DrawStrlen( ci->name ) * SMALLCHAR_WIDTH / 2, 106, ci->name,
			colorWhite, qfalse, qtrue, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 15 );
		CG_DrawBigString( col[p] + 40 - CG_DrawStrlen( va( "%i", cg.duelStats[p].score ) ) * BIGCHAR_WIDTH / 2,
			126, va( "%i", cg.duelStats[p].score ), 1.0f );
	}

	// totals
	y = 160;
	for ( i = 0 ; i < ARRAY_LEN( rowLabels ) ; i++ ) {
		CG_DrawSmallString( 80, y, BG_String( rowLabels[i] ), 1.0f );
		for ( p = 0 ; p < cg.numDuelStats ; p++ ) {
			switch ( i ) {
			case 0:		value = cg.duelStats[p].given; break;
			case 1:		value = cg.duelStats[p].taken; break;
			case 2:		value = cg.duelStats[p].mega; break;
			case 3:		value = cg.duelStats[p].heavy; break;
			default:	value = cg.duelStats[p].light; break;
			}
			CG_DrawSmallString( col[p] + 40 - CG_DrawStrlen( va( "%i", value ) ) * SMALLCHAR_WIDTH / 2, y, va( "%i", value ), 1.0f );
		}
		y += 18;
	}

	// accuracy per weapon
	y += 8;
	CG_DrawSmallString( 80, y, BG_String( STR_DB_ACCURACY ), 1.0f );
	y += 20;
	for ( w = WP_GAUNTLET + 1 ; w < WP_NUM_WEAPONS && y < 404 ; w++ ) {
		if ( !cg.duelStats[0].shots[w] && ( cg.numDuelStats < 2 || !cg.duelStats[1].shots[w] ) ) {
			continue;
		}
		CG_RegisterWeapon( w );
		if ( cg_weapons[w].weaponIcon ) {
			CG_DrawPic( 80, y, 16, 16, cg_weapons[w].weaponIcon );
		}
		for ( p = 0 ; p < cg.numDuelStats ; p++ ) {
			const char	*s = cg.duelStats[p].shots[w]
				? va( "%i%% %i/%i", cg.duelStats[p].hits[w] * 100 / cg.duelStats[p].shots[w], cg.duelStats[p].hits[w], cg.duelStats[p].shots[w] )
				: "-";

			CG_DrawStringExt( col[p] + 40 - CG_DrawStrlen( s ) * TINYCHAR_WIDTH / 2, y + 4, s,
				colorWhite, qfalse, qtrue, TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
		}
		y += 18;
	}
}

/*
=================
CG_TeamScoreboard
=================
*/
static int CG_TeamScoreboard( int y, team_t team, float fade, int maxClients, int lineHeight ) {
	int		i;
	score_t	*score;
	float	color[4];
	int		count;
	clientInfo_t	*ci;

	color[0] = color[1] = color[2] = 1.0;
	color[3] = fade;

	count = 0;
	for ( i = 0 ; i < cg.numScores && count < maxClients ; i++ ) {
		score = &cg.scores[i];
		ci = &cgs.clientinfo[ score->client ];

		if ( team != ci->team ) {
			continue;
		}

		CG_DrawClientScore( y + lineHeight * count, score, color, fade, lineHeight == SB_NORMAL_HEIGHT );

		count++;
	}

	return count;
}

/*
=================
CG_DrawScoreboard

Draw the normal in-game scoreboard
=================
*/
qboolean CG_DrawOldScoreboard( void ) {
	int		x, y, w, i, n1, n2;
	float	fade;
	float	*fadeColor;
	char	*s;
	int maxClients;
	int lineHeight;
	int topBorderSize, bottomBorderSize;

	// don't draw amuthing if the menu or console is up
	if ( cg_paused.integer ) {
		cg.deferredPlayerLoading = 0;
		return qfalse;
	}

	if ( cgs.gametype == GT_SINGLE_PLAYER && cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		cg.deferredPlayerLoading = 0;
		return qfalse;
	}

	// scoreboard.md: the RmlUi board, when the client has it
	if ( CG_RmlScoreboard() ) {
		return cg.showScores || cg.predictedPlayerState.pm_type == PM_INTERMISSION;
	}

	// don't draw scoreboard during death while warmup up
	if ( cg.warmup && !cg.showScores ) {
		return qfalse;
	}

	// duel: the two players side by side
	if ( cgs.gametype == GT_TOURNAMENT && ( cg.showScores || cg.predictedPlayerState.pm_type == PM_INTERMISSION ) ) {
		CG_DrawDuelBoard();
		return qtrue;
	}

	// a death shows the fragged-by panel (hud.md 7.5), not the board
	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		fade = 1.0;
		fadeColor = colorWhite;
	} else {
		fadeColor = CG_FadeColor( cg.scoreFadeTime, FADE_TIME );
		
		if ( !fadeColor ) {
			// next time scoreboard comes up, don't print killer
			cg.deferredPlayerLoading = 0;
			cg.killerName[0] = 0;
			return qfalse;
		}
		fade = *fadeColor;
	}


	// current rank
	if ( cgs.gametype < GT_TEAM) {
		if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
			s = va("%s place with %i",
				CG_PlaceString( cg.snap->ps.persistant[PERS_RANK] + 1 ),
				cg.snap->ps.persistant[PERS_SCORE] );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
			x = ( SCREEN_WIDTH - w ) / 2;
			y = 60;
			CG_DrawBigString( x, y, s, fade );
		}
	} else {
		if ( cg.teamScores[0] == cg.teamScores[1] ) {
			s = va("Teams are tied at %i", cg.teamScores[0] );
		} else if ( cg.teamScores[0] >= cg.teamScores[1] ) {
			s = va("Red leads %i to %i",cg.teamScores[0], cg.teamScores[1] );
		} else {
			s = va("Blue leads %i to %i",cg.teamScores[1], cg.teamScores[0] );
		}

		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		x = ( SCREEN_WIDTH - w ) / 2;
		y = 60;
		CG_DrawBigString( x, y, s, fade );
	}

	// scoreboard
	y = SB_HEADER;

	// column titles over the score line's fields (see CG_DrawClientScore)
	{
		static const int	titles[] = { STR_SB_SCORE, STR_SB_DEATHS, STR_SB_DAMAGE, STR_SB_PING, STR_SB_NAME };
		static const int	columns[] = { 0, 5, 9, 15, 19 };		// in big characters
		static const int	flagTitles[] = { STR_SB_SCORE, STR_SB_DEATHS, STR_SB_CAPS, STR_SB_ASSISTS, STR_SB_DEFENDS, STR_SB_PING, STR_SB_NAME };
		static const int	flagColumns[] = { 0, 5, 9, 12, 15, 18, 22 };
		static const int	sacTitles[] = { STR_SB_SCORE, STR_SB_DEATHS, STR_SB_DAMAGE, STR_SB_PLANTS, STR_SB_EXTRACTS, STR_SB_PING, STR_SB_NAME };
		static const int	sacColumns[] = { 0, 5, 9, 15, 18, 21, 25 };
		static const int	caTitles[] = { STR_SB_SCORE, STR_SB_DEATHS, STR_SB_DAMAGE, STR_SB_ROUNDS, STR_SB_PING, STR_SB_NAME };
		static const int	caColumns[] = { 0, 5, 9, 15, 18, 22 };
		int					i, x0 = SB_SCORELINE_X + ( SB_RATING_WIDTH / 2 );

		if ( CG_ModeBoard() == 1 ) {
			for ( i = 0 ; i < ARRAY_LEN( sacTitles ) ; i++ ) {
				CG_DrawSmallString( x0 + sacColumns[i] * BIGCHAR_WIDTH, y + 12, BG_String( sacTitles[i] ), fade );
			}
		} else if ( CG_ModeBoard() == 2 ) {
			for ( i = 0 ; i < ARRAY_LEN( caTitles ) ; i++ ) {
				CG_DrawSmallString( x0 + caColumns[i] * BIGCHAR_WIDTH, y + 12, BG_String( caTitles[i] ), fade );
			}
		} else if ( CG_FlagBoard() ) {
			for ( i = 0 ; i < ARRAY_LEN( flagTitles ) ; i++ ) {
				CG_DrawSmallString( x0 + flagColumns[i] * BIGCHAR_WIDTH, y + 12, BG_String( flagTitles[i] ), fade );
			}
		} else {
			for ( i = 0 ; i < ARRAY_LEN( titles ) ; i++ ) {
				CG_DrawSmallString( x0 + columns[i] * BIGCHAR_WIDTH, y + 12, BG_String( titles[i] ), fade );
			}
		}
	}

	y = SB_TOP;

	// If there are more than SB_MAXCLIENTS_NORMAL, use the interleaved scores
	if ( cg.numScores > SB_MAXCLIENTS_NORMAL ) {
		maxClients = SB_MAXCLIENTS_INTER;
		lineHeight = SB_INTER_HEIGHT;
		topBorderSize = 8;
		bottomBorderSize = 16;
	} else {
		maxClients = SB_MAXCLIENTS_NORMAL;
		lineHeight = SB_NORMAL_HEIGHT;
		topBorderSize = 16;
		bottomBorderSize = 16;
	}

	localClient = qfalse;

	if ( cgs.gametype >= GT_TEAM ) {
		//
		// teamplay scoreboard
		//
		y += lineHeight/2;

		if ( cg.teamScores[0] >= cg.teamScores[1] ) {
			n1 = CG_TeamScoreboard( y, TEAM_RED, fade, maxClients, lineHeight );
			CG_DrawTeamBackground( 0, y - topBorderSize, 640, n1 * lineHeight + bottomBorderSize, 0.33f, TEAM_RED );
			y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n1;
			n2 = CG_TeamScoreboard( y, TEAM_BLUE, fade, maxClients, lineHeight );
			CG_DrawTeamBackground( 0, y - topBorderSize, 640, n2 * lineHeight + bottomBorderSize, 0.33f, TEAM_BLUE );
			y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n2;
		} else {
			n1 = CG_TeamScoreboard( y, TEAM_BLUE, fade, maxClients, lineHeight );
			CG_DrawTeamBackground( 0, y - topBorderSize, 640, n1 * lineHeight + bottomBorderSize, 0.33f, TEAM_BLUE );
			y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n1;
			n2 = CG_TeamScoreboard( y, TEAM_RED, fade, maxClients, lineHeight );
			CG_DrawTeamBackground( 0, y - topBorderSize, 640, n2 * lineHeight + bottomBorderSize, 0.33f, TEAM_RED );
			y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
			maxClients -= n2;
		}
		n1 = CG_TeamScoreboard( y, TEAM_SPECTATOR, fade, maxClients, lineHeight );
		y += (n1 * lineHeight) + BIGCHAR_HEIGHT;

	} else {
		//
		// free for all scoreboard
		//
		n1 = CG_TeamScoreboard( y, TEAM_FREE, fade, maxClients, lineHeight );
		y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
		n2 = CG_TeamScoreboard( y, TEAM_SPECTATOR, fade, maxClients - n1, lineHeight );
		y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
	}

	if (!localClient) {
		// draw local client at the bottom
		for ( i = 0 ; i < cg.numScores ; i++ ) {
			if ( cg.scores[i].client == cg.snap->ps.clientNum ) {
				CG_DrawClientScore( y, &cg.scores[i], fadeColor, fade, lineHeight == SB_NORMAL_HEIGHT );
				break;
			}
		}
	}

	// load any models that have been deferred
	if ( ++cg.deferredPlayerLoading > 10 ) {
		CG_LoadDeferredPlayers();
	}

	return qtrue;
}

//================================================================================

/*
================
CG_CenterGiantLine
================
*/
static void CG_CenterGiantLine( float y, const char *string ) {
	float		x;
	vec4_t		color;

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	x = 0.5 * ( 640 - GIANT_WIDTH * CG_DrawStrlen( string ) );

	CG_DrawStringExt( x, y, string, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
}

/*
=================
CG_DrawTourneyScoreboard

Draw the oversize scoreboard for tournements
=================
*/
void CG_DrawTourneyScoreboard( void ) {
	const char		*s;
	vec4_t			color;
	int				min, tens, ones;
	clientInfo_t	*ci;
	int				y;
	int				i;

	// request more scores regularly
	if ( cg.scoresRequestTime + 2000 < cg.time ) {
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );
	}

	// draw the dialog background
	color[0] = color[1] = color[2] = 0;
	color[3] = 1;
	CG_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color );

	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	// print the mesage of the day
	s = CG_ConfigString( CS_MOTD );
	if ( !s[0] ) {
		s = "Scoreboard";
	}

	// print optional title
	CG_CenterGiantLine( 8, s );

	// print server time
	ones = cg.time / 1000;
	min = ones / 60;
	ones %= 60;
	tens = ones / 10;
	ones %= 10;
	s = va("%i:%i%i", min, tens, ones );

	CG_CenterGiantLine( 64, s );


	// print the two scores

	y = 160;
	if ( cgs.gametype >= GT_TEAM ) {
		//
		// teamplay scoreboard
		//
		CG_DrawStringExt( 8, y, "Red Team", color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
		s = va("%i", cg.teamScores[0] );
		CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
		
		y += 64;

		CG_DrawStringExt( 8, y, "Blue Team", color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
		s = va("%i", cg.teamScores[1] );
		CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
	} else {
		//
		// free for all scoreboard
		//
		for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
			ci = &cgs.clientinfo[i];
			if ( !ci->infoValid ) {
				continue;
			}
			if ( ci->team != TEAM_FREE ) {
				continue;
			}

			CG_DrawStringExt( 8, y, ci->name, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
			s = va("%i", ci->score );
			CG_DrawStringExt( 632 - GIANT_WIDTH * strlen(s), y, s, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );
			y += 64;
		}
	}


}
/*
=================
CG_FlagBoard

Capture the flag (not sacrifice) shows captures, assists and defends
=================
*/
/*
=================
CG_ModeBoard

1 for sacrifice's columns, 2 for clan arena's, 0 for neither
=================
*/
static int CG_ModeBoard( void ) {
	const char *info = CG_ConfigString( CS_SERVERINFO );

	if ( atoi( Info_ValueForKey( info, "g_sacrifice" ) ) ) {
		return 1;
	}
	if ( atoi( Info_ValueForKey( info, "g_clanArena" ) ) ) {
		return 2;
	}
	return 0;
}

static qboolean CG_FlagBoard( void ) {
	return cgs.gametype == GT_CTF && !atoi( Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "g_sacrifice" ) );
}



/*
================================================================================

THE RMLUI SCOREBOARD (scoreboard.md)

The board is the document ui/scoreboard.rml, drawn by the client over the
game while rml_overlay names it. The game fills it through sb_* cvars:
rows are '|'-separated fields (see the document), written only when they
change and at most every 100 ms.

================================================================================
*/

char	cg_portraitPath[32][MAX_QPATH];

#define SB_ROWS			24		// free-for-all rows
#define SB_TEAM_ROWS	12		// rows per team
#define SB_WEAPON_ROWS	8		// duel weapon lines

static void CG_BoardCvar( const char *name, const char *value ) {
	char	old[MAX_CVAR_VALUE_STRING];

	trap_Cvar_VariableStringBuffer( name, old, sizeof( old ) );
	if ( strcmp( old, value ) ) {
		trap_Cvar_Set( name, value );
	}
}

// a name for a row: no colour codes, no field separators
static const char *CG_BoardName( int client ) {
	static char	name[MAX_QPATH];
	char		*p;

	Q_strncpyz( name, cgs.clientinfo[client].name, sizeof( name ) );
	Q_CleanStr( name );
	for ( p = name ; *p ; p++ ) {
		if ( *p == '|' ) {
			*p = '/';
		}
	}
	return name;
}

static const char *CG_BoardPortrait( int client ) {
	int		champion = BG_ChampionForModel( cgs.clientinfo[client].modelName );

	return champion >= 0 && champion < 32 ? cg_portraitPath[champion] : "";
}

// the ping text and its colour class (green to 60 ms, yellow to 120)
static void CG_BoardPing( int client, int ping, char *text, int size, const char **cls ) {
	if ( cgs.clientinfo[client].botSkill ) {
		Q_strncpyz( text, BG_String( STR_BD_BOT ), size );
		*cls = "bot";
		return;
	}
	Com_sprintf( text, size, "%i", ping );
	*cls = ping <= 60 ? "good" : ping <= 120 ? "mid" : "bad";
}

// the viewed player: you, or whoever you follow
static int CG_BoardViewed( void ) {
	return cg.snap ? cg.snap->ps.clientNum : cg.clientNum;
}

/*
=================
CG_BoardRow

classes|portrait|rank|name|v4|v5|v6|v7|v8|v9|ping|ping class
=================
*/
static void CG_BoardRow( const char *cvar, const score_t *s, const char *rank, const char *classes, const char **v ) {
	char		ping[16], row[MAX_CVAR_VALUE_STRING];
	const char	*pingClass;
	clientInfo_t	*ci = &cgs.clientinfo[s->client];

	CG_BoardPing( s->client, s->ping, ping, sizeof( ping ), &pingClass );
	Com_sprintf( row, sizeof( row ), "%s%s%s%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s",
		classes,
		ci->team == TEAM_RED ? " red" : ci->team == TEAM_BLUE ? " blue" : "",
		s->client == CG_BoardViewed() ? " self" : "",
		s->ping == 999 ? " connecting" : "",
		CG_BoardPortrait( s->client ), rank, CG_BoardName( s->client ),
		v[0], v[1], v[2], v[3], v[4], v[5], ping, pingClass );
	CG_BoardCvar( cvar, row );
}

// one score's values for its mode's columns; returns how many
static int CG_BoardValues( const score_t *s, char values[6][16] ) {
	int		i, n = 0, mode = CG_ModeBoard();

	for ( i = 0 ; i < 6 ; i++ ) {
		values[i][0] = 0;
	}
	if ( cgs.gametype < GT_TEAM ) {
		Com_sprintf( values[n++], 16, "%i", s->score );
		Com_sprintf( values[n++], 16, "%i", s->deaths );
		{
			// kills / max(deaths, 1), one decimal; the frag score stands in
			// for kills (suicides lower it), never below 0
			int		kd = ( s->score > 0 ? s->score : 0 ) * 10 / ( s->deaths > 1 ? s->deaths : 1 );

			Com_sprintf( values[n++], 16, "%i.%i", kd / 10, kd % 10 );
		}
		Com_sprintf( values[n++], 16, "%i", s->damage );
		Com_sprintf( values[n++], 16, "%i%%", s->accuracy );
	} else if ( mode == 1 ) {		// Sacrifice
		Com_sprintf( values[n++], 16, "%i", s->score );
		Com_sprintf( values[n++], 16, "%i", s->deaths );
		Com_sprintf( values[n++], 16, "%i", s->modeStat[0] );
		Com_sprintf( values[n++], 16, "%i", s->modeStat[1] );
	} else if ( mode == 2 ) {		// Clan Arena
		Com_sprintf( values[n++], 16, "%i", s->damage );
		Com_sprintf( values[n++], 16, "%i", s->score );
		Com_sprintf( values[n++], 16, "%i", s->deaths );
	} else if ( CG_FlagBoard() ) {
		Com_sprintf( values[n++], 16, "%i", s->score );
		Com_sprintf( values[n++], 16, "%i", s->captures );
		Com_sprintf( values[n++], 16, "%i", s->assistCount );
		Com_sprintf( values[n++], 16, "%i", s->defendCount );
		Com_sprintf( values[n++], 16, "%i", s->deaths );
	} else {
		Com_sprintf( values[n++], 16, "%i", s->score );
		Com_sprintf( values[n++], 16, "%i", s->deaths );
		Com_sprintf( values[n++], 16, "%i", s->damage );
	}
	return n;
}

// the column titles in the same order
static int CG_BoardTitles( const char **t ) {
	int		n = 0, mode = CG_ModeBoard();

	if ( cgs.gametype < GT_TEAM ) {
		t[n++] = BG_String( STR_BD_SCORE );
		t[n++] = BG_String( STR_BD_DEATHS );
		t[n++] = BG_String( STR_BD_KD );
		t[n++] = BG_String( STR_BD_DAMAGE );
		t[n++] = BG_String( STR_BD_ACC );
	} else if ( mode == 1 ) {
		t[n++] = BG_String( STR_BD_SCORE );
		t[n++] = BG_String( STR_BD_DEATHS );
		t[n++] = BG_String( STR_BD_PLANTS );
		t[n++] = BG_String( STR_BD_EXTRACTS );
	} else if ( mode == 2 ) {
		t[n++] = BG_String( STR_BD_DAMAGE );
		t[n++] = BG_String( STR_BD_SCORE );
		t[n++] = BG_String( STR_BD_DEATHS );
	} else if ( CG_FlagBoard() ) {
		t[n++] = BG_String( STR_BD_SCORE );
		t[n++] = BG_String( STR_BD_CAPS );
		t[n++] = BG_String( STR_BD_ASSISTS );
		t[n++] = BG_String( STR_BD_DEFENDS );
		t[n++] = BG_String( STR_BD_DEATHS );
	} else {
		t[n++] = BG_String( STR_BD_SCORE );
		t[n++] = BG_String( STR_BD_DEATHS );
		t[n++] = BG_String( STR_BD_DAMAGE );
	}
	return n;
}

/*
=================
CG_BoardHeader

The mode and map, the score line, the time and the round
=================
*/
static void CG_BoardHeader( void ) {
	char	text[MAX_CVAR_VALUE_STRING], map[MAX_QPATH];
	int		sec, me = CG_BoardViewed(), mine = -1, best = -1, i;
	const char	*info = CG_ConfigString( CS_SERVERINFO );

	COM_StripExtension( COM_SkipPath( cgs.mapname ), map, sizeof( map ) );
	Com_sprintf( text, sizeof( text ), "%s  -  %s", CG_ModeString(), map );
	CG_BoardCvar( "sb_title", text );

	// the score line: teams by their scores, else you against the best of
	// the rest
	text[0] = 0;
	if ( cgs.gametype >= GT_TEAM ) {
		Com_sprintf( text, sizeof( text ), "%s %i  -  %i %s", BG_String( STR_HUD_RED ), cg.teamScores[0],
			cg.teamScores[1], BG_String( STR_HUD_BLUE ) );
	} else if ( cg.snap && cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		for ( i = 0 ; i < cg.numScores ; i++ ) {
			if ( cg.scores[i].client == me ) {
				mine = cg.scores[i].score;
			} else if ( best < 0 || cg.scores[i].score > best ) {
				best = cg.scores[i].score;
			}
		}
		if ( mine >= 0 && best >= 0 ) {
			Com_sprintf( text, sizeof( text ), BG_String( mine > best ? STR_BD_LEAD : mine < best ? STR_BD_TRAIL : STR_BD_TIED ),
				mine > best ? mine - best : mine < best ? best - mine : mine );
		}
	}
	CG_BoardCvar( "sb_line", text );

	// time and limit
	sec = ( cg.time - cgs.levelStartTime ) / 1000;
	if ( sec < 0 || cg.warmup ) {
		sec = 0;
	}
	if ( cgs.timelimit > 0 ) {
		Com_sprintf( text, sizeof( text ), "%i:%02i / %i:00", sec / 60, sec % 60, cgs.timelimit );
	} else {
		Com_sprintf( text, sizeof( text ), "%i:%02i", sec / 60, sec % 60 );
	}
	CG_BoardCvar( "sb_time", text );

	// round modes: the round and the target
	text[0] = 0;
	if ( CG_ModeBoard() || atoi( Info_ValueForKey( info, "g_roundDuel" ) ) ) {
		const char	*limit = Info_ValueForKey( info, CG_ModeBoard() == 1 ? "g_sacRounds" : CG_ModeBoard() == 2 ? "g_caRounds" : "g_rdRounds" );

		if ( atoi( limit ) > 0 ) {
			Com_sprintf( text, sizeof( text ), BG_String( STR_BD_ROUND ), cg.teamScores[0] + cg.teamScores[1] + 1, atoi( limit ) );
		}
	}
	CG_BoardCvar( "sb_round", text );
}

/*
=================
CG_BoardSpectators
=================
*/
static void CG_BoardSpectators( void ) {
	char	names[MAX_CVAR_VALUE_STRING], text[MAX_CVAR_VALUE_STRING];
	int		i;

	names[0] = 0;
	for ( i = 0 ; i < cg.numScores ; i++ ) {
		if ( cgs.clientinfo[cg.scores[i].client].team != TEAM_SPECTATOR ) {
			continue;
		}
		if ( names[0] ) {
			Q_strcat( names, sizeof( names ), ", " );
		}
		Q_strcat( names, sizeof( names ), CG_BoardName( cg.scores[i].client ) );
	}
	if ( names[0] ) {
		Com_sprintf( text, sizeof( text ), BG_String( STR_BD_SPECTATORS ), names );
	} else {
		text[0] = 0;
	}
	CG_BoardCvar( "sb_specs", text );
}

/*
=================
CG_BoardList

Free-for-all and the team columns: the column titles and the rows
=================
*/
static void CG_BoardList( void ) {
	const char	*titles[6] = { "", "", "", "", "", "" }, *v[6];
	char		values[6][16], rank[8], head[MAX_CVAR_VALUE_STRING], cols[16], cvar[16];
	int			i, j, n, row, side, team, rows[2] = { 0, 0 }, total[2] = { 0, 0 }, shownRank = 1;
	int			left = TEAM_RED;
	score_t		*s;

	n = CG_BoardTitles( titles );
	Com_sprintf( cols, sizeof( cols ), "cols%i", n );
	Com_sprintf( head, sizeof( head ), "head %s||%s|%s|%s|%s|%s|%s|%s|%s|%s|",
		cols, cgs.gametype < GT_TEAM ? "#" : "", BG_String( STR_BD_PLAYER ),
		titles[0], titles[1], titles[2], titles[3], titles[4], titles[5], BG_String( STR_BD_PING ) );
	CG_BoardCvar( "sb_head", head );

	if ( cgs.gametype < GT_TEAM ) {
		row = 0;
		for ( i = 0 ; i < cg.numScores && row < SB_ROWS ; i++ ) {
			s = &cg.scores[i];
			if ( cgs.clientinfo[s->client].team == TEAM_SPECTATOR ) {
				continue;
			}
			// ties share a rank
			if ( row && s->score != cg.scores[i - 1].score ) {
				shownRank = row + 1;
			}
			Com_sprintf( rank, sizeof( rank ), "%i.", shownRank );
			CG_BoardValues( s, values );
			for ( j = 0 ; j < 6 ; j++ ) {
				v[j] = values[j];
			}
			Com_sprintf( cvar, sizeof( cvar ), "sb_f%i", row++ );
			CG_BoardRow( cvar, s, rank, cols, v );
		}
		for ( ; row < SB_ROWS ; row++ ) {
			CG_BoardCvar( va( "sb_f%i", row ), "" );
		}
		return;
	}

	// teams: yours on the left when you play, red on the left otherwise;
	// the viewed player first in their team
	if ( cg.snap && cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE ) {
		left = TEAM_BLUE;
	}
	for ( side = 0 ; side < 2 ; side++ ) {
		team = side ? ( left == TEAM_RED ? TEAM_BLUE : TEAM_RED ) : left;
		for ( j = 0 ; j < 2 ; j++ ) {		// pass 0: the viewed player, pass 1: the rest
			for ( i = 0 ; i < cg.numScores && rows[side] < SB_TEAM_ROWS ; i++ ) {
				s = &cg.scores[i];
				if ( cgs.clientinfo[s->client].team != team || ( s->client == CG_BoardViewed() ) != !j ) {
					continue;
				}
				CG_BoardValues( s, values );
				for ( n = 0 ; n < 6 ; n++ ) {
					v[n] = values[n];
				}
				total[side] += s->damage;
				Com_sprintf( cvar, sizeof( cvar ), "sb_%c%i", side ? 'r' : 'l', rows[side]++ );
				CG_BoardRow( cvar, s, "", cols, v );
			}
		}
		for ( ; rows[side] < SB_TEAM_ROWS ; rows[side]++ ) {
			CG_BoardCvar( va( "sb_%c%i", side ? 'r' : 'l', rows[side] ), "" );
		}
		// the team header: classes|name|score|totals
		Com_sprintf( head, sizeof( head ), "%s|%s|%i|%s", team == TEAM_RED ? "red" : "blue",
			BG_String( team == TEAM_RED ? STR_HUD_RED : STR_HUD_BLUE ), cg.teamScores[team == TEAM_RED ? 0 : 1],
			va( (char *)BG_String( STR_BD_TEAM_DAMAGE ), total[side] ) );
		CG_BoardCvar( side ? "sb_rt" : "sb_lt", head );
	}
}

/*
=================
CG_BoardDuel

The two players side by side; the opponent's detailed stats hidden during
the match (shown to spectators and at the end)
=================
*/
static void CG_BoardDuel( void ) {
	char	row[MAX_CVAR_VALUE_STRING], ping[16], acc[16], hs[24];
	int		p, w, line = 0, shots, hits, pingValue, i;
	int		me = CG_BoardViewed();
	qboolean	open = cg.snap && ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR
		|| cg.predictedPlayerState.pm_type == PM_INTERMISSION );
	const char	*pingClass;

	if ( cg.time - cg.duelStatsTime > 1000 || cg.duelStatsTime > cg.time ) {
		cg.duelStatsTime = cg.time;
		trap_SendClientCommand( "dstats" );
	}
	// you on the left (the board's left column is p 0)
	for ( p = 0 ; p < 2 ; p++ ) {
		int		idx = p;
		qboolean	hidden;

		if ( cg.numDuelStats == 2 && cg.duelStats[1].client == me ) {
			idx = !p;
		}
		if ( idx >= cg.numDuelStats ) {
			CG_BoardCvar( va( "sb_d%i", p ), "" );
			continue;
		}
		hidden = !open && cg.duelStats[idx].client != me;
		pingValue = 0;
		for ( i = 0 ; i < cg.numScores ; i++ ) {
			if ( cg.scores[i].client == cg.duelStats[idx].client ) {
				pingValue = cg.scores[i].ping;
			}
		}
		CG_BoardPing( cg.duelStats[idx].client, pingValue, ping, sizeof( ping ), &pingClass );
		shots = hits = 0;
		for ( w = WP_GAUNTLET + 1 ; w < WP_NUM_WEAPONS ; w++ ) {
			shots += cg.duelStats[idx].shots[w];
			hits += cg.duelStats[idx].hits[w];
		}
		Com_sprintf( acc, sizeof( acc ), shots ? "%i%%" : "-", shots ? hits * 100 / shots : 0 );
		Com_sprintf( hs, sizeof( hs ), "%i / %i", hits, shots );
		// classes|portrait|name|ping|ping class|frags|given|taken|accuracy|hits/shots|mega|heavy|light
		Com_sprintf( row, sizeof( row ), "%s|%s|%s|%s|%s|%i|%s|%s|%s|%s|%i|%i|%i",
			cg.duelStats[idx].client == me ? "self" : "",
			CG_BoardPortrait( cg.duelStats[idx].client ), CG_BoardName( cg.duelStats[idx].client ),
			ping, pingClass, cg.duelStats[idx].score,
			hidden ? "-" : va( "%i", cg.duelStats[idx].given ),
			hidden ? "-" : va( "%i", cg.duelStats[idx].taken ),
			hidden ? "-" : acc, hidden ? "-" : hs,
			cg.duelStats[idx].mega, cg.duelStats[idx].heavy, cg.duelStats[idx].light );
		CG_BoardCvar( va( "sb_d%i", p ), row );
	}

	// the weapons either player used: icon|acc|hits/shots|acc|hits/shots
	for ( w = WP_GAUNTLET + 1 ; w < WP_NUM_WEAPONS && line < SB_WEAPON_ROWS ; w++ ) {
		char	cell[2][32];
		gitem_t	*item;
		int		any = 0;

		for ( p = 0 ; p < 2 ; p++ ) {
			int		idx = p;

			if ( cg.numDuelStats == 2 && cg.duelStats[1].client == me ) {
				idx = !p;
			}
			cell[p][0] = 0;
			if ( idx >= cg.numDuelStats ) {
				continue;
			}
			if ( open || cg.duelStats[idx].client == me ) {
				any |= cg.duelStats[idx].shots[w];		// (the opponent's weapons stay hidden)
			}
			if ( !open && cg.duelStats[idx].client != me ) {
				Q_strncpyz( cell[p], "-|-", sizeof( cell[p] ) );
			} else if ( cg.duelStats[idx].shots[w] ) {
				Com_sprintf( cell[p], sizeof( cell[p] ), "%i%%|%i / %i", cg.duelStats[idx].hits[w] * 100 / cg.duelStats[idx].shots[w],
					cg.duelStats[idx].hits[w], cg.duelStats[idx].shots[w] );
			} else {
				Q_strncpyz( cell[p], "-|-", sizeof( cell[p] ) );
			}
		}
		if ( !any ) {
			continue;
		}
		item = BG_FindItemForWeapon( w );
		Com_sprintf( row, sizeof( row ), "%s.tga|%s|%s", item && item->icon ? item->icon : "", cell[0], cell[1] );
		CG_BoardCvar( va( "sb_w%i", line++ ), row );
	}
	for ( ; line < SB_WEAPON_ROWS ; line++ ) {
		CG_BoardCvar( va( "sb_w%i", line ), "" );
	}
	// the banner
	if ( cg.numDuelStats == 2 ) {
		int		mine = cg.duelStats[0].client == me ? cg.duelStats[0].score : cg.duelStats[1].score;
		int		theirs = cg.duelStats[0].client == me ? cg.duelStats[1].score : cg.duelStats[0].score;

		if ( !open ) {
			CG_BoardCvar( "sb_line", va( (char *)BG_String( mine > theirs ? STR_BD_LEAD : mine < theirs ? STR_BD_TRAIL : STR_BD_TIED ),
				mine > theirs ? mine - theirs : mine < theirs ? theirs - mine : mine ) );
		}
	}
}

/*
=================
CG_RmlScoreboard

True when the RmlUi board takes the place of the drawn one; shows it while
Tab is held or at intermission, hides it otherwise
=================
*/
static qboolean	boardShownThisFrame;

/*
=================
CG_RmlScoreboardEndFrame

After each frame's drawing: a board not drawn this frame (a menu, the
podium, the draft, cg_draw2D 0) is put away
=================
*/
void CG_RmlScoreboardEndFrame( void ) {
	if ( !boardShownThisFrame ) {
		CG_BoardCvar( "rml_overlay", "" );
	}
	boardShownThisFrame = qfalse;
}

qboolean CG_RmlScoreboard( void ) {
	static int	lastFill;
	char		ok[8];
	qboolean	show;

	trap_Cvar_VariableStringBuffer( "rml_ok", ok, sizeof( ok ) );
	if ( !cg_rmlScoreboard.integer || atoi( ok ) != 1 ) {
		CG_BoardCvar( "rml_overlay", "" );
		return qfalse;
	}
	show = cg.showScores || cg.predictedPlayerState.pm_type == PM_INTERMISSION;
	if ( !show ) {
		CG_BoardCvar( "rml_overlay", "" );
		return qtrue;
	}
	if ( cg.time - lastFill >= 100 || cg.time < lastFill ) {
		lastFill = cg.time;
		CG_BoardCvar( "sb_mode", cgs.gametype == GT_TOURNAMENT ? "duel" : cgs.gametype >= GT_TEAM ? "team" : "ffa" );
		CG_BoardHeader();
		if ( cgs.gametype == GT_TOURNAMENT ) {
			CG_BoardDuel();
		} else {
			CG_BoardList();
		}
		CG_BoardSpectators();
	}
	CG_BoardCvar( "rml_overlay", "ui/scoreboard.rml" );
	boardShownThisFrame = qtrue;
	return qtrue;
}
