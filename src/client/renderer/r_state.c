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

#include "r_local.h"

static cvar_t *r_get_error;

r_state_t r_state;

const vec_t default_texcoords[] = { // useful for particles, pics, etc..
	0.0, 0.0,
	1.0, 0.0,
	1.0, 1.0,
	0.0, 1.0
};

/*
 * @brief Queries OpenGL for any errors and prints them as warnings.
 */
void R_GetError_(const char *function, const char *msg) {
	GLenum err;
	char *s;

	if (!r_get_error->value)
		return;

	while (true) {

		if ((err = glGetError()) == GL_NO_ERROR)
			return;

		switch (err) {
		case GL_INVALID_ENUM:
			s = "GL_INVALID_ENUM";
			break;
		case GL_INVALID_VALUE:
			s = "GL_INVALID_VALUE";
			break;
		case GL_INVALID_OPERATION:
			s = "GL_INVALID_OPERATION";
			break;
		case GL_STACK_OVERFLOW:
			s = "GL_STACK_OVERFLOW";
			break;
		case GL_OUT_OF_MEMORY:
			s = "GL_OUT_OF_MEMORY";
			break;
		default:
			s = "Unkown error";
			break;
		}

		Sys_Backtrace();

		Com_Warn("%s: %s: %s.\n", s, function, msg);
	}
}

/*
 * @brief Sets the current color. Pass NULL to reset to default.
 */
void R_Color(const vec4_t color) {
	static const vec4_t white = { 1.0, 1.0, 1.0, 1.0 };

	if (color) {
		glColor4fv(color);
	} else {
		glColor4fv(white);
	}
}

/*
 * @brief Set the active texture unit. If the diffuse or lightmap texture units are
 * selected, set the client state to allow binding of texture coordinates.
 */
void R_SelectTexture(r_texunit_t *texunit) {

	if (texunit == r_state.active_texunit)
		return;

	r_state.active_texunit = texunit;

	qglActiveTexture(texunit->texture);

	if (texunit == &texunit_diffuse || texunit == &texunit_lightmap)
		qglClientActiveTexture(texunit->texture);
}

/*
 * @brief Binds the specified texture for the active texture unit.
 */
void R_BindTexture(GLuint texnum) {

	if (texnum == r_state.active_texunit->texnum)
		return;

	r_state.active_texunit->texnum = texnum;

	glBindTexture(GL_TEXTURE_2D, texnum);

	r_view.num_bind_texture++;
}

/*
 * @brief Binds the specified texture for the lightmap texture unit.
 */
void R_BindLightmapTexture(GLuint texnum) {

	if (texnum == texunit_lightmap.texnum)
		return;

	R_SelectTexture(&texunit_lightmap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);

	r_view.num_bind_lightmap++;
}

/*
 * @brief Binds the specified texture for the deluxemap texture unit.
 */
void R_BindDeluxemapTexture(GLuint texnum) {

	if (texnum == texunit_deluxemap.texnum)
		return;

	R_SelectTexture(&texunit_deluxemap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);

	r_view.num_bind_deluxemap++;
}

/*
 * @brief Binds the specified texture for the normalmap texture unit.
 */
void R_BindNormalmapTexture(GLuint texnum) {

	if (texnum == texunit_normalmap.texnum)
		return;

	R_SelectTexture(&texunit_normalmap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);

	r_view.num_bind_normalmap++;
}

/*
 * @brief Binds the specified texture for the glossmap texture unit.
 */
void R_BindGlossmapTexture(GLuint texnum) {

	if (texnum == texunit_glossmap.texnum)
		return;

	R_SelectTexture(&texunit_glossmap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);

	r_view.num_bind_glossmap++;
}

/*
 * @brief Binds the specified array for the given target.
 */
void R_BindArray(GLenum target, GLenum type, GLvoid *array) {

	switch (target) {
	case GL_VERTEX_ARRAY:
		if (r_state.ortho)
			glVertexPointer(2, type, 0, array);
		else
			glVertexPointer(3, type, 0, array);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		glTexCoordPointer(2, type, 0, array);
		break;
	case GL_COLOR_ARRAY:
		glColorPointer(4, type, 0, array);
		break;
	case GL_NORMAL_ARRAY:
		glNormalPointer(type, 0, array);
		break;
	case GL_TANGENT_ARRAY:
		R_AttributePointer("TANGENT", 4, array);
		break;
	default:
		break;
	}
}

/*
 * @brief Binds the appropriate shared vertex array to the specified target.
 */
void R_BindDefaultArray(GLenum target) {

	switch (target) {
	case GL_VERTEX_ARRAY:
		if (r_state.ortho)
			R_BindArray(target, GL_SHORT, r_state.vertex_array_2d);
		else
			R_BindArray(target, GL_FLOAT, r_state.vertex_array_3d);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.active_texunit->texcoord_array);
		break;
	case GL_COLOR_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.color_array);
		break;
	case GL_NORMAL_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.normal_array);
		break;
	case GL_TANGENT_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.tangent_array);
		break;
	default:
		break;
	}
}

