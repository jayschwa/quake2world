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

#ifndef __IMAGES_H__
#define __IMAGES_H__

#include "filesystem.h"

#ifdef BUILD_CLIENT
#include <SDL/SDL_image.h>

// 8bit palette for wal images and particles
extern uint32_t palette[256];

bool Img_LoadImage(const char *name, SDL_Surface **surf);
bool Img_LoadTypedImage(const char *name, const char *type, SDL_Surface **surf);
void Img_InitPalette(void);
void Img_ColorFromPalette(byte c, float *res);
void Img_WriteJPEG(const char *path, byte *img_data, int32_t width, int32_t height, int32_t quality);
void Img_WriteTGARLE(const char *path, byte *img_data, int32_t width, int32_t height, int32_t quality);

#endif /* BUILD_CLIENT */
#endif /*__IMAGES_H__*/
