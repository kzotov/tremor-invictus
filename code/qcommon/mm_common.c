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
// mm_common.c -- talking to the matchmaker: signed HTTP requests, keys, JSON
//
// Plain HTTP/1.0 over a non-blocking TCP socket, one request per
// connection. The matchmaker checks every request's ed25519 signature, so
// a reply is only trusted as far as the connection is.

#include "q_shared.h"
#include "qcommon.h"
#include "mm_common.h"
#include "mm_crypto.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define MM_CLOSE closesocket
#define MM_WOULDBLOCK( e ) ( (e) == WSAEWOULDBLOCK || (e) == WSAEINPROGRESS )
#define MM_ERRNO WSAGetLastError()
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define MM_CLOSE close
#define MM_WOULDBLOCK( e ) ( (e) == EWOULDBLOCK || (e) == EAGAIN || (e) == EINPROGRESS )
#define MM_ERRNO errno
#endif

#define MM_TIMEOUT_MSEC	5000

static void HexEncode( const byte *in, int len, char *out ) {
	static const char digits[] = "0123456789abcdef";
	int i;

	for ( i = 0; i < len; i++ ) {
		out[i * 2] = digits[in[i] >> 4];
		out[i * 2 + 1] = digits[in[i] & 15];
	}
	out[len * 2] = 0;
}

qboolean MM_LoadKey( const char *file, byte seed[32], char pubHex[65] ) {
	fileHandle_t	f;
	byte			pub[32];
	int				len;

	len = FS_BaseDir_FOpenFileRead( file, &f );
	if ( f && len == 32 && FS_Read( seed, 32, f ) == 32 ) {
		FS_FCloseFile( f );
	} else {
		if ( f ) {
			FS_FCloseFile( f );
		}
		if ( !Sys_RandomBytes( seed, 32 ) ) {
			Com_Printf( S_COLOR_YELLOW "matchmaker: no system random source, the key is weaker\n" );
			Com_RandomBytes( seed, 32 );
		}
		f = FS_BaseDir_FOpenFileWrite_HomeState( file );
		if ( !f ) {
			Com_Printf( S_COLOR_YELLOW "matchmaker: can't write %s\n", file );
			return qfalse;
		}
		FS_Write( seed, 32, f );
		FS_FCloseFile( f );
		Com_Printf( "matchmaker: made a new key in %s\n", file );
	}
	MM_PublicKey( seed, pub );
	HexEncode( pub, 32, pubHex );
	return qtrue;
}

void MM_Abort( mmRequest_t *r ) {
	if ( r->state == MMR_CONNECTING || r->state == MMR_SENDING || r->state == MMR_RECEIVING ) {
		MM_CLOSE( r->sock );
	}
	r->state = MMR_IDLE;
}

static qboolean Fail( mmRequest_t *r, const char *why ) {
	Com_DPrintf( "matchmaker: %s: %s\n", r->what, why );
	if ( r->state == MMR_CONNECTING || r->state == MMR_SENDING || r->state == MMR_RECEIVING ) {
		MM_CLOSE( r->sock );
	}
	r->state = MMR_FAILED;
	r->status = 0;
	r->body = "";
	return qfalse;
}

qboolean MM_Start( mmRequest_t *r, const char *url, const byte seed[32], const char *pubHex,
	const char *method, const char *path, const char *body, const char *what ) {
	char			host[256], port[16], timeStr[32], sigHex[129];
	char			signedText[MM_BODY_SIZE];
	byte			sig[64];
	const char		*p, *colon, *slash;
	struct addrinfo	hints, *res;
	int				bodyLen, n;

	MM_Abort( r );
	Q_strncpyz( r->what, what, sizeof( r->what ) );
	r->startTime = Sys_Milliseconds();

	// http://host[:port]
	p = url;
	if ( !Q_stricmpn( p, "http://", 7 ) ) {
		p += 7;
	}
	slash = strchr( p, '/' );
	colon = strchr( p, ':' );
	if ( colon && ( !slash || colon < slash ) ) {
		n = colon - p;
		Q_strncpyz( port, colon + 1, sizeof( port ) );
		if ( slash ) {
			port[ slash - colon - 1 < (int)sizeof( port ) - 1 ? slash - colon - 1 : (int)sizeof( port ) - 1 ] = 0;
		}
	} else {
		n = slash ? slash - p : (int)strlen( p );
		Q_strncpyz( port, "80", sizeof( port ) );
	}
	if ( n <= 0 || n >= (int)sizeof( host ) ) {
		return Fail( r, "bad address" );
	}
	Q_strncpyz( host, p, n + 1 );

	// the signature covers method, path, time and body
	bodyLen = strlen( body );
	Com_sprintf( timeStr, sizeof( timeStr ), "%lld", (long long)time( NULL ) );
	n = Com_sprintf( signedText, sizeof( signedText ), "%s\n%s\n%s\n%s", method, path, timeStr, body );
	MM_Sign( seed, (const byte *)signedText, n, sig );
	HexEncode( sig, 64, sigHex );

	r->outLen = Com_sprintf( r->out, sizeof( r->out ),
		"%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\nContent-Type: application/json\r\n"
		"Content-Length: %d\r\nX-Key: %s\r\nX-Time: %s\r\nX-Signature: %s\r\n\r\n%s",
		method, path, host, bodyLen, pubHex, timeStr, sigHex, body );
	r->outSent = 0;
	r->inLen = 0;

	Com_Memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ( getaddrinfo( host, port, &hints, &res ) != 0 || !res ) {
		return Fail( r, "can't resolve the address" );
	}
	r->sock = socket( res->ai_family, res->ai_socktype, res->ai_protocol );
	if ( r->sock < 0 ) {
		freeaddrinfo( res );
		return Fail( r, "no socket" );
	}
#ifdef _WIN32
	{
		u_long on = 1;
		ioctlsocket( r->sock, FIONBIO, &on );
	}
#else
	fcntl( r->sock, F_SETFL, fcntl( r->sock, F_GETFL, 0 ) | O_NONBLOCK );
#endif
#ifdef SO_NOSIGPIPE
	{
		int on = 1;
		setsockopt( r->sock, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof( on ) );
	}
#endif
	r->state = MMR_CONNECTING;
	if ( connect( r->sock, res->ai_addr, res->ai_addrlen ) < 0 && !MM_WOULDBLOCK( MM_ERRNO ) ) {
		freeaddrinfo( res );
		return Fail( r, "can't connect" );
	}
	freeaddrinfo( res );
	return qtrue;
}

