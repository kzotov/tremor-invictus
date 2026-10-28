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
//
// cl_rmlui.h -- the client's RmlUi menus (cl_rmlui.cpp), behind a plain C
// boundary so the C++ side never includes the engine's headers
//

#ifndef CL_RMLUI_H
#define CL_RMLUI_H

#ifdef __cplusplus
extern "C" {
#endif

// what the interface needs from the engine and the renderer
typedef struct {
	int		(*RegisterShaderRGBA)( const char *name, int width, int height, const unsigned char *rgba );
	int		(*RegisterShaderNoMip)( const char *name );
	void	(*DrawTriangles2D)( int numVerts, const void *polyVerts, int numIndexes, const int *indexes, int shader );
	void	(*SetScissor2D)( int x, int y, int w, int h );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader );
	void	(*SetColor)( const float *rgba );
	void	(*Print)( const char *text );
	void	(*ExecuteText)( const char *text );		// appended to the command buffer
	int		(*Milliseconds)( void );
	int		(*ReadFile)( const char *path, void **buffer );	// game file system; -1 when missing
	void	(*FreeFile)( void *buffer );
	int		(*ImageSize)( const char *path, int *width, int *height );	// a picture file's size; 0 when missing or unknown
	void	(*SetCvar)( const char *name, const char *value );
	void	(*GetCvar)( const char *name, char *buffer, int size );
} rmlImport_t;

void	RML_Init( const rmlImport_t *import, int width, int height );
void	RML_Shutdown( void );
void	RML_RendererRestart( int width, int height );	// the renderer's textures are gone
int		RML_Active( void );								// a document is showing
void	RML_Show( const char *document );
void	RML_Hide( void );
void	RML_Reload( void );
void	RML_Toggle( const char *id, const char *className );	// a class on an element of the document
void	RML_Frame( void );
void	RML_OverlayFrame( void );						// the overlay named by rml_overlay, over the game
int		RML_KeyEvent( int key, int down );				// nonzero when the interface took it
int		RML_CharEvent( int ch );
void	RML_MouseMove( int dx, int dy );
void	RML_Capture( const char *command );		// the next key goes to this command

#ifdef __cplusplus
}
#endif

#endif
