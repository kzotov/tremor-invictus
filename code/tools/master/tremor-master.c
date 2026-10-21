/*
===========================================================================
tremor-master: the server list for Tremor Invictus

Copyright (C) 2026 the Tremor Invictus contributors

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
more details.
===========================================================================
*/
//
// A master server speaking the protocol the engine already uses:
//
//   game server -> master   "heartbeat <name>"
//   master -> game server   "getinfo <challenge>"
//   game server -> master   "infoResponse\n<info string>" (with the challenge)
//   client -> master        "getservers <protocol> [empty] [full]"
//                           or "getserversExt <game> <protocol> ..."
//   master -> client        "getserversResponse" then "\" + 4-byte address
//                           + 2-byte port per server, ended by "\EOT\0\0\0"
//
// Every packet starts with four 0xff bytes. Servers drop off the list 15
// minutes after their last heartbeat. A "status" request answers with a
// readable list (one line per server: address, mode, map, players, name).
//
// Usage: tremor-master [-p port] [-v]      (port 27950 by default)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_SERVERS			1024
#define SERVER_TIMEOUT		( 15 * 60 )		// seconds without a heartbeat
#define CHALLENGE_TIMEOUT	10
#define MAX_PACKET			1400
#define INFO_LEN			1024

typedef struct {
	struct sockaddr_in	addr;
	int					used;
	int					challenge;		// sent with getinfo, 0 once answered
	time_t				challengeTime;
	time_t				lastSeen;		// last valid infoResponse
	int					protocol;
	char				gamename[64];
	char				info[INFO_LEN];
} server_t;

static server_t	servers[MAX_SERVERS];
static int		verbose;

static const unsigned char	oob[4] = { 0xff, 0xff, 0xff, 0xff };

/*
==============
InfoValue

The value for a key in a "\key\value\key\value" string, or ""
==============
*/
static void InfoValue( const char *info, const char *key, char *out, size_t size ) {
	char	k[128], v[256];
	const char	*p = info;
	size_t	n;

	out[0] = 0;
	while ( *p == '\\' ) {
		p++;
		for ( n = 0 ; *p && *p != '\\' && n < sizeof( k ) - 1 ; ) {
			k[n++] = *p++;
		}
		k[n] = 0;
		if ( *p != '\\' ) {
			return;
		}
		p++;
		for ( n = 0 ; *p && *p != '\\' && n < sizeof( v ) - 1 ; ) {
			v[n++] = *p++;
		}
		v[n] = 0;
		if ( !strcmp( k, key ) ) {
			snprintf( out, size, "%s", v );
			return;
		}
	}
}

