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
// mtl_gl.m -- Metal backend for the classic renderer on macOS.
//
// The classic renderer talks to the graphics card through a small set of
// fixed-function OpenGL calls. This file implements exactly that set on top
// of Metal: it keeps the same state the calls set, and turns each draw into
// a Metal draw with matching pipeline, depth/stencil, culling and textures.

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL.h>
#include <SDL_metal.h>
#include <SDL_opengl.h>

#include "../renderercommon/tr_common.h"

#define MAX_TEXTURES		32768
#define MAX_TEXTURE_UNITS	2
#define MATRIX_STACK		16
#define FRAMES_IN_FLIGHT	3
#define RING_SIZE			( 64 * 1024 * 1024 )
#define MAX_IMMEDIATE		16384

// one vertex as the shaders read it
typedef struct {
	float		xyz[3];
	float		st0[2];
	float		st1[2];
	uint8_t		rgba[4];
} mtlVertex_t;

typedef struct {
	float		mvp[16];
	float		mv[16];
	float		clipPlane[4];
	int32_t		clipEnabled;
	int32_t		pad[3];
} vertexUniforms_t;

typedef struct {
	int32_t		tex0, tex1;
	int32_t		env0, env1;
	int32_t		alphaFunc;
	float		alphaRef;
	int32_t		pad[2];
} fragmentUniforms_t;

typedef struct {
	id<MTLTexture>		texture;
	int					width, height, levels;
	GLenum				minFilter, magFilter, wrapS, wrapT;
	float				anisotropy;
	id<MTLSamplerState>	sampler;
} mtlTexture_t;

#define MAX_STATES			512

typedef struct {
	uint64_t	key;
	id			state;
} stateCache_t;

void *objc_autoreleasePoolPush( void );
void objc_autoreleasePoolPop( void *pool );

typedef struct {
	const GLubyte	*pointer;
	GLint			size;
	GLenum			type;
	GLsizei			stride;
	qboolean		enabled;
} clientArray_t;

static struct {
	id<MTLDevice>				device;
	id<MTLCommandQueue>			queue;
	CAMetalLayer				*layer;
	SDL_MetalView				view;
	id<MTLLibrary>				library;
	id<MTLFunction>				vertexFunction, fragmentFunction;
	MTLVertexDescriptor			*vertexDescriptor;

	id<MTLTexture>				colorTarget, depthTarget;
	int							width, height;

	id<MTLCommandBuffer>		commandBuffer;
	id<MTLRenderCommandEncoder>	encoder;
	dispatch_semaphore_t		frameSemaphore;

	id<MTLBuffer>				ring[FRAMES_IN_FLIGHT];
	int							ringIndex;
	size_t						ringOffset;

	stateCache_t				pipelines[MAX_STATES];
	stateCache_t				depthStates[MAX_STATES];
	int							numPipelines, numDepthStates;
	NSMutableDictionary			*samplers;
	void						*framePool;

	// what the current encoder already has, to skip repeated sets
	__unsafe_unretained id		lastPipeline, lastDepthState;
	id<MTLTexture>				whiteTexture;
	id<MTLSamplerState>			whiteSampler;

	// clears asked for before the next draw
	qboolean					clearColor, clearDepth, clearStencil;
	float						clearValues[4];
	double						clearDepthValue;
	int							clearStencilValue;

	// state set by the GL calls
	mtlTexture_t				textures[MAX_TEXTURES];
	GLuint						nextTexture;
	int							activeUnit, clientUnit;
	GLuint						bound[MAX_TEXTURE_UNITS];
	qboolean					texEnabled[MAX_TEXTURE_UNITS];
	GLenum						texEnv[MAX_TEXTURE_UNITS];

	qboolean					blend, depthTest, cullFace, alphaTest, polygonOffset, scissorTest, stencilTest, clipPlane;
	GLenum						blendSrc, blendDst;
	GLenum						depthFunc;
	qboolean					depthMask;
	GLenum						cullMode;
	GLenum						alphaFunc;
	float						alphaRef;
	qboolean					colorMask[4];
	float						offsetFactor, offsetUnits;
	GLenum						polygonMode;
	int							viewport[4], scissor[4];
	double						depthNear, depthFar;
	GLenum						stencilFunc, stencilFail, stencilZFail, stencilZPass;
	int							stencilRef;
	GLuint						stencilReadMask, stencilWriteMask;
	float						clipPlaneEye[4];

	GLenum						matrixMode;
	float						modelview[MATRIX_STACK][16];
	float						projection[MATRIX_STACK][16];
	int							mvDepth, projDepth;

	clientArray_t				vertexArray, colorArray, texArray[MAX_TEXTURE_UNITS];
	float						currentColor[4];
	float						currentTexCoord[MAX_TEXTURE_UNITS][2];

	// glBegin / glEnd
	GLenum						immediateMode;
	int							immediateCount;
	mtlVertex_t					immediate[MAX_IMMEDIATE];

	int							swapInterval;

	// brightness: the game's gamma table, applied when the frame goes to the window
	id<MTLTexture>				gammaTable;
	id<MTLRenderPipelineState>	gammaPipeline;
	qboolean					gammaActive;
} mtl;

static const char *mtlShaderSource =
	"#include <metal_stdlib>\n"
	"using namespace metal;\n"
	"struct VIn { float3 xyz [[attribute(0)]]; float2 st0 [[attribute(1)]]; float2 st1 [[attribute(2)]]; float4 rgba [[attribute(3)]]; };\n"
	"struct VOut { float4 pos [[position]]; float2 st0 [[user(st0)]]; float2 st1 [[user(st1)]]; float4 rgba [[user(rgba)]]; float clip [[clip_distance]] [1]; };\n"
	"struct FIn { float4 pos [[position]]; float2 st0 [[user(st0)]]; float2 st1 [[user(st1)]]; float4 rgba [[user(rgba)]]; };\n"
	"struct VU { float4x4 mvp; float4x4 mv; float4 clipPlane; int clipEnabled; int3 pad; };\n"
	"struct FU { int tex0; int tex1; int env0; int env1; int alphaFunc; float alphaRef; int2 pad; };\n"
	"vertex VOut vmain( VIn in [[stage_in]], constant VU &u [[buffer(1)]] ) {\n"
	"  VOut o; float4 p = float4( in.xyz, 1.0 );\n"
	"  o.pos = u.mvp * p;\n"
	"  o.pos.z = ( o.pos.z + o.pos.w ) * 0.5;\n"	// GL depth range -1..1 to Metal 0..1
	"  o.st0 = in.st0; o.st1 = in.st1; o.rgba = in.rgba;\n"
	"  o.clip[0] = u.clipEnabled ? dot( u.clipPlane, u.mv * p ) : 1.0;\n"
	"  return o;\n"
	"}\n"
	// texture environment: 1 modulate, 2 replace, 3 add, 4 decal
	"static float4 env( int mode, float4 c, float4 t ) {\n"
	"  if ( mode == 2 ) return t;\n"
	"  if ( mode == 3 ) return float4( c.rgb + t.rgb, c.a * t.a );\n"
	"  if ( mode == 4 ) return float4( mix( c.rgb, t.rgb, t.a ), c.a );\n"
	"  return c * t;\n"
	"}\n"
	// the finished frame through the gamma table, onto the window
	"struct GOut { float4 pos [[position]]; };\n"
	"vertex GOut gvmain( uint id [[vertex_id]] ) {\n"
	"  GOut o; float2 p = float2( ( id << 1 ) & 2, id & 2 );\n"
	"  o.pos = float4( p * 2.0 - 1.0, 0.0, 1.0 ); return o;\n"
	"}\n"
	"fragment float4 gfmain( GOut in [[stage_in]], texture2d<float> frame [[texture(0)]], texture2d<float> table [[texture(1)]] ) {\n"
	"  float4 c = frame.read( uint2( in.pos.xy ) );\n"
	"  uint3 i = uint3( c.rgb * 255.0 + 0.5 );\n"
	"  return float4( table.read( uint2( i.r, 0 ) ).r, table.read( uint2( i.g, 0 ) ).g, table.read( uint2( i.b, 0 ) ).b, 1.0 );\n"
	"}\n"
	"fragment float4 fmain( FIn in [[stage_in]], constant FU &u [[buffer(0)]],\n"
	"    texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]],\n"
	"    sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]] ) {\n"
	"  float4 c = in.rgba;\n"
	"  if ( u.tex0 ) c = env( u.env0, c, t0.sample( s0, in.st0 ) );\n"
	"  if ( u.tex1 ) c = env( u.env1, c, t1.sample( s1, in.st1 ) );\n"
	// alpha test: 1 greater than, 2 less than, 3 greater or equal
	"  if ( u.alphaFunc == 1 && !( c.a > u.alphaRef ) ) discard_fragment();\n"
	"  if ( u.alphaFunc == 2 && !( c.a < u.alphaRef ) ) discard_fragment();\n"
	"  if ( u.alphaFunc == 3 && !( c.a >= u.alphaRef ) ) discard_fragment();\n"
	"  return c;\n"
	"}\n";

/*
==============================================================================

MATRICES

==============================================================================
*/

