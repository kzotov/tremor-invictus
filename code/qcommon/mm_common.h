/*
===========================================================================
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
// mm_common.h -- talking to the matchmaker: signed HTTP requests, keys, JSON

#ifndef MM_COMMON_H
#define MM_COMMON_H

#include "q_shared.h"

#define MM_BODY_SIZE	16384

typedef enum {
	MMR_IDLE,
	MMR_CONNECTING,
	MMR_SENDING,
	MMR_RECEIVING,
	MMR_DONE,
	MMR_FAILED
} mmRequestState_t;

typedef struct {
	mmRequestState_t	state;
	int					sock;
	int					startTime;
	char				out[MM_BODY_SIZE];
	int					outLen, outSent;
	char				in[MM_BODY_SIZE];
	int					inLen;
	int					status;		// HTTP status once done
	const char			*body;		// points into in[] once done
	char				what[32];	// the caller's label for this request
} mmRequest_t;

// load the key from a file in the home folder, making one on first use;
// fills the hex public key
qboolean MM_LoadKey( const char *file, byte seed[32], char pubHex[65] );

// start a signed request; body may be "" for none
qboolean MM_Start( mmRequest_t *r, const char *url, const byte seed[32], const char *pubHex,
	const char *method, const char *path, const char *body, const char *what );

// move the request along without blocking; qtrue once it is done or failed
qboolean MM_Poll( mmRequest_t *r );

void MM_Abort( mmRequest_t *r );

// the value of "key" in a flat JSON object: strings unquoted, numbers and
// literals as written, arrays and objects with their brackets
qboolean MM_JsonValue( const char *json, const char *key, char *out, int size );

#endif
