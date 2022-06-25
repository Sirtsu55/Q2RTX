/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

//
// client.h -- Win32 client stuff
//

#include "shared/shared.h"
#include "common/common.h"
#include "common/files.h"
#if USE_CLIENT
#include "client/video.h"
#endif
#include "system/system.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef STATIC
#define STATIC static
#endif

// supported in XP SP3 or greater
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x01
#endif
#ifndef PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x02
#endif

extern HINSTANCE                    hGlobalInstance;

#if USE_DBGHELP
extern LPTOP_LEVEL_EXCEPTION_FILTER prevExceptionFilter;

LONG WINAPI Sys_ExceptionFilter(LPEXCEPTION_POINTERS);
#endif