static void Mat_Identity( float *m ) {
	memset( m, 0, sizeof( float ) * 16 );
	m[0] = m[5] = m[10] = m[15] = 1;
}

// out = a * b, column-major as OpenGL keeps them
static void Mat_Multiply( const float *a, const float *b, float *out ) {
	float	r[16];
	int		i, j;

	for ( i = 0 ; i < 4 ; i++ ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			r[j * 4 + i] = a[0 * 4 + i] * b[j * 4 + 0] + a[1 * 4 + i] * b[j * 4 + 1]
				+ a[2 * 4 + i] * b[j * 4 + 2] + a[3 * 4 + i] * b[j * 4 + 3];
		}
	}
	memcpy( out, r, sizeof( r ) );
}

static qboolean Mat_Invert( const float *m, float *out ) {
	float	inv[16], det;
	int		i;

	inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
	inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
	inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
	inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
	inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
	inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
	inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
	inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
	inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
	inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
	inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
	inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
	inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
	inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
	inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
	inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if ( det == 0 ) {
		return qfalse;
	}
	det = 1.0f / det;
	for ( i = 0 ; i < 16 ; i++ ) {
		out[i] = inv[i] * det;
	}
	return qtrue;
}

static float *Mat_Current( void ) {
	if ( mtl.matrixMode == GL_PROJECTION ) {
		return mtl.projection[mtl.projDepth];
	}
	return mtl.modelview[mtl.mvDepth];
}

/*
==============================================================================

PIPELINE AND STATE OBJECTS

==============================================================================
*/

static MTLBlendFactor MTL_BlendFactor( GLenum f ) {
	switch ( f ) {
	case GL_ZERO:					return MTLBlendFactorZero;
	case GL_ONE:					return MTLBlendFactorOne;
	case GL_SRC_COLOR:				return MTLBlendFactorSourceColor;
	case GL_ONE_MINUS_SRC_COLOR:	return MTLBlendFactorOneMinusSourceColor;
	case GL_DST_COLOR:				return MTLBlendFactorDestinationColor;
	case GL_ONE_MINUS_DST_COLOR:	return MTLBlendFactorOneMinusDestinationColor;
	case GL_SRC_ALPHA:				return MTLBlendFactorSourceAlpha;
	case GL_ONE_MINUS_SRC_ALPHA:	return MTLBlendFactorOneMinusSourceAlpha;
	case GL_DST_ALPHA:				return MTLBlendFactorDestinationAlpha;
	case GL_ONE_MINUS_DST_ALPHA:	return MTLBlendFactorOneMinusDestinationAlpha;
	case GL_SRC_ALPHA_SATURATE:		return MTLBlendFactorSourceAlphaSaturated;
	default:						return MTLBlendFactorOne;
	}
}

static MTLCompareFunction MTL_Compare( GLenum f ) {
	switch ( f ) {
	case GL_NEVER:		return MTLCompareFunctionNever;
	case GL_LESS:		return MTLCompareFunctionLess;
	case GL_EQUAL:		return MTLCompareFunctionEqual;
	case GL_LEQUAL:		return MTLCompareFunctionLessEqual;
	case GL_GREATER:	return MTLCompareFunctionGreater;
	case GL_NOTEQUAL:	return MTLCompareFunctionNotEqual;
	case GL_GEQUAL:		return MTLCompareFunctionGreaterEqual;
	default:			return MTLCompareFunctionAlways;
	}
}

static MTLStencilOperation MTL_StencilOp( GLenum op ) {
	switch ( op ) {
	case GL_ZERO:		return MTLStencilOperationZero;
	case GL_REPLACE:	return MTLStencilOperationReplace;
	case GL_INCR:		return MTLStencilOperationIncrementClamp;
	case GL_DECR:		return MTLStencilOperationDecrementClamp;
	case GL_INVERT:		return MTLStencilOperationInvert;
	default:			return MTLStencilOperationKeep;
	}
}

static id MTL_FindState( stateCache_t *cache, int count, uint64_t key ) {
	int	i;

	for ( i = 0 ; i < count ; i++ ) {
		if ( cache[i].key == key ) {
			return cache[i].state;
		}
	}
	return nil;
}

static id<MTLRenderPipelineState> MTL_Pipeline( void ) {
	uint64_t						key;
	id<MTLRenderPipelineState>		state;
	MTLRenderPipelineDescriptor		*desc;
	MTLRenderPipelineColorAttachmentDescriptor	*ca;
	MTLColorWriteMask				mask = 0;
	NSError							*error = nil;

	key = (uint64_t)( mtl.blend ? 1 : 0 ) | (uint64_t)( mtl.blendSrc & 0xffff ) << 1 | (uint64_t)( mtl.blendDst & 0xffff ) << 17
		| (uint64_t)( mtl.colorMask[0] | mtl.colorMask[1] << 1 | mtl.colorMask[2] << 2 | mtl.colorMask[3] << 3 ) << 33;
	if ( !mtl.blend ) {
		key &= ~(uint64_t)0x1fffffffe;
	}
	state = MTL_FindState( mtl.pipelines, mtl.numPipelines, key );
	if ( state ) {
		return state;
	}

	desc = [MTLRenderPipelineDescriptor new];
	desc.vertexFunction = mtl.vertexFunction;
	desc.fragmentFunction = mtl.fragmentFunction;
	desc.vertexDescriptor = mtl.vertexDescriptor;
	desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
	desc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
	ca = desc.colorAttachments[0];
	ca.pixelFormat = MTLPixelFormatBGRA8Unorm;
	if ( mtl.colorMask[0] ) mask |= MTLColorWriteMaskRed;
	if ( mtl.colorMask[1] ) mask |= MTLColorWriteMaskGreen;
	if ( mtl.colorMask[2] ) mask |= MTLColorWriteMaskBlue;
	if ( mtl.colorMask[3] ) mask |= MTLColorWriteMaskAlpha;
	ca.writeMask = mask;
	if ( mtl.blend ) {
		ca.blendingEnabled = YES;
		ca.sourceRGBBlendFactor = ca.sourceAlphaBlendFactor = MTL_BlendFactor( mtl.blendSrc );
		ca.destinationRGBBlendFactor = ca.destinationAlphaBlendFactor = MTL_BlendFactor( mtl.blendDst );
	}

	state = [mtl.device newRenderPipelineStateWithDescriptor:desc error:&error];
	if ( !state ) {
		ri.Error( ERR_FATAL, "Metal pipeline: %s", error.localizedDescription.UTF8String );
	}
	if ( mtl.numPipelines < MAX_STATES ) {
		mtl.pipelines[mtl.numPipelines].key = key;
		mtl.pipelines[mtl.numPipelines++].state = state;
	}
	return state;
}

static id<MTLDepthStencilState> MTL_DepthState( void ) {
	uint64_t					key;
	id<MTLDepthStencilState>	state;
	MTLDepthStencilDescriptor	*desc;
	MTLStencilDescriptor		*st;

	// GL enums here all sit in 0x0000..0x1fff
	key = (uint64_t)( mtl.depthTest ? 1 : 0 ) | (uint64_t)( mtl.depthMask ? 2 : 0 ) | (uint64_t)( mtl.depthFunc & 0xf ) << 2;
	if ( mtl.stencilTest ) {
		key |= (uint64_t)1 << 6 | (uint64_t)( mtl.stencilFunc & 0xf ) << 7
			| (uint64_t)( mtl.stencilFail & 0x1fff ) << 11 | (uint64_t)( mtl.stencilZFail & 0x1fff ) << 24
			| (uint64_t)( mtl.stencilZPass & 0x1fff ) << 37 | (uint64_t)( mtl.stencilReadMask & 0xff ) << 50
			| (uint64_t)( mtl.stencilWriteMask & 0x3f ) << 58;
	}
	state = MTL_FindState( mtl.depthStates, mtl.numDepthStates, key );
	if ( state ) {
		return state;
	}

	desc = [MTLDepthStencilDescriptor new];
	// without a depth test OpenGL does not write depth either
	desc.depthCompareFunction = mtl.depthTest ? MTL_Compare( mtl.depthFunc ) : MTLCompareFunctionAlways;
	desc.depthWriteEnabled = mtl.depthTest && mtl.depthMask;
	if ( mtl.stencilTest ) {
		st = [MTLStencilDescriptor new];
		st.stencilCompareFunction = MTL_Compare( mtl.stencilFunc );
		st.stencilFailureOperation = MTL_StencilOp( mtl.stencilFail );
		st.depthFailureOperation = MTL_StencilOp( mtl.stencilZFail );
		st.depthStencilPassOperation = MTL_StencilOp( mtl.stencilZPass );
		st.readMask = mtl.stencilReadMask;
		st.writeMask = mtl.stencilWriteMask;
		desc.frontFaceStencil = st;
		desc.backFaceStencil = st;
	}
	state = [mtl.device newDepthStencilStateWithDescriptor:desc];
	if ( mtl.numDepthStates < MAX_STATES ) {
		mtl.depthStates[mtl.numDepthStates].key = key;
		mtl.depthStates[mtl.numDepthStates++].state = state;
	}
	return state;
}

