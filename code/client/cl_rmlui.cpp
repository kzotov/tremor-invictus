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
// cl_rmlui.cpp -- RmlUi in the client
//
// RmlUi lays out documents (.rml, .rcss under ui/ in the game folders) and
// hands back triangles; they are drawn through the renderer's 2D path, so
// the menus work with Metal and OpenGL alike and sit over the game. Events
// in documents are console commands: onclick="ui_practice" runs that command.

#include "cl_rmlui.h"

#include <RmlUi/Core.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

extern "C" {
#include "keycodes.h"
}

namespace {

rmlImport_t		imp;
bool			initialised;
Rml::Context	*context;
Rml::ElementDocument	*document;
Rml::String		documentPath;
int				screenWidth, screenHeight;
float			cursorX, cursorY;
int				cursorShader;
int				whiteShader;
int				textureCount;

// polyVert_t as the renderer takes it
struct PolyVert {
	float			xyz[3];
	float			st[2];
	unsigned char	modulate[4];
};

struct Geometry {
	std::vector<Rml::Vertex>	vertices;
	std::vector<int>			indices;
};

/*
=================
StraightAlpha

RmlUi works in premultiplied alpha; the 2D shaders blend straight alpha
=================
*/
void StraightAlpha( unsigned char *rgba ) {
	if ( rgba[3] && rgba[3] < 255 ) {
		for ( int i = 0 ; i < 3 ; i++ ) {
			int		c = rgba[i] * 255 / rgba[3];

			rgba[i] = (unsigned char)( c > 255 ? 255 : c );
		}
	}
}

int WhiteShader( void ) {
	if ( !whiteShader ) {
		static const unsigned char	white[4 * 4] = {
			255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };

		whiteShader = imp.RegisterShaderRGBA( "*rml_white", 2, 2, white );
	}
	return whiteShader;
}

class RenderInterface : public Rml::RenderInterface {
public:
	Rml::CompiledGeometryHandle CompileGeometry( Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices ) override {
		Geometry	*g = new Geometry;

		g->vertices.assign( vertices.begin(), vertices.end() );
		g->indices.assign( indices.begin(), indices.end() );
		return reinterpret_cast<Rml::CompiledGeometryHandle>( g );
	}

	void RenderGeometry( Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture ) override {
		const Geometry			*g = reinterpret_cast<const Geometry *>( handle );
		std::vector<PolyVert>	verts( g->vertices.size() );

		for ( size_t i = 0 ; i < verts.size() ; i++ ) {
			const Rml::Vertex	&v = g->vertices[i];

			verts[i].xyz[0] = v.position.x + translation.x;
			verts[i].xyz[1] = v.position.y + translation.y;
			verts[i].xyz[2] = 0;
			verts[i].st[0] = v.tex_coord.x;
			verts[i].st[1] = v.tex_coord.y;
			verts[i].modulate[0] = v.colour.red;
			verts[i].modulate[1] = v.colour.green;
			verts[i].modulate[2] = v.colour.blue;
			verts[i].modulate[3] = v.colour.alpha;
			StraightAlpha( verts[i].modulate );
		}
		imp.DrawTriangles2D( (int)verts.size(), verts.data(), (int)g->indices.size(), g->indices.data(),
			texture ? (int)texture : WhiteShader() );
	}

	void ReleaseGeometry( Rml::CompiledGeometryHandle handle ) override {
		delete reinterpret_cast<Geometry *>( handle );
	}

	// images by game path; their size comes from the file's header (PNG,
	// TGA), else documents give it in their style
	Rml::TextureHandle LoadTexture( Rml::Vector2i &dimensions, const Rml::String &source ) override {
		unsigned char	*data = nullptr;
		int				len = imp.ReadFile( source.c_str(), (void **)&data );
		Rml::String		name = source;
		size_t			dot;

		dimensions = Rml::Vector2i( 256, 256 );
		if ( len > 24 && data ) {
			if ( !memcmp( data, "\x89PNG", 4 ) ) {
				dimensions.x = ( data[16] << 24 ) | ( data[17] << 16 ) | ( data[18] << 8 ) | data[19];
				dimensions.y = ( data[20] << 24 ) | ( data[21] << 16 ) | ( data[22] << 8 ) | data[23];
			} else if ( data[2] == 2 || data[2] == 10 ) {
				dimensions.x = data[12] | ( data[13] << 8 );
				dimensions.y = data[14] | ( data[15] << 8 );
			}
		}
		if ( data ) {
			imp.FreeFile( data );
		}
		dot = name.find_last_of( '.' );
		if ( dot != Rml::String::npos ) {
			name = name.substr( 0, dot );
		}
		return (Rml::TextureHandle)imp.RegisterShaderNoMip( name.c_str() );
	}

