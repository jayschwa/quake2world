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

#include <unistd.h>

#include "qbsp.h"
#include "pak.h"


static char paths[MAX_PAK_ENTRIES][MAX_QPATH];
static int num_paths;


/*
AddPath

Adds the specified path to the resouce list, after ensuring
that it is unique.
*/
static void AddPath(char *path){
	int i;

	for(i = 0; i < num_paths; i++)
		if(!strncmp(paths[i], path, MAX_QPATH))
			return;

	if(num_paths == MAX_PAK_ENTRIES){
		Error("MAX_PAK_ENTRIES exhausted\n");
		return;
	}

	Debug("AddPath: %s\n", path);
	strncpy(paths[num_paths++], path, MAX_QPATH);
}


/*
AddSound

Attempts the add the specified sound.
*/
static void AddSound(char *sound){
	char path[MAX_QPATH];

	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "sounds/%s", sound);

	if(!strstr(path, ".wav"))
		strcat(path, ".wav");

	AddPath(path);
}


#define NUM_IMAGE_FORMATS 5
static const char *image_formats[NUM_IMAGE_FORMATS] = {"tga", "png", "jpg", "pcx", "wal"};

/*
AddImage

Attempts to add the specified image in any available format.
*/
static void AddImage(const char *image){
	char path[MAX_QPATH];
	FILE *f;
	int i;

	memset(path, 0, sizeof(path));

	for(i = 0; i < NUM_IMAGE_FORMATS; i++){

		snprintf(path, sizeof(path), "%s.%s", image, image_formats[i]);

		if(Fs_OpenFile(path, &f, FILE_READ) > 0){
			AddPath(path);
			Fs_CloseFile(f);
			break;
		}
	}
}


#define NUM_MODEL_FORMATS 2
static char *model_formats[NUM_MODEL_FORMATS] = {"md3", "md2"};

/*
AddModel

Attempts to add the specified mesh model.
*/
static void AddModel(char *model){
	char path[MAX_QPATH];
	FILE *f;
	int i;

	if(model[0] == '*')  // bsp submodel
		return;

	memset(path, 0, sizeof(path));

	for(i = 0; i < NUM_MODEL_FORMATS; i++){

		snprintf(path, sizeof(path), "models/%s/tris.%s", model, model_formats[i]);

		if(Fs_OpenFile(path, &f, FILE_READ) > 0){
			AddPath(path);
			Fs_CloseFile(f);
			break;
		}
	}

	snprintf(path, sizeof(path), "models/%s/skin", model);
	AddImage(path);

	snprintf(path, sizeof(path), "models/%s/world.cfg", model);
	AddPath(path);
}


static char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
static qboolean added_sky = false;

/*
AddSky
*/
static void AddSky(char *sky){
	int i;

	for(i = 0; i < 6; i++)
		AddImage(va("env/%s%s", sky, suf[i]));

	added_sky = true;
}


/*
AddAnimation
*/
static void AddAnimation(char *name, int count){
	int i, j, k;

	Debug("Adding %d frames for %s\n", count, name);

	j = strlen(name);

	if((i = atoi(&name[j - 1])) < 0)
		return;

	name[j - 1] = 0;

	for(k = 1, i = i + 1; k < count; k++, i++){
		const char *c = va("%s%d", name, i);
		AddImage(c);
	}
}


/*
AddMaterials

Adds all resources specified by the materials file, and the materials
file itself.  See src/r_material.c for materials reference.
*/
static void AddMaterials(void){
	char path[MAX_QPATH];
	char *buffer;
	const char *buf;
	const char *c;
	char texture[MAX_QPATH];
	int i, num_frames;

	// load the materials file
	snprintf(path, sizeof(path), "materials/%s", Com_Basename(bspname));
	strcpy(path + strlen(path) - 3, "mat");

	if((i = Fs_LoadFile(path, (void **)(char *)&buffer)) < 1){
		Print("Couldn't load %s\n", path);
		return;
	}

	AddPath(path);  // add the materials file itself

	buf = buffer;

	num_frames = 0;
	memset(texture, 0, sizeof(texture));

	while(true){

		c = Com_Parse(&buf);

		if(!strlen(c))
			break;

		// texture references should all be added
		if(!strcmp(c, "texture")){
			snprintf(texture, sizeof(texture), "textures/%s", Com_Parse(&buf));
			AddImage(texture);
			continue;
		}

		// and custom envmaps
		if(!strcmp(c, "envmap")){

			c = Com_Parse(&buf);
			i = atoi(c);

			if(i == -1){  // custom, add it
				snprintf(texture, sizeof(texture), "envmaps/%s", c);
				AddImage(texture);
			}
			continue;
		}

		// and custom flares
		if(!strcmp(c, "flare")){

			c = Com_Parse(&buf);
			i = atoi(c);

			if(i == -1){  // custom, add it
				snprintf(texture, sizeof(texture), "flares/%s", c);
				AddImage(texture);
			}
			continue;
		}

		if(!strcmp(c, "anim")){
			num_frames = atoi(Com_Parse(&buf));
			Com_Parse(&buf);  // read fps
			continue;
		}

		if(*c == '}'){

			if(num_frames)  // add animation frames
				AddAnimation(texture, num_frames);

			num_frames = 0;
			continue;
		}
	}

	Fs_FreeFile(buffer);
}