static MTLSamplerAddressMode MTL_Wrap( GLenum wrap ) {
	return ( wrap == GL_CLAMP || wrap == GL_CLAMP_TO_EDGE ) ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
}

static id<MTLSamplerState> MTL_Sampler( mtlTexture_t *t ) {
	NSString				*key;
	id<MTLSamplerState>		state;
	MTLSamplerDescriptor	*desc;
	GLenum					min = t->minFilter;

	if ( t->sampler ) {
		return t->sampler;
	}
	key = [NSString stringWithFormat:@"%x %x %x %x %d %d", min, t->magFilter, t->wrapS, t->wrapT, (int)t->anisotropy, t->levels];
	state = mtl.samplers[key];
	if ( !state ) {
		desc = [MTLSamplerDescriptor new];
		desc.magFilter = t->magFilter == GL_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
		desc.minFilter = ( min == GL_NEAREST || min == GL_NEAREST_MIPMAP_NEAREST || min == GL_NEAREST_MIPMAP_LINEAR )
			? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
		if ( t->levels > 1 && ( min == GL_LINEAR_MIPMAP_LINEAR || min == GL_NEAREST_MIPMAP_LINEAR ) ) {
			desc.mipFilter = MTLSamplerMipFilterLinear;
		} else if ( t->levels > 1 && ( min == GL_LINEAR_MIPMAP_NEAREST || min == GL_NEAREST_MIPMAP_NEAREST ) ) {
			desc.mipFilter = MTLSamplerMipFilterNearest;
		} else {
			desc.mipFilter = MTLSamplerMipFilterNotMipmapped;
		}
		desc.sAddressMode = MTL_Wrap( t->wrapS );
		desc.tAddressMode = MTL_Wrap( t->wrapT );
		desc.maxAnisotropy = t->anisotropy > 1 ? (NSUInteger)t->anisotropy : 1;
		state = [mtl.device newSamplerStateWithDescriptor:desc];
		mtl.samplers[key] = state;
	}
	t->sampler = state;
	return state;
}

/*
==============================================================================

FRAMES AND RENDER PASSES

==============================================================================
*/

static void MTL_CreateTargets( int width, int height ) {
	MTLTextureDescriptor	*desc;

	mtl.width = width;
	mtl.height = height;

	desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
		width:width height:height mipmapped:NO];
	desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	desc.storageMode = MTLStorageModePrivate;
	mtl.colorTarget = [mtl.device newTextureWithDescriptor:desc];

	desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8
		width:width height:height mipmapped:NO];
	desc.usage = MTLTextureUsageRenderTarget;
	desc.storageMode = MTLStorageModePrivate;
	mtl.depthTarget = [mtl.device newTextureWithDescriptor:desc];
}

static void MTL_BeginFrame( void ) {
	if ( mtl.commandBuffer ) {
		return;
	}
	dispatch_semaphore_wait( mtl.frameSemaphore, DISPATCH_TIME_FOREVER );
	if ( !mtl.framePool ) {
		mtl.framePool = objc_autoreleasePoolPush();
	}
	mtl.commandBuffer = [mtl.queue commandBuffer];
	mtl.ringIndex = ( mtl.ringIndex + 1 ) % FRAMES_IN_FLIGHT;
	mtl.ringOffset = 0;
}

static void MTL_EndPass( void ) {
	if ( mtl.encoder ) {
		[mtl.encoder endEncoding];
		mtl.encoder = nil;
	}
}

static void MTL_BeginPass( void ) {
	MTLRenderPassDescriptor	*pass;

	if ( mtl.encoder ) {
		return;
	}
	MTL_BeginFrame();

	pass = [MTLRenderPassDescriptor renderPassDescriptor];
	pass.colorAttachments[0].texture = mtl.colorTarget;
	pass.colorAttachments[0].loadAction = mtl.clearColor ? MTLLoadActionClear : MTLLoadActionLoad;
	pass.colorAttachments[0].clearColor = MTLClearColorMake( mtl.clearValues[0], mtl.clearValues[1],
		mtl.clearValues[2], mtl.clearValues[3] );
	pass.colorAttachments[0].storeAction = MTLStoreActionStore;
	pass.depthAttachment.texture = mtl.depthTarget;
	pass.depthAttachment.loadAction = mtl.clearDepth ? MTLLoadActionClear : MTLLoadActionLoad;
	pass.depthAttachment.clearDepth = mtl.clearDepthValue;
	pass.depthAttachment.storeAction = MTLStoreActionStore;
	pass.stencilAttachment.texture = mtl.depthTarget;
	pass.stencilAttachment.loadAction = mtl.clearStencil ? MTLLoadActionClear : MTLLoadActionLoad;
	pass.stencilAttachment.clearStencil = mtl.clearStencilValue;
	pass.stencilAttachment.storeAction = MTLStoreActionStore;
	mtl.clearColor = mtl.clearDepth = mtl.clearStencil = qfalse;

	mtl.encoder = [mtl.commandBuffer renderCommandEncoderWithDescriptor:pass];
	mtl.lastPipeline = mtl.lastDepthState = nil;
	[mtl.encoder setFrontFacingWinding:MTLWindingCounterClockwise];
}

// space in this frame's vertex and index ring
static void *MTL_RingAlloc( size_t size, size_t *offset ) {
	size_t	aligned = ( mtl.ringOffset + 255 ) & ~(size_t)255;

	if ( aligned + size > RING_SIZE ) {
		// out of room: finish this frame's work and start over
		MTL_EndPass();
		[mtl.commandBuffer commit];
		[mtl.commandBuffer waitUntilCompleted];
		dispatch_semaphore_signal( mtl.frameSemaphore );
		mtl.commandBuffer = nil;
		MTL_BeginPass();
		aligned = 0;
	}
	*offset = aligned;
	mtl.ringOffset = aligned + size;
	return (uint8_t *)mtl.ring[mtl.ringIndex].contents + aligned;
}

/*
==============================================================================

DRAWING

==============================================================================
*/

static void MTL_FetchVertex( int index, mtlVertex_t *v ) {
	const float		*f;
	const GLubyte	*b;
	int				u;

	f = (const float *)( mtl.vertexArray.pointer + index * mtl.vertexArray.stride );
	v->xyz[0] = f[0];
	v->xyz[1] = f[1];
	v->xyz[2] = mtl.vertexArray.size > 2 ? f[2] : 0;

	for ( u = 0 ; u < MAX_TEXTURE_UNITS ; u++ ) {
		float	*st = u ? v->st1 : v->st0;

		if ( mtl.texArray[u].enabled && mtl.texArray[u].pointer ) {
			f = (const float *)( mtl.texArray[u].pointer + index * mtl.texArray[u].stride );
			st[0] = f[0];
			st[1] = f[1];
		} else {
			st[0] = mtl.currentTexCoord[u][0];
			st[1] = mtl.currentTexCoord[u][1];
		}
	}

	if ( mtl.colorArray.enabled && mtl.colorArray.pointer ) {
		if ( mtl.colorArray.type == GL_UNSIGNED_BYTE ) {
			b = mtl.colorArray.pointer + index * mtl.colorArray.stride;
			v->rgba[0] = b[0]; v->rgba[1] = b[1]; v->rgba[2] = b[2];
			v->rgba[3] = mtl.colorArray.size > 3 ? b[3] : 255;
		} else {
			f = (const float *)( mtl.colorArray.pointer + index * mtl.colorArray.stride );
			v->rgba[0] = f[0] * 255; v->rgba[1] = f[1] * 255; v->rgba[2] = f[2] * 255;
			v->rgba[3] = mtl.colorArray.size > 3 ? f[3] * 255 : 255;
		}
	} else {
		for ( u = 0 ; u < 4 ; u++ ) {
			v->rgba[u] = mtl.currentColor[u] * 255;
		}
	}
}

static int MTL_EnvMode( GLenum mode ) {
	switch ( mode ) {
	case GL_REPLACE:	return 2;
	case GL_ADD:		return 3;
	case GL_DECAL:		return 4;
	default:			return 1;
	}
}