	Rml::TextureHandle GenerateTexture( Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions ) override {
		std::vector<unsigned char>	rgba( source.begin(), source.end() );
		char	name[64];

		for ( size_t i = 0 ; i + 3 < rgba.size() ; i += 4 ) {
			StraightAlpha( &rgba[i] );
		}
		snprintf( name, sizeof( name ), "*rml_%d", ++textureCount );
		return (Rml::TextureHandle)imp.RegisterShaderRGBA( name, dimensions.x, dimensions.y, rgba.data() );
	}

	void ReleaseTexture( Rml::TextureHandle ) override {
		// the renderer frees its images when it restarts
	}

	void EnableScissorRegion( bool enable ) override {
		if ( !enable ) {
			imp.SetScissor2D( 0, 0, 0, 0 );
		}
	}

	void SetScissorRegion( Rml::Rectanglei region ) override {
		imp.SetScissor2D( region.Left(), region.Top(), region.Width(), region.Height() );
	}
};

// [key] in documents becomes text from ui/strings.txt, so the words live
// in one table and not in the markup
std::map<Rml::String, Rml::String>	strings;

void LoadStrings( void ) {
	char		*buffer = nullptr;
	int			len = imp.ReadFile( "ui/strings.txt", (void **)&buffer );
	Rml::String	text, line;
	size_t		start = 0, end;

	strings.clear();
	if ( len <= 0 || !buffer ) {
		return;
	}
	text.assign( buffer, len );
	imp.FreeFile( buffer );
	while ( start < text.size() ) {
		size_t	space, q1, q2;

		end = text.find( '\n', start );
		if ( end == Rml::String::npos ) {
			end = text.size();
		}
		line = text.substr( start, end - start );
		start = end + 1;
		if ( line.empty() || line[0] == '/' || line[0] == '#' ) {
			continue;
		}
		space = line.find( ' ' );
		q1 = line.find( '"' );
		q2 = line.rfind( '"' );
		if ( space == Rml::String::npos || q1 == Rml::String::npos || q2 <= q1 ) {
			continue;
		}
		strings[line.substr( 0, space )] = line.substr( q1 + 1, q2 - q1 - 1 );
	}
}

class SystemInterface : public Rml::SystemInterface {
public:
	double GetElapsedTime() override {
		return imp.Milliseconds() * 0.001;
	}

	int TranslateString( Rml::String &translated, const Rml::String &input ) override {
		size_t	open = input.find( '[' ), close;
		int		count = 0;

		translated = input;
		while ( open != Rml::String::npos ) {
			close = translated.find( ']', open );
			if ( close == Rml::String::npos ) {
				break;
			}
			Rml::String	key = translated.substr( open + 1, close - open - 1 );
			auto	it = strings.find( key );

			// [$name] is a cvar's value
			if ( !key.empty() && key[0] == '$' ) {
				char	value[256];

				imp.GetCvar( key.c_str() + 1, value, sizeof( value ) );
				translated.replace( open, close - open + 1, value );
				count++;
				open = translated.find( '[', open + strlen( value ) );
				continue;
			}
			if ( it != strings.end() ) {
				translated.replace( open, close - open + 1, it->second );
				count++;
				open = translated.find( '[', open + it->second.size() );
			} else {
				open = translated.find( '[', close );
			}
		}
		return count;
	}

	// "/path" is from the game folder's top, other paths from the document's
	void JoinPath( Rml::String &translated, const Rml::String &documentPath, const Rml::String &path ) override {
		size_t	slash;

		if ( !path.empty() && path[0] == '/' ) {
			translated = path.substr( 1 );
			return;
		}
		slash = documentPath.find_last_of( '/' );
		translated = ( slash == Rml::String::npos ? Rml::String() : documentPath.substr( 0, slash + 1 ) ) + path;
	}