/*
 * @brief
 */
void R_BindBuffer(GLenum target, GLenum type, GLuint id) {

	if (!qglBindBuffer)
		return;

	if (!r_vertex_buffers->value)
		return;

	qglBindBuffer(GL_ARRAY_BUFFER, id);

	if (type && id) // assign the array pointer as well
		R_BindArray(target, type, NULL);

	R_GetError(NULL);
}

/*
 * @brief
 */
void R_BlendFunc(GLenum src, GLenum dest) {

	if (r_state.blend_src == src && r_state.blend_dest == dest)
		return;

	r_state.blend_src = src;
	r_state.blend_dest = dest;

	glBlendFunc(src, dest);
}

/*
 * @brief
 */
void R_EnableBlend(_Bool enable) {

	if (r_state.blend_enabled == enable)
		return;

	r_state.blend_enabled = enable;

	if (enable) {
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
	} else {
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}
}

/*
 * @brief
 */
void R_EnableAlphaTest(_Bool enable) {

	if (r_state.alpha_test_enabled == enable)
		return;

	r_state.alpha_test_enabled = enable;

	if (enable)
		glEnable(GL_ALPHA_TEST);
	else
		glDisable(GL_ALPHA_TEST);
}

/*
 * @brief Enables the stencil test for e.g. rendering shadow volumes.
 */
void R_EnableStencilTest(_Bool enable) {

	if (r_state.stencil_test_enabled == enable)
		return;

	r_state.stencil_test_enabled = enable;

	if (enable)
		glEnable(GL_STENCIL_TEST);
	else
		glDisable(GL_STENCIL_TEST);
}

/*
 * @brief Enable the specified texture unit for multi-texture operations. This is not
 * necessary for texture units only accessed by GLSL shaders.
 */
void R_EnableTexture(r_texunit_t *texunit, _Bool enable) {

	if (enable == texunit->enabled)
		return;

	texunit->enabled = enable;

	R_SelectTexture(texunit);

	if (enable) { // activate texture unit
		glEnable(GL_TEXTURE_2D);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	} else { // or deactivate it
		glDisable(GL_TEXTURE_2D);

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	R_SelectTexture(&texunit_diffuse);
}

/*
 * @brief
 */
void R_EnableColorArray(_Bool enable) {

	if (r_state.color_array_enabled == enable)
		return;

	r_state.color_array_enabled = enable;

	if (enable)
		glEnableClientState(GL_COLOR_ARRAY);
	else
		glDisableClientState(GL_COLOR_ARRAY);
}

/*
 * @brief Enables hardware-accelerated lighting with the specified program. This
 * should be called after any texture units which will be active for lighting
 * have been enabled.
 */
void R_EnableLighting(r_program_t *program, _Bool enable) {

	if (!r_programs->value)
		return;

	if (enable && (!program || !program->id))
		return;

	if (!r_lighting->value || r_state.lighting_enabled == enable)
		return;

	r_state.lighting_enabled = enable;

	if (enable) { // toggle state
		R_UseProgram(program);

		R_ResetLights();

		glEnableClientState(GL_NORMAL_ARRAY);
	} else {
		glDisableClientState(GL_NORMAL_ARRAY);

		R_UseProgram(NULL);
	}

	R_GetError(NULL);
}

/*
 * @brief Enables the warp shader for drawing liquids and other effects.
 */
void R_EnableWarp(r_program_t *program, _Bool enable) {

	if (!r_programs->value)
		return;

	if (enable && (!program || !program->id))
		return;

	if (!r_warp->value || r_state.warp_enabled == enable)
		return;

	r_state.warp_enabled = enable;

	R_SelectTexture(&texunit_lightmap);

	if (enable) {
		R_BindTexture(r_image_state.warp->texnum);

		R_UseProgram(program);
	} else {
		R_UseProgram(NULL);
	}

	R_SelectTexture(&texunit_diffuse);

	R_GetError(NULL);
}

/*
 * @brief
 */
void R_EnableShell(_Bool enable) {
	r_uniform1f_t offset;

	if (enable == r_state.shell_enabled)
		return;

	r_state.shell_enabled = enable;

	if (r_state.active_program)
		R_ProgramVariable(&offset, R_UNIFORM_FLOAT, "OFFSET");

	if (enable) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0, 0.0);

		R_EnableBlend(true);
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

		if (r_state.active_program)
			R_ProgramParameter1f(&offset, r_view.time * 0.00033);
	} else {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		R_EnableBlend(false);

		glPolygonOffset(0.0, 0.0);
		glDisable(GL_POLYGON_OFFSET_FILL);

		if (r_state.active_program)
			R_ProgramParameter1f(&offset, 0.0);
	}

	R_GetError(NULL);
}

/*
 * @brief
 */