/*
AddLocation
*/
static void AddLocation(void){
	char base[MAX_QPATH];

	Com_StripExtension(bspname, base);
	AddPath(va("%s.loc", base));
}


/*
AddDocumentation
*/
static void AddDocumentation(void){
	char base[MAX_OSPATH];
	char *c;

	Com_StripExtension(bspname, base);

	c = strstr(base, "maps/");
	c = c ? c + 5 : base;

	AddPath(va("docs/map-%s.txt", c));
}


/*
GetPakfile

Returns a suitable pakfile name for the current bsp name, e.g.

maps/my.bsp -> map-my.pak.
*/
static pak_t *GetPakfile(void){
	char base[MAX_OSPATH];
	char pakfile[MAX_OSPATH];
	const char *c;
	pak_t *pak;

	Com_StripExtension(bspname, base);

	c = strstr(base, "maps/");
	c = c ? c + 5 : base;

	snprintf(pakfile, sizeof(pakfile), "%s/map-%s-%d.pak", Fs_Gamedir(), c, getpid());

	if(!(pak = Pak_CreatePakstream(pakfile)))
		Error("Failed to open %s\n", pakfile);

	return pak;
}


/*
PAK_Main

Loads the specified BSP file, resolves all resources referenced by it,
and generates a new pak archive for the project.  This is a very inefficient
but straightforward implementation.
*/
int PAK_Main(void){
	time_t start, end;
	int total_pak_time;
	int i, j;
	epair_t *e;
	pak_t *pak;
	void *p;

	#ifdef _WIN32
		char title[MAX_OSPATH];
		sprintf(title, "Q2WMap [Compiling PAK]");
		SetConsoleTitle(title);
	#endif

	Print("\n----- PAK -----\n\n");

	start = time(NULL);

	LoadBSPFile(bspname);

	// add the textures and normalmaps
	for(i = 0; i < numtexinfo; i++){
		AddImage(va("textures/%s", texinfo[i].texture));
		AddImage(va("textures/%s_nm", texinfo[i].texture));
		AddImage(va("textures/%s_norm", texinfo[i].texture));
		AddImage(va("textures/%s_local", texinfo[i].texture));
	}

	Print("Resolved %d textures, ", num_paths);
	j = num_paths;

	// and the materials
	AddMaterials();

	Print("%d materials, ", num_paths - j);
	j = num_paths;

	// and the sounds
	ParseEntities();

	for(i = 0; i < num_entities; i++){
		e = entities[i].epairs;
		while(e){

			if(!strncmp(e->key, "noise", 5) || !strncmp(e->key, "sound", 5))
				AddSound(e->value);
			else if(!strncmp(e->key, "model", 5))
				AddModel(e->value);
			else if(!strncmp(e->key, "sky", 3))
				AddSky(e->value);

			e = e->next;
		}
	}

	if(added_sky)
		j += 6;

	Print("%d sounds.\n", num_paths - j);

	// add location and docs
	AddLocation();
	AddDocumentation();

	// and of course the bsp and map
	AddPath(bspname);
	AddPath(mapname);

	pak = GetPakfile();
	Print("Paking %d resources to %s..\n", num_paths, pak->filename);

	for(i = 0; i < num_paths; i++){

		j = Fs_LoadFile(paths[i], (void **)(char *)&p);

		if(j == -1){
			Print("Couldn't open %s\n", paths[i]);
			continue;
		}

		Pak_AddEntry(pak, paths[i], j, p);
		Z_Free(p);
	}

	Pak_ClosePakstream(pak);

	end = time(NULL);
	total_pak_time = (int)(end - start);
	Print("\nPAK Time: ");
	if(total_pak_time > 59)
		Print("%d Minutes ", total_pak_time / 60);
	Print("%d Seconds\n", total_pak_time % 60);

	return 0;
}