	bool LogMessage( Rml::Log::Type type, const Rml::String &message ) override {
		if ( type <= Rml::Log::LT_WARNING ) {
			imp.Print( ( "RmlUi: " + message + "\n" ).c_str() );
		}
		return true;
	}
};

// files from the game's file system; fonts may also come from the
// system's font folder until we ship our own
struct MemoryFile {
	std::vector<char>	data;
	size_t				pos;
};

class FileInterface : public Rml::FileInterface {
public:
	Rml::FileHandle Open( const Rml::String &path ) override {
		MemoryFile	*file = new MemoryFile;
		void		*buffer = nullptr;
		int			len;

		file->pos = 0;
		if ( !path.compare( 0, 21, "/System/Library/Fonts" ) ) {
			FILE	*f = fopen( path.c_str(), "rb" );

			if ( !f ) {
				delete file;
				return 0;
			}
			fseek( f, 0, SEEK_END );
			file->data.resize( ftell( f ) );
			fseek( f, 0, SEEK_SET );
			if ( fread( file->data.data(), 1, file->data.size(), f ) != file->data.size() ) {
				file->data.clear();
			}
			fclose( f );
			return reinterpret_cast<Rml::FileHandle>( file );
		}
		len = imp.ReadFile( path.c_str(), &buffer );
		if ( len < 0 || !buffer ) {
			delete file;
			return 0;
		}
		file->data.assign( (char *)buffer, (char *)buffer + len );
		imp.FreeFile( buffer );
		return reinterpret_cast<Rml::FileHandle>( file );
	}

	void Close( Rml::FileHandle handle ) override {
		delete reinterpret_cast<MemoryFile *>( handle );
	}

	size_t Read( void *buffer, size_t size, Rml::FileHandle handle ) override {
		MemoryFile	*file = reinterpret_cast<MemoryFile *>( handle );
		size_t		left = file->data.size() - file->pos;

		if ( size > left ) {
			size = left;
		}
		memcpy( buffer, file->data.data() + file->pos, size );
		file->pos += size;
		return size;
	}

	bool Seek( Rml::FileHandle handle, long offset, int origin ) override {
		MemoryFile	*file = reinterpret_cast<MemoryFile *>( handle );
		long		base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? (long)file->pos : (long)file->data.size();

		if ( base + offset < 0 || base + offset > (long)file->data.size() ) {
			return false;
		}
		file->pos = base + offset;
		return true;
	}

	size_t Tell( Rml::FileHandle handle ) override {
		return reinterpret_cast<MemoryFile *>( handle )->pos;
	}
};

// onclick="..." and friends run their text as console commands
class CommandListener : public Rml::EventListener {
public:
	explicit CommandListener( const Rml::String &command ) : command( command ) {}

	void ProcessEvent( Rml::Event & ) override {
		imp.ExecuteText( ( command + "\n" ).c_str() );
	}

	void OnDetach( Rml::Element * ) override {
		delete this;
	}

private:
	Rml::String	command;
};

class CommandListenerInstancer : public Rml::EventListenerInstancer {
public:
	Rml::EventListener *InstanceEventListener( const Rml::String &value, Rml::Element * ) override {
		return new CommandListener( value );
	}
};

RenderInterface			renderInterface;
SystemInterface			systemInterface;
FileInterface			fileInterface;
CommandListenerInstancer	listenerInstancer;

/*
=================
LoadFonts

Our fonts from ui/fonts/ when the game has them, else the system's
=================
*/
void LoadFonts( void ) {
	static const char	*fonts[] = {
		"ui/fonts/display.ttf", "ui/fonts/body.ttf", "ui/fonts/body-bold.ttf",
		"/System/Library/Fonts/Supplemental/Arial Black.ttf",
		"/System/Library/Fonts/Supplemental/Arial Narrow.ttf",
		"/System/Library/Fonts/Supplemental/Arial Narrow Bold.ttf",
		"/System/Library/Fonts/Supplemental/Arial.ttf"
	};

	for ( const char *font : fonts ) {
		Rml::FileHandle	f = fileInterface.Open( font );

		if ( f ) {
			fileInterface.Close( f );
			Rml::LoadFontFace( font, !strcmp( font, fonts[6] ) );
		}
	}
}

Rml::Input::KeyIdentifier KeyIdentifier( int key ) {
	using namespace Rml::Input;

	if ( key >= 'a' && key <= 'z' ) {
		return (KeyIdentifier)( KI_A + key - 'a' );
	}
	if ( key >= '0' && key <= '9' ) {
		return (KeyIdentifier)( KI_0 + key - '0' );
	}
	switch ( key ) {
	case K_ENTER: case K_KP_ENTER:	return KI_RETURN;
	case K_ESCAPE:		return KI_ESCAPE;
	case K_BACKSPACE:	return KI_BACK;
	case K_TAB:			return KI_TAB;
	case K_SPACE:		return KI_SPACE;
	case K_UPARROW:		return KI_UP;
	case K_DOWNARROW:	return KI_DOWN;
	case K_LEFTARROW:	return KI_LEFT;
	case K_RIGHTARROW:	return KI_RIGHT;
	case K_DEL:			return KI_DELETE;
	case K_HOME:		return KI_HOME;
	case K_END:			return KI_END;
	case K_PGUP:		return KI_PRIOR;
	case K_PGDN:		return KI_NEXT;
	default:			return KI_UNKNOWN;
	}
}

}	// namespace