// set everything a draw needs from the current state
static void MTL_PrepareDraw( void ) {
	vertexUniforms_t	vu;
	fragmentUniforms_t	fu;
	MTLViewport			vp;
	MTLScissorRect		sr;
	int					u;

	MTL_BeginPass();

	{
		id<MTLRenderPipelineState>	pso = MTL_Pipeline();
		id<MTLDepthStencilState>	dss = MTL_DepthState();

		if ( pso != mtl.lastPipeline ) {
			[mtl.encoder setRenderPipelineState:pso];
			mtl.lastPipeline = pso;
		}
		if ( dss != mtl.lastDepthState ) {
			[mtl.encoder setDepthStencilState:dss];
			mtl.lastDepthState = dss;
		}
	}
	if ( mtl.stencilTest ) {
		[mtl.encoder setStencilReferenceValue:mtl.stencilRef];
	}

	if ( !mtl.cullFace ) {
		[mtl.encoder setCullMode:MTLCullModeNone];
	} else {
		[mtl.encoder setCullMode:mtl.cullMode == GL_FRONT ? MTLCullModeFront : MTLCullModeBack];
	}
	[mtl.encoder setTriangleFillMode:mtl.polygonMode == GL_LINE ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
	if ( mtl.polygonOffset ) {
		[mtl.encoder setDepthBias:mtl.offsetUnits * 2.0f / 16777216.0f slopeScale:mtl.offsetFactor clamp:0];
	} else {
		[mtl.encoder setDepthBias:0 slopeScale:0 clamp:0];
	}

	// OpenGL counts rows from the bottom, Metal from the top
	vp.originX = mtl.viewport[0];
	vp.originY = mtl.height - ( mtl.viewport[1] + mtl.viewport[3] );
	vp.width = mtl.viewport[2];
	vp.height = mtl.viewport[3];
	vp.znear = mtl.depthNear;
	vp.zfar = mtl.depthFar;
	[mtl.encoder setViewport:vp];

	if ( mtl.scissorTest ) {
		int	x = mtl.scissor[0], y = mtl.height - ( mtl.scissor[1] + mtl.scissor[3] );
		int	w = mtl.scissor[2], h = mtl.scissor[3];

		if ( x < 0 ) { w += x; x = 0; }
		if ( y < 0 ) { h += y; y = 0; }
		if ( x + w > mtl.width ) w = mtl.width - x;
		if ( y + h > mtl.height ) h = mtl.height - y;
		sr.x = x; sr.y = y;
		sr.width = w > 0 ? w : 0;
		sr.height = h > 0 ? h : 0;
	} else {
		sr.x = sr.y = 0;
		sr.width = mtl.width;
		sr.height = mtl.height;
	}
	[mtl.encoder setScissorRect:sr];

	Mat_Multiply( mtl.projection[mtl.projDepth], mtl.modelview[mtl.mvDepth], vu.mvp );
	memcpy( vu.mv, mtl.modelview[mtl.mvDepth], sizeof( vu.mv ) );
	memcpy( vu.clipPlane, mtl.clipPlaneEye, sizeof( vu.clipPlane ) );
	vu.clipEnabled = mtl.clipPlane;
	[mtl.encoder setVertexBytes:&vu length:sizeof( vu ) atIndex:1];

	memset( &fu, 0, sizeof( fu ) );
	for ( u = 0 ; u < MAX_TEXTURE_UNITS ; u++ ) {
		mtlTexture_t	*t = &mtl.textures[mtl.bound[u]];
		qboolean		on = mtl.texEnabled[u] && mtl.bound[u] && t->texture;

		if ( u == 0 ) {
			fu.tex0 = on;
			fu.env0 = MTL_EnvMode( mtl.texEnv[0] );
		} else {
			fu.tex1 = on;
			fu.env1 = MTL_EnvMode( mtl.texEnv[1] );
		}
		[mtl.encoder setFragmentTexture:on ? t->texture : mtl.whiteTexture atIndex:u];
		[mtl.encoder setFragmentSamplerState:on ? MTL_Sampler( t ) : mtl.whiteSampler atIndex:u];
	}
	if ( mtl.alphaTest ) {
		fu.alphaFunc = mtl.alphaFunc == GL_GREATER ? 1 : mtl.alphaFunc == GL_LESS ? 2 : mtl.alphaFunc == GL_GEQUAL ? 3 : 0;
		fu.alphaRef = mtl.alphaRef;
	}
	[mtl.encoder setFragmentBytes:&fu length:sizeof( fu ) atIndex:0];
}

static MTLPrimitiveType MTL_Primitive( GLenum mode, qboolean *lines ) {
	*lines = ( mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP );
	if ( mode == GL_LINES ) return MTLPrimitiveTypeLine;
	if ( mode == GL_LINE_STRIP ) return MTLPrimitiveTypeLineStrip;
	if ( mode == GL_POINTS ) return MTLPrimitiveTypePoint;
	return MTLPrimitiveTypeTriangle;
}

// expand strips, fans and quads into a triangle list
static int MTL_TriangulateIndices( GLenum mode, const uint32_t *in, int count, uint32_t *out ) {
	int	i, n = 0;

	switch ( mode ) {
	case GL_TRIANGLE_STRIP:
		for ( i = 2 ; i < count ; i++ ) {
			if ( i & 1 ) {
				out[n++] = in[i - 1]; out[n++] = in[i - 2]; out[n++] = in[i];
			} else {
				out[n++] = in[i - 2]; out[n++] = in[i - 1]; out[n++] = in[i];
			}
		}
		return n;
	case GL_TRIANGLE_FAN:
	case GL_POLYGON:
		for ( i = 2 ; i < count ; i++ ) {
			out[n++] = in[0]; out[n++] = in[i - 1]; out[n++] = in[i];
		}
		return n;
	case GL_QUADS:
		for ( i = 0 ; i + 3 < count ; i += 4 ) {
			out[n++] = in[i]; out[n++] = in[i + 1]; out[n++] = in[i + 2];
			out[n++] = in[i]; out[n++] = in[i + 2]; out[n++] = in[i + 3];
		}
		return n;
	case GL_LINE_LOOP:
		memcpy( out, in, count * sizeof( uint32_t ) );
		out[count] = in[0];
		return count + 1;
	default:
		memcpy( out, in, count * sizeof( uint32_t ) );
		return count;
	}
}

// draw vertices already gathered into the ring
static void MTL_DrawGathered( GLenum mode, size_t vertexOffset, const uint32_t *indices, int count ) {
	uint32_t			*out;
	size_t				indexOffset;
	int					n, maxOut;
	qboolean			lines;
	MTLPrimitiveType	prim;

	if ( count <= 0 ) {
		return;
	}
	maxOut = count * 3 + 6;
	out = MTL_RingAlloc( maxOut * sizeof( uint32_t ), &indexOffset );
	n = MTL_TriangulateIndices( mode, indices, count, out );
	mtl.ringOffset = indexOffset + n * sizeof( uint32_t );
	if ( n <= 0 ) {
		return;
	}

	prim = MTL_Primitive( mode, &lines );
	if ( mode == GL_LINE_LOOP ) {
		prim = MTLPrimitiveTypeLineStrip;
	}
	[mtl.encoder setVertexBuffer:mtl.ring[mtl.ringIndex] offset:vertexOffset atIndex:0];
	[mtl.encoder drawIndexedPrimitives:prim indexCount:n indexType:MTLIndexTypeUInt32
		indexBuffer:mtl.ring[mtl.ringIndex] indexBufferOffset:indexOffset];
}

static void MTL_DrawFromArrays( GLenum mode, const uint32_t *indices, int count ) {
	uint32_t		minIndex = 0xffffffff, maxIndex = 0;
	uint32_t		*rebased;
	mtlVertex_t		*verts;
	size_t			vertexOffset, scratchOffset;
	int				i, numVerts;

	if ( count <= 0 || !mtl.vertexArray.enabled || !mtl.vertexArray.pointer ) {
		return;
	}
	for ( i = 0 ; i < count ; i++ ) {
		if ( indices[i] < minIndex ) minIndex = indices[i];
		if ( indices[i] > maxIndex ) maxIndex = indices[i];
	}
	numVerts = maxIndex - minIndex + 1;

	MTL_PrepareDraw();

	verts = MTL_RingAlloc( numVerts * sizeof( mtlVertex_t ), &vertexOffset );
	for ( i = 0 ; i < numVerts ; i++ ) {
		MTL_FetchVertex( minIndex + i, &verts[i] );
	}

	// indices relative to the first gathered vertex
	rebased = MTL_RingAlloc( count * sizeof( uint32_t ), &scratchOffset );
	for ( i = 0 ; i < count ; i++ ) {
		rebased[i] = indices[i] - minIndex;
	}
	MTL_DrawGathered( mode, vertexOffset, rebased, count );
}

/*
==============================================================================

THE OPENGL CALLS

==============================================================================
*/

static void APIENTRY mglBindTexture( GLenum target, GLuint texture ) {
	if ( texture < MAX_TEXTURES ) {
		mtl.bound[mtl.activeUnit] = texture;
	}
}

static void APIENTRY mglBlendFunc( GLenum sfactor, GLenum dfactor ) {
	mtl.blendSrc = sfactor;
	mtl.blendDst = dfactor;
}

static void APIENTRY mglClearColor( GLclampf r, GLclampf g, GLclampf b, GLclampf a ) {
	mtl.clearValues[0] = r; mtl.clearValues[1] = g; mtl.clearValues[2] = b; mtl.clearValues[3] = a;
}

static void APIENTRY mglClear( GLbitfield mask ) {
	// a clear starts a new pass that clears on load
	MTL_EndPass();
	if ( mask & GL_COLOR_BUFFER_BIT ) mtl.clearColor = qtrue;
	if ( mask & GL_DEPTH_BUFFER_BIT ) mtl.clearDepth = qtrue;
	if ( mask & GL_STENCIL_BUFFER_BIT ) mtl.clearStencil = qtrue;
	MTL_BeginPass();
}

static void APIENTRY mglClearStencil( GLint s ) { mtl.clearStencilValue = s; }
static void APIENTRY mglClearDepth( GLclampd d ) { mtl.clearDepthValue = d; }

static void APIENTRY mglColorMask( GLboolean r, GLboolean g, GLboolean b, GLboolean a ) {
	mtl.colorMask[0] = r; mtl.colorMask[1] = g; mtl.colorMask[2] = b; mtl.colorMask[3] = a;
}

static void APIENTRY mglCopyTexSubImage2D( GLenum target, GLint level, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h ) {}
static void APIENTRY mglCullFace( GLenum mode ) { mtl.cullMode = mode; }

static void APIENTRY mglDeleteTextures( GLsizei n, const GLuint *textures ) {
	int	i;

	for ( i = 0 ; i < n ; i++ ) {
		if ( textures[i] < MAX_TEXTURES ) {
			mtl.textures[textures[i]].texture = nil;
			mtl.textures[textures[i]].sampler = nil;
		}
	}
}

static void APIENTRY mglDepthFunc( GLenum func ) { mtl.depthFunc = func; }
static void APIENTRY mglDepthMask( GLboolean flag ) { mtl.depthMask = flag; }
static void APIENTRY mglDepthRange( GLclampd n, GLclampd f ) { mtl.depthNear = n; mtl.depthFar = f; }

static void MTL_SetCap( GLenum cap, qboolean on ) {
	switch ( cap ) {
	case GL_TEXTURE_2D:				mtl.texEnabled[mtl.activeUnit] = on; break;
	case GL_BLEND:					mtl.blend = on; break;
	case GL_DEPTH_TEST:				mtl.depthTest = on; break;
	case GL_CULL_FACE:				mtl.cullFace = on; break;
	case GL_ALPHA_TEST:				mtl.alphaTest = on; break;
	case GL_POLYGON_OFFSET_FILL:	mtl.polygonOffset = on; break;
	case GL_SCISSOR_TEST:			mtl.scissorTest = on; break;
	case GL_STENCIL_TEST:			mtl.stencilTest = on; break;
	case GL_CLIP_PLANE0:			mtl.clipPlane = on; break;
	default:						break;
	}
}

static void APIENTRY mglEnable( GLenum cap ) { MTL_SetCap( cap, qtrue ); }
static void APIENTRY mglDisable( GLenum cap ) { MTL_SetCap( cap, qfalse ); }

static void APIENTRY mglDrawElements( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices ) {
	uint32_t	stackIndices[4096];
	uint32_t	*idx = stackIndices;
	int			i;

	if ( type == GL_UNSIGNED_INT ) {
		MTL_DrawFromArrays( mode, (const uint32_t *)indices, count );
		return;
	}
	if ( count > (int)ARRAY_LEN( stackIndices ) ) {
		idx = malloc( count * sizeof( uint32_t ) );
	}
	for ( i = 0 ; i < count ; i++ ) {
		idx[i] = type == GL_UNSIGNED_SHORT ? ((const uint16_t *)indices)[i] : ((const uint8_t *)indices)[i];
	}
	MTL_DrawFromArrays( mode, idx, count );
	if ( idx != stackIndices ) {
		free( idx );
	}
}

static void APIENTRY mglDrawArrays( GLenum mode, GLint first, GLsizei count ) {
	uint32_t	*idx;
	int			i;

	idx = malloc( count * sizeof( uint32_t ) );
	for ( i = 0 ; i < count ; i++ ) {
		idx[i] = first + i;
	}
	MTL_DrawFromArrays( mode, idx, count );
	free( idx );
}

static void MTL_Flush( qboolean wait ) {
	MTL_EndPass();
	if ( mtl.commandBuffer ) {
		dispatch_semaphore_t	sem = mtl.frameSemaphore;

		[mtl.commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
			dispatch_semaphore_signal( sem );
		}];
		[mtl.commandBuffer commit];
		if ( wait ) {
			[mtl.commandBuffer waitUntilCompleted];
		}
		mtl.commandBuffer = nil;
	}
}