static int SameAddr( const struct sockaddr_in *a, const struct sockaddr_in *b ) {
	return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

static const char *AddrString( const struct sockaddr_in *a ) {
	static char	buf[64];

	snprintf( buf, sizeof( buf ), "%s:%d", inet_ntoa( a->sin_addr ), ntohs( a->sin_port ) );
	return buf;
}

static void Send( int sock, const struct sockaddr_in *to, const void *data, size_t len ) {
	sendto( sock, data, len, 0, (const struct sockaddr *)to, sizeof( *to ) );
}

/*
==============
Heartbeat

Ask the server for its info with a fresh challenge
==============
*/
static void Heartbeat( int sock, const struct sockaddr_in *from ) {
	server_t	*s = NULL, *freeSlot = NULL;
	char		msg[64];
	int			i;
	time_t		now = time( NULL );

	for ( i = 0 ; i < MAX_SERVERS ; i++ ) {
		if ( servers[i].used && SameAddr( &servers[i].addr, from ) ) {
			s = &servers[i];
			break;
		}
		if ( !servers[i].used && !freeSlot ) {
			freeSlot = &servers[i];
		}
	}
	if ( !s ) {
		if ( !freeSlot ) {
			return;		// full
		}
		s = freeSlot;
		memset( s, 0, sizeof( *s ) );
		s->addr = *from;
		s->used = 1;
	}
	s->challenge = ( rand() & 0x7fffffff ) | 1;
	s->challengeTime = now;
	memcpy( msg, oob, 4 );
	snprintf( msg + 4, sizeof( msg ) - 4, "getinfo %d", s->challenge );
	Send( sock, from, msg, strlen( msg + 4 ) + 4 );
	if ( verbose ) {
		printf( "heartbeat from %s\n", AddrString( from ) );
	}
}

/*
==============
InfoResponse
==============
*/
static void InfoResponse( const struct sockaddr_in *from, const char *info ) {
	char	value[256];
	int		i;

	for ( i = 0 ; i < MAX_SERVERS ; i++ ) {
		server_t	*s = &servers[i];

		if ( !s->used || !SameAddr( &s->addr, from ) ) {
			continue;
		}
		InfoValue( info, "challenge", value, sizeof( value ) );
		if ( !s->challenge || atoi( value ) != s->challenge
			|| time( NULL ) - s->challengeTime > CHALLENGE_TIMEOUT ) {
			return;		// not the answer to our question
		}
		s->challenge = 0;
		s->lastSeen = time( NULL );
		InfoValue( info, "protocol", value, sizeof( value ) );
		s->protocol = atoi( value );
		InfoValue( info, "gamename", s->gamename, sizeof( s->gamename ) );
		snprintf( s->info, sizeof( s->info ), "%s", info );
		if ( verbose ) {
			printf( "registered %s protocol %d\n", AddrString( from ), s->protocol );
		}
		return;
	}
}

/*
==============
GetServers

The list for one protocol, in as many packets as it needs
==============
*/
static void GetServers( int sock, const struct sockaddr_in *from, const char *args, int extended ) {
	unsigned char	packet[MAX_PACKET];
	const char		*header = extended ? "getserversExtResponse" : "getserversResponse";
	char			game[64] = "";
	int				protocol, i;
	size_t			len, start;
	time_t			now = time( NULL );

	if ( extended ) {
		if ( sscanf( args, "%63s %d", game, &protocol ) < 2 ) {
			return;
		}
	} else if ( sscanf( args, "%d", &protocol ) < 1 ) {
		return;
	}

	memcpy( packet, oob, 4 );
	start = 4 + strlen( header );
	memcpy( packet + 4, header, strlen( header ) );
	len = start;
	for ( i = 0 ; i < MAX_SERVERS ; i++ ) {
		server_t	*s = &servers[i];

		if ( !s->used || !s->lastSeen || now - s->lastSeen > SERVER_TIMEOUT || s->protocol != protocol ) {
			continue;
		}
		if ( extended && game[0] && strcmp( game, s->gamename ) ) {
			continue;
		}
		if ( len + 7 + 7 > sizeof( packet ) ) {
			Send( sock, from, packet, len );
			len = start;
		}
		packet[len++] = '\\';
		memcpy( packet + len, &s->addr.sin_addr.s_addr, 4 );
		len += 4;
		memcpy( packet + len, &s->addr.sin_port, 2 );
		len += 2;
	}
	memcpy( packet + len, "\\EOT\0\0\0", 7 );
	len += 7;
	Send( sock, from, packet, len );
}

/*
==============
Status

A readable list, for people and scripts
==============
*/
static void Status( int sock, const struct sockaddr_in *from ) {
	char	packet[MAX_PACKET], mode[32], map[64], clients[16], maxc[16], name[64], line[256];
	size_t	len = 4;
	time_t	now = time( NULL );
	int		i;

	memcpy( packet, oob, 4 );
	len += snprintf( packet + len, sizeof( packet ) - len, "statusResponse\n" );
	for ( i = 0 ; i < MAX_SERVERS ; i++ ) {
		server_t	*s = &servers[i];

		if ( !s->used || !s->lastSeen || now - s->lastSeen > SERVER_TIMEOUT ) {
			continue;
		}
		InfoValue( s->info, "mode", mode, sizeof( mode ) );
		InfoValue( s->info, "mapname", map, sizeof( map ) );
		InfoValue( s->info, "clients", clients, sizeof( clients ) );
		InfoValue( s->info, "sv_maxclients", maxc, sizeof( maxc ) );
		InfoValue( s->info, "hostname", name, sizeof( name ) );
		snprintf( line, sizeof( line ), "%s %s %s %s/%s %s\n", AddrString( &s->addr ),
			mode[0] ? mode : "-", map, clients, maxc, name );
		if ( len + strlen( line ) >= sizeof( packet ) ) {
			break;
		}
		memcpy( packet + len, line, strlen( line ) );
		len += strlen( line );
	}
	Send( sock, from, packet, len );
}

int main( int argc, char **argv ) {
	struct sockaddr_in	addr, from;
	socklen_t			fromLen;
	char				buf[2048];
	int					sock, port = 27950, i;
	ssize_t				n;

	for ( i = 1 ; i < argc ; i++ ) {
		if ( !strcmp( argv[i], "-p" ) && i + 1 < argc ) {
			port = atoi( argv[++i] );
		} else if ( !strcmp( argv[i], "-v" ) ) {
			verbose = 1;
		} else {
			fprintf( stderr, "usage: %s [-p port] [-v]\n", argv[0] );
			return 1;
		}
	}

	srand( (unsigned)time( NULL ) ^ (unsigned)getpid() );
	sock = socket( AF_INET, SOCK_DGRAM, 0 );
	if ( sock < 0 ) {
		perror( "socket" );
		return 1;
	}
	memset( &addr, 0, sizeof( addr ) );
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl( INADDR_ANY );
	addr.sin_port = htons( (unsigned short)port );
	if ( bind( sock, (struct sockaddr *)&addr, sizeof( addr ) ) < 0 ) {
		perror( "bind" );
		return 1;
	}
	printf( "tremor-master listening on UDP %d\n", port );
	fflush( stdout );

	for ( ;; ) {
		fromLen = sizeof( from );
		n = recvfrom( sock, buf, sizeof( buf ) - 1, 0, (struct sockaddr *)&from, &fromLen );
		if ( n < 5 || memcmp( buf, oob, 4 ) ) {
			continue;
		}
		buf[n] = 0;
		if ( !strncmp( buf + 4, "heartbeat ", 10 ) ) {
			Heartbeat( sock, &from );
		} else if ( !strncmp( buf + 4, "infoResponse\n", 13 ) ) {
			InfoResponse( &from, buf + 4 + 13 );
		} else if ( !strncmp( buf + 4, "getserversExt ", 14 ) ) {
			GetServers( sock, &from, buf + 4 + 14, 1 );
		} else if ( !strncmp( buf + 4, "getservers ", 11 ) ) {
			GetServers( sock, &from, buf + 4 + 11, 0 );
		} else if ( !strncmp( buf + 4, "status", 6 ) ) {
			Status( sock, &from );
		}
		fflush( stdout );
	}
}