void R_EnableFog(_Bool enable) {

	if (!r_fog->value || r_state.fog_enabled == enable)
		return;

	r_state.fog_enabled = false;

	if (enable) {
		if ((r_view.weather & WEATHER_FOG) || r_fog->integer == 2) {

			r_state.fog_enabled = true;

			glFogfv(GL_FOG_COLOR, r_view.fog_color);

			glFogf(GL_FOG_DENSITY, 1.0);
			glEnable(GL_FOG);
		}
	} else {
		glFogf(GL_FOG_DENSITY, 0.0);
		glDisable(GL_FOG);
	}
}

/*
 * @brief Setup the GLSL shaders for the specified surface and primary material. If no
 * shader is bound, this function simply returns.
 */
void R_UseMaterial(const r_bsp_surface_t *surf, const r_material_t *material) {

	if (!r_state.active_program)
		return;

	if (r_state.active_program->UseMaterial)
		r_state.active_program->UseMaterial(surf, material);

	R_GetError(material ? material->diffuse->media.name : r_state.active_program->name);
}

#define NEAR_Z 4
#define FAR_Z 16384

/*
 * @brief Prepare OpenGL for drawing the 3D scene. Update the view-port definition
 * and load our projection and model-view matrices.
 */
void R_Setup3D(void) {
	vec_t xmin, xmax, ymin, ymax;
	vec_t aspect;

	if (!r_view.width || !r_view.height)
		return;

	glViewport(r_view.x, r_view.y, r_view.width, r_view.height);

	// set up projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	aspect = (vec_t) r_view.width / (vec_t) r_view.height;

	ymax = NEAR_Z * tan(r_view.fov[1] * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustum(xmin, xmax, ymin, ymax, NEAR_Z, FAR_Z);

	// setup the model-view matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90.0, 1.0, 0.0, 0.0); // put Z going up
	glRotatef(90.0, 0.0, 0.0, 1.0); // put Z going up

	glRotatef(-r_view.angles[2], 1.0, 0.0, 0.0);
	glRotatef(-r_view.angles[0], 0.0, 1.0, 0.0);
	glRotatef(-r_view.angles[1], 0.0, 0.0, 1.0);

	glTranslatef(-r_view.origin[0], -r_view.origin[1], -r_view.origin[2]);

	r_state.ortho = false;

	// bind default vertex array
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);

	glEnable(GL_DEPTH_TEST);
}

/*
 * @brief Prepare OpenGL for drawing the 2D overlay. Update the view-port definition
 * and reset the project and model-view matrices.
 */
void R_Setup2D(void) {

	glViewport(0, 0, r_context.width, r_context.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0, r_context.width, r_context.height, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	r_state.ortho = true;

	// bind default vertex array
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	// and set default texcoords for all 2d pics
	memcpy(texunit_diffuse.texcoord_array, default_texcoords, sizeof(vec2_t) * 4);

	glEnable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);
}

/*
 * @brief Initializes the OpenGL state cache and sets OpenGL state parameters to
 * appropriate defaults.
 */
void R_InitState(void) {
	int32_t i;

	r_get_error = Cvar_Get("r_get_error", "0", 0, NULL);

	memset(&r_state, 0, sizeof(r_state));

	// setup vertex array pointers
	glEnableClientState(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_EnableColorArray(true);
	R_BindDefaultArray(GL_COLOR_ARRAY);
	R_EnableColorArray(false);

	glEnableClientState(GL_NORMAL_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	// setup texture units
	const size_t len = MAX_GL_ARRAY_LENGTH * sizeof(vec2_t);
	for (i = 0; i < MAX_GL_TEXUNITS; i++) {
		r_texunit_t *texunit = &r_state.texunits[i];

		if (i < r_config.max_teximage_units) {
			texunit->texture = GL_TEXTURE0_ARB + i;

			if (i < r_config.max_texunits) {
				texunit->texcoord_array = Mem_TagMalloc(len, MEM_TAG_RENDERER);

				R_EnableTexture(texunit, true);

				R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

				if (texunit == &texunit_lightmap)
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

				R_EnableTexture(texunit, false);
			}
		}
	}

	R_EnableTexture(&texunit_diffuse, true);

	// alpha test parameters
	glAlphaFunc(GL_GREATER, 0.25);

	// stencil test parameters
	glStencilFunc(GL_GEQUAL, 1, 0xff);
	glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

	// fog parameters
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_DENSITY, 0.0);
	glFogf(GL_FOG_START, FOG_START);
	glFogf(GL_FOG_END, FOG_END);

	// alpha blend parameters
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_GetError(NULL);
}

/*
 * @brief
 */
void R_ShutdownState(void) {
	int32_t i;

	for (i = 0; i < r_config.max_texunits; i++) {
		r_texunit_t *texunit = &r_state.texunits[i];

		if (texunit->texcoord_array)
			Mem_Free(texunit->texcoord_array);
	}

	memset(&r_state, 0, sizeof(r_state));
}