static void APIENTRY mglFinish( void ) { MTL_Flush( qtrue ); }
static void APIENTRY mglFlush( void ) {}

static void APIENTRY mglGenTextures( GLsizei n, GLuint *textures ) {
	int	i;

	for ( i = 0 ; i < n ; i++ ) {
		if ( mtl.nextTexture >= MAX_TEXTURES - 1 ) {
			ri.Error( ERR_DROP, "Metal: out of texture names" );
		}
		textures[i] = ++mtl.nextTexture;
	}
}

static void APIENTRY mglGetBooleanv( GLenum pname, GLboolean *params ) {
	if ( pname == GL_COLOR_WRITEMASK ) {
		params[0] = mtl.colorMask[0]; params[1] = mtl.colorMask[1];
		params[2] = mtl.colorMask[2]; params[3] = mtl.colorMask[3];
	} else {
		params[0] = GL_FALSE;
	}
}

static GLenum APIENTRY mglGetError( void ) { return GL_NO_ERROR; }

static void APIENTRY mglGetIntegerv( GLenum pname, GLint *params ) {
	switch ( pname ) {
	case GL_MAX_TEXTURE_SIZE:		*params = 16384; break;
	case GL_MAX_TEXTURE_UNITS_ARB:	*params = MAX_TEXTURE_UNITS; break;
	case GL_NUM_EXTENSIONS:			*params = 0; break;
	case GL_PACK_ALIGNMENT:
	case GL_UNPACK_ALIGNMENT:		*params = 4; break;
	default:						*params = 0; break;
	}
}

static const char *mtlExtensions =
	"GL_ARB_multitexture GL_EXT_compiled_vertex_array GL_EXT_texture_env_add "
	"GL_EXT_texture_edge_clamp GL_SGIS_texture_edge_clamp GL_EXT_texture_filter_anisotropic";

static const GLubyte * APIENTRY mglGetString( GLenum name ) {
	static char	renderer[128];

	switch ( name ) {
	case GL_VENDOR:		return (const GLubyte *)"Apple";
	case GL_VERSION:	return (const GLubyte *)"1.3 Metal";
	case GL_EXTENSIONS:	return (const GLubyte *)mtlExtensions;
	case GL_RENDERER:
		Com_sprintf( renderer, sizeof( renderer ), "Metal: %s", mtl.device ? mtl.device.name.UTF8String : "none" );
		return (const GLubyte *)renderer;
	default:			return (const GLubyte *)"";
	}
}

static void APIENTRY mglLineWidth( GLfloat width ) {}

static void APIENTRY mglPolygonOffset( GLfloat factor, GLfloat units ) {
	mtl.offsetFactor = factor;
	mtl.offsetUnits = units;
}

static void APIENTRY mglReadPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels ) {
	id<MTLBuffer>			staging;
	id<MTLCommandBuffer>	cb;
	id<MTLBlitCommandEncoder>	blit;
	const uint8_t			*src;
	uint8_t					*dst = pixels;
	int						row, col, bpp, rowBytes, srcY;

	if ( format != GL_RGB && format != GL_RGBA ) {
		memset( pixels, 0, width * height );
		return;
	}
	MTL_Flush( qtrue );

	staging = [mtl.device newBufferWithLength:width * height * 4 options:MTLResourceStorageModeShared];
	cb = [mtl.queue commandBuffer];
	blit = [cb blitCommandEncoder];
	// rows from the bottom as OpenGL counts them
	srcY = mtl.height - ( y + height );
	[blit copyFromTexture:mtl.colorTarget sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake( x, srcY, 0 )
		sourceSize:MTLSizeMake( width, height, 1 ) toBuffer:staging destinationOffset:0
		destinationBytesPerRow:width * 4 destinationBytesPerImage:width * height * 4];
	[blit endEncoding];
	[cb commit];
	[cb waitUntilCompleted];

	bpp = format == GL_RGB ? 3 : 4;
	rowBytes = width * bpp;
	rowBytes = ( rowBytes + 3 ) & ~3;		// GL_PACK_ALIGNMENT 4
	src = staging.contents;
	for ( row = 0 ; row < height ; row++ ) {
		const uint8_t	*s = src + ( height - 1 - row ) * width * 4;
		uint8_t			*d = dst + row * rowBytes;

		for ( col = 0 ; col < width ; col++ ) {
			d[0] = s[2]; d[1] = s[1]; d[2] = s[0];		// BGRA to RGB
			if ( bpp == 4 ) d[3] = s[3];
			s += 4;
			d += bpp;
		}
	}
}

static void APIENTRY mglScissor( GLint x, GLint y, GLsizei w, GLsizei h ) {
	mtl.scissor[0] = x; mtl.scissor[1] = y; mtl.scissor[2] = w; mtl.scissor[3] = h;
}

static void APIENTRY mglStencilFunc( GLenum func, GLint ref, GLuint mask ) {
	mtl.stencilFunc = func;
	mtl.stencilRef = ref;
	mtl.stencilReadMask = mask;
}

static void APIENTRY mglStencilMask( GLuint mask ) { mtl.stencilWriteMask = mask; }

static void APIENTRY mglStencilOp( GLenum fail, GLenum zfail, GLenum zpass ) {
	mtl.stencilFail = fail;
	mtl.stencilZFail = zfail;
	mtl.stencilZPass = zpass;
}

