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
// mm_crypto.h -- SHA-512 and Ed25519 signing for matchmaker requests

#ifndef MM_CRYPTO_H
#define MM_CRYPTO_H

#include <stddef.h>

void MM_SHA512( const unsigned char *data, size_t len, unsigned char out[64] );

// the public key for a 32-byte secret seed
void MM_PublicKey( const unsigned char seed[32], unsigned char pub[32] );

// a 64-byte signature of msg
void MM_Sign( const unsigned char seed[32], const unsigned char *msg, size_t len, unsigned char sig[64] );

#endif
