/*
Copyright (C) 2013 Andrey Nazarov
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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
// system.c
//

#include "shared/shared.h"
#include "common/cvar.h"
#include "common/common.h"
#include "common/files.h"
#include "common/zone.h"
#include "system/system.h"
#include <SDL.h>

#if USE_CLIENT
#include "client/client.h"
#include "client/input.h"
#include "client/keys.h"
#include "client/ui.h"
#include "client/video.h"
#include "refresh/refresh.h"
#include "res/nacicon.xbm"
#ifdef _WINDOWS
#include <ShellScalingAPI.h>

typedef HRESULT(__stdcall *PFN_SetProcessDpiAwareness_t)(_In_ PROCESS_DPI_AWARENESS value);
PFN_SetProcessDpiAwareness_t PFN_SetProcessDpiAwareness = NULL;
HMODULE h_ShCoreDLL = 0;
#endif

SDL_Window       *sdl_window;
static vidFlags_t       sdl_flags;

extern cvar_t* vid_display;
extern cvar_t* vid_displaylist;
#endif

/*
===============================================================================

GENERAL ROUTINES

===============================================================================
*/

unsigned Sys_Milliseconds(void)
{
    return SDL_GetTicks();
}

/*
===============================================================================

ASYNC WORK QUEUE

===============================================================================
*/

#if USE_CLIENT

static bool work_initialized;
static bool work_terminate;
static SDL_mutex *work_lock;
static SDL_cond *work_cond;
static SDL_Thread *work_thread;
static asyncwork_t *pend_head;
static asyncwork_t *done_head;

static void append_work(asyncwork_t **head, asyncwork_t *work)
{
    asyncwork_t *c, **p;
    for (p = head, c = *head; c; p = &c->next, c = c->next);
    work->next = NULL;
    *p = work;
}

void Sys_CompleteAsyncQueue(void)
{
    asyncwork_t *work, *next;

    if (!work_initialized)
        return;
    if (SDL_TryLockMutex(work_lock))
        return;
    if (q_unlikely(done_head)) {
        for (work = done_head; work; work = next) {
            next = work->next;
            if (work->done_cb)
                work->done_cb(work->cb_arg);
            Z_Free(work);
        }
        done_head = NULL;
    }
    SDL_UnlockMutex(work_lock);
}

static int thread_func(void *arg)
{
    SDL_LockMutex(work_lock);
    while (1) {
        while (!pend_head && !work_terminate)
            SDL_CondWait(work_cond, work_lock);

        asyncwork_t *work = pend_head;
        if (!work)
            break;
        pend_head = work->next;

        SDL_UnlockMutex(work_lock);
        work->work_cb(work->cb_arg);
        SDL_LockMutex(work_lock);

        append_work(&done_head, work);
    }
    SDL_UnlockMutex(work_lock);

    return 0;
}

void Sys_ShutdownAsyncQueue(void)
{
    if (!work_initialized)
        return;

    SDL_LockMutex(work_lock);
    work_terminate = true;
    SDL_CondSignal(work_cond);
    SDL_UnlockMutex(work_lock);

    SDL_WaitThread(work_thread, NULL);
    Sys_CompleteAsyncQueue();

    SDL_DestroyMutex(work_lock);
    SDL_DestroyCond(work_cond);

    work_lock = NULL;
    work_cond = NULL;
    work_thread = NULL;
    work_initialized = false;
}

void Sys_QueueAsyncWork(asyncwork_t *work)
{
    if (!work_initialized) {
        work_lock = SDL_CreateMutex();
        work_cond = SDL_CreateCond();
        if (!(work_thread = SDL_CreateThread(thread_func, "async work queue", NULL)))
            Sys_Error("Couldn't create async work thread");
        work_initialized = true;
    }

    SDL_LockMutex(work_lock);
    append_work(&pend_head, Z_CopyStruct(work));
    SDL_CondSignal(work_cond);
    SDL_UnlockMutex(work_lock);
}

#endif

/*
===============================================================================

OPENGL STUFF

===============================================================================
*/

#if USE_CLIENT

/*
===============================================================================

VIDEO

===============================================================================
*/

