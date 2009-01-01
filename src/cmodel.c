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

#include "common.h"

typedef struct {
	cplane_t *plane;
	int children[2];  // negative numbers are leafs
} cnode_t;

typedef struct {
	cplane_t *plane;
	csurface_t *surface;
} cbrushside_t;

typedef struct {
	int contents;
	int cluster;
	int area;
	unsigned short firstleafbrush;
	unsigned short numleafbrushes;
} cleaf_t;

typedef struct {
	int contents;
	int numsides;
	int firstbrushside;
	int checkcount;  // to avoid repeated testings
} cbrush_t;

typedef struct {
	int numareaportals;
	int firstareaportal;
	int floodnum;  // if two areas have equal floodnums, they are connected
	int floodvalid;
} carea_t;

int checkcount;

static char map_name[MAX_QPATH];

static int numbrushsides;
static cbrushside_t map_brushsides[MAX_BSP_BRUSHSIDES];

static int numtexinfo;
static csurface_t csurfaces[MAX_BSP_TEXINFO];

static int numplanes;
static cplane_t map_planes[MAX_BSP_PLANES + 6];  // extra for box hull

static int numnodes;
static cnode_t map_nodes[MAX_BSP_NODES + 6];  // extra for box hull

static int numleafs = 1;  // allow leaf funcs to be called without a map
static cleaf_t map_leafs[MAX_BSP_LEAFS];
static int emptyleaf, solidleaf;

static int numleafbrushes;
static unsigned short map_leafbrushes[MAX_BSP_LEAFBRUSHES];

int numcmodels;
cmodel_t map_cmodels[MAX_BSP_MODELS];

static int numbrushes;
static cbrush_t map_brushes[MAX_BSP_BRUSHES];

static int numvisibility;
static byte map_visibility[MAX_BSP_VISIBILITY];
static dvis_t *map_vis = (dvis_t *)map_visibility;

int numentitychars;
char map_entitystring[MAX_BSP_ENTSTRING];

static int numareas = 1;
static carea_t map_areas[MAX_BSP_AREAS];

int numareaportals;
dareaportal_t map_areaportals[MAX_BSP_AREAPORTALS];

static int numclusters = 1;

static csurface_t nullsurface;

static int floodvalid;

static qboolean portalopen[MAX_BSP_AREAPORTALS];

static cvar_t *map_noareas;

static void Cm_InitBoxHull(void);
static void Cm_FloodAreaConnections(void);


int c_pointcontents;
int c_traces, c_brush_traces;


/*
 *
 * MAP LOADING
 *
 */

static const byte *cmod_base;

/*
 * Cm_LoadSubmodels
 */
static void Cm_LoadSubmodels(const lump_t *l){
	const dbspmodel_t *in;
	int i, j, count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Map with no models.");
	}
	if(count > MAX_BSP_MODELS){
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Map has too many models.");
	}

	numcmodels = count;

	for(i = 0; i < count; i++, in++){
		cmodel_t *out = &map_cmodels[i];

		for(j = 0; j < 3; j++){  // spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		out->headnode = LittleLong(in->headnode);
	}
}


/*
 * Cm_LoadSurfaces
 */
static void Cm_LoadSurfaces(const lump_t *l){
	const dtexinfo_t *in;
	csurface_t *out;
	int i, count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Funny lump size.");
		return;
	}
	count = l->filelen / sizeof(*in);
	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Map with no surfaces.");
	}
	if(count > MAX_BSP_TEXINFO){
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Map has too many surfaces.");
	}
	numtexinfo = count;
	out = csurfaces;

	for(i = 0; i < count; i++, in++, out++){
		strncpy(out->name, in->texture, sizeof(out->name) - 1);
		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);
	}
}


/*
 * Cm_LoadNodes
 */
static void Cm_LoadNodes(const lump_t *l){
	const dnode_t *in;
	int child;
	cnode_t *out;
	int i, j, count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadNodes: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadNodes: Map has no nodes.");
	}
	if(count > MAX_BSP_NODES){
		Com_Error(ERR_DROP, "Cm_LoadNodes: Map has too many nodes.");
	}

	out = map_nodes;

	numnodes = count;

	for(i = 0; i < count; i++, out++, in++){
		out->plane = map_planes + LittleLong(in->planenum);
		for(j = 0; j < 2; j++){
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}
}


/*
 * Cm_LoadBrushes
 */
