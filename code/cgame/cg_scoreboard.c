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

	// draw the face
	VectorClear( headAngles );
	headAngles[YAW] = 180;
	if( largeFormat ) {
		CG_DrawHead( headx, y - ( ICON_SIZE - BIGCHAR_HEIGHT ) / 2, ICON_SIZE, ICON_SIZE, 
			score->client, headAngles );
	}
	else {
		CG_DrawHead( headx, y, 16, 16, score->client, headAngles );
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

	// add the "ready" marker for intermission exiting
	if ( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << score->client ) ) {
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

	// don't draw scoreboard during death while warmup up
	if ( cg.warmup && !cg.showScores ) {
		return qfalse;
	}

	// duel: the two players side by side
	if ( cgs.gametype == GT_TOURNAMENT && ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD
		|| cg.predictedPlayerState.pm_type == PM_INTERMISSION ) ) {
		CG_DrawDuelBoard();
		return qtrue;
	}

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ||
		 cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
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


	// fragged by ... line
	if ( cg.killerName[0] ) {
		s = va("Fragged by %s", cg.killerName );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		x = ( SCREEN_WIDTH - w ) / 2;
		y = 40;
		CG_DrawBigString( x, y, s, fade );
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