// pixels in any of the classic formats, as RGBA
static uint8_t *MTL_ToRGBA( GLenum format, int width, int height, const GLvoid *pixels ) {
	uint8_t			*out;
	const uint8_t	*in = pixels;
	int				i, n = width * height;

	out = malloc( n * 4 );
	if ( !pixels ) {
		memset( out, 0, n * 4 );
		return out;
	}
	for ( i = 0 ; i < n ; i++ ) {
		switch ( format ) {
		case GL_RGB:
			out[i*4+0] = in[i*3+0]; out[i*4+1] = in[i*3+1]; out[i*4+2] = in[i*3+2]; out[i*4+3] = 255;
			break;
		case GL_LUMINANCE:
			out[i*4+0] = out[i*4+1] = out[i*4+2] = in[i]; out[i*4+3] = 255;
			break;
		case GL_LUMINANCE_ALPHA:
			out[i*4+0] = out[i*4+1] = out[i*4+2] = in[i*2]; out[i*4+3] = in[i*2+1];
			break;
		case GL_ALPHA:
			out[i*4+0] = out[i*4+1] = out[i*4+2] = 255; out[i*4+3] = in[i];
			break;
		default:
			memcpy( out + i * 4, in + i * 4, 4 );
			break;
		}
	}
	return out;
}

static void APIENTRY mglTexImage2D( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
	GLint border, GLenum format, GLenum type, const GLvoid *pixels ) {
	mtlTexture_t			*t = &mtl.textures[mtl.bound[mtl.activeUnit]];
	MTLTextureDescriptor	*desc;
	uint8_t					*rgba;
	int						levels;

	if ( !mtl.bound[mtl.activeUnit] || width <= 0 || height <= 0 ) {
		return;
	}
	if ( level == 0 ) {
		// a new image: room for its whole mip chain
		for ( levels = 1 ; ( width >> levels ) || ( height >> levels ) ; levels++ )
			;
		desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
			width:width height:height mipmapped:levels > 1];
		desc.usage = MTLTextureUsageShaderRead;
		desc.storageMode = mtl.device.hasUnifiedMemory ? MTLStorageModeShared : MTLStorageModeManaged;
		t->texture = [mtl.device newTextureWithDescriptor:desc];
		t->width = width;
		t->height = height;
		t->levels = 1;
		t->sampler = nil;
		if ( !t->minFilter ) {
			t->minFilter = GL_NEAREST_MIPMAP_LINEAR;
			t->magFilter = GL_LINEAR;
			t->wrapS = t->wrapT = GL_REPEAT;
		}
	} else if ( !t->texture || level >= (int)t->texture.mipmapLevelCount ) {
		return;
	} else if ( level + 1 > t->levels ) {
		t->levels = level + 1;
		t->sampler = nil;
	}

	rgba = MTL_ToRGBA( format, width, height, pixels );
	[t->texture replaceRegion:MTLRegionMake2D( 0, 0, width, height ) mipmapLevel:level withBytes:rgba bytesPerRow:width * 4];
	free( rgba );
}

static void APIENTRY mglTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
	GLenum format, GLenum type, const GLvoid *pixels ) {
	mtlTexture_t	*t = &mtl.textures[mtl.bound[mtl.activeUnit]];
	uint8_t			*rgba;

	if ( !t->texture || level >= (int)t->texture.mipmapLevelCount ) {
		return;
	}
	rgba = MTL_ToRGBA( format, width, height, pixels );
	[t->texture replaceRegion:MTLRegionMake2D( xoffset, yoffset, width, height ) mipmapLevel:level withBytes:rgba bytesPerRow:width * 4];
	free( rgba );
}

static void APIENTRY mglTexParameterf( GLenum target, GLenum pname, GLfloat param ) {
	mtlTexture_t	*t = &mtl.textures[mtl.bound[mtl.activeUnit]];

	switch ( pname ) {
	case GL_TEXTURE_MIN_FILTER:			t->minFilter = (GLenum)param; break;
	case GL_TEXTURE_MAG_FILTER:			t->magFilter = (GLenum)param; break;
	case GL_TEXTURE_WRAP_S:				t->wrapS = (GLenum)param; break;
	case GL_TEXTURE_WRAP_T:				t->wrapT = (GLenum)param; break;
	case GL_TEXTURE_MAX_ANISOTROPY_EXT:	t->anisotropy = param; break;
	default:							return;
	}
	t->sampler = nil;
}

static void APIENTRY mglTexParameteri( GLenum target, GLenum pname, GLint param ) {
	mglTexParameterf( target, pname, (GLfloat)param );
}

static void APIENTRY mglViewport( GLint x, GLint y, GLsizei w, GLsizei h ) {
	mtl.viewport[0] = x; mtl.viewport[1] = y; mtl.viewport[2] = w; mtl.viewport[3] = h;
}

// fixed function

static void APIENTRY mglAlphaFunc( GLenum func, GLclampf ref ) { mtl.alphaFunc = func; mtl.alphaRef = ref; }

static void APIENTRY mglColor4f( GLfloat r, GLfloat g, GLfloat b, GLfloat a ) {
	mtl.currentColor[0] = r; mtl.currentColor[1] = g; mtl.currentColor[2] = b; mtl.currentColor[3] = a;
}

static void APIENTRY mglColor3f( GLfloat r, GLfloat g, GLfloat b ) { mglColor4f( r, g, b, 1 ); }

static void APIENTRY mglColor4ubv( const GLubyte *v ) {
	mglColor4f( v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f, v[3] / 255.0f );
}

static void MTL_SetArray( clientArray_t *a, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr, int elementSize ) {
	a->size = size;
	a->type = type;
	a->stride = stride ? stride : size * elementSize;
	a->pointer = ptr;
}

static void APIENTRY mglColorPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr ) {
	MTL_SetArray( &mtl.colorArray, size, type, stride, ptr, type == GL_UNSIGNED_BYTE ? 1 : 4 );
}

static void APIENTRY mglTexCoordPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr ) {
	MTL_SetArray( &mtl.texArray[mtl.clientUnit], size, type, stride, ptr, 4 );
}

static void APIENTRY mglVertexPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *ptr ) {
	MTL_SetArray( &mtl.vertexArray, size, type, stride, ptr, 4 );
}

static void MTL_SetClientState( GLenum cap, qboolean on ) {
	switch ( cap ) {
	case GL_VERTEX_ARRAY:			mtl.vertexArray.enabled = on; break;
	case GL_COLOR_ARRAY:			mtl.colorArray.enabled = on; break;
	case GL_TEXTURE_COORD_ARRAY:	mtl.texArray[mtl.clientUnit].enabled = on; break;
	default:						break;
	}
}

static void APIENTRY mglEnableClientState( GLenum cap ) { MTL_SetClientState( cap, qtrue ); }
static void APIENTRY mglDisableClientState( GLenum cap ) { MTL_SetClientState( cap, qfalse ); }

static void APIENTRY mglMatrixMode( GLenum mode ) { mtl.matrixMode = mode; }
static void APIENTRY mglLoadIdentity( void ) { Mat_Identity( Mat_Current() ); }
static void APIENTRY mglLoadMatrixf( const GLfloat *m ) { memcpy( Mat_Current(), m, sizeof( float ) * 16 ); }

static void APIENTRY mglPushMatrix( void ) {
	if ( mtl.matrixMode == GL_PROJECTION ) {
		if ( mtl.projDepth < MATRIX_STACK - 1 ) {
			memcpy( mtl.projection[mtl.projDepth + 1], mtl.projection[mtl.projDepth], sizeof( float ) * 16 );
			mtl.projDepth++;
		}
	} else if ( mtl.mvDepth < MATRIX_STACK - 1 ) {
		memcpy( mtl.modelview[mtl.mvDepth + 1], mtl.modelview[mtl.mvDepth], sizeof( float ) * 16 );
		mtl.mvDepth++;
	}
}

static void APIENTRY mglPopMatrix( void ) {
	if ( mtl.matrixMode == GL_PROJECTION ) {
		if ( mtl.projDepth > 0 ) mtl.projDepth--;
	} else if ( mtl.mvDepth > 0 ) {
		mtl.mvDepth--;
	}
}

static void APIENTRY mglShadeModel( GLenum mode ) {}

static void APIENTRY mglTexEnvf( GLenum target, GLenum pname, GLfloat param ) {
	if ( pname == GL_TEXTURE_ENV_MODE ) {
		mtl.texEnv[mtl.activeUnit] = (GLenum)param;
	}
}

static void APIENTRY mglTranslatef( GLfloat x, GLfloat y, GLfloat z ) {
	float	t[16];

	Mat_Identity( t );
	t[12] = x; t[13] = y; t[14] = z;
	Mat_Multiply( Mat_Current(), t, Mat_Current() );
}

static void APIENTRY mglOrtho( GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f ) {
	float	m[16];

	Mat_Identity( m );
	m[0] = 2 / ( r - l );
	m[5] = 2 / ( t - b );
	m[10] = -2 / ( f - n );
	m[12] = -( r + l ) / ( r - l );
	m[13] = -( t + b ) / ( t - b );
	m[14] = -( f + n ) / ( f - n );
	Mat_Multiply( Mat_Current(), m, Mat_Current() );
}