static void Cm_LoadBrushes(const lump_t *l){
	const dbrush_t *in;
	cbrush_t *out;
	int i, count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadBrushes: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	if(count > MAX_BSP_BRUSHES){
		Com_Error(ERR_DROP, "Cm_LoadBrushes: Map has too many brushes.");
	}

	out = map_brushes;

	numbrushes = count;

	for(i = 0; i < count; i++, out++, in++){
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
	}
}


/*
 * Cm_LoadLeafs
 */
static void Cm_LoadLeafs(const lump_t *l){
	int i;
	cleaf_t *out;
	const dleaf_t *in;
	int count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map with no leafs.");
	}
	// need to save space for box planes
	if(count > MAX_BSP_PLANES){
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map has too many planes.");
	}

	out = map_leafs;
	numleafs = count;
	numclusters = 0;

	for(i = 0; i < count; i++, in++, out++){
		out->contents = LittleLong(in->contents);
		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);
		out->firstleafbrush = LittleShort(in->firstleafbrush);
		out->numleafbrushes = LittleShort(in->numleafbrushes);

		if(out->cluster >= numclusters)
			numclusters = out->cluster + 1;
	}

	if(map_leafs[0].contents != CONTENTS_SOLID){
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map leaf 0 is not CONTENTS_SOLID.");
	}
	solidleaf = 0;
	emptyleaf = -1;
	for(i = 1; i < numleafs; i++){
		if(!map_leafs[i].contents){
			emptyleaf = i;
			break;
		}
	}
	if(emptyleaf == -1)
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map does not have an empty leaf.");
}


/*
 * Cm_LoadPlanes
 */
