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

#ifndef __R_TYPES_H__
#define __R_TYPES_H__

#include <SDL/SDL_opengl.h>

#include "files.h"
#include "image.h"
#include "thread.h"
#include "r_matrix.h"

// media handles
typedef struct r_media_s {
	char name[MAX_QPATH];
	GList *dependencies;
	void (*Register)(struct r_media_s *self);
	_Bool (*Retain)(struct r_media_s *self);
	void (*Free)(struct r_media_s *self);
	int32_t seed;
} r_media_t;

typedef int16_t r_pixel_t;

// high bits OR'ed with image types
#define IT_MASK_MIPMAP 128
#define IT_MASK_FILTER 256

// image types
typedef enum {
	IT_NULL = 0,
	IT_PROGRAM = 1,
	IT_FONT = 2 + (IT_MASK_FILTER),
	IT_EFFECT = 3 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_DIFFUSE = 4 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_LIGHTMAP = 5 + (IT_MASK_FILTER),
	IT_DELUXEMAP = 6,
	IT_NORMALMAP = 7 + (IT_MASK_MIPMAP),
	IT_GLOSSMAP = 8,
	IT_ENVMAP =  9 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_FLARE = 10 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_SKY = 11 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_PIC = 12 + (IT_MASK_MIPMAP | IT_MASK_FILTER)
} r_image_type_t;

/*
 * @brief Images are referenced by materials, models, entities, particles, etc.
 */
typedef struct {
	r_media_t media;
	r_image_type_t type;
	r_pixel_t width, height; // image dimensions
	GLuint texnum; // OpenGL texture binding
	vec3_t color; // average color
} r_image_t;

typedef struct {
	GLenum src, dest;
} r_stage_blend_t;

typedef struct {
	vec_t hz, dhz;
} r_stage_pulse_t;

typedef struct {
	vec_t hz, dhz;
	vec_t amp, damp;
} r_stage_stretch_t;

typedef struct {
	vec_t hz, deg;
} r_stage_rotate_t;

typedef struct {
	vec_t s, t;
	vec_t ds, dt;
} r_stage_scroll_t;

typedef struct {
	vec_t s, t;
} r_stage_scale_t;

typedef struct {
	vec_t floor, ceil;
	vec_t height;
} r_stage_terrain_t;

typedef struct {
	vec_t intensity;
} r_stage_dirt_t;

// frame based material animation, lerp between consecutive images
typedef struct {
	uint16_t num_frames;
	r_image_t **frames;
	vec_t fps;
	uint32_t dtime;
	uint16_t dframe;
} r_stage_anim_t;

typedef struct r_stage_s {
	uint32_t flags;
	r_image_t *image;
	struct r_material_s *material;
	r_stage_blend_t blend;
	vec3_t color;
	r_stage_pulse_t pulse;
	r_stage_stretch_t stretch;
	r_stage_rotate_t rotate;
	r_stage_scroll_t scroll;
	r_stage_scale_t scale;
	r_stage_terrain_t terrain;
	r_stage_dirt_t dirt;
	r_stage_anim_t anim;
	struct r_stage_s *next;
} r_stage_t;

#define DEFAULT_BUMP 1.0
#define DEFAULT_PARALLAX 1.0
#define DEFAULT_HARDNESS 1.0
#define DEFAULT_SPECULAR 1.0

typedef struct r_material_s {
	r_media_t media;
	r_image_t *diffuse;
	r_image_t *normalmap;
	r_image_t *glossmap;
	uint32_t flags;
	uint32_t time;
	vec_t bump;
	vec_t parallax;
	vec_t hardness;
	vec_t specular;
	r_stage_t *stages;
	uint16_t num_stages;
} r_material_t;

// bsp model memory representation
typedef struct {
	vec3_t position;
	vec3_t normal;
} r_bsp_vertex_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	vec_t radius;
	int32_t head_node;
	uint16_t first_surface, num_surfaces;
	uint32_t lights; // bit mask of enabled light sources
} r_bsp_inline_model_t;

typedef struct {
	uint16_t v[2];
} r_bsp_edge_t;