static void APIENTRY mglFrustum( GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f ) {
	float	m[16];

	memset( m, 0, sizeof( m ) );
	m[0] = 2 * n / ( r - l );
	m[5] = 2 * n / ( t - b );
	m[8] = ( r + l ) / ( r - l );
	m[9] = ( t + b ) / ( t - b );
	m[10] = -( f + n ) / ( f - n );
	m[11] = -1;
	m[14] = -2 * f * n / ( f - n );
	Mat_Multiply( Mat_Current(), m, Mat_Current() );
}

// desktop

static void APIENTRY mglDrawBuffer( GLenum mode ) {}
static void APIENTRY mglPolygonMode( GLenum face, GLenum mode ) { mtl.polygonMode = mode; }

static void APIENTRY mglClipPlane( GLenum plane, const GLdouble *equation ) {
	float	inv[16], eq[4];
	float	*mv = mtl.modelview[mtl.mvDepth];
	int		i;

	if ( plane != GL_CLIP_PLANE0 ) {
		return;
	}
	// kept in eye space: the plane times the inverse modelview at this moment
	for ( i = 0 ; i < 4 ; i++ ) {
		eq[i] = equation[i];
	}
	if ( !Mat_Invert( mv, inv ) ) {
		Mat_Identity( inv );
	}
	for ( i = 0 ; i < 4 ; i++ ) {
		mtl.clipPlaneEye[i] = eq[0] * inv[i * 4 + 0] + eq[1] * inv[i * 4 + 1] + eq[2] * inv[i * 4 + 2] + eq[3] * inv[i * 4 + 3];
	}
}

// immediate mode

static void APIENTRY mglBegin( GLenum mode ) {
	mtl.immediateMode = mode;
	mtl.immediateCount = 0;
}

static void MTL_EmitVertex( float x, float y, float z ) {
	mtlVertex_t	*v;
	int			i;

	if ( mtl.immediateCount >= MAX_IMMEDIATE ) {
		return;
	}
	v = &mtl.immediate[mtl.immediateCount++];
	v->xyz[0] = x; v->xyz[1] = y; v->xyz[2] = z;
	v->st0[0] = mtl.currentTexCoord[0][0]; v->st0[1] = mtl.currentTexCoord[0][1];
	v->st1[0] = mtl.currentTexCoord[1][0]; v->st1[1] = mtl.currentTexCoord[1][1];
	for ( i = 0 ; i < 4 ; i++ ) {
		v->rgba[i] = mtl.currentColor[i] * 255;
	}
}

static void APIENTRY mglVertex3f( GLfloat x, GLfloat y, GLfloat z ) { MTL_EmitVertex( x, y, z ); }
static void APIENTRY mglVertex3fv( const GLfloat *v ) { MTL_EmitVertex( v[0], v[1], v[2] ); }
static void APIENTRY mglVertex2f( GLfloat x, GLfloat y ) { MTL_EmitVertex( x, y, 0 ); }

static void APIENTRY mglTexCoord2f( GLfloat s, GLfloat t ) {
	mtl.currentTexCoord[0][0] = s;
	mtl.currentTexCoord[0][1] = t;
}

static void APIENTRY mglTexCoord2fv( const GLfloat *v ) { mglTexCoord2f( v[0], v[1] ); }

static void APIENTRY mglArrayElement( GLint i ) {
	mtlVertex_t	v;

	if ( mtl.immediateCount >= MAX_IMMEDIATE || !mtl.vertexArray.pointer ) {
		return;
	}
	MTL_FetchVertex( i, &v );
	mtl.immediate[mtl.immediateCount++] = v;
}

static void APIENTRY mglEnd( void ) {
	mtlVertex_t	*verts;
	uint32_t	*idx;
	size_t		vertexOffset, indexOffset;
	int			i, count = mtl.immediateCount;

	mtl.immediateCount = 0;
	if ( count <= 0 ) {
		return;
	}
	MTL_PrepareDraw();
	verts = MTL_RingAlloc( count * sizeof( mtlVertex_t ), &vertexOffset );
	memcpy( verts, mtl.immediate, count * sizeof( mtlVertex_t ) );
	idx = MTL_RingAlloc( count * sizeof( uint32_t ), &indexOffset );
	for ( i = 0 ; i < count ; i++ ) {
		idx[i] = i;
	}
	MTL_DrawGathered( mtl.immediateMode, vertexOffset, idx, count );
}

// extensions

static void APIENTRY mglActiveTextureARB( GLenum texture ) {
	mtl.activeUnit = texture == GL_TEXTURE1_ARB ? 1 : 0;
}

static void APIENTRY mglClientActiveTextureARB( GLenum texture ) {
	mtl.clientUnit = texture == GL_TEXTURE1_ARB ? 1 : 0;
}

static void APIENTRY mglMultiTexCoord2fARB( GLenum target, GLfloat s, GLfloat t ) {
	int	u = target == GL_TEXTURE1_ARB ? 1 : 0;

	mtl.currentTexCoord[u][0] = s;
	mtl.currentTexCoord[u][1] = t;
}

static void APIENTRY mglLockArraysEXT( GLint first, GLsizei count ) {}
static void APIENTRY mglUnlockArraysEXT( void ) {}

/*
==============================================================================

CONTEXT, WINDOW AND LOOKUP

==============================================================================
*/

typedef struct {
	const char	*name;
	void		*function;
} mtlProc_t;

#define P( name ) { "gl" #name, (void *)mgl##name }

static const mtlProc_t mtlProcs[] = {
	P( BindTexture ), P( BlendFunc ), P( ClearColor ), P( Clear ), P( ClearStencil ), P( ColorMask ),
	P( CopyTexSubImage2D ), P( CullFace ), P( DeleteTextures ), P( DepthFunc ), P( DepthMask ), P( Disable ),
	P( DrawArrays ), P( DrawElements ), P( Enable ), P( Finish ), P( Flush ), P( GenTextures ), P( GetBooleanv ),
	P( GetError ), P( GetIntegerv ), P( GetString ), P( LineWidth ), P( PolygonOffset ), P( ReadPixels ),
	P( Scissor ), P( StencilFunc ), P( StencilMask ), P( StencilOp ), P( TexImage2D ), P( TexParameterf ),
	P( TexParameteri ), P( TexSubImage2D ), P( Viewport ),
	P( AlphaFunc ), P( Color4f ), P( ColorPointer ), P( DisableClientState ), P( EnableClientState ),
	P( LoadIdentity ), P( LoadMatrixf ), P( MatrixMode ), P( PopMatrix ), P( PushMatrix ), P( ShadeModel ),
	P( TexCoordPointer ), P( TexEnvf ), P( Translatef ), P( VertexPointer ),
	P( ClearDepth ), P( DepthRange ), P( DrawBuffer ), P( PolygonMode ),
	P( ArrayElement ), P( Begin ), P( ClipPlane ), P( Color3f ), P( Color4ubv ), P( End ), P( Frustum ),
	P( Ortho ), P( TexCoord2f ), P( TexCoord2fv ), P( Vertex2f ), P( Vertex3f ), P( Vertex3fv ),
	P( ActiveTextureARB ), P( ClientActiveTextureARB ), P( MultiTexCoord2fARB ),
	P( LockArraysEXT ), P( UnlockArraysEXT ),
	{ NULL, NULL }
};

#undef P

void *MTL_GetProcAddress( const char *name ) {
	const mtlProc_t	*p;

	for ( p = mtlProcs ; p->name ; p++ ) {
		if ( !strcmp( p->name, name ) ) {
			return p->function;
		}
	}
	return NULL;
}

SDL_bool MTL_ExtensionSupported( const char *extension ) {
	return strstr( mtlExtensions, extension ) ? SDL_TRUE : SDL_FALSE;
}

int MTL_GetAttribute( SDL_GLattr attr, int *value ) {
	switch ( attr ) {
	case SDL_GL_RED_SIZE: case SDL_GL_GREEN_SIZE: case SDL_GL_BLUE_SIZE: case SDL_GL_ALPHA_SIZE:
		*value = 8; break;
	case SDL_GL_DEPTH_SIZE:		*value = 24; break;
	case SDL_GL_STENCIL_SIZE:	*value = 8; break;
	default:					*value = 0; break;
	}
	return 0;
}

static void MTL_ResetState( void ) {
	int	u;

	mtl.depthMask = qtrue;
	mtl.depthFunc = GL_LESS;
	mtl.depthNear = 0;
	mtl.depthFar = 1;
	mtl.cullMode = GL_BACK;
	mtl.blendSrc = GL_ONE;
	mtl.blendDst = GL_ZERO;
	mtl.colorMask[0] = mtl.colorMask[1] = mtl.colorMask[2] = mtl.colorMask[3] = qtrue;
	mtl.polygonMode = GL_FILL;
	mtl.stencilFunc = GL_ALWAYS;
	mtl.stencilFail = mtl.stencilZFail = mtl.stencilZPass = GL_KEEP;
	mtl.stencilReadMask = mtl.stencilWriteMask = 0xff;
	mtl.clearDepthValue = 1;
	mtl.currentColor[0] = mtl.currentColor[1] = mtl.currentColor[2] = mtl.currentColor[3] = 1;
	mtl.matrixMode = GL_MODELVIEW;
	Mat_Identity( mtl.modelview[0] );
	Mat_Identity( mtl.projection[0] );
	for ( u = 0 ; u < MAX_TEXTURE_UNITS ; u++ ) {
		mtl.texEnv[u] = GL_MODULATE;
	}
	mtl.viewport[2] = mtl.scissor[2] = mtl.width;
	mtl.viewport[3] = mtl.scissor[3] = mtl.height;
}

