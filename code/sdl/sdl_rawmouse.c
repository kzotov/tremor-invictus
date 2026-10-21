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
// sdl_rawmouse.c -- mouse counts straight from the mice on macOS. SDL's
// relative mode there reports movement after the system's pointer
// acceleration; aiming needs the counts the mouse actually sent.

#ifdef __APPLE__

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDUsageTables.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sdl_rawmouse.h"

static IOHIDManagerRef	rawManager;
static int				rawDx, rawDy;

static void IN_RawMouseValue( void *context, IOReturn result, void *sender, IOHIDValueRef value ) {
	IOHIDElementRef	element = IOHIDValueGetElement( value );
	CFIndex			delta;

	if ( IOHIDElementGetUsagePage( element ) != kHIDPage_GenericDesktop ) {
		return;
	}
	delta = IOHIDValueGetIntegerValue( value );
	switch ( IOHIDElementGetUsage( element ) ) {
	case kHIDUsage_GD_X:
		rawDx += (int)delta;
		break;
	case kHIDUsage_GD_Y:
		rawDy += (int)delta;
		break;
	default:
		break;
	}
}

static CFDictionaryRef IN_RawMouseMatch( int usage ) {
	CFMutableDictionaryRef	dict;
	int						page = kHIDPage_GenericDesktop;
	CFNumberRef				n;

	dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	n = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &page );
	CFDictionarySetValue( dict, CFSTR( kIOHIDDeviceUsagePageKey ), n );
	CFRelease( n );
	n = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &usage );
	CFDictionarySetValue( dict, CFSTR( kIOHIDDeviceUsageKey ), n );
	CFRelease( n );
	return dict;
}

/*
===============
IN_RawMouseStart

True if raw counts are now coming in
===============
*/
qboolean IN_RawMouseStart( void ) {
	CFDictionaryRef		matches[2];
	CFArrayRef			array;

	if ( rawManager ) {
		return qtrue;
	}
	rawManager = IOHIDManagerCreate( kCFAllocatorDefault, kIOHIDOptionsTypeNone );
	if ( !rawManager ) {
		return qfalse;
	}

	matches[0] = IN_RawMouseMatch( kHIDUsage_GD_Mouse );
	matches[1] = IN_RawMouseMatch( kHIDUsage_GD_Pointer );
	array = CFArrayCreate( kCFAllocatorDefault, (const void **)matches, 2, &kCFTypeArrayCallBacks );
	IOHIDManagerSetDeviceMatchingMultiple( rawManager, array );
	CFRelease( array );
	CFRelease( matches[0] );
	CFRelease( matches[1] );

	IOHIDManagerRegisterInputValueCallback( rawManager, IN_RawMouseValue, NULL );
	// called back while SDL pumps the main run loop for its own events
	IOHIDManagerScheduleWithRunLoop( rawManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
	if ( IOHIDManagerOpen( rawManager, kIOHIDOptionsTypeNone ) != kIOReturnSuccess ) {
		IN_RawMouseStop();
		return qfalse;
	}
	rawDx = rawDy = 0;
	return qtrue;
}

void IN_RawMouseStop( void ) {
	if ( !rawManager ) {
		return;
	}
	IOHIDManagerUnscheduleFromRunLoop( rawManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
	IOHIDManagerClose( rawManager, kIOHIDOptionsTypeNone );
	CFRelease( rawManager );
	rawManager = NULL;
}

/*
===============
IN_RawMouseTake

The counts gathered since the last call
===============
*/
void IN_RawMouseTake( int *dx, int *dy ) {
	*dx = rawDx;
	*dy = rawDy;
	rawDx = rawDy = 0;
}

#endif