typedef struct {
	char name[32];
	vec_t vecs[2][4];
	uint32_t flags;
	int32_t value;
	r_material_t *material;
} r_bsp_texinfo_t;

typedef struct {
	vec3_t origin;
	vec_t radius;
	const r_image_t *image;
	vec3_t color;
	uint32_t time;
	vec_t alpha;
} r_bsp_flare_t;

// r_bsp_surface_t flags
#define R_SURF_SIDE_BACK	1
#define R_SURF_LIGHTMAP		2

typedef struct {
	int16_t vis_frame; // PVS frame
	int16_t frame; // renderer frame
	int16_t back_frame; // back-facing renderer frame
	int16_t light_frame; // dynamic lighting frame

	c_bsp_plane_t *plane;
	uint16_t flags; // R_SURF flags

	int32_t first_edge; // look up in model->surf_edges, negative numbers
	uint16_t num_edges; // are backwards edges

	vec3_t mins;
	vec3_t maxs;
	vec3_t center;
	vec3_t normal;

	vec2_t st_mins;
	vec2_t st_maxs;
	vec2_t st_center;
	vec2_t st_extents;

	GLuint index; // index into world vertex buffers

	r_bsp_texinfo_t *texinfo; // SURF_ flags

	r_bsp_flare_t *flare;

	r_pixel_t light_s, light_t; // lightmap texcoords
	r_image_t *lightmap;
	r_image_t *deluxemap;

	uint64_t lights; // bit mask of enabled light sources
} r_bsp_surface_t;

/*
 * @brief Surfaces are assigned to arrays based on their render path and then
 * sorted by material to reduce glBindTexture calls.
 */
typedef struct {
	r_bsp_surface_t **surfaces;
	size_t count;
} r_bsp_surfaces_t;

typedef struct {
	r_bsp_surfaces_t sky;
	r_bsp_surfaces_t opaque;
	r_bsp_surfaces_t opaque_warp;
	r_bsp_surfaces_t alpha_test;
	r_bsp_surfaces_t blend;
	r_bsp_surfaces_t blend_warp;
	r_bsp_surfaces_t material;
	r_bsp_surfaces_t flare;
	r_bsp_surfaces_t back;
} r_sorted_bsp_surfaces_t;

#define R_SurfaceToSurfaces(surfs, surf)\
	(surfs)->surfaces[(surfs)->count++] = surf

typedef void (*BspSurfacesDrawFunc)(const r_bsp_surfaces_t *surfs);

/*
 * @brief BSP nodes comprise the tree representation of the world. At compile
 * time, the map is divided into convex volumes that fall along brushes
 * (walls). These volumes become nodes. The planes these divisions create
 * provide a basis for testing all other nodes in the world for sidedness
 * using the dot-product check: DOT(point, plane.normal) - plane.dist.
 * Starting from the origin, this information is gathered into a tree structure
 * with which a simple recursion can quickly determine:
 *
 *  a. Which nodes are in front of my view vector from my current origin?
 *  b. Which nodes are facing me?
 *
 * This is the basis for all collision detection and rendering in Quake.
 */
typedef struct r_bsp_node_s {
	// common with leaf
	int32_t contents; // -1, to differentiate from leafs
	int16_t vis_frame; // node needs to be traversed if current

	vec3_t mins; // for bounded box culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct r_model_s *model;

	// node specific
	c_bsp_plane_t *plane;
	struct r_bsp_node_s *children[2];

	uint16_t first_surface;
	uint16_t num_surfaces;
} r_bsp_node_t;

/*
 * @brief BSP leafs terminate the branches of the BSP tree and provide grouping
 * for surfaces. If a leaf is found to be in the potentially visible set (PVS)
 * for a given frame, then all surfaces associated to that leaf are flagged for
 * drawing.
 */
typedef struct {
	// common with node
	int32_t contents; // will be a negative contents number
	int16_t vis_frame; // node needs to be traversed if current

	vec3_t mins; // for bounding box culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct r_model_s *model;

	// leaf specific
	int16_t cluster;
	int16_t area;

	r_bsp_surface_t **first_leaf_surface;
	int16_t num_leaf_surfaces;
} r_bsp_leaf_t;