static void VID_SDL_ModeChanged(void)
{
    int width, height;
    void *pixels;
    int rowbytes;

    SDL_GetWindowSize(sdl_window, &width, &height);

    Uint32 flags = SDL_GetWindowFlags(sdl_window);
    if (flags & SDL_WINDOW_FULLSCREEN)
        sdl_flags |= QVF_FULLSCREEN;
    else
        sdl_flags &= ~QVF_FULLSCREEN;

    pixels = NULL;
    rowbytes = 0;

    R_ModeChanged(width, height, sdl_flags, rowbytes, pixels);
    SCR_ModeChanged();
}

static void VID_SDL_SetMode(void)
{
    Uint32 flags;
    vrect_t rc;
    int freq;

    if (vid_fullscreen->integer) {
        // move the window onto the selected display
        SDL_Rect display_bounds;
        SDL_GetDisplayBounds(vid_display->integer, &display_bounds);
        SDL_SetWindowPosition(sdl_window, display_bounds.x, display_bounds.y);

        if (VID_GetFullscreen(&rc, &freq, NULL)) {
            SDL_DisplayMode mode = {
                .format         = SDL_PIXELFORMAT_UNKNOWN,
                .w              = rc.width,
                .h              = rc.height,
                .refresh_rate   = freq,
                .driverdata     = NULL
            };
            SDL_SetWindowDisplayMode(sdl_window, &mode);
            flags = SDL_WINDOW_FULLSCREEN;
        } else {
            flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
    } else {
        if (VID_GetGeometry(&rc)) {
            SDL_SetWindowSize(sdl_window, rc.width, rc.height);
            SDL_SetWindowPosition(sdl_window, rc.x, rc.y);
        }
        flags = 0;
    }

    SDL_SetWindowFullscreen(sdl_window, flags);
}

void VID_SetMode(void)
{
    VID_SDL_SetMode();
    VID_SDL_ModeChanged();
}

void VID_BeginFrame(void)
{
}

void VID_EndFrame(void)
{
#if USE_REF == REF_VKPT
	/* subsystem does it itself */
#else
    SDL_UpdateWindowSurface(sdl_window);
#endif
}

void VID_FatalShutdown(void)
{
    SDL_SetWindowGrab(sdl_window, SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_Quit();
}

char *VID_GetClipboardData(void)
{
    // returned data must be Z_Free'd
    char *text = SDL_GetClipboardText();
    char *copy = Z_CopyString(text);
    SDL_free(text);
    return copy;
}

void VID_SetClipboardData(const char *data)
{
    SDL_SetClipboardText(data);
}

void VID_UpdateGamma(const byte *table)
{
    Uint16 ramp[256];
    int i;

    if (sdl_flags & QVF_GAMMARAMP) {
        for (i = 0; i < 256; i++) {
            ramp[i] = table[i] << 8;
        }
        SDL_SetWindowGammaRamp(sdl_window, ramp, ramp, ramp);
    }
}

static int VID_SDL_InitSubSystem(void)
{
    int ret;

    if (SDL_WasInit(SDL_INIT_VIDEO))
        return 0;

    ret = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (ret == -1)
        Com_EPrintf("Couldn't initialize SDL video: %s\n", SDL_GetError());

    return ret;
}

static int VID_SDL_EventFilter(void *userdata, SDL_Event *event)
{
    // SDL uses relative time, we need absolute
    event->common.timestamp = Sys_Milliseconds();
    return 1;
}

static void VID_GetDisplayList(void)
{
    int num_displays = SDL_GetNumVideoDisplays();
    int string_size = 0;
    int max_display_name_length = 0;

    for (int display = 0; display < num_displays; display++)
    {
        int len = strlen(SDL_GetDisplayName(display));
        max_display_name_length = max(max_display_name_length, len);
        string_size += 12 + len;
    }
    
    string_size += 1;
    max_display_name_length += 1;

    char *display_list = Z_Malloc(string_size);
    char *display_name = Z_Malloc(max_display_name_length);
    display_list[0] = 0;

    for (int display = 0; display < num_displays; display++)
    {
        Q_strlcpy(display_name, SDL_GetDisplayName(display), max_display_name_length);

        // remove quotes from the display name to avoid weird parsing errors later
        for (char* p = display_name; *p; p++)
        {
            if (*p == '\"')
                *p = '\'';
        }

        // Append something like `"1 (Generic PnP Monitor)" 0` to the display list,
        // where the first quoted field is the UI string,
        // and the second field is the value for vid_display.
        // Add 1 to the display index because that's how Windows control panel shows the displays.
        Q_strlcat(display_list, va("\"%d (%s)\" %d ", display + 1, display_name, display), string_size);
    }

    Cvar_SetByVar(vid_displaylist, display_list, FROM_CODE);

    Z_Free(display_list);
    Z_Free(display_name);
}

char *VID_GetDefaultModeList(void)
{
    SDL_DisplayMode mode;
    size_t size, len;
    char *buf;
    int i, num_modes;

    if (VID_SDL_InitSubSystem())
        return NULL;

    Cvar_ClampInteger(vid_display, 0, SDL_GetNumVideoDisplays() - 1);

    VID_GetDisplayList();

    num_modes = SDL_GetNumDisplayModes(vid_display->integer);
    if (num_modes < 1)
        return Z_CopyString(VID_MODELIST);

    size = 8 + num_modes * 32 + 1;
    buf = Z_Malloc(size);

    len = Q_strlcpy(buf, "desktop borderless ", size);
    for (i = 0; i < num_modes; i++) {
        if (SDL_GetDisplayMode(vid_display->integer, i, &mode) < 0)
            break;
        if (mode.refresh_rate == 0)
            continue;
        len += Q_scnprintf(buf + len, size - len, "%dx%d@%d ",
                           mode.w, mode.h, mode.refresh_rate);
    }

    if (len == 0)
        buf[0] = 0;
    else if (buf[len - 1] == ' ')
        buf[len - 1] = 0;

    return buf;
}

bool VID_Init(graphics_api_t api)
{
#ifdef _WINDOWS
	// Load the DLL and function dynamically to avoid exe file incompatibility with Windows 7

	if (!h_ShCoreDLL)
	{
		h_ShCoreDLL = LoadLibraryA("shcore.dll");
	}

	if (h_ShCoreDLL && !PFN_SetProcessDpiAwareness)
	{
		PFN_SetProcessDpiAwareness = (PFN_SetProcessDpiAwareness_t)GetProcAddress(h_ShCoreDLL, "SetProcessDpiAwareness");
	}

	if (PFN_SetProcessDpiAwareness)
	{
		PFN_SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
	}
#endif

	Uint32 flags = SDL_WINDOW_RESIZABLE;
	vrect_t rc;

	if (VID_SDL_InitSubSystem()) {
		return false;
	}

	SDL_SetEventFilter(VID_SDL_EventFilter, NULL);

	if (!VID_GetGeometry(&rc)) {
		rc.x = SDL_WINDOWPOS_UNDEFINED;
		rc.y = SDL_WINDOWPOS_UNDEFINED;
	}

	if (api == GAPI_VULKAN)
	{
		flags |= SDL_WINDOW_VULKAN;
	}

	sdl_window = SDL_CreateWindow(PRODUCT, rc.x, rc.y, rc.width, rc.height, flags);

	if (!sdl_window) {
		Com_EPrintf("Couldn't create SDL window: %s\n", SDL_GetError());
		return false;
	}

	SDL_SetWindowMinimumSize(sdl_window, 320, 240);

	uint32_t icon_rgb[q2icon_height][q2icon_width];
	for (int y = 0; y < q2icon_height; y++)
	{
		for (int x = 0; x < q2icon_height; x++)
		{
			byte b = q2icon_bits[(y * q2icon_width + x) / 8];
			if ((b >> (x & 7)) & 1)
				icon_rgb[y][x] = 0xFF7AB632; // NVIDIA green color
			else
				icon_rgb[y][x] = 0x00000000;
		}
	}

    SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(icon_rgb, q2icon_width, q2icon_height, 32, q2icon_width * sizeof(uint32_t), 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    if (icon) {
        SDL_SetWindowIcon(sdl_window, icon);
        SDL_FreeSurface(icon);
    }

    VID_SDL_SetMode();

    cvar_t *vid_hwgamma = Cvar_Get("vid_hwgamma", "0", CVAR_REFRESH);
    if (vid_hwgamma->integer) {
        Uint16  gamma[3][256];

        if (SDL_GetWindowGammaRamp(sdl_window, gamma[0], gamma[1], gamma[2]) == 0 &&
            SDL_SetWindowGammaRamp(sdl_window, gamma[0], gamma[1], gamma[2]) == 0) {
            Com_Printf("...enabling hardware gamma\n");
            sdl_flags |= QVF_GAMMARAMP;
        } else {
            Com_Printf("...hardware gamma not supported\n");
            Cvar_Reset(vid_hwgamma);
        }
    }

    VID_SetMode();

    /* Explicitly set an "active" state to ensure at least one frame is displayed.
       Required for Wayland (on Fedora 36/GNOME/NVIDIA driver 510.68.02/SDL 2.0.22) -
       without that, we never get a window event and thus the activation state sticks
       at ACT_MINIMIZED, never rendering anything. */
    CL_Activate(ACT_RESTORED);

    return true;
}

void VID_Shutdown(void)
{
    if (sdl_window) {
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    sdl_flags = 0;
}

/*
==========================================================================

EVENTS

==========================================================================
*/

static void window_event(SDL_WindowEvent *event)
{
    Uint32 flags = SDL_GetWindowFlags(sdl_window);
    active_t active;
    vrect_t rc;

    switch (event->event) {
    case SDL_WINDOWEVENT_RESTORED:
    case SDL_WINDOWEVENT_ENTER:
    case SDL_WINDOWEVENT_FOCUS_GAINED:
        if (flags & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS)) {
            active = ACT_ACTIVATED;
        } else {
            active = ACT_RESTORED;
        }
        CL_Activate(active);
        break;

    case SDL_WINDOWEVENT_MINIMIZED:
    case SDL_WINDOWEVENT_LEAVE:
    case SDL_WINDOWEVENT_FOCUS_LOST:
        if (flags & SDL_WINDOW_MINIMIZED) {
            active = ACT_MINIMIZED;
        } else {
            active = ACT_RESTORED;
        }
        CL_Activate(active);
        break;

    case SDL_WINDOWEVENT_MOVED:
        if (!(flags & SDL_WINDOW_FULLSCREEN)) {
            SDL_GetWindowSize(sdl_window, &rc.width, &rc.height);
            rc.x = event->data1;
            rc.y = event->data2;
            VID_SetGeometry(&rc);
        }
        break;

    case SDL_WINDOWEVENT_RESIZED:
        if (!(flags & SDL_WINDOW_FULLSCREEN)) {
            SDL_GetWindowPosition(sdl_window, &rc.x, &rc.y);
            rc.width = event->data1;
            rc.height = event->data2;
            VID_SetGeometry(&rc);
        }
        VID_SDL_ModeChanged();
        break;
    }
}

static const byte scantokey[128] = {
//  0               1               2               3               4               5               6                   7
//  8               9               A               B               C               D               E                   F
    0,              0,              0,              0,              'a',            'b',            'c',                'd',            // 0
    'e',            'f',            'g',            'h',            'i',            'j',            'k',                'l',
    'm',            'n',            'o',            'p',            'q',            'r',            's',                't',            // 1
    'u',            'v',            'w',            'x',            'y',            'z',            '1',                '2',
    '3',            '4',            '5',            '6',            '7',            '8',            '9',                '0',            // 2
    K_ENTER,        K_ESCAPE,       K_BACKSPACE,    K_TAB,          K_SPACE,        '-',            '=',                '[',
    ']',            '\\',           0,              ';',            '\'',           '`',            ',',                '.',            // 3
    '/' ,           K_CAPSLOCK,     K_F1,           K_F2,           K_F3,           K_F4,           K_F5,               K_F6,
    K_F7,           K_F8,           K_F9,           K_F10,          K_F11,          K_F12,          K_PRINTSCREEN,      K_SCROLLOCK,    // 4
    K_PAUSE,        K_INS,          K_HOME,         K_PGUP,         K_DEL,          K_END,          K_PGDN,             K_RIGHTARROW,
    K_LEFTARROW,    K_DOWNARROW,    K_UPARROW,      K_NUMLOCK,      K_KP_SLASH,     K_KP_MULTIPLY,  K_KP_MINUS,         K_KP_PLUS,      // 5
    K_KP_ENTER,     K_KP_END,       K_KP_DOWNARROW, K_KP_PGDN,      K_KP_LEFTARROW, K_KP_5,         K_KP_RIGHTARROW,    K_KP_HOME,
    K_KP_UPARROW,   K_KP_PGUP,      K_KP_INS,       K_KP_DEL,       K_102ND,        0,              0,                  0,              // 6
    0,              0,              0,              0,              0,              0,              0,                  0,
    0,              0,              0,              0,              0,              0,              K_MENU,             0,              // 7
    K_LCTRL,        K_LSHIFT,       K_LALT,         K_LWINKEY,      K_RCTRL,        K_RSHIFT,       K_RALT,             K_RWINKEY,      // E
};

static void key_event(SDL_KeyboardEvent *event)
{
    byte result;

    if(event->keysym.sym < 127)
      result = event->keysym.sym; // direct mapping of ascii
    else if (event->keysym.scancode >= 224 && event->keysym.scancode < 224 + 8)
      result = scantokey[event->keysym.scancode - 104];
    else if (event->keysym.scancode > 0x30)
      result = scantokey[event->keysym.scancode];
    else result = 0;

    if (result == 0) {
        Com_DPrintf("%s: unknown scancode %d\n", __func__, event->keysym.scancode);
        return;
    }

    if (result == K_LALT || result == K_RALT)
        Key_Event(K_ALT, event->state, event->timestamp);
    else if (result == K_LCTRL || result == K_RCTRL)
        Key_Event(K_CTRL, event->state, event->timestamp);
    else if (result == K_LSHIFT || result == K_RSHIFT)
        Key_Event(K_SHIFT, event->state, event->timestamp);

    Key_Event(result, event->state, event->timestamp);
}

static void mouse_button_event(SDL_MouseButtonEvent *event)
{
    unsigned key;

    switch (event->button) {
    case SDL_BUTTON_LEFT:
        key = K_MOUSE1;
        break;
    case SDL_BUTTON_RIGHT:
        key = K_MOUSE2;
        break;
    case SDL_BUTTON_MIDDLE:
        key = K_MOUSE3;
        break;
    case SDL_BUTTON_X1:
        key = K_MOUSE4;
        break;
    case SDL_BUTTON_X2:
        key = K_MOUSE5;
        break;
    default:
        Com_DPrintf("%s: unknown button %d\n", __func__, event->button);
        return;
    }

    Key_Event(key, event->state, event->timestamp);
}

static void mouse_wheel_event(SDL_MouseWheelEvent *event)
{
    if (event->x > 0) {
        Key_Event(K_MWHEELRIGHT, true, event->timestamp);
        Key_Event(K_MWHEELRIGHT, false, event->timestamp);
    } else if (event->x < 0) {
        Key_Event(K_MWHEELLEFT, true, event->timestamp);
        Key_Event(K_MWHEELLEFT, false, event->timestamp);
    }

    if (event->y > 0) {
        Key_Event(K_MWHEELUP, true, event->timestamp);
        Key_Event(K_MWHEELUP, false, event->timestamp);
    } else if (event->y < 0) {
        Key_Event(K_MWHEELDOWN, true, event->timestamp);
        Key_Event(K_MWHEELDOWN, false, event->timestamp);
    }
}

/*
============
VID_PumpEvents
============
*/
void VID_PumpEvents(void)
{
    SDL_Event    event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            Com_Quit(NULL, ERR_DISCONNECT);
            break;
        case SDL_WINDOWEVENT:
            window_event(&event.window);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            key_event(&event.key);
            break;
        case SDL_MOUSEMOTION:
            UI_MouseEvent(event.motion.x, event.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            mouse_button_event(&event.button);
            break;
        case SDL_MOUSEWHEEL:
            mouse_wheel_event(&event.wheel);
            break;
        }
    }
}

/*
===============================================================================

MOUSE

===============================================================================
*/

bool IN_GetMouseMotion(int *dx, int *dy)
{
    if (!SDL_GetRelativeMouseMode()) {
        return false;
    }
    SDL_GetRelativeMouseState(dx, dy);
    return true;
}

void IN_WarpMouse(int x, int y)
{
    SDL_WarpMouseInWindow(sdl_window, x, y);
    SDL_GetRelativeMouseState(NULL, NULL);
}

void IN_ShutdownMouse(void)
{
    SDL_SetWindowGrab(sdl_window, SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
}

bool IN_InitMouse(void)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        return false;
    }

    Com_Printf("SDL mouse initialized.\n");
    return true;
}

void IN_GrabMouse(bool grab)
{
    SDL_SetWindowGrab(sdl_window, grab);
    SDL_SetRelativeMouseMode(grab && !(Key_GetDest() & KEY_MENU));
    SDL_GetRelativeMouseState(NULL, NULL);
    SDL_ShowCursor(!(sdl_flags & QVF_FULLSCREEN));
}

#endif