extern "C" void RML_Init( const rmlImport_t *import, int width, int height ) {
	if ( initialised ) {
		return;
	}
	imp = *import;
	screenWidth = width;
	screenHeight = height;
	cursorX = width * 0.5f;
	cursorY = height * 0.5f;

	Rml::SetRenderInterface( &renderInterface );
	Rml::SetSystemInterface( &systemInterface );
	Rml::SetFileInterface( &fileInterface );
	if ( !Rml::Initialise() ) {
		imp.Print( "RmlUi: could not start\n" );
		return;
	}
	Rml::Factory::RegisterEventListenerInstancer( &listenerInstancer );
	LoadFonts();
	LoadStrings();
	context = Rml::CreateContext( "main", Rml::Vector2i( width, height ) );
	// documents are laid out at 1080 lines; dp units scale to the window
	context->SetDensityIndependentPixelRatio( height / 1080.0f );
	initialised = true;
	imp.SetCvar( "rml_ok", "1" );		// the menus can use RmlUi
}

extern "C" void RML_Shutdown( void ) {
	if ( !initialised ) {
		return;
	}
	Rml::Shutdown();
	initialised = false;
	context = nullptr;
	document = nullptr;
}

extern "C" void RML_RendererRestart( int width, int height ) {
	if ( !initialised ) {
		return;
	}
	whiteShader = 0;
	cursorShader = 0;
	Rml::ReleaseTextures();
	screenWidth = width;
	screenHeight = height;
	context->SetDimensions( Rml::Vector2i( width, height ) );
	context->SetDensityIndependentPixelRatio( height / 1080.0f );
}

extern "C" int RML_Active( void ) {
	return initialised && document && document->IsVisible();
}

/*
=================
Condition

data-when="cvar=value&cvar!=value&cvar>=n": an element shows only when all
of them hold (numbers compare as numbers)
=================
*/
bool Condition( const Rml::String &when ) {
	size_t	start = 0;

	while ( start < when.size() ) {
		size_t			end = when.find( '&', start ), op;
		Rml::String		part, name, value, op2;
		char			buffer[256];

		if ( end == Rml::String::npos ) {
			end = when.size();
		}
		part = when.substr( start, end - start );
		start = end + 1;
		op = part.find_first_of( "=!<>" );
		if ( op == Rml::String::npos ) {
			continue;
		}
		name = part.substr( 0, op );
		op2 = part.substr( op, part[op + 1] == '=' ? 2 : 1 );
		value = part.substr( op + op2.size() );
		imp.GetCvar( name.c_str(), buffer, sizeof( buffer ) );
		{
			float	a = (float)atof( buffer ), b = (float)atof( value.c_str() );
			bool	equal = value == buffer || ( buffer[0] && a == b && ( value[0] == '-' || isdigit( (unsigned char)value[0] ) ) );
			bool	ok = op2 == "=" ? equal : op2 == "!=" ? !equal : op2 == ">=" ? a >= b
				: op2 == "<=" ? a <= b : op2 == ">" ? a > b : op2 == "<" ? a < b : true;

			if ( !ok ) {
				return false;
			}
		}
	}
	return true;
}

void ApplyConditions( Rml::Element *element ) {
	for ( int i = 0 ; i < element->GetNumChildren() ; i++ ) {
		Rml::Element	*child = element->GetChild( i );
		Rml::Variant	*when = child->GetAttribute( "data-when" );

		if ( when ) {
			if ( Condition( when->Get<Rml::String>() ) ) {
				child->RemoveProperty( "display" );
			} else {
				child->SetProperty( "display", "none" );
			}
		}
		ApplyConditions( child );
	}
}