/*
 * @brief
 */
typedef struct {
	int16_t vis_frame; // PVS eligibility
} r_bsp_cluster_t;

/*
 * @brief BSP lightmap parameters.
 */
typedef struct {
	uint16_t scale;
	uint32_t size;
	byte *data;
} r_bsp_lightmaps_t;

/*
 * @brief BSP light sources.
 */
typedef struct {
	vec3_t origin;
	vec_t radius;
	vec3_t color;
	uint16_t count;
	const r_bsp_leaf_t *leaf;
} r_bsp_light_t;

// md3 model memory representation
typedef struct {
	vec3_t point;
	vec3_t normal;
	vec4_t tangent;
} r_md3_vertex_t;

typedef struct {
	char name[MD3_MAX_PATH];
	matrix4x4_t matrix;
} r_md3_tag_t;

typedef struct {
	char id[4];

	char name[MD3_MAX_PATH];

	int32_t flags;

	uint16_t num_skins;

	uint16_t num_verts;
	r_md3_vertex_t *verts;
	d_md3_texcoord_t *coords;

	uint16_t num_tris;
	uint32_t *tris;
} r_md3_mesh_t;

typedef struct {
	uint16_t first_frame;
	uint16_t num_frames;
	uint16_t looped_frames;
	uint16_t hz;
} r_md3_animation_t;

/*
 * @brief Quake3 (MD3) model in-memory representation.
 */
typedef struct {
	int32_t id;
	int32_t version;

	char file_name[MD3_MAX_PATH];

	int32_t flags;

	uint16_t num_frames;
	d_md3_frame_t *frames;

	uint16_t num_tags;
	r_md3_tag_t *tags;

	uint16_t num_meshes;
	r_md3_mesh_t *meshes;

	uint16_t num_animations;
	r_md3_animation_t *animations;
} r_md3_t;

typedef struct {
	uint16_t vert;
	uint16_t normal;
	uint16_t texcoord;
} r_obj_vert_t;

typedef struct {
	r_obj_vert_t verts[3];
} r_obj_tri_t;

/*
 * brief Object (OBJ) model in-memory representation.
 */
typedef struct {
	uint16_t num_verts;
	uint16_t num_verts_parsed;
	vec_t *verts;

	uint16_t num_normals;
	uint16_t num_normals_parsed;
	vec_t *normals;

	uint16_t num_tangents;
	vec_t *tangents;

	uint16_t num_texcoords;
	uint16_t num_texcoords_parsed;
	vec_t *texcoords;

	uint16_t num_tris;
	uint16_t num_tris_parsed;
	r_obj_tri_t *tris;
} r_obj_t;

// shared structure for all model types
typedef enum {
	MOD_BAD, MOD_BSP, MOD_BSP_INLINE, MOD_MD3, MOD_OBJ
} r_model_type_t;

typedef struct {
	int32_t version;

	uint16_t num_inline_models;
	r_bsp_inline_model_t *inline_models;

	uint16_t num_planes;
	c_bsp_plane_t *planes;

	uint16_t num_leafs;
	r_bsp_leaf_t *leafs;

	uint16_t num_vertexes;
	r_bsp_vertex_t *vertexes;

	uint32_t num_edges;
	r_bsp_edge_t *edges;

	uint16_t num_nodes;
	r_bsp_node_t *nodes;

	uint16_t num_texinfo;
	r_bsp_texinfo_t *texinfo;

	uint16_t num_surfaces;
	r_bsp_surface_t *surfaces;

	uint32_t num_surface_edges;
	int32_t *surface_edges;

	uint16_t num_leaf_surfaces;
	r_bsp_surface_t **leaf_surfaces;

	uint16_t num_clusters;
	r_bsp_cluster_t *clusters;

	r_bsp_lightmaps_t *lightmaps;

	uint16_t num_bsp_lights;
	r_bsp_light_t *bsp_lights;

	// sorted surfaces arrays
	r_sorted_bsp_surfaces_t *sorted_surfaces;
} r_bsp_model_t;

/*
 * @brief Provides load-time normalization of mesh models.
 */