static void Cm_LoadPlanes(const lump_t *l){
	int i, j;
	cplane_t *out;
	const dplane_t *in;
	int count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Map with no planes.");
	}
	// need to save space for box planes
	if(count > MAX_BSP_PLANES){
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Map has too many planes.");
	}

	out = map_planes;
	numplanes = count;

	for(i = 0; i < count; i++, in++, out++){
		int bits = 0;
		for(j = 0; j < 3; j++){
			out->normal[j] = LittleFloat(in->normal[j]);
			if(out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}


/*
 * Cm_LoadLeafBrushes
 */
static void Cm_LoadLeafBrushes(const lump_t *l){
	int i;
	unsigned short *out;
	const unsigned short *in;
	int count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadLeafBrushes: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadLeafBrushes: Map with no planes.");
	}
	// need to save space for box planes
	if(count > MAX_BSP_LEAFBRUSHES){
		Com_Error(ERR_DROP, "Cm_LoadLeafBrushes: Map has too many leafbrushes.");
	}

	out = map_leafbrushes;
	numleafbrushes = count;

	for(i = 0; i < count; i++, in++, out++)
		*out = LittleShort(*in);
}


/*
 * Cm_LoadBrushSides
 */
static void Cm_LoadBrushSides(const lump_t *l){
	int i;
	cbrushside_t *out;
	const dbrushside_t *in;
	int count;
	int num;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadBrushSides: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	// need to save space for box planes
	if(count > MAX_BSP_BRUSHSIDES){
		Com_Error(ERR_DROP, "Cm_LoadBrushSides: Map has too many planes.");
	}

	out = map_brushsides;
	numbrushsides = count;

	for(i = 0; i < count; i++, in++, out++){
		num = LittleShort(in->planenum);
		out->plane = &map_planes[num];
		num = LittleShort(in->surfnum);
		if(num >= numtexinfo){
			Com_Error(ERR_DROP, "Cm_LoadBrushSides: Bad brushside surfnum.");
		}
		out->surface = &csurfaces[num];
	}
}


/*
 * Cm_LoadAreas
 */
static void Cm_LoadAreas(const lump_t *l){
	int i;
	carea_t *out;
	const darea_t *in;
	int count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "MOD_LoadAreas: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	if(count > MAX_BSP_AREAS){
		Com_Error(ERR_DROP, "Cm_LoadAreas: Map has too many areas.");
	}

	out = map_areas;
	numareas = count;

	for(i = 0; i < count; i++, in++, out++){
		out->numareaportals = LittleLong(in->numareaportals);
		out->firstareaportal = LittleLong(in->firstareaportal);
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}


/*
 * Cm_LoadAreaPortals
 */
static void Cm_LoadAreaPortals(const lump_t *l){
	int i;
	dareaportal_t *out;
	const dareaportal_t *in;
	int count;

	in = (const void *)(cmod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadAreaPortals: Funny lump size.");
	}
	count = l->filelen / sizeof(*in);

	if(count > MAX_BSP_AREAS){
		Com_Error(ERR_DROP, "Cm_LoadAreaPortals: Map has too many areas.");
	}

	out = map_areaportals;
	numareaportals = count;

	for(i = 0; i < count; i++, in++, out++){
		out->portalnum = LittleLong(in->portalnum);
		out->otherarea = LittleLong(in->otherarea);
	}
}


/*
 * Cm_LoadVisibility
 */
static void Cm_LoadVisibility(const lump_t *l){
	int i;

	numvisibility = l->filelen;
	if(l->filelen > MAX_BSP_VISIBILITY){
		Com_Error(ERR_DROP, "Cm_LOadVisibility: Map has too large visibility lump.");
	}

	memcpy(map_visibility, cmod_base + l->fileofs, l->filelen);

	map_vis->numclusters = LittleLong(map_vis->numclusters);
	for(i = 0; i < map_vis->numclusters; i++){
		map_vis->bitofs[i][0] = LittleLong(map_vis->bitofs[i][0]);
		map_vis->bitofs[i][1] = LittleLong(map_vis->bitofs[i][1]);
	}
}


/*
 * Cm_LoadEntityString
 */
static void Cm_LoadEntityString(const lump_t *l){
	numentitychars = l->filelen;
	if(l->filelen > MAX_BSP_ENTSTRING){
		Com_Error(ERR_DROP, "Cm_LoadEntityString: Map has too large entity lump.");
	}

	memcpy(map_entitystring, cmod_base + l->fileofs, l->filelen);
}


/*
 * Cm_LoadMap
 *
 * Loads in the map and all submodels
 */
cmodel_t *Cm_LoadMap(const char *name, int *mapsize){
	void *buf;
	int i;
	dbspheader_t header;

	map_noareas = Cvar_Get("map_noareas", "0", 0, NULL);

	// free old stuff
	numplanes = 0;
	numnodes = 0;
	numleafs = 0;
	numcmodels = 0;
	numvisibility = 0;
	numentitychars = 0;
	map_entitystring[0] = 0;
	map_name[0] = 0;

	// if we've been asked to load a demo, just clean up and return
	if(!name){
		numleafs = numclusters = numareas = 1;
		*mapsize = 0;
		return &map_cmodels[0];
	}

	// load the file
	*mapsize = Fs_LoadFile(name, &buf);
	if(!buf){
		Com_Error(ERR_DROP, "Cm_LoadMap: Couldn't load %s.", name);
	}

	header = *(dbspheader_t *)buf;
	for(i = 0; i < sizeof(dbspheader_t) / 4; i++)
		((int *)&header)[i] = LittleLong(((int *)&header)[i]);

	if(header.version != BSP_VERSION && header.version != BSP_VERSION_){
		Fs_FreeFile(buf);
		Com_Error(ERR_DROP, "Cm_LoadMap: %s has unsupported version: %d.",
				name, header.version);
	}

	cmod_base = (const byte *)buf;

	// load into heap
	Cm_LoadSurfaces(&header.lumps[LUMP_TEXINFO]);
	Cm_LoadLeafs(&header.lumps[LUMP_LEAFS]);
	Cm_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES]);
	Cm_LoadPlanes(&header.lumps[LUMP_PLANES]);
	Cm_LoadBrushes(&header.lumps[LUMP_BRUSHES]);
	Cm_LoadBrushSides(&header.lumps[LUMP_BRUSHSIDES]);
	Cm_LoadSubmodels(&header.lumps[LUMP_MODELS]);
	Cm_LoadNodes(&header.lumps[LUMP_NODES]);
	Cm_LoadAreas(&header.lumps[LUMP_AREAS]);
	Cm_LoadAreaPortals(&header.lumps[LUMP_AREAPORTALS]);
	Cm_LoadVisibility(&header.lumps[LUMP_VISIBILITY]);
	Cm_LoadEntityString(&header.lumps[LUMP_ENTITIES]);

	Fs_FreeFile(buf);

	Cm_InitBoxHull();

	memset(portalopen, 0, sizeof(portalopen));
	Cm_FloodAreaConnections();

	strcpy(map_name, name);

	return &map_cmodels[0];
}


/*
 * Cm_InlineModel
 */
cmodel_t *Cm_InlineModel(const char *name){
	int num;

	if(!name || name[0] != '*'){
		Com_Error(ERR_DROP, "Cm_InlineModel: Bad name.");
	}
	num = atoi(name + 1);
	if(num < 1 || num >= numcmodels){
		Com_Error(ERR_DROP, "Cm_InlineModel: Bad number: %d.", num);
	}

	return &map_cmodels[num];
}

