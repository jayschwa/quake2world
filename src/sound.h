/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

typedef struct {
	int length;
	int loopstart;
	int rate;  // not needed, because converted on load?
	int width;
	int stereo;
	byte data[1];  // variable sized
} sfxcache_t;

typedef struct sfx_s {
	char name[MAX_QPATH];
	sfxcache_t *cache;
	char *truename;
} sfx_t;

// a playsound_t will be generated by each call to S_StartSound,
// when the mixer reaches playsound->begin, the playsound will
// be assigned to a channel
typedef struct playsound_s {
	struct playsound_s *prev, *next;
	sfx_t *sfx;
	float volume;
	float attenuation;
	int entnum;
	int entchannel;
	qboolean fixed_origin;  // use origin field instead of entnum's origin
	vec3_t origin;
	unsigned begin;  // begin on this sample
} playsound_t;

typedef struct {
	int channels;
	int samples;  // mono samples in buffer
	int chunk;  // don't mix less than this #
	int offset;  // in mono samples
	int bits;
	int rate;
	byte *buffer;
} dma_t;

extern dma_t dma;

typedef struct {
	sfx_t *sfx;  // sfx number
	int leftvol;  // 0-255 volume
	int rightvol;  // 0-255 volume
	int end;  // end time in global paintsamples
	int pos;  // sample position in sfx
	int looping;  // where to loop, -1 = no looping OBSOLETE?
	int entnum;  // to allow overriding a specific sound
	int entchannel;  //
	vec3_t origin;  // only use if fixed_origin is set
	vec_t dist_mult;  // distance multiplier(attenuation/clipK)
	int master_vol;  // 0-255 master volume
	qboolean fixed_origin;  // use origin instead of fetching entnum's origin
	qboolean autosound;  // from an entity->sound, cleared each frame
} channel_t;

#define MAX_CHANNELS 32
extern channel_t channels[MAX_CHANNELS];

typedef struct {
	int rate;
	int width;
	int channels;
	int loopstart;
	int samples;
	int dataofs;  // chunk starts this many bytes from file start
} wavinfo_t;

extern int s_paintedtime;
extern int s_rawend;

extern playsound_t s_pendingplays;

extern cvar_t *s_channels;
extern cvar_t *s_mixahead;
extern cvar_t *s_rate;
extern cvar_t *s_testsound;
extern cvar_t *s_volume;

// s_device.c
qboolean S_InitDevice(void);
void S_ShutdownDevice(void);

// s_main.c
void S_Init(void);
void S_Shutdown(void);
void S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx,
		float volume, float attenuation, float timeofs);
void S_StartLocalSound(char *s);
void S_AddLoopSound(vec3_t org, sfx_t *sfx);
void S_Update(void);
sfx_t *S_LoadSound(const char *name);
void S_LoadSounds(void);
void S_IssuePlaysound(playsound_t *ps);

// s_mix.c
void S_InitScaletable(void);
void S_PaintChannels(int endtime);

// s_sample.c
sfxcache_t *S_LoadSfx(sfx_t *s);