typedef struct {
	vec3_t translate;
	vec_t scale;
	uint32_t flags; // EF_ALPHA_TEST, etc..
} r_mesh_config_t;

typedef struct {
	uint16_t num_frames;
	uint32_t flags;

	r_material_t *material;

	r_mesh_config_t *world_config;
	r_mesh_config_t *view_config;
	r_mesh_config_t *link_config;

	void *data; // raw model data (r_md3_t, r_obj_t, ..)
} r_mesh_model_t;

/*
 * @brief Models represent a subset of the BSP or an OBJ / MD3 mesh.
 */
typedef struct r_model_s {
	r_media_t media;
	r_model_type_t type;

	r_bsp_model_t *bsp;
	r_bsp_inline_model_t *bsp_inline;
	r_mesh_model_t *mesh;

	vec3_t mins, maxs;
	vec_t radius;

	GLsizei num_verts; // raw vertex primitive count, used to build arrays

	GLfloat *verts; // vertex arrays
	GLfloat *texcoords;
	GLfloat *lightmap_texcoords;
	GLfloat *normals;
	GLfloat *tangents;

	GLuint vertex_buffer; // vertex buffer objects
	GLuint texcoord_buffer;
	GLuint lightmap_texcoord_buffer;
	GLuint normal_buffer;
	GLuint tangent_buffer;
} r_model_t;

#define IS_MESH_MODEL(m) (m && m->mesh)
#define IS_BSP_INLINE_MODEL(m) (m && m->bsp_inline)

/*
 * @brief Dynamic light sources expire immediately and must be re-added
 * for each frame they appear.
 */
typedef struct r_light_s {
	vec3_t origin;
	vec_t radius;
	vec3_t color;
} r_light_t;

#define MAX_LIGHTS			64
#define MAX_ACTIVE_LIGHTS	8

/*
 * @brief Sustains are light flashes which slowly decay over time. These
 * persist over multiple frames.
 */
typedef struct r_sustained_light_s {
	r_light_t light;
	uint32_t time;
	uint32_t sustain;
} r_sustained_light_t;

/*
 * @brief A reference to a static BSP light source plus a light level.
 */
typedef struct r_bsp_light_ref_s {
	r_bsp_light_t *bsp_light;
	vec3_t dir;
	vec_t intensity;
} r_bsp_light_ref_t;

typedef enum {
	LIGHTING_INIT, LIGHTING_DIRTY, LIGHTING_READY
} r_lighting_state_t;

/*
 * @brief Provides static lighting information for mesh entities.
 */
typedef struct r_lighting_s {
	uint16_t number; // entity number
	vec3_t origin; // entity origin
	vec_t radius; // entity radius
	vec3_t mins, maxs; // entity bounding box in world space
	vec3_t shadow_origin;
	vec3_t shadow_normal;
	vec3_t color;
	r_bsp_light_ref_t bsp_light_refs[MAX_ACTIVE_LIGHTS]; // light sources
	r_lighting_state_t state;
} r_lighting_t;

#define LIGHTING_MAX_SHADOW_DISTANCE 128.0

/*
 * @brief Entities provide a means to add model instances to the view.
 */
typedef struct r_entity_s {
	struct r_entity_s *next; // for draw lists

	const struct r_entity_s *parent; // for linked models
	const char *tag_name;

	vec3_t origin;
	vec3_t angles;

	matrix4x4_t matrix;
	_Bool culled;

	const r_model_t *model;

	uint16_t frame, old_frame; // frame-based animations
	vec_t lerp, back_lerp;

	vec_t scale; // for mesh models

	r_material_t *skins[MD3_MAX_MESHES]; // NULL for default skin
	uint16_t num_skins;

	uint32_t effects; // e.g. EF_ROCKET, EF_WEAPON, ..

	vec3_t shell; // shell color
	vec_t alpha; // EF_ALPHA value

	r_lighting_t *lighting; // static lighting information
} r_entity_t;

#define MAX_ENTITIES		(MAX_EDICTS * 2) // to allow for linked entities

typedef void (*MeshModelDrawFunc)(const r_entity_t *e);