void *MTL_CreateContext( SDL_Window *window ) {
	NSError					*error = nil;
	MTLTextureDescriptor	*desc;
	MTLSamplerDescriptor	*sdesc;
	uint32_t				white = 0xffffffff;
	int						w, h, i;

	mtl.device = MTLCreateSystemDefaultDevice();
	if ( !mtl.device ) {
		return NULL;
	}
	mtl.queue = [mtl.device newCommandQueue];
	mtl.view = SDL_Metal_CreateView( window );
	mtl.layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer( mtl.view );
	mtl.layer.device = mtl.device;
	mtl.layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	mtl.layer.framebufferOnly = NO;

	// draw at the window's size in points, as the OpenGL path does
	SDL_GetWindowSize( window, &w, &h );
	mtl.layer.drawableSize = CGSizeMake( w, h );
	MTL_CreateTargets( w, h );

	mtl.library = [mtl.device newLibraryWithSource:[NSString stringWithUTF8String:mtlShaderSource] options:nil error:&error];
	if ( !mtl.library ) {
		ri.Printf( PRINT_ALL, "Metal shaders failed: %s\n", error.localizedDescription.UTF8String );
		SDL_Metal_DestroyView( mtl.view );
		mtl.view = NULL;
		return NULL;
	}
	mtl.vertexFunction = [mtl.library newFunctionWithName:@"vmain"];
	mtl.fragmentFunction = [mtl.library newFunctionWithName:@"fmain"];

	{
		MTLRenderPipelineDescriptor	*gdesc = [MTLRenderPipelineDescriptor new];

		gdesc.vertexFunction = [mtl.library newFunctionWithName:@"gvmain"];
		gdesc.fragmentFunction = [mtl.library newFunctionWithName:@"gfmain"];
		gdesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		mtl.gammaPipeline = [mtl.device newRenderPipelineStateWithDescriptor:gdesc error:&error];
		desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:256 height:1 mipmapped:NO];
		mtl.gammaTable = [mtl.device newTextureWithDescriptor:desc];
		mtl.gammaActive = qfalse;
	}

	mtl.vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
	mtl.vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
	mtl.vertexDescriptor.attributes[0].offset = offsetof( mtlVertex_t, xyz );
	mtl.vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
	mtl.vertexDescriptor.attributes[1].offset = offsetof( mtlVertex_t, st0 );
	mtl.vertexDescriptor.attributes[2].format = MTLVertexFormatFloat2;
	mtl.vertexDescriptor.attributes[2].offset = offsetof( mtlVertex_t, st1 );
	mtl.vertexDescriptor.attributes[3].format = MTLVertexFormatUChar4Normalized;
	mtl.vertexDescriptor.attributes[3].offset = offsetof( mtlVertex_t, rgba );
	for ( i = 0 ; i < 4 ; i++ ) {
		mtl.vertexDescriptor.attributes[i].bufferIndex = 0;
	}
	mtl.vertexDescriptor.layouts[0].stride = sizeof( mtlVertex_t );

	for ( i = 0 ; i < FRAMES_IN_FLIGHT ; i++ ) {
		mtl.ring[i] = [mtl.device newBufferWithLength:RING_SIZE options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
	}
	mtl.frameSemaphore = dispatch_semaphore_create( FRAMES_IN_FLIGHT );
	mtl.samplers = [NSMutableDictionary dictionary];

	desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
	mtl.whiteTexture = [mtl.device newTextureWithDescriptor:desc];
	[mtl.whiteTexture replaceRegion:MTLRegionMake2D( 0, 0, 1, 1 ) mipmapLevel:0 withBytes:&white bytesPerRow:4];
	sdesc = [MTLSamplerDescriptor new];
	mtl.whiteSampler = [mtl.device newSamplerStateWithDescriptor:sdesc];

	MTL_ResetState();
	mtl.swapInterval = 0;
	mtl.layer.displaySyncEnabled = NO;

	ri.Printf( PRINT_ALL, "Metal device: %s\n", mtl.device.name.UTF8String );
	return (__bridge void *)mtl.device;
}

void MTL_DestroyContext( void *context ) {
	int	i;

	MTL_Flush( qtrue );
	if ( mtl.view ) {
		SDL_Metal_DestroyView( mtl.view );
	}
	for ( i = 0 ; i < mtl.numPipelines ; i++ ) {
		mtl.pipelines[i].state = nil;
	}
	for ( i = 0 ; i < mtl.numDepthStates ; i++ ) {
		mtl.depthStates[i].state = nil;
	}
	for ( i = 0 ; i < MAX_TEXTURES ; i++ ) {
		mtl.textures[i].texture = nil;
		mtl.textures[i].sampler = nil;
	}
	for ( i = 0 ; i < FRAMES_IN_FLIGHT ; i++ ) {
		mtl.ring[i] = nil;
	}
	mtl.lastPipeline = mtl.lastDepthState = nil;
	mtl.colorTarget = mtl.depthTarget = mtl.whiteTexture = nil;
	mtl.whiteSampler = nil;
	mtl.gammaTable = nil;
	mtl.gammaPipeline = nil;
	mtl.samplers = nil;
	mtl.vertexFunction = mtl.fragmentFunction = nil;
	mtl.library = nil;
	mtl.vertexDescriptor = nil;
	mtl.frameSemaphore = nil;
	mtl.layer = nil;
	mtl.queue = nil;
	mtl.device = nil;
	memset( (void *)&mtl, 0, sizeof( mtl ) );
}

int MTL_SetSwapInterval( int interval ) {
	mtl.swapInterval = interval;
	mtl.layer.displaySyncEnabled = interval ? YES : NO;
	return 0;
}

void MTL_SwapWindow( SDL_Window *window ) {
	id<CAMetalDrawable>			drawable;
	id<MTLBlitCommandEncoder>	blit;
	dispatch_semaphore_t		sem = mtl.frameSemaphore;
	int							w, h;

	MTL_BeginFrame();
	MTL_EndPass();

	@autoreleasepool {
		drawable = [mtl.layer nextDrawable];
		if ( drawable && drawable.texture.width == (NSUInteger)mtl.width && drawable.texture.height == (NSUInteger)mtl.height ) {
			if ( mtl.gammaActive && mtl.gammaPipeline ) {
				MTLRenderPassDescriptor		*pass = [MTLRenderPassDescriptor renderPassDescriptor];
				id<MTLRenderCommandEncoder>	enc;

				pass.colorAttachments[0].texture = drawable.texture;
				pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
				pass.colorAttachments[0].storeAction = MTLStoreActionStore;
				enc = [mtl.commandBuffer renderCommandEncoderWithDescriptor:pass];
				[enc setRenderPipelineState:mtl.gammaPipeline];
				[enc setFragmentTexture:mtl.colorTarget atIndex:0];
				[enc setFragmentTexture:mtl.gammaTable atIndex:1];
				[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
				[enc endEncoding];
			} else {
				blit = [mtl.commandBuffer blitCommandEncoder];
				[blit copyFromTexture:mtl.colorTarget toTexture:drawable.texture];
				[blit endEncoding];
			}
			[mtl.commandBuffer presentDrawable:drawable];
		}
		[mtl.commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
			dispatch_semaphore_signal( sem );
		}];
		[mtl.commandBuffer commit];
	}
	mtl.commandBuffer = nil;
	if ( mtl.framePool ) {
		objc_autoreleasePoolPop( mtl.framePool );
		mtl.framePool = NULL;
	}

	// follow a window that changed size
	SDL_GetWindowSize( window, &w, &h );
	if ( w != mtl.width || h != mtl.height ) {
		mtl.layer.drawableSize = CGSizeMake( w, h );
		MTL_CreateTargets( w, h );
	}
}

/*
=================
MTL_SetGamma

The game's brightness table, applied to the finished frame as it goes to
the window: works in a window too, and never touches the display itself
=================
*/
void MTL_SetGamma( const unsigned char *red, const unsigned char *green, const unsigned char *blue ) {
	uint8_t		table[256 * 4];
	qboolean	identity = qtrue;
	int			i;

	if ( !mtl.gammaTable ) {
		return;
	}
	for ( i = 0 ; i < 256 ; i++ ) {
		table[i * 4 + 0] = red[i];
		table[i * 4 + 1] = green[i];
		table[i * 4 + 2] = blue[i];
		table[i * 4 + 3] = 255;
		if ( red[i] != i || green[i] != i || blue[i] != i ) {
			identity = qfalse;
		}
	}
	[mtl.gammaTable replaceRegion:MTLRegionMake2D( 0, 0, 256, 1 ) mipmapLevel:0 withBytes:table bytesPerRow:256 * 4];
	mtl.gammaActive = !identity;
}