extern "C" void RML_Show( const char *path ) {
	if ( !initialised ) {
		return;
	}
	// showing: nothing to do; hidden: load it again, so text taken from
	// cvars ([$name]) is current
	if ( document && documentPath == path && document->IsVisible() ) {
		return;
	}
	if ( document ) {
		document->Close();
		document = nullptr;
	}
	document = context->LoadDocument( path );
	if ( !document ) {
		imp.Print( ( Rml::String( "RmlUi: could not load " ) + path + "\n" ).c_str() );
		imp.SetCvar( "rml_active", "0" );
		imp.SetCvar( "rml_ok", "0" );		// back to the classic menus
		return;
	}
	documentPath = path;
	imp.SetCvar( "rml_doc", path );
	ApplyConditions( document );
	document->Show();
	imp.SetCvar( "rml_active", "1" );
}

extern "C" void RML_Hide( void ) {
	if ( document ) {
		document->Hide();
	}
	imp.SetCvar( "rml_active", "0" );
}

extern "C" void RML_Reload( void ) {
	Rml::String	path = documentPath;

	if ( !initialised || path.empty() ) {
		return;
	}
	if ( document ) {
		document->Close();
		document = nullptr;
	}
	LoadStrings();
	Rml::Factory::ClearStyleSheetCache();
	Rml::Factory::ClearTemplateCache();
	documentPath.clear();
	RML_Show( path.c_str() );
}

extern "C" void RML_Toggle( const char *id, const char *className ) {
	Rml::Element	*element;

	if ( !document ) {
		return;
	}
	element = document->GetElementById( id );
	if ( element ) {
		element->SetClass( className, !element->IsClassSet( className ) );
	}
}

extern "C" void RML_Frame( void ) {
	static const float	white[4] = { 1, 1, 1, 1 };
	float	size;

	if ( !RML_Active() ) {
		return;
	}
	context->Update();
	context->Render();
	imp.SetScissor2D( 0, 0, 0, 0 );

	// the cursor
	if ( !cursorShader ) {
		cursorShader = imp.RegisterShaderNoMip( "menu/art/3_cursor2" );
	}
	size = screenHeight / 30.0f;
	imp.SetColor( white );
	imp.DrawStretchPic( cursorX - size * 0.5f, cursorY - size * 0.5f, size, size, 0, 0, 1, 1, cursorShader );
	imp.SetColor( nullptr );
}

extern "C" int RML_KeyEvent( int key, int down ) {
	if ( !RML_Active() ) {
		return 0;
	}
	if ( key >= K_MOUSE1 && key <= K_MOUSE1 + 2 ) {
		if ( down ) {
			context->ProcessMouseButtonDown( key - K_MOUSE1, 0 );
		} else {
			context->ProcessMouseButtonUp( key - K_MOUSE1, 0 );
		}
		return 1;
	}
	if ( key == K_MWHEELUP || key == K_MWHEELDOWN ) {
		if ( down ) {
			context->ProcessMouseWheel( Rml::Vector2f( 0, key == K_MWHEELUP ? -1.0f : 1.0f ), 0 );
		}
		return 1;
	}
	// Escape runs the document's data-escape command, if it has one
	if ( key == K_ESCAPE && down && document->GetAttribute( "data-escape" ) ) {
		imp.ExecuteText( ( document->GetAttribute<Rml::String>( "data-escape", "" ) + "\n" ).c_str() );
		return 1;
	}
	if ( down ) {
		context->ProcessKeyDown( KeyIdentifier( key ), 0 );
	} else {
		context->ProcessKeyUp( KeyIdentifier( key ), 0 );
	}
	return 1;
}

extern "C" int RML_CharEvent( int ch ) {
	if ( !RML_Active() ) {
		return 0;
	}
	if ( ch >= 32 && ch != 127 ) {
		context->ProcessTextInput( (Rml::Character)ch );
	}
	return 1;
}

extern "C" void RML_MouseMove( int dx, int dy ) {
	if ( !RML_Active() ) {
		return;
	}
	cursorX += dx;
	cursorY += dy;
	if ( cursorX < 0 ) cursorX = 0;
	if ( cursorY < 0 ) cursorY = 0;
	if ( cursorX > screenWidth - 1 ) cursorX = screenWidth - 1;
	if ( cursorY > screenHeight - 1 ) cursorY = screenHeight - 1;
	context->ProcessMouseMove( (int)cursorX, (int)cursorY, 0 );
}
