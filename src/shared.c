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

#include <ctype.h>

#include "shared.h"
#include "files.h"

vec3_t vec3_origin = { 0.0, 0.0, 0.0 };

/*
 * @brief Returns a pseudo-random positive integer.
 *
 * Uses a Linear Congruence Generator, values kindly borrowed from glibc.
 * Look up the rules required for the constants before just replacing them;
 * performance is dictated by the selection.
 */
int32_t Random(void) {

	static uint32_t state = 0;
	static _Bool uninitalized = true;

	if (uninitalized) {
		state = (uint32_t) time(NULL);
		uninitalized = false;
	}

	state = (1103515245 * state + 12345);
	return state & 0x7fffffff;
}

/*
 * @brief Returns a pseudo-random vec_t between 0.0 and 1.0.
 */
vec_t Randomf(void) {
	return (Random()) * (1.0 / 0x7fffffff);
}

/*
 * @brief Returns a pseudo-random vec_t between -1.0 and 1.0.
 */
vec_t Randomc(void) {
	return (Random()) * (2.0 / 0x7fffffff) - 1.0;
}

/*
 * @brief
 */
void RotatePointAroundVector(const vec3_t point, const vec3_t dir, const vec_t degrees, vec3_t out) {
	vec_t m[3][3];
	vec_t im[3][3];
	vec_t zrot[3][3];
	vec_t tmpmat[3][3];
	vec_t rot[3][3];
	int32_t i;
	vec3_t vr, vu, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector(dir, vr);
	CrossProduct(vr, vf, vu);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vu[0];
	m[1][1] = vu[1];
	m[2][1] = vu[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy(im, m, sizeof(im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset(zrot, 0, sizeof(zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = cos(Radians(degrees));
	zrot[0][1] = sin(Radians(degrees));
	zrot[1][0] = -sin(Radians(degrees));
	zrot[1][1] = cos(Radians(degrees));

	ConcatRotations(m, zrot, tmpmat);
	ConcatRotations(tmpmat, im, rot);

	for (i = 0; i < 3; i++) {
		out[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}

/*
 * @brief Derives Euler angles for the specified directional vector.
 */
void VectorAngles(const vec3_t vector, vec3_t angles) {
	const vec_t forward = sqrt(vector[0] * vector[0] + vector[1] * vector[1]);
	vec_t pitch = atan2(vector[2], forward) * 180.0 / M_PI;
	const vec_t yaw = atan2(vector[1], vector[0]) * 180.0 / M_PI;

	if (pitch < 0.0) {
		pitch += 360.0;
	}

	VectorSet(angles, -pitch, yaw, 0.0);
}

/*
 * @brief Produces the forward, right and up directional vectors for the given angles.
 */
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
	vec_t angle;
	vec_t sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI * 2.0 / 360.0);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI * 2.0 / 360.0);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI * 2.0 / 360.0);
	sr = sin(angle);
	cr = cos(angle);

	if (forward) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if (right) {
		right[0] = -1 * sr * sp * cy + -1 * cr * -sy;
		right[1] = -1 * sr * sp * sy + -1 * cr * cy;
		right[2] = -1 * sr * cp;
	}
	if (up) {
		up[0] = cr * sp * cy + -sr * -sy;
		up[1] = cr * sp * sy + -sr * cy;
		up[2] = cr * cp;
	}
}

/*
 * @brief
 */
void ProjectPointOnPlane(const vec3_t p, const vec3_t normal, vec3_t out) {
	vec3_t n;

	const vec_t inv_denom = 1.0 / DotProduct(normal, normal);

	const vec_t d = DotProduct(normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	out[0] = p[0] - d * n[0];
	out[1] = p[1] - d * n[1];
	out[2] = p[2] - d * n[2];
}

/*
 * @brief Assumes input vector is normalized.
 */
void PerpendicularVector(const vec3_t in, vec3_t out) {
	int32_t i, pos = 0;
	vec_t min_elem = 1.0;
	vec3_t tmp;

	// find the smallest magnitude axially aligned vector
	for (i = 0; i < 3; i++) {
		if (fabsf(in[i]) < min_elem) {
			pos = i;
			min_elem = fabsf(in[i]);
		}
	}
	VectorClear(tmp);
	tmp[pos] = 1.0;

	// project the point onto the plane defined by src
	ProjectPointOnPlane(tmp, in, out);

	// normalize the result
	VectorNormalize(out);
}

/*
 * @brief Projects the normalized directional vectors on to the normal's plane.
 * The fourth component of the resulting tangent vector represents sidedness.
 */
void TangentVectors(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec4_t tangent,
		vec3_t bitangent) {

	vec3_t s, t;

	// normalize the directional vectors
	VectorCopy(sdir, s);
	VectorNormalize(s);

	VectorCopy(tdir, t);
	VectorNormalize(t);

	// project the directional vector onto the plane
	VectorMA(s, -DotProduct(s, normal), normal, tangent);
	VectorNormalize(tangent);

	// resolve sidedness, encode as fourth tangent component
	CrossProduct(normal, tangent, bitangent);

	if (DotProduct(t, bitangent) < 0.0)
		tangent[3] = -1.0;
	else
		tangent[3] = 1.0;

	VectorScale(bitangent, tangent[3], bitangent);
}

/*
 * @brief
 */
void ConcatRotations(vec3_t in1[3], vec3_t in2[3], vec3_t out[3]) {
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
}

/*
 * @brief Produces the linear interpolation of the two vectors for the given fraction.
 */
void VectorLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out) {
	int32_t i;

	for (i = 0; i < 3; i++)
		out[i] = from[i] + frac * (to[i] - from[i]);
}

/*
 * @brief Produces the linear interpolation of the two angles for the given fraction.
 * Care is taken to keep the values between -180.0 and 180.0.
 */
void AngleLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out) {
	vec3_t _from, _to;
	int32_t i;

	// copy the vectors to safely clamp this lerp
	VectorCopy(from, _from);
	VectorCopy(to, _to);

	for (i = 0; i < 3; i++) {

		if (_to[i] - _from[i] > 180)
			_to[i] -= 360;

		if (_to[i] - _from[i] < -180)
			_to[i] += 360;
	}

	VectorLerp(_from, _to, frac, out);
}

/*
 * @brief Returns true if the specified planes are equal, false otherwise.
 */
_Bool PlaneCompare(const c_bsp_plane_t *p1, const c_bsp_plane_t *p2) {

	if (VectorCompare(p1->normal, p2->normal)) {
		return fabs(p1->dist - p2->dist) < 0.1;
	}

	return false;
}

/*
 * @brief Returns a bit mask hinting at the sign of each normal vector component. This
 * is used as an optimization for the box-on-plane-side test.
 */
byte SignBitsForPlane(const c_bsp_plane_t *plane) {
	int32_t i;
	byte bits = 0;

	for (i = 0; i < 3; i++) {
		if (plane->normal[i] < 0)
			bits |= 1 << i;
	}

	return bits;
}

/*
 * @brief Returns the sidedness of the given bounding box relative to the specified
 * plane. If the box straddles the plane, this function returns SIDE_BOTH.
 */
int32_t BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const c_bsp_plane_t *p) {
	vec_t dist1, dist2;
	int32_t sides;

	// axial planes
	if (AXIAL(p)) {
		if (p->dist - SIDE_EPSILON <= emins[p->type])
			return SIDE_FRONT;
		if (p->dist + SIDE_EPSILON >= emaxs[p->type])
			return SIDE_BACK;
		return SIDE_BOTH;
	}

	// general case
	switch (p->sign_bits) {
		case 0:
			dist1 = DotProduct(p->normal, emaxs);
			dist2 = DotProduct(p->normal, emins);
			break;
		case 1:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			break;
		case 2:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;
		case 3:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			break;
		case 4:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;
		case 5:
			dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
			break;
		case 6:
			dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
			dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
			break;
		case 7:
			dist1 = DotProduct(p->normal, emins);
			dist2 = DotProduct(p->normal, emaxs);
			break;
		default:
			dist1 = dist2 = 0.0; // shut up compiler
			break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = SIDE_FRONT;
	if (dist2 < p->dist)
		sides |= SIDE_BACK;

	return sides;
}

/*
 * @brief Initializes the specified bounds so that they may be safely calculated.
 */
void ClearBounds(vec3_t mins, vec3_t maxs) {
	mins[0] = mins[1] = mins[2] = 99999.0;
	maxs[0] = maxs[1] = maxs[2] = -99999.0;
}

/*
 * @brief Useful for accumulating a bounding box over a series of points.
 */
void AddPointToBounds(const vec3_t point, vec3_t mins, vec3_t maxs) {
	int32_t i;

	for (i = 0; i < 3; i++) {
		if (point[i] < mins[i])
			mins[i] = point[i];
		if (point[i] > maxs[i])
			maxs[i] = point[i];
	}
}

/*
 * @brief Returns true if the specified vectors are equal, false otherwise.
 */
_Bool VectorCompare(const vec3_t v1, const vec3_t v2) {

	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
		return false;

	return true;
}

/*
 * @brief Returns true if the first vector is closer to the point of interest, false
 * otherwise.
 */
_Bool VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t point) {
	vec3_t d1, d2;

	VectorSubtract(point, v1, d1);
	VectorSubtract(point, v2, d2);

	return VectorLength(d1) < VectorLength(d2);
}

/*
 * @brief Normalizes the specified vector to unit-length, returning the original
 * vector's length.
 */
vec_t VectorNormalize(vec3_t v) {
	vec_t length, ilength;

	length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	if (length) {
		ilength = 1.0 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

/*
 * @brief Scales vecb and adds it to veca to produce vecc. Useful for projection.
 */
void VectorMA(const vec3_t veca, const vec_t scale, const vec3_t vecb, vec3_t vecc) {
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

/*
 * @brief Calculates the cross-product of the specified vectors.
 */
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross) {
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

/*
 * @brief Returns the length of the specified vector.
 */
vec_t VectorLength(const vec3_t v) {
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/*
 * @brief Combines a fraction of the second vector with the first.
 */
void VectorMix(const vec3_t v1, const vec3_t v2, vec_t mix, vec3_t out) {
	int32_t i;

	for (i = 0; i < 3; i++)
		out[i] = v1[i] * (1.0 - mix) + v2[i] * mix;
}

/*
 * @brief Packs the specified floating point vector to the int16_t array in out
 * for network transmission.
 */
void PackVector(const vec3_t in, int16_t *out) {
	VectorScale(in, 8.0, out);
}

/*
 * @brief Unpacks the compressed vector to 32 bit floating point in out.
 */
void UnpackVector(const int16_t *in, vec3_t out) {
	VectorScale(in, 0.125, out);
}

/*
 * @brief Packs the specified floating point Euler angles to the int16_t array in out
 * for network transmission.
 */
void PackAngles(const vec3_t in, int16_t *out) {
	int32_t i;

	for (i = 0; i < 3; i++) {
		out[i] = PackAngle(in[i]);
	}
}

/*
 * @brief Unpacks the compressed angles to Euler 32 bit floating point in out.
 */
void UnpackAngles(const int16_t *in, vec3_t out) {
	int32_t i;

	for (i = 0; i < 3; i++) {
		out[i] = UnpackAngle(in[i]);
	}
}

/*
 * @brief Circularly clamps the specified angles between 0.0 and 360.0. Pitch is
 * clamped to not exceed 90' up or down.
 */
void ClampAngles(vec3_t angles) {
	int32_t i;

	// first wrap all angles to 0.0 - 360.0
	for (i = 0; i < 3; i++) {

		while (angles[i] > 360.0) {
			angles[i] -= 360.0;
		}

		while (angles[i] < 0.0) {
			angles[i] += 360.0;
		}
	}

	// clamp pitch to prevent the player from looking up or down more than 90'
	if (angles[PITCH] > 90.0 && angles[PITCH] < 270.0) {
		angles[PITCH] = 90.0;
	} else if (angles[PITCH] < 360.0 && angles[PITCH] >= 270.0) {
		angles[PITCH] -= 360.0;
	}
}

/*
 * @brief Packs the specified bounding box to a limited precision integer
 * representation. Bits 0-5 represent X/Y, scaled down by a factor of 0.125.
 * Bits 5-10 contain the Z-mins, and 10-15 contain the Z-maxs.
 */
void PackBounds(const vec3_t mins, const vec3_t maxs, uint16_t *out) {

	// x/y are assumed equal and symmetric
	int32_t xy = Clamp(maxs[0] * 0.125, 1.0, 31.0);

	// z is asymmetric
	int32_t zd = Clamp(-mins[2] * 0.125, 1.0, 31.0);

	// and z maxs can be negative, so shift them +32 units
	int32_t zu = Clamp((maxs[2] + 32.0) * 0.125, 1.0, 63.0);

	*out = (zu << 10) | (zd << 5) | xy;
}

/*
 * @brief Unpacks the specified bounding box to mins and maxs.
 */
void UnpackBounds(const uint16_t in, vec3_t mins, vec3_t maxs) {

	const vec_t xy = (in & 31) * 8.0;
	const vec_t zd = ((in >> 5) & 31) * 8.0;
	const vec_t zu = ((in >> 10 & 63) - 32) * 8.0;

	VectorSet(mins, -xy, -xy, -zd);
	VectorSet(maxs, xy, xy, zu);
}

/*
 * @brief Clamps the components of the specified vector to 1.0, scaling the vector
 * down if necessary.
 */
vec_t ColorNormalize(const vec3_t in, vec3_t out) {
	vec_t max = 0.0;
	int32_t i;

	VectorCopy(in, out);

	for (i = 0; i < 3; i++) { // find the brightest component

		if (out[i] < 0.0) // enforcing positive values
			out[i] = 0.0;

		if (out[i] > max)
			max = out[i];
	}

	if (max > 1.0) // clamp without changing hue
		VectorScale(out, 1.0 / max, out);

	return max;
}

/*
 * @brief Applies brightness, saturation and contrast to the specified input color.
 */
void ColorFilter(const vec3_t in, vec3_t out, vec_t brightness, vec_t saturation, vec_t contrast) {
	const vec3_t luminosity = { 0.2125, 0.7154, 0.0721 };
	vec3_t intensity;
	vec_t d;
	int32_t i;

	ColorNormalize(in, out);

	if (brightness != 1.0) { // apply brightness
		VectorScale(out, brightness, out);

		ColorNormalize(out, out);
	}

	if (contrast != 1.0) { // apply contrast

		for (i = 0; i < 3; i++) {
			out[i] -= 0.5; // normalize to -0.5 through 0.5
			out[i] *= contrast; // scale
			out[i] += 0.5;
		}

		ColorNormalize(out, out);
	}

	if (saturation != 1.0) { // apply saturation
		d = DotProduct(out, luminosity);

		VectorSet(intensity, d, d, d);
		VectorMix(intensity, out, saturation, out);

		ColorNormalize(out, out);
	}
}

/*
 * @brief Returns true if the specified string has some upper case characters.
 */
_Bool MixedCase(const char *s) {
	const char *c = s;
	while (*c) {
		if (isupper(*c))
			return true;
		c++;
	}
	return false;
}

/*
 * @brief Lowercases the specified string.
 */
void Lowercase(const char *in, char *out) {
	while (*in) {
		*out++ = tolower(*in++);
	}
	*out = '\0';
}

/*
 * @brief Trims leading and trailing whitespace from the specified string.
 */
char *Trim(char *s) {
	char *left, *right;

	left = s;

	while (isspace(*left))
		left++;

	right = left + strlen(left) - 1;

	while (isspace(*right))
		*right-- = '\0';

	return left;
}

/*
 * @brief Returns the longest common prefix the specified words share.
 */
char *CommonPrefix(GList *words) {
	static char common_prefix[MAX_TOKEN_CHARS];
	size_t i;

	memset(common_prefix, 0, sizeof(common_prefix));

	if (!words)
		return common_prefix;

	for (i = 0; i < sizeof(common_prefix) - 1; i++) {
		GList *e = words;
		const char c = ((char *) e->data)[i];
		while (e) {
			const char *w = (char *) e->data;

			if (!c || w[i] != c) // prefix no longer common
				return common_prefix;

			e = e->next;
		}
		common_prefix[i] = c;
	}

	return common_prefix;
}

/*
 * @brief Handles wildcard suffixes for GlobMatch.
 */
static _Bool GlobMatchStar(const char *pattern, const char *text) {
	const char *p = pattern, *t = text;
	register char c, c1;

	while ((c = *p++) == '?' || c == '*')
		if (c == '?' && *t++ == '\0')
			return false;

	if (c == '\0')
		return true;

	if (c == '\\')
		c1 = *p;
	else
		c1 = c;

	while (true) {
		if ((c == '[' || *t == c1) && GlobMatch(p - 1, t))
			return true;
		if (*t++ == '\0')
			return false;
	}

	return false;
}

/*
 * @brief Matches the pattern against specified text, returning true if the pattern
 * matches, false otherwise.
 *
 * A match means the entire string TEXT is used up in matching.
 *
 * In the pattern string, `*' matches any sequence of characters,
 * `?' matches any character, [SET] matches any character in the specified set,
 * [!SET] matches any character not in the specified set.
 *
 * A set is composed of characters or ranges; a range looks like
 * character hyphen character(as in 0-9 or A-Z).
 * [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
 * Any other character in the pattern must be matched exactly.
 *
 * To suppress the special syntactic significance of any of `[]*?!-\',
 * and match the character exactly, precede it with a `\'.
 */
_Bool GlobMatch(const char *pattern, const char *text) {
	const char *p = pattern, *t = text;
	register char c;

	if (!p || !t) {
		return false;
	}

	while ((c = *p++) != '\0')
		switch (c) {
			case '?':
				if (*t == '\0')
					return 0;
				else
					++t;
				break;

			case '\\':
				if (*p++ != *t++)
					return 0;
				break;

			case '*':
				return GlobMatchStar(p, t);

			case '[': {
				register char c1 = *t++;
				int32_t invert;

				if (!c1)
					return 0;

				invert = ((*p == '!') || (*p == '^'));
				if (invert)
					p++;

				c = *p++;
				while (true) {
					register char cstart = c, cend = c;

					if (c == '\\') {
						cstart = *p++;
						cend = cstart;
					}
					if (c == '\0')
						return 0;

					c = *p++;
					if (c == '-' && *p != ']') {
						cend = *p++;
						if (cend == '\\')
							cend = *p++;
						if (cend == '\0')
							return 0;
						c = *p++;
					}
					if (c1 >= cstart && c1 <= cend)
						goto match;
					if (c == ']')
						break;
				}
				if (!invert)
					return 0;
				break;

				match:
				/* Skip the rest of the [...] construct that already matched. */
				while (c != ']') {
					if (c == '\0')
						return 0;
					c = *p++;
					if (c == '\0')
						return 0;
					else if (c == '\\')
						++p;
				}
				if (invert)
					return 0;
				break;
			}

			default:
				if (c != *t++)
					return 0;
				break;
		}

	return *t == '\0';
}

/*
 * @brief Returns the base name for the given file or path.
 */
const char *Basename(const char *path) {
	const char *last;

	last = path;
	while (*path) {
		if (*path == '/')
			last = path + 1;
		path++;
	}
	return last;
}

/*
 * @brief Returns the directory name for the given file or path name.
 */
void Dirname(const char *in, char *out) {
	char *c;

	if (!(c = strrchr(in, '/'))) {
		strcpy(out, "./");
		return;
	}

	while (in <= c) {
		*out++ = *in++;
	}

	*out = '\0';
}

/*
 * @brief Removes any file extension(s) from the specified input string.
 */
void StripExtension(const char *in, char *out) {

	while (*in && *in != '.') {
		*out++ = *in++;
	}

	*out = '\0';
}

/*
 * @brief Strips color escape sequences from the specified input string.
 */
void StripColor(const char *in, char *out) {

	while (*in) {

		if (IS_COLOR(in)) {
			in += 2;
			continue;
		}

		if (IS_LEGACY_COLOR(in)) {
			in++;
			continue;
		}

		*out++ = *in++;
	}
	*out = '\0';
}

/*
 * @brief Performs a color- and case-insensitive string comparison.
 */
int32_t StrColorCmp(const char *s1, const char *s2) {
	char string1[MAX_STRING_CHARS], string2[MAX_STRING_CHARS];

	StripColor(s1, string1);
	StripColor(s2, string2);

	return g_ascii_strcasecmp(string1, string2);
}

/*
 * @brief A shorthand g_snprintf into a statically allocated buffer. Several
 * buffers are maintained internally so that nested va()'s are safe within
 * reasonable limits. This function is not thread safe.
 */
char *va(const char *format, ...) {
	static char strings[8][MAX_STRING_CHARS];
	static uint16_t index;

	char *string = strings[index++ % 8];

	va_list args;

	va_start(args, format);
	vsnprintf(string, MAX_STRING_CHARS, format, args);
	va_end(args);

	return string;
}

/*
 * @brief A convenience function for printing vectors.
 */
char *vtos(const vec3_t v) {
	static uint32_t index;
	static char str[8][32];
	char *s;

	// use an array so that multiple vtos won't collide
	s = str[index++ % 8];

	g_snprintf(s, 32, "(%04.2f %04.2f %04.2f)", v[0], v[1], v[2]);

	return s;
}

/*
 * @brief Parse a token out of a string. Tokens are delimited by white space, and
 * may be grouped by quotation marks.
 */
char *ParseToken(const char **data_p) {
	static char token[MAX_TOKEN_CHARS];
	int32_t c;
	int32_t len;
	const char *data;

	data = *data_p;
	len = 0;
	token[0] = '\0';

	if (!data) {
		*data_p = NULL;
		return "";
	}

	// skip whitespace
	skipwhite: while ((c = *data) <= ' ') {
		if (c == '\0') {
			*data_p = NULL;
			return "";
		}
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (true) {
			c = *data++;
			if (c == '\"' || !c) {
				token[len] = '\0';
				*data_p = data;
				return token;
			}
			if (len < MAX_TOKEN_CHARS) {
				token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do {
		if (len < MAX_TOKEN_CHARS) {
			token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c > 32);

	if (len == MAX_TOKEN_CHARS) {
		len = 0;
	}
	token[len] = '\0';

	*data_p = data;
	return token;
}

/*
 * @brief Searches the string for the given key and returns the associated value,
 * or an empty string.
 */
char *GetUserInfo(const char *s, const char *key) {
	char pkey[512];
	static char value[2][512]; // use two buffers so compares
	// work without stomping on each other
	static int32_t value_index;
	char *o;

	value_index ^= 1;
	if (*s == '\\')
		s++;
	while (true) {
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = '\0';
		s++;

		o = value[value_index];

		while (*s != '\\' && *s) {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = '\0';

		if (!g_strcmp0(key, pkey))
			return value[value_index];

		if (!*s)
			return "";
		s++;
	}

	return "";
}

/*
 * @brief
 */
void DeleteUserInfo(char *s, const char *key) {
	char *start;
	char pkey[512];
	char value[512];
	char *o;

	if (strstr(key, "\\")) {
		return;
	}

	while (true) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = '\0';
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = '\0';

		if (!g_strcmp0(key, pkey)) {
			strcpy(start, s); // remove this part
			return;
		}

		if (!*s)
			return;
	}
}

/*
 * @brief Returns true if the specified user-info string appears valid, false
 * otherwise.
 */
_Bool ValidateUserInfo(const char *s) {
	if (!s || !*s)
		return false;
	if (strstr(s, "\""))
		return false;
	if (strstr(s, ";"))
		return false;
	return true;
}

/*
 * @brief
 */
void SetUserInfo(char *s, const char *key, const char *value) {
	char newi[MAX_USER_INFO_STRING], *v;
	int32_t c;
	uint32_t max_size = MAX_USER_INFO_STRING;

	if (strstr(key, "\\") || strstr(value, "\\")) {
		//Com_Print("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr(key, ";")) {
		//Com_Print("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr(key, "\"") || strstr(value, "\"")) {
		//Com_Print("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_USER_INFO_KEY - 1 || strlen(value) > MAX_USER_INFO_VALUE - 1) {
		//Com_Print("Keys and values must be < 64 characters\n");
		return;
	}
	DeleteUserInfo(s, key);
	if (!value || *value == '\0')
		return;

	g_snprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > max_size) {
		//Com_Print("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v) {
		c = *v++;
		c &= 127; // strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = '\0';
}