int Cm_NumClusters(void){
	return numclusters;
}

int Cm_NumInlineModels(void){
	return numcmodels;
}

char *Cm_EntityString(void){
	return map_entitystring;
}

int Cm_LeafContents(int leafnum){
	if(leafnum < 0 || leafnum >= numleafs){
		Com_Error(ERR_DROP, "Cm_LeafContents: Bad number.");
	}
	return map_leafs[leafnum].contents;
}

int Cm_LeafCluster(int leafnum){
	if(leafnum < 0 || leafnum >= numleafs){
		Com_Error(ERR_DROP, "Cm_LeafCluster: Bad number.");
	}
	return map_leafs[leafnum].cluster;
}

int Cm_LeafArea(int leafnum){
	if(leafnum < 0 || leafnum >= numleafs){
		Com_Error(ERR_DROP, "Cm_LeafArea: Bad number.");
	}
	return map_leafs[leafnum].area;
}


static cplane_t *box_planes;
static int box_headnode;
static cbrush_t *box_brush;
static cleaf_t *box_leaf;

/*
 * Cm_InitBoxHull
 *
 * Set up the planes and nodes so that the six floats of a bounding box
 * can just be stored out and get a proper clipping hull structure.
 */
static void Cm_InitBoxHull(void){
	int i;

	box_headnode = numnodes;
	box_planes = &map_planes[numplanes];
	if(numnodes + 6 > MAX_BSP_NODES
			|| numbrushes + 1 > MAX_BSP_BRUSHES
			|| numleafbrushes + 1 > MAX_BSP_LEAFBRUSHES
			|| numbrushsides + 6 > MAX_BSP_BRUSHSIDES
			|| numplanes + 12 > MAX_BSP_PLANES){
		Com_Error(ERR_DROP, "Cm_InitBoxHull: Not enough room for box tree.");
	}

	box_brush = &map_brushes[numbrushes];
	box_brush->numsides = 6;
	box_brush->firstbrushside = numbrushsides;
	box_brush->contents = CONTENTS_MONSTER;

	box_leaf = &map_leafs[numleafs];
	box_leaf->contents = CONTENTS_MONSTER;
	box_leaf->firstleafbrush = numleafbrushes;
	box_leaf->numleafbrushes = 1;

	map_leafbrushes[numleafbrushes] = numbrushes;

	for(i = 0; i < 6; i++){
		const int side = i & 1;
		cnode_t *c;
		cplane_t *p;
		cbrushside_t *s;

		// brush sides
		s = &map_brushsides[numbrushsides + i];
		s->plane = map_planes + (numplanes + i * 2 + side);
		s->surface = &nullsurface;

		// nodes
		c = &map_nodes[box_headnode + i];
		c->plane = map_planes + (numplanes + i * 2);
		c->children[side] = -1 - emptyleaf;
		if(i != 5)
			c->children[side ^ 1] = box_headnode + i + 1;
		else
			c->children[side ^ 1] = -1 - numleafs;

		// planes
		p = &box_planes[i * 2];
		p->type = i >> 1;
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &box_planes[i * 2 + 1];
		p->type = PLANE_ANYX + (i >> 1);
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = -1;
	}
}


/*
 * Cm_HeadnodeForBox
 *
 * To keep everything totally uniform, bounding boxes are turned into small
 * BSP trees instead of being compared directly.
 */
int Cm_HeadnodeForBox(const vec3_t mins, const vec3_t maxs){
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	return box_headnode;
}


/*
 * Cm_PointLeafnum
 */
