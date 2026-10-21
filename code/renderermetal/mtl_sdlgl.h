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
// mtl_sdlgl.h -- lets the SDL window code drive the Metal backend: the SDL
// OpenGL context calls it makes are pointed at Metal equivalents

#ifndef MTL_SDLGL_H
#define MTL_SDLGL_H

void			*MTL_CreateContext( SDL_Window *window );
void			MTL_DestroyContext( void *context );
void			MTL_SwapWindow( SDL_Window *window );
int				MTL_SetSwapInterval( int interval );
void			*MTL_GetProcAddress( const char *name );
SDL_bool		MTL_ExtensionSupported( const char *extension );
int				MTL_GetAttribute( SDL_GLattr attr, int *value );

#define SDL_GLContext				void *
#define SDL_GL_CreateContext		MTL_CreateContext
#define SDL_GL_DeleteContext		MTL_DestroyContext
#define SDL_GL_SwapWindow			MTL_SwapWindow
#define SDL_GL_SetSwapInterval		MTL_SetSwapInterval
#define SDL_GL_GetProcAddress		MTL_GetProcAddress
#define SDL_GL_ExtensionSupported	MTL_ExtensionSupported
#define SDL_GL_GetAttribute			MTL_GetAttribute
#define SDL_GL_SetAttribute( attr, value )	( (void)(attr), (void)(value), 0 )
#define SDL_GL_ResetAttributes()	( (void)0 )

#endif