static void Finish( mmRequest_t *r ) {
	char *end;

	MM_CLOSE( r->sock );
	r->in[r->inLen] = 0;
	r->state = MMR_DONE;
	r->status = 0;
	r->body = "";
	if ( !Q_stricmpn( r->in, "HTTP/", 5 ) ) {
		const char *sp = strchr( r->in, ' ' );
		r->status = sp ? atoi( sp + 1 ) : 0;
	}
	end = strstr( r->in, "\r\n\r\n" );
	if ( end ) {
		r->body = end + 4;
	}
}

qboolean MM_Poll( mmRequest_t *r ) {
	int n;

	if ( r->state == MMR_IDLE || r->state == MMR_DONE || r->state == MMR_FAILED ) {
		return qtrue;
	}
	if ( Sys_Milliseconds() - r->startTime > MM_TIMEOUT_MSEC ) {
		Fail( r, "timed out" );
		return qtrue;
	}

	if ( r->state == MMR_CONNECTING ) {
		fd_set			w;
		struct timeval	tv = { 0, 0 };
		int				err = 0;
		socklen_t		len = sizeof( err );

		FD_ZERO( &w );
		FD_SET( r->sock, &w );
		if ( select( r->sock + 1, NULL, &w, NULL, &tv ) <= 0 ) {
			return qfalse;
		}
		getsockopt( r->sock, SOL_SOCKET, SO_ERROR, (char *)&err, &len );
		if ( err ) {
			Fail( r, "connection refused" );
			return qtrue;
		}
		r->state = MMR_SENDING;
	}

	if ( r->state == MMR_SENDING ) {
		n = send( r->sock, r->out + r->outSent, r->outLen - r->outSent, 0 );
		if ( n < 0 ) {
			if ( MM_WOULDBLOCK( MM_ERRNO ) ) {
				return qfalse;
			}
			Fail( r, "send failed" );
			return qtrue;
		}
		r->outSent += n;
		if ( r->outSent < r->outLen ) {
			return qfalse;
		}
		r->state = MMR_RECEIVING;
	}

	// read until the service closes the connection
	for ( ;; ) {
		if ( r->inLen >= (int)sizeof( r->in ) - 1 ) {
			Fail( r, "reply too long" );
			return qtrue;
		}
		n = recv( r->sock, r->in + r->inLen, sizeof( r->in ) - 1 - r->inLen, 0 );
		if ( n > 0 ) {
			r->inLen += n;
			continue;
		}
		if ( n == 0 ) {
			Finish( r );
			return qtrue;
		}
		if ( MM_WOULDBLOCK( MM_ERRNO ) ) {
			return qfalse;
		}
		Fail( r, "receive failed" );
		return qtrue;
	}
}

qboolean MM_JsonValue( const char *json, const char *key, char *out, int size ) {
	char		pattern[128];
	const char	*p, *start;
	int			depth, n;

	out[0] = 0;
	Com_sprintf( pattern, sizeof( pattern ), "\"%s\"", key );
	p = strstr( json, pattern );
	if ( !p ) {
		return qfalse;
	}
	p += strlen( pattern );
	while ( *p == ' ' || *p == ':' ) {
		p++;
	}
	if ( *p == '"' ) {
		start = ++p;
		while ( *p && *p != '"' ) {
			if ( *p == '\\' && p[1] ) {
				p++;
			}
			p++;
		}
	} else if ( *p == '[' || *p == '{' ) {
		start = p;
		depth = 0;
		while ( *p ) {
			if ( *p == '[' || *p == '{' ) {
				depth++;
			} else if ( *p == ']' || *p == '}' ) {
				if ( --depth == 0 ) {
					p++;
					break;
				}
			}
			p++;
		}
	} else {
		start = p;
		while ( *p && *p != ',' && *p != '}' && *p != ']' ) {
			p++;
		}
	}
	n = p - start;
	if ( n >= size ) {
		n = size - 1;
	}
	memcpy( out, start, n );
	out[n] = 0;
	return qtrue;
}