typedef enum {
	PARTICLE_NORMAL,
	PARTICLE_ROLL,
	PARTICLE_DECAL,
	PARTICLE_BUBBLE,
	PARTICLE_BEAM,
	PARTICLE_WEATHER,
	PARTICLE_SPLASH
} r_particle_type_t;

/*
 * @brief Particles are alpha-blended, textured quads.
 */
typedef struct r_particle_s {
	r_particle_type_t type;
	const r_image_t *image;
	GLenum blend;
	byte color;
	vec_t alpha;
	vec_t scale;
	vec_t scroll_s;
	vec_t scroll_t;
	vec_t roll;
	vec3_t org;
	vec3_t end;
	vec3_t dir;
} r_particle_t;

#define MAX_PARTICLES		16384

/*
 * @brief Coronas are soft, alpha-blended, rounded sprites.
 */
typedef struct {
	vec3_t origin;
	vec_t radius;
	vec_t flicker;
	vec3_t color;
} r_corona_t;

/*
 * @brief Renderer element types.
 */
typedef enum {
	ELEMENT_NONE,
	ELEMENT_BSP_SURFACE_BLEND,
	ELEMENT_BSP_SURFACE_BLEND_WARP,
	ELEMENT_ENTITY,
	ELEMENT_PARTICLE
} r_element_type_t;

/*
 * @brief Element abstraction to allow sorting of mixed draw lists,
 * asynchronous rendering via commands, etc.
 */
typedef struct {
	r_element_type_t type;
	const void *element;
	const vec_t *origin;
	vec_t depth; // resolved for all elements
	void *data;
} r_element_t;

/*
 * @brief Allows alternate renderer plugins to be dropped in.
 */
typedef enum {
	RENDER_MODE_DEFAULT
} r_render_mode_t;

#define MAX_CORONAS 		1024

#define WEATHER_NONE		0
#define WEATHER_RAIN 		1
#define WEATHER_SNOW 		2
#define WEATHER_FOG 		4
#define WEATHER_PRECIP_MASK	(WEATHER_RAIN | WEATHER_SNOW)

#define FOG_START			300.0
#define FOG_END				2500.0

/*
 * @brief Provides read-write visibility and scene management to the client.
 */
typedef struct {
	r_pixel_t x, y, width, height; // in virtual screen coordinates
	vec2_t fov;

	vec3_t origin; // client's view origin, angles, and vectors
	vec3_t angles;
	vec3_t forward;
	vec3_t right;
	vec3_t up;

	uint32_t contents; // view origin contents mask
	vec_t bob;

	uint32_t time;

	byte *area_bits; // if not NULL, only areas with set bits will be drawn

	r_render_mode_t render_mode; // active renderer plugin

	byte weather; // weather effects
	vec4_t fog_color;

	uint16_t num_entities;
	r_entity_t entities[MAX_ENTITIES];

	uint16_t num_particles;
	r_particle_t particles[MAX_PARTICLES];

	uint16_t num_coronas;
	r_corona_t coronas[MAX_CORONAS];

	uint16_t num_lights;
	r_light_t lights[MAX_LIGHTS];

	r_sustained_light_t sustained_lights[MAX_LIGHTS];

	thread_t *thread; // client thread which populates view

	c_trace_t trace; // occlusion testing
	r_entity_t *trace_ent;

	// counters, reset each frame

	uint32_t num_bind_texture;
	uint32_t num_bind_lightmap;
	uint32_t num_bind_deluxemap;
	uint32_t num_bind_normalmap;
	uint32_t num_bind_glossmap;

	uint32_t num_bsp_clusters;
	uint32_t num_bsp_leafs;
	uint32_t num_bsp_surfaces;

	uint32_t num_mesh_models;
	uint32_t num_mesh_tris;

	_Bool update; // inform the client of state changes
} r_view_t;

/*
 * @brief OpenGL context information.
 */
typedef struct {
	r_pixel_t width, height;

	_Bool fullscreen;

	int32_t red_bits, green_bits, blue_bits, alpha_bits;
	int32_t stencil_bits, depth_bits, double_buffer;
} r_context_t;

#endif /* __R_TYPES_H__ */
