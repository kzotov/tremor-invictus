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
// mm_crypto.c -- SHA-512 (FIPS 180-4) and Ed25519 signing (RFC 8032)
//
// Only what a client or server needs to sign its matchmaker requests:
// key derivation and signing. Verification happens in the matchmaker.
// Field elements are five 51-bit limbs; products use 128-bit integers.

#include <string.h>
#include <stdint.h>
#include "mm_crypto.h"

/*
===============================================================================

SHA-512

===============================================================================
*/

static const uint64_t sha512_k[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static const uint64_t sha512_h0[8] = {
	0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

#define ROTR64( x, n ) ( ( (x) >> (n) ) | ( (x) << ( 64 - (n) ) ) )

static void SHA512_Block( uint64_t h[8], const unsigned char *p ) {
	uint64_t w[80], a, b, c, d, e, f, g, hh, t1, t2;
	int i, j;

	for ( i = 0; i < 16; i++ ) {
		w[i] = 0;
		for ( j = 0; j < 8; j++ ) {
			w[i] = ( w[i] << 8 ) | p[i * 8 + j];
		}
	}
	for ( i = 16; i < 80; i++ ) {
		uint64_t s0 = ROTR64( w[i - 15], 1 ) ^ ROTR64( w[i - 15], 8 ) ^ ( w[i - 15] >> 7 );
		uint64_t s1 = ROTR64( w[i - 2], 19 ) ^ ROTR64( w[i - 2], 61 ) ^ ( w[i - 2] >> 6 );
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	a = h[0]; b = h[1]; c = h[2]; d = h[3];
	e = h[4]; f = h[5]; g = h[6]; hh = h[7];
	for ( i = 0; i < 80; i++ ) {
		t1 = hh + ( ROTR64( e, 14 ) ^ ROTR64( e, 18 ) ^ ROTR64( e, 41 ) ) + ( ( e & f ) ^ ( ~e & g ) ) + sha512_k[i] + w[i];
		t2 = ( ROTR64( a, 28 ) ^ ROTR64( a, 34 ) ^ ROTR64( a, 39 ) ) + ( ( a & b ) ^ ( a & c ) ^ ( b & c ) );
		hh = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d;
	h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

typedef struct {
	uint64_t		h[8];
	unsigned char	buf[128];
	size_t			used;
	uint64_t		total;
} sha512_t;

static void SHA512_Init( sha512_t *s ) {
	memcpy( s->h, sha512_h0, sizeof( s->h ) );
	s->used = 0;
	s->total = 0;
}

static void SHA512_Update( sha512_t *s, const unsigned char *data, size_t len ) {
	s->total += len;
	while ( len > 0 ) {
		size_t n = 128 - s->used;
		if ( n > len ) {
			n = len;
		}
		memcpy( s->buf + s->used, data, n );
		s->used += n;
		data += n;
		len -= n;
		if ( s->used == 128 ) {
			SHA512_Block( s->h, s->buf );
			s->used = 0;
		}
	}
}

static void SHA512_Final( sha512_t *s, unsigned char out[64] ) {
	uint64_t bits = s->total * 8;
	int i;

	s->buf[s->used++] = 0x80;
	if ( s->used > 112 ) {
		memset( s->buf + s->used, 0, 128 - s->used );
		SHA512_Block( s->h, s->buf );
		s->used = 0;
	}
	// the length as a 128-bit big-endian number; the top half stays zero
	memset( s->buf + s->used, 0, 128 - s->used );
	for ( i = 0; i < 8; i++ ) {
		s->buf[127 - i] = (unsigned char)( bits >> ( 8 * i ) );
	}
	SHA512_Block( s->h, s->buf );
	for ( i = 0; i < 64; i++ ) {
		out[i] = (unsigned char)( s->h[i / 8] >> ( 56 - 8 * ( i % 8 ) ) );
	}
}

void MM_SHA512( const unsigned char *data, size_t len, unsigned char out[64] ) {
	sha512_t s;

	SHA512_Init( &s );
	SHA512_Update( &s, data, len );
	SHA512_Final( &s, out );
}

/*
===============================================================================

Field arithmetic modulo p = 2^255 - 19

===============================================================================
*/

typedef uint64_t fe[5];
typedef unsigned __int128 u128;

#define MASK51 ( ( (uint64_t)1 << 51 ) - 1 )

static void fe_copy( fe r, const fe a ) {
	memcpy( r, a, sizeof( fe ) );
}

static void fe_carry( fe r ) {
	int i;
	uint64_t c;

	for ( i = 0; i < 4; i++ ) {
		c = r[i] >> 51;
		r[i] &= MASK51;
		r[i + 1] += c;
	}
	c = r[4] >> 51;
	r[4] &= MASK51;
	r[0] += c * 19;
	c = r[0] >> 51;
	r[0] &= MASK51;
	r[1] += c;
}

static void fe_add( fe r, const fe a, const fe b ) {
	int i;

	for ( i = 0; i < 5; i++ ) {
		r[i] = a[i] + b[i];
	}
	fe_carry( r );
}

// a - b, with 4p added so no limb goes negative
static void fe_sub( fe r, const fe a, const fe b ) {
	r[0] = a[0] + ( ( MASK51 - 18 ) << 2 ) - b[0];
	r[1] = a[1] + ( MASK51 << 2 ) - b[1];
	r[2] = a[2] + ( MASK51 << 2 ) - b[2];
	r[3] = a[3] + ( MASK51 << 2 ) - b[3];
	r[4] = a[4] + ( MASK51 << 2 ) - b[4];
	fe_carry( r );
}

static void fe_mul( fe r, const fe a, const fe b ) {
	u128 t[5];
	uint64_t b19[5], c;
	int i, j;

	for ( i = 0; i < 5; i++ ) {
		b19[i] = b[i] * 19;
		t[i] = 0;
	}
	// limb i * limb j lands at i + j; past limb 4 it wraps times 19
	for ( i = 0; i < 5; i++ ) {
		for ( j = 0; j < 5; j++ ) {
			if ( i + j < 5 ) {
				t[i + j] += (u128)a[i] * b[j];
			} else {
				t[i + j - 5] += (u128)a[i] * b19[j];
			}
		}
	}
	for ( i = 0; i < 4; i++ ) {
		t[i + 1] += t[i] >> 51;
		r[i] = (uint64_t)t[i] & MASK51;
	}
	c = (uint64_t)( t[4] >> 51 );
	r[4] = (uint64_t)t[4] & MASK51;
	r[0] += c * 19;
	fe_carry( r );
}

// a^(p-2) = 1/a, by square and multiply over the exponent's bits
static void fe_invert( fe r, const fe a ) {
	// p - 2 = 2^255 - 21: bits 254..5 set, then 01011
	fe acc;
	int i;

	fe_copy( acc, a );
	for ( i = 253; i >= 0; i-- ) {
		fe_mul( acc, acc, acc );
		if ( i >= 5 || i == 3 || i == 1 || i == 0 ) {
			fe_mul( acc, acc, a );
		}
	}
	fe_copy( r, acc );
}

static void fe_tobytes( unsigned char out[32], const fe a ) {
	fe t;
	uint64_t c;
	int i, bit;

	fe_copy( t, a );
	fe_carry( t );
	fe_carry( t );
	// fully reduce: t < 2p here, so subtract p once if t >= p
	c = ( t[0] + 19 ) >> 51;
	c = ( t[1] + c ) >> 51;
	c = ( t[2] + c ) >> 51;
	c = ( t[3] + c ) >> 51;
	c = ( t[4] + c ) >> 51;
	t[0] += 19 * c;
	for ( i = 0; i < 4; i++ ) {
		t[i + 1] += t[i] >> 51;
		t[i] &= MASK51;
	}
	t[4] &= MASK51;

	memset( out, 0, 32 );
	for ( bit = 0; bit < 255; bit++ ) {
		if ( ( t[bit / 51] >> ( bit % 51 ) ) & 1 ) {
			out[bit / 8] |= 1 << ( bit % 8 );
		}
	}
}

/*
===============================================================================

Edwards curve points, extended coordinates (X:Y:Z:T), x = X/Z, y = Y/Z, xy = T/Z

===============================================================================
*/

typedef struct {
	fe x, y, z, t;
} point_t;

static const fe curve_d2 = { 0x69b9426b2f159ULL, 0x35050762add7aULL, 0x3cf44c0038052ULL, 0x6738cc7407977ULL, 0x2406d9dc56dffULL };

static const point_t base_point = {
	{ 0x62d608f25d51aULL, 0x412a4b4f6592aULL, 0x75b7171a4b31dULL, 0x1ff60527118feULL, 0x216936d3cd6e5ULL },
	{ 0x6666666666658ULL, 0x4ccccccccccccULL, 0x1999999999999ULL, 0x3333333333333ULL, 0x6666666666666ULL },
	{ 1, 0, 0, 0, 0 },
	{ 0x68ab3a5b7dda3ULL, 0x00eea2a5eadbbULL, 0x2af8df483c27eULL, 0x332b375274732ULL, 0x67875f0fd78b7ULL }
};

// the unified addition law for a = -1; it also doubles
static void point_add( point_t *r, const point_t *p, const point_t *q ) {
	fe a, b, c, d, e, f, g, h, t;

	fe_sub( a, p->y, p->x );
	fe_sub( t, q->y, q->x );
	fe_mul( a, a, t );
	fe_add( b, p->y, p->x );
	fe_add( t, q->y, q->x );
	fe_mul( b, b, t );
	fe_mul( c, p->t, q->t );
	fe_mul( c, c, curve_d2 );
	fe_mul( d, p->z, q->z );
	fe_add( d, d, d );
	fe_sub( e, b, a );
	fe_sub( f, d, c );
	fe_add( g, d, c );
	fe_add( h, b, a );
	fe_mul( r->x, e, f );
	fe_mul( r->y, g, h );
	fe_mul( r->t, e, h );
	fe_mul( r->z, f, g );
}

// pick b over a when flag is 1, without branching on it
static void point_select( point_t *r, const point_t *a, const point_t *b, uint64_t flag ) {
	uint64_t mask = (uint64_t)0 - flag;
	int i;

	for ( i = 0; i < 5; i++ ) {
		r->x[i] = a->x[i] ^ ( mask & ( a->x[i] ^ b->x[i] ) );
		r->y[i] = a->y[i] ^ ( mask & ( a->y[i] ^ b->y[i] ) );
		r->z[i] = a->z[i] ^ ( mask & ( a->z[i] ^ b->z[i] ) );
		r->t[i] = a->t[i] ^ ( mask & ( a->t[i] ^ b->t[i] ) );
	}
}

// scalar (32 bytes, little-endian) times the base point
static void point_mul_base( point_t *r, const unsigned char scalar[32] ) {
	point_t acc, sum;
	int bit;

	memset( &acc, 0, sizeof( acc ) );
	acc.y[0] = 1;
	acc.z[0] = 1;
	for ( bit = 255; bit >= 0; bit-- ) {
		point_add( &acc, &acc, &acc );
		point_add( &sum, &acc, &base_point );
		point_select( &acc, &acc, &sum, ( scalar[bit / 8] >> ( bit % 8 ) ) & 1 );
	}
	*r = acc;
}

static void point_encode( unsigned char out[32], const point_t *p ) {
	fe zinv, x, y;
	unsigned char xb[32];

	fe_invert( zinv, p->z );
	fe_mul( x, p->x, zinv );
	fe_mul( y, p->y, zinv );
	fe_tobytes( out, y );
	fe_tobytes( xb, x );
	out[31] |= ( xb[0] & 1 ) << 7;
}

/*
===============================================================================

Scalars modulo the group order L = 2^252 + 27742317777372353535851937790883648493

===============================================================================
*/

static const uint32_t group_order[8] = {
	0x5cf5d3ed, 0x5812631a, 0xa2f79cd6, 0x14def9de, 0x00000000, 0x00000000, 0x00000000, 0x10000000
};

// little-endian 32-bit words, n of them, reduced bit by bit into 32 bytes
static void sc_reduce( unsigned char out[32], const uint32_t *num, int words ) {
	uint32_t rem[9];
	int bit, i;

	memset( rem, 0, sizeof( rem ) );
	for ( bit = words * 32 - 1; bit >= 0; bit-- ) {
		uint32_t in = ( num[bit / 32] >> ( bit % 32 ) ) & 1;
		uint64_t borrow;
		uint32_t diff[9];

		// rem = rem * 2 + bit
		for ( i = 8; i > 0; i-- ) {
			rem[i] = ( rem[i] << 1 ) | ( rem[i - 1] >> 31 );
		}
		rem[0] = ( rem[0] << 1 ) | in;

		// if rem >= L, rem -= L
		borrow = 0;
		for ( i = 0; i < 9; i++ ) {
			uint64_t l = i < 8 ? group_order[i] : 0;
			uint64_t v = (uint64_t)rem[i] - l - borrow;
			diff[i] = (uint32_t)v;
			borrow = ( v >> 63 ) & 1;
		}
		if ( !borrow ) {
			memcpy( rem, diff, sizeof( rem ) );
		}
	}
	for ( i = 0; i < 32; i++ ) {
		out[i] = (unsigned char)( rem[i / 4] >> ( 8 * ( i % 4 ) ) );
	}
}

static void bytes_to_words( uint32_t *w, const unsigned char *b, int bytes ) {
	int i;

	memset( w, 0, ( bytes / 4 ) * sizeof( uint32_t ) );
	for ( i = 0; i < bytes; i++ ) {
		w[i / 4] |= (uint32_t)b[i] << ( 8 * ( i % 4 ) );
	}
}

// (a * b + c) mod L, all 32-byte little-endian
static void sc_muladd( unsigned char out[32], const unsigned char a[32], const unsigned char b[32], const unsigned char c[32] ) {
	uint32_t aw[8], bw[8], cw[8], prod[17];
	uint64_t carry;
	int i, j;

	bytes_to_words( aw, a, 32 );
	bytes_to_words( bw, b, 32 );
	bytes_to_words( cw, c, 32 );
	memset( prod, 0, sizeof( prod ) );
	for ( i = 0; i < 8; i++ ) {
		carry = 0;
		for ( j = 0; j < 8; j++ ) {
			uint64_t v = (uint64_t)aw[i] * bw[j] + prod[i + j] + carry;
			prod[i + j] = (uint32_t)v;
			carry = v >> 32;
		}
		prod[i + 8] = (uint32_t)carry;
	}
	carry = 0;
	for ( i = 0; i < 17; i++ ) {
		uint64_t v = (uint64_t)prod[i] + ( i < 8 ? cw[i] : 0 ) + carry;
		prod[i] = (uint32_t)v;
		carry = v >> 32;
	}
	sc_reduce( out, prod, 17 );
}

/*
===============================================================================

Ed25519

===============================================================================
*/

static void ExpandSeed( const unsigned char seed[32], unsigned char scalar[32], unsigned char prefix[32] ) {
	unsigned char h[64];

	MM_SHA512( seed, 32, h );
	memcpy( scalar, h, 32 );
	scalar[0] &= 248;
	scalar[31] &= 127;
	scalar[31] |= 64;
	memcpy( prefix, h + 32, 32 );
}

void MM_PublicKey( const unsigned char seed[32], unsigned char pub[32] ) {
	unsigned char scalar[32], prefix[32];
	point_t a;

	ExpandSeed( seed, scalar, prefix );
	point_mul_base( &a, scalar );
	point_encode( pub, &a );
}

void MM_Sign( const unsigned char seed[32], const unsigned char *msg, size_t len, unsigned char sig[64] ) {
	unsigned char scalar[32], prefix[32], pub[32], h[64], r[32], k[32];
	uint32_t hw[16];
	point_t p;
	sha512_t s;

	ExpandSeed( seed, scalar, prefix );
	point_mul_base( &p, scalar );
	point_encode( pub, &p );

	// r = H(prefix || msg) mod L, R = rB
	SHA512_Init( &s );
	SHA512_Update( &s, prefix, 32 );
	SHA512_Update( &s, msg, len );
	SHA512_Final( &s, h );
	bytes_to_words( hw, h, 64 );
	sc_reduce( r, hw, 16 );
	point_mul_base( &p, r );
	point_encode( sig, &p );

	// k = H(R || A || msg) mod L, S = r + k * scalar
	SHA512_Init( &s );
	SHA512_Update( &s, sig, 32 );
	SHA512_Update( &s, pub, 32 );
	SHA512_Update( &s, msg, len );
	SHA512_Final( &s, h );
	bytes_to_words( hw, h, 64 );
	sc_reduce( k, hw, 16 );
	sc_muladd( sig + 32, k, scalar, r );
}