static int Cm_PointLeafnum_r(const vec3_t p, int num){

	while(num >= 0){
		float d;
		cnode_t *node = map_nodes + num;
		cplane_t *plane = node->plane;

		if(AXIAL(plane))
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct(plane->normal, p) - plane->dist;

		if(d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++;  // optimize counter

	return -1 - num;
}

int Cm_PointLeafnum(const vec3_t p){
	if(!numplanes)
		return 0;  // sound may call this without map loaded
	return Cm_PointLeafnum_r(p, 0);
}


/*
 * Cm_BoxLeafnums
 *
 * Fills in a list of all the leafs touched
 */
static int leaf_count, leaf_maxcount;
static int *leaf_list;
static const float *leaf_mins, *leaf_maxs;
static int leaf_topnode;

static void Cm_BoxLeafnums_r(int nodenum){
	cplane_t *plane;
	cnode_t *node;
	int s;

	while(true){
		if(nodenum < 0){
			if(leaf_count >= leaf_maxcount){
				return;
			}
			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}

		node = &map_nodes[nodenum];
		plane = node->plane;
		s = BOX_ON_PLANE_SIDE(leaf_mins, leaf_maxs, plane);
		if(s == 1)
			nodenum = node->children[0];
		else if(s == 2)
			nodenum = node->children[1];
		else {  // go down both
			if(leaf_topnode == -1)
				leaf_topnode = nodenum;
			Cm_BoxLeafnums_r(node->children[0]);
			nodenum = node->children[1];
		}
	}
}

static int Cm_BoxLeafnums_headnode(const vec3_t mins, const vec3_t maxs, int *list, int listsize, int headnode, int *topnode){
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	Cm_BoxLeafnums_r(headnode);

	if(topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}

int Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *topnode){
	return Cm_BoxLeafnums_headnode(mins, maxs, list, listsize, map_cmodels[0].headnode, topnode);
}


/*
 * Cm_PointContents
 */
int Cm_PointContents(const vec3_t p, int headnode){
	int l;

	if(!numnodes)  // map not loaded
		return 0;

	l = Cm_PointLeafnum_r(p, headnode);

	return map_leafs[l].contents;
}


/*
 * Cm_TransformedPointContents
 *
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
int Cm_TransformedPointContents(const vec3_t p, int headnode, const vec3_t origin, const vec3_t angles){
	vec3_t p_l;
	vec3_t temp;
	vec3_t forward, right, up;
	int l;

	// subtract origin offset
	VectorSubtract(p, origin, p_l);

	// rotate start and end into the models frame of reference
	if(headnode != box_headnode &&
			(angles[0] || angles[1] || angles[2])){
		AngleVectors(angles, forward, right, up);

		VectorCopy(p_l, temp);
		p_l[0] = DotProduct(temp, forward);
		p_l[1] = -DotProduct(temp, right);
		p_l[2] = DotProduct(temp, up);
	}

	l = Cm_PointLeafnum_r(p_l, headnode);

	return map_leafs[l].contents;
}


/*
 *
 * BOX TRACING
 *
 */

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(0.03125)

static vec3_t trace_start, trace_end;
static vec3_t trace_mins, trace_maxs;
static vec3_t trace_extents;

static trace_t trace_trace;
static int trace_contents;
static qboolean trace_ispoint;  // optimized case


/*
 * Cm_ClipBoxToBrush
 *
 * Clips the bounded box to all brush sides for the given brush.  Returns
 * true if the box was clipped, false otherwise.
 */
static void Cm_ClipBoxToBrush(vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
			trace_t *trace, cleaf_t *leaf, cbrush_t *brush){
	int i, j;
	cplane_t *clipplane;
	float dist;
	float enterfrac, leavefrac;
	vec3_t ofs;
	float d1, d2;
	qboolean getout, startout;
	cbrushside_t *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if(!brush->numsides)
		return;

	c_brush_traces++;

	getout = startout = false;
	leadside = NULL;

	for(i = 0; i < brush->numsides; i++){
		cbrushside_t *side = &map_brushsides[brush->firstbrushside + i];
		cplane_t *plane = side->plane;

		// FIXME: special case for axial

		if(!trace_ispoint){  // general box case

			// push the plane out apropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for(j = 0; j < 3; j++){
				if(plane->normal[j] < 0.0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct(ofs, plane->normal);
			dist = plane->dist - dist;
		} else {  // special point case
			dist = plane->dist;
		}

		d1 = DotProduct(p1, plane->normal) - dist;
		d2 = DotProduct(p2, plane->normal) - dist;

		if(d2 > 0.0)
			getout = true;  // endpoint is not in solid
		if(d1 > 0.0)
			startout = true;

		// if completely in front of face, no intersection
		if(d1 > 0.0 && d2 >= d1)
			return;

		if(d1 <= 0.0 && d2 <= 0.0)
			continue;

		// crosses face
		if(d1 > d2){  // enter
			const float f = (d1 - DIST_EPSILON) / (d1 - d2);
			if(f > enterfrac){
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		} else {  // leave
			const float f = (d1 + DIST_EPSILON) / (d1 - d2);
			if(f < leavefrac)
				leavefrac = f;
		}
	}

	if(!startout){  // original point was inside brush
		trace->startsolid = true;
		if(!getout)
			trace->allsolid = true;
		trace->leafnum = leaf - map_leafs;
	}

	if(enterfrac < leavefrac){  // pierced brush
		if(enterfrac > -1.0 && enterfrac < trace->fraction){
			if(enterfrac < 0.0)
				enterfrac = 0.0;
			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = leadside->surface;
			trace->contents = brush->contents;
			trace->leafnum = leaf - map_leafs;
		}
	}
}


/*
 * Cm_TestBoxInBrush
 */
static void Cm_TestBoxInBrush(vec3_t mins, vec3_t maxs, vec3_t p1, trace_t *trace, cbrush_t *brush){
	int i, j;
	cplane_t *plane;
	float dist;
	vec3_t ofs;
	float d1;
	cbrushside_t *side;

	if(!brush->numsides)
		return;

	for(i = 0; i < brush->numsides; i++){
		side = &map_brushsides[brush->firstbrushside + i];
		plane = side->plane;

		// FIXME: special case for axial

		// general box case

		// push the plane out apropriately for mins/maxs

		// FIXME: use signbits into 8 way lookup for each mins/maxs
		for(j = 0; j < 3; j++){
			if(plane->normal[j] < 0.0)
				ofs[j] = maxs[j];
			else
				ofs[j] = mins[j];
		}
		dist = DotProduct(ofs, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct(p1, plane->normal) - dist;

		// if completely in front of face, no intersection
		if(d1 > 0.0)
			return;
	}

	// inside this brush
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0.0;
	trace->contents = brush->contents;
}


/*
 * Cm_TraceToLeaf
 */
static void Cm_TraceToLeaf(int leafnum){
	int k;
	cleaf_t *leaf;

	leaf = &map_leafs[leafnum];

	if(!(leaf->contents & trace_contents))
		return;

	// trace line against all brushes in the leaf
	for(k = 0; k < leaf->numleafbrushes; k++){
		const int brushnum = map_leafbrushes[leaf->firstleafbrush + k];
		cbrush_t *b = &map_brushes[brushnum];

		if(b->checkcount == checkcount)
			continue;  // already checked this brush in another leaf

		b->checkcount = checkcount;

		if(!(b->contents & trace_contents))
			continue;

		Cm_ClipBoxToBrush(trace_mins, trace_maxs, trace_start, trace_end,
				&trace_trace, leaf, b);

		if(trace_trace.allsolid)
			return;
	}
}


/*
 * Cm_TestInLeaf
 */
static void Cm_TestInLeaf(int leafnum){
	int k;
	cleaf_t *leaf;

	leaf = &map_leafs[leafnum];
	if(!(leaf->contents & trace_contents))
		return;

	// trace line against all brushes in the leaf
	for(k = 0; k < leaf->numleafbrushes; k++){
		const int brushnum = map_leafbrushes[leaf->firstleafbrush + k];
		cbrush_t *b = &map_brushes[brushnum];
		if(b->checkcount == checkcount)
			continue;  // already checked this brush in another leaf
		b->checkcount = checkcount;

		if(!(b->contents & trace_contents))
			continue;
		Cm_TestBoxInBrush(trace_mins, trace_maxs, trace_start, &trace_trace, b);
		if(trace_trace.allsolid)
			return;
	}
}


/*
 * Cm_RecursiveHullCheck
 */
static void Cm_RecursiveHullCheck(int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2){
	const cnode_t *node;
	const cplane_t *plane;
	float t1, t2, offset;
	float frac, frac2;
	int i;
	vec3_t mid;
	int side;
	float midf;

	if(trace_trace.fraction <= p1f)
		return;  // already hit something nearer

	// if < 0, we are in a leaf node
	if(num < 0){
		Cm_TraceToLeaf(-1 - num);
		return;
	}

	// find the point distances to the seperating plane
	// and the offset for the size of the box
	node = map_nodes + num;
	plane = node->plane;

	if(AXIAL(plane)){
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	} else {
		t1 = DotProduct(plane->normal, p1) - plane->dist;
		t2 = DotProduct(plane->normal, p2) - plane->dist;
		if(trace_ispoint)
			offset = 0;
		else
			offset = fabsf(trace_extents[0] * plane->normal[0]) +
					 fabsf(trace_extents[1] * plane->normal[1]) +
					 fabsf(trace_extents[2] * plane->normal[2]);
	}

	// see which sides we need to consider
	if(t1 >= offset && t2 >= offset){
		Cm_RecursiveHullCheck(node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if(t1 <= -offset && t2 <= -offset){
		Cm_RecursiveHullCheck(node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if(t1 < t2){
		const float idist = 1.0 / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac = (t1 - offset + DIST_EPSILON) * idist;
	} else if(t1 > t2){
		const float idist = 1.0 / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON) * idist;
		frac = (t1 + offset + DIST_EPSILON) * idist;
	} else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	if(frac < 0)
		frac = 0;
	if(frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f) * frac;
	for(i = 0; i < 3; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	Cm_RecursiveHullCheck(node->children[side], p1f, midf, p1, mid);

	// go past the node
	if(frac2 < 0)
		frac2 = 0;
	if(frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f) * frac2;
	for(i = 0; i < 3; i++)
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);

	Cm_RecursiveHullCheck(node->children[side ^ 1], midf, p2f, mid, p2);
}


static int pointleafs[1024];

/*
 * Cm_BoxTrace
 */
trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end,
			const vec3_t mins, const vec3_t maxs, int headnode, int brushmask){
	int i;

	checkcount++;  // for multi-check avoidance

	c_traces++;  // for statistics, may be zeroed

	// fill in a default trace
	memset(&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1.0;
	trace_trace.surface = &nullsurface;

	if(!numnodes)  // map not loaded
		return trace_trace;

	trace_contents = brushmask;
	VectorCopy(start, trace_start);
	VectorCopy(end, trace_end);
	VectorCopy(mins, trace_mins);
	VectorCopy(maxs, trace_maxs);

	// check for position test special case
	if(VectorCompare(start, end)){
		int i, numleafs;
		vec3_t c1, c2;
		int topnode;

		VectorAdd(start, mins, c1);
		VectorAdd(start, maxs, c2);
		for(i = 0; i < 3; i++){
			c1[i] -= 1.0;
			c2[i] += 1.0;
		}

		numleafs = Cm_BoxLeafnums_headnode(c1, c2, pointleafs,
				sizeof(pointleafs) * sizeof(int), headnode, &topnode);

		for(i = 0; i < numleafs; i++){
			Cm_TestInLeaf(pointleafs[i]);
			if(trace_trace.allsolid)
				break;
		}
		VectorCopy(start, trace_trace.endpos);
		return trace_trace;
	}

	// check for point special case
	if(VectorCompare(mins, vec3_origin) && VectorCompare(maxs, vec3_origin)){
		trace_ispoint = true;
		VectorClear(trace_extents);
	} else {
		trace_ispoint = false;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	// general sweeping through world
	Cm_RecursiveHullCheck(headnode, 0, 1, start, end);

	if(trace_trace.fraction == 1){
		VectorCopy(end, trace_trace.endpos);
	} else {
		for(i = 0; i < 3; i++)
			trace_trace.endpos[i] = start[i] + trace_trace.fraction * (end[i] - start[i]);
	}
	return trace_trace;
}


/*
 * Cm_TransformedBoxTrace
 *
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end,
		const vec3_t mins, const vec3_t maxs, int headnode, int brushmask,
		const vec3_t origin, const vec3_t angles){

	trace_t trace;
	vec3_t start_l, end_l;
	vec3_t a;
	vec3_t forward, right, up;
	vec3_t temp;
	qboolean rotated;

	// subtract origin offset
	VectorSubtract(start, origin, start_l);
	VectorSubtract(end, origin, end_l);

	// rotate start and end into the models frame of reference
	if(headnode != box_headnode &&
			(angles[0] || angles[1] || angles[2]))
		rotated = true;
	else
		rotated = false;

	if(rotated){
		AngleVectors(angles, forward, right, up);

		VectorCopy(start_l, temp);
		start_l[0] = DotProduct(temp, forward);
		start_l[1] = -DotProduct(temp, right);
		start_l[2] = DotProduct(temp, up);

		VectorCopy(end_l, temp);
		end_l[0] = DotProduct(temp, forward);
		end_l[1] = -DotProduct(temp, right);
		end_l[2] = DotProduct(temp, up);
	}

	// sweep the box through the model
	trace = Cm_BoxTrace(start_l, end_l, mins, maxs, headnode, brushmask);

	if(rotated && trace.fraction != 1.0){
		// FIXME: figure out how to do this with existing angles
		VectorNegate(angles, a);
		AngleVectors(a, forward, right, up);

		VectorCopy(trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct(temp, forward);
		trace.plane.normal[1] = -DotProduct(temp, right);
		trace.plane.normal[2] = DotProduct(temp, up);
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}


/*
 *
 * PVS / PHS
 *
 */


/*
 * Cm_DecompressVis
 */
static void Cm_DecompressVis(const byte *in, byte *out){
	int c;
	byte *out_p;
	int row;

	row = (numclusters + 7) >> 3;
	out_p = out;

	if(!in || !numvisibility){  // no vis info, so make all visible
		while(row){
			*out_p++ = 0xff;
			row--;
		}
		return;
	}

	do {
		if(*in){
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		if((out_p - out) + c > row){
			c = row -(out_p - out);
			Com_Warn("Cm_DecompressVis: Overrun.\n");
		}
		while(c){
			*out_p++ = 0;
			c--;
		}
	} while(out_p - out < row);
}

static byte pvsrow[MAX_BSP_LEAFS / 8];
static byte phsrow[MAX_BSP_LEAFS / 8];

byte *Cm_ClusterPVS(int cluster){
	if(cluster == -1)
		memset(pvsrow, 0, (numclusters + 7) >> 3);
	else
		Cm_DecompressVis(map_visibility + map_vis->bitofs[cluster][DVIS_PVS], pvsrow);
	return pvsrow;
}

byte *Cm_ClusterPHS(int cluster){
	if(cluster == -1)
		memset(phsrow, 0, (numclusters + 7) >> 3);
	else
		Cm_DecompressVis(map_visibility + map_vis->bitofs[cluster][DVIS_PHS], phsrow);
	return phsrow;
}


/*
 *
 * AREAPORTALS
 *
 */

static void Cm_FloodArea(carea_t *area, int floodnum){
	int i;
	const dareaportal_t *p;

	if(area->floodvalid == floodvalid){
		if(area->floodnum == floodnum)
			return;
		Com_Error(ERR_DROP, "Cm_FloodArea: Reflooded.");
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	p = &map_areaportals[area->firstareaportal];
	for(i = 0; i < area->numareaportals; i++, p++){
		if(portalopen[p->portalnum])
			Cm_FloodArea(&map_areas[p->otherarea], floodnum);
	}
}


/*
 * Cm_FloodAreaConnections
 */
static void Cm_FloodAreaConnections(void){
	int i;
	int floodnum;

	// all current floods are now invalid
	floodvalid++;
	floodnum = 0;

	// area 0 is not used
	for(i = 1; i < numareas; i++){
		carea_t *area = &map_areas[i];
		if(area->floodvalid == floodvalid)
			continue;  // already flooded into
		floodnum++;
		Cm_FloodArea(area, floodnum);
	}
}

void Cm_SetAreaPortalState(int portalnum, qboolean open){
	if(portalnum > numareaportals){
		Com_Error(ERR_DROP, "Cm_SetAreaPortalState: areaportal > numareaportals.");
	}

	portalopen[portalnum] = open;
	Cm_FloodAreaConnections();
}

qboolean Cm_AreasConnected(int area1, int area2){
	if(map_noareas->value)
		return true;

	if(area1 > numareas || area2 > numareas){
		Com_Error(ERR_DROP, "Cm_AreasConnected: area > numareas.");
	}

	if(map_areas[area1].floodnum == map_areas[area2].floodnum)
		return true;
	return false;
}


/*
 * Cm_WriteAreaBits
 *
 * Writes a length byte followed by a bit vector of all the areas
 * that are in the same flood as the area parameter
 *
 * This is used by the client view to cull visibility
 */
int Cm_WriteAreaBits(byte *buffer, int area){
	int i;
	const int bytes = (numareas + 7) >> 3;

	if(map_noareas->value){  // for debugging, send everything
		memset(buffer, 255, bytes);
	} else {
		const int floodnum = map_areas[area].floodnum;
		memset(buffer, 0, bytes);

		for(i = 0; i < numareas; i++){
			if(map_areas[i].floodnum == floodnum || !area)
				buffer[i >> 3] |= 1 <<(i & 7);
		}
	}

	return bytes;
}


/*
 * Cm_HeadnodeVisible
 *
 * Returns true if any leaf under headnode has a cluster that
 * is potentially visible
 */
qboolean Cm_HeadnodeVisible(int nodenum, byte *visbits){
	const cnode_t *node;

	if(nodenum < 0){  // at a leaf, check it
		const int leafnum = -1 - nodenum;
		const int cluster = map_leafs[leafnum].cluster;
		if(cluster == -1)
			return false;
		if(visbits[cluster >> 3] & (1 << (cluster & 7)))
			return true;
		return false;
	}

	node = &map_nodes[nodenum];

	if(Cm_HeadnodeVisible(node->children[0], visbits))
		return true;

	return Cm_HeadnodeVisible(node->children[1], visbits);
}
