/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "sys.h"
#include "filesystem.h"

#include <signal.h>
#include <sys/time.h>

#ifdef _WIN32
#define RTLD_NOW 0
#define dlopen(file_name, mode) LoadLibrary(file_name)
#define dlerror() "Windows.. go figure."
#define dlsym(handle, symbol) GetProcAddress(handle, symbol)
#define dlclose(handle) FreeLibrary(handle)

#else
#include <dlfcn.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#define MAX_BACKTRACE_SYMBOLS 50
#endif

/*
 * @return Milliseconds since Quake execution began.
 */
uint32_t Sys_Milliseconds(void) {
	static uint32_t base;
	GTimeVal time;

	g_get_current_time(&time);

	if (!base)
		base = time.tv_sec;

	return (time.tv_sec - base) * 1000 + time.tv_usec / 1000;
}

/*
 * @return The current executable path (argv[0]).
 */
const char *Sys_ExecutablePath(void) {
	static char path[MAX_OSPATH];

#ifdef __APPLE__
	uint32_t i = sizeof(path);

	if (_NSGetExecutablePath(path, &i) > -1) {
		return path;
	}

#elif __linux__

	if (readlink(va("/proc/%d/exe", getpid()), path, sizeof(path)) > -1) {
		return path;
	}

#elif _WIN32

	if (GetModuleFileName(0, path, sizeof(path))) {
		return path;
	}

#endif

	Com_Warn("Failed to resolve executable path\n");
	return NULL;
}

/*
 * @return The current user's name.
 */
const char *Sys_Username(void) {
	return g_get_user_name();
}

/*
 * @brief Returns the current user's home directory.
 */
const char *Sys_UserDir(void) {
	static char user_dir[MAX_OSPATH];
	const char *home = g_get_home_dir();

#ifdef _WIN32
	g_snprintf(user_dir, sizeof(user_dir), "%s\\My Games\\Quake2World", home);
#else
	g_snprintf(user_dir, sizeof(user_dir), "%s/.quake2world", home);
#endif
	return user_dir;
}

/*
 * @brief Closes an open game module.
 */
void Sys_CloseLibrary(void **handle) {
	if (*handle)
		dlclose(*handle);
	*handle = NULL;
}

/*
 * @brief
 */
void Sys_OpenLibrary(const char *name, void **handle) {
	*handle = NULL;

#ifdef _WIN32
	char *so_name = va("%s.dll", name);
#else
	char *so_name = va("%s.so", name);
#endif

	if (Fs_Exists(so_name)) {
		char path[MAX_OSPATH];

		g_snprintf(path, sizeof(path), "%s/%s", Fs_RealDir(so_name), so_name);
		Com_Print("Trying %s...\n", path);

		if ((*handle = dlopen(path, RTLD_NOW)))
			return;

		Com_Error(ERR_DROP, "%s\n", dlerror());
	}

	Com_Error(ERR_DROP, "Couldn't find %s\n", so_name);
}

/*
 * @brief Opens and loads the specified shared library. The function identified by
 * entry_point is resolved and invoked with the specified parameters, its
 * return value returned by this function.
 */
void *Sys_LoadLibrary(const char *name, void **handle, const char *entry_point, void *params) {
	typedef void *EntryPointFunc(void *);
	EntryPointFunc *EntryPoint;

	if (*handle) {
		Com_Warn("%s: handle already open\n", name);
		Sys_CloseLibrary(handle);
	}

	Sys_OpenLibrary(name, handle);

	EntryPoint = (EntryPointFunc *) dlsym(*handle, entry_point);

	if (!EntryPoint) {
		Sys_CloseLibrary(handle);
		Com_Error(ERR_DROP, "%s: Failed to resolve %s\n", name, entry_point);
	}

	return EntryPoint(params);
}

/*
 * @brief On platforms supporting it, print a backtrace.
 */
void Sys_Backtrace(void) {
#ifdef HAVE_EXECINFO
	void *symbols[MAX_BACKTRACE_SYMBOLS];
	int32_t i;

	i = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);
	backtrace_symbols_fd(symbols, i, STDERR_FILENO);

	fflush(stderr);
#endif
}

/*
 * @brief Catch kernel interrupts and dispatch the appropriate exit routine.
 */
void Sys_Signal(int32_t s) {

	switch (s) {
		case SIGINT:
		case SIGTERM:
#ifndef _WIN32
		case SIGHUP:
		case SIGQUIT:
#endif
			Com_Shutdown("Received signal %d, quitting...\n", s);
			break;
		default:
			Com_Error(ERR_FATAL, "Received signal %d\n", s);
			break;
	}
}
