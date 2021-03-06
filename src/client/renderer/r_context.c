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

#include <SDL/SDL.h>

#include "r_local.h"

r_context_t r_context;

/*
 * @brief
 */
static void R_SetIcon(void) {
	SDL_Surface *surf;

	if (!Img_LoadImage("pics/icon", &surf))
		return;

	SDL_WM_SetIcon(surf, NULL);

	SDL_FreeSurface(surf);
}

/*
 * @brief Initialize the OpenGL context, returning true on success, false on failure.
 */
void R_InitContext(void) {
	r_pixel_t w, h;
	uint32_t flags;
	SDL_Surface *surface;

	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			Com_Error(ERR_FATAL, "%s\n", SDL_GetError());
		}
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	const int32_t s = Clamp(r_multisample->integer, 0, 8);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, s ? 1 : 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, s);

	const int32_t i = Clamp(r_swap_interval->integer, 0, 2);

	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, i);

	const vec_t g = Clamp(r_gamma->value, 0.1, 3.0);

	SDL_SetGamma(g, g, g);

	flags = SDL_OPENGL;

	if (r_fullscreen->integer) {
		w = r_width->integer > 0 ? r_width->integer : 0;
		h = r_height->integer > 0 ? r_height->integer : 0;

		flags |= SDL_FULLSCREEN;
	} else {
		w = r_windowed_width->integer > 0 ? r_windowed_width->integer : 0;
		h = r_windowed_height->integer > 0 ? r_windowed_height->integer : 0;

		flags |= SDL_RESIZABLE;
	}

	if ((surface = SDL_SetVideoMode(w, h, 0, flags)) == NULL) {
		if (r_context.width && r_context.height) {
			Com_Warn("Failed to set video mode: %s\n", SDL_GetError());
			return;
		}
		Com_Error(ERR_FATAL, "Failed to set video mode: %s\n", SDL_GetError());
	}

	r_context.width = surface->w;
	r_context.height = surface->h;

	r_context.fullscreen = r_fullscreen->integer;

	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r_context.red_bits);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &r_context.green_bits);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &r_context.blue_bits);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &r_context.alpha_bits);

	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &r_context.stencil_bits);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &r_context.depth_bits);
	SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &r_context.double_buffer);

	SDL_WM_SetCaption("Quake2World", "Quake2World");

	// don't show SDL cursor because the game will draw one
	SDL_ShowCursor(false);

	SDL_EnableUNICODE(1);

	R_SetIcon();
}

/*
 * @brief
 */
void R_ShutdownContext(void) {
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
