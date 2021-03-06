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
#include "client.h"

#define LIGHTING_MAX_BSP_LIGHT_REFS 64

/*
 * @brief A comparator for sorting r_bsp_light_ref_t elements via quick sort.
 */
static int32_t R_UpdateBspLightReferences_Compare(const void *l1, const void *l2) {
	const r_bsp_light_ref_t *_l1 = (r_bsp_light_ref_t *) l1;
	const r_bsp_light_ref_t *_l2 = (r_bsp_light_ref_t *) l2;

	return (int32_t) (_l2->intensity - _l1->intensity);
}

/*
 * @brief Resolves the strongest static light sources, populating the light references
 * for the specified structure and returning the number of light sources found.
 * This facilitates directional shading in the fragment program.
 */
static int32_t R_UpdateBspLightReferences(r_lighting_t *lighting) {
	r_bsp_light_ref_t light_refs[LIGHTING_MAX_BSP_LIGHT_REFS];
	r_bsp_light_ref_t *r;
	r_bsp_light_t *l;
	int32_t i, j;

	memset(lighting->bsp_light_refs, 0, sizeof(lighting->bsp_light_refs));

	l = r_model_state.world->bsp->bsp_lights;
	j = 0;

	// resolve all of the light sources that could contribute to this lighting
	for (i = 0; i < r_model_state.world->bsp->num_bsp_lights; i++, l++) {
		vec3_t dir;
		vec_t intensity;

		// is the light source within the PVS this frame
		if (lighting->state == LIGHTING_DIRTY && l->leaf->vis_frame != r_locals.vis_frame)
			continue;

		// is it within range of the entity
		VectorSubtract(l->origin, lighting->origin, dir);
		intensity = (l->radius + lighting->radius - VectorNormalize(dir)) / l->radius;

		if (intensity <= 0.0)
			continue;

		// is it visible to the entity; trace to origin and corners of bounding box

		const uint16_t skip = lighting->number;

		c_trace_t tr = Cl_Trace(l->origin, lighting->origin, NULL, NULL, skip, CONTENTS_SOLID);

		if (tr.fraction < 1.0) {
			tr = Cl_Trace(l->origin, lighting->mins, NULL, NULL, skip, CONTENTS_SOLID);

			if (tr.fraction < 1.0) {
				tr = Cl_Trace(l->origin, lighting->maxs, NULL, NULL, skip, CONTENTS_SOLID);

				if (tr.fraction < 1.0) {
					continue;
				}
			}
		}

		// everything checks out, so keep it
		r = &light_refs[j++];

		r->bsp_light = l;
		VectorCopy(dir, r->dir);
		r->intensity = intensity;

		if (j == LIGHTING_MAX_BSP_LIGHT_REFS) {
			Com_Debug("LIGHTING_MAX_BSP_LIGHT_REFS reached\n");
			break;
		}
	}

	if (j) {
		// sort them by intensity
		qsort((void *) light_refs, j, sizeof(r_bsp_light_ref_t), R_UpdateBspLightReferences_Compare);

		if (j > MAX_ACTIVE_LIGHTS)
			j = MAX_ACTIVE_LIGHTS;

		// and copy them in
		memcpy(lighting->bsp_light_refs, light_refs, j * sizeof(r_bsp_light_ref_t));
	}

	return j;
}

/*
 * @brief Resolves static lighting information for the specified point, including
 * most relevant static light sources and shadow positioning.
 */
void R_UpdateLighting(r_lighting_t *lighting) {
	vec3_t start, end;
	int32_t i, j;

	VectorCopy(lighting->origin, start);
	VectorCopy(lighting->origin, end);

	VectorCopy(r_bsp_light_state.ambient_light, lighting->color);

	// resolve the static light sources
	i = R_UpdateBspLightReferences(lighting);

	// resolve the shadow origin, factoring in light sources
	for (j = 0; j < i; j++) {
		r_bsp_light_ref_t *r = &lighting->bsp_light_refs[j];

		// only consider light sources above us
		if (r->bsp_light->origin[2] > lighting->origin[2]) {
			const vec_t scale = LIGHTING_MAX_SHADOW_DISTANCE * r->intensity;
			vec3_t dir;

			// and only factor in the X and Y components
			VectorCopy(r->dir, dir);
			dir[2] = 0.0;

			// translate the projected end point
			VectorMA(end, -scale, dir, end);
		}

		// factor in the light source color
		if (!r_lighting->value) {
			VectorMA(lighting->color, r->intensity, r->bsp_light->color, lighting->color);
		}
	}

	end[2] = start[2] - LIGHTING_MAX_SHADOW_DISTANCE;

	// do the trace
	c_trace_t tr = Cl_Trace(start, end, NULL, NULL, lighting->number, MASK_SOLID);

	// resolve the shadow origin and direction
	if (tr.fraction < 1.0) { // hit something
		VectorCopy(tr.end, lighting->shadow_origin);
		VectorCopy(tr.plane.normal, lighting->shadow_normal);
	} else { // clear it
		VectorClear(lighting->shadow_origin);
		VectorClear(lighting->shadow_normal);
	}

	lighting->state = LIGHTING_READY; // mark it clean
}

#define LIGHTING_AMBIENT_ATTENUATION 150.0

/*
 * @brief Populates the remaining hardware light sources with static BSP lighting
 * information, if it is available. No state changes are persisted through
 * r_view or r_locals.
 */
void R_ApplyLighting(const r_lighting_t *lighting) {
	const vec3_t up = { 0.0, 0.0, 64.0 };
	int32_t i, count;

	count = r_locals.active_light_count;

	if (count < MAX_ACTIVE_LIGHTS) {
		vec4_t position;
		vec4_t diffuse;

		position[3] = diffuse[3] = 1.0;

		VectorAdd(lighting->origin, up, position);
		glLightfv(GL_LIGHT0 + count, GL_POSITION, position);

		VectorScale(r_bsp_light_state.ambient_light, r_lighting->value, diffuse);
		glLightfv(GL_LIGHT0 + count, GL_DIFFUSE, diffuse);

		glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, LIGHTING_AMBIENT_ATTENUATION);
		count++;

		i = 0;
		while (count < MAX_ACTIVE_LIGHTS) {
			const r_bsp_light_ref_t *r = &lighting->bsp_light_refs[i];

			if (!r->intensity)
				break;

			VectorCopy(r->bsp_light->origin, position);
			glLightfv(GL_LIGHT0 + count, GL_POSITION, position);

			VectorScale(r->bsp_light->color, r_lighting->value, diffuse);
			glLightfv(GL_LIGHT0 + count, GL_DIFFUSE, diffuse);

			glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, r->bsp_light->radius);

			count++;
			i++;
		}
	}

	if (count < MAX_ACTIVE_LIGHTS) // disable the next light as a stop
		glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, 0.0);
}
