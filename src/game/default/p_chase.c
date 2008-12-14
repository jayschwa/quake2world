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

#include "g_local.h"

void P_UpdateChaseCam(edict_t *ent){
	const edict_t *targ;

	targ = ent->client->chase_target;

	// copy origin
	VectorCopy(targ->s.origin, ent->s.origin);

	// and angles
	VectorCopy(targ->client->angles, ent->client->ps.angles);
	VectorCopy(targ->client->angles, ent->client->angles);

	if(targ->dead)  // drop view towards the floor
		ent->client->ps.pmove.pm_flags |= PMF_DUCKED;

	// disable the spectator's input
	ent->client->ps.pmove.pm_type = PM_FREEZE;

	// disable client prediction
	ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

	gi.LinkEntity(ent);
}


/*
P_ChaseNext
*/
void P_ChaseNext(edict_t *ent){
	int i;
	edict_t *e;

	if(!ent->client->chase_target)
		return;

	i = ent->client->chase_target - g_edicts;
	do {
		i++;

		if(i > sv_maxclients->value)
			i = 1;

		e = g_edicts + i;

		if(!e->inuse)
			continue;

		if(!e->client->locals.spectator)
			break;

	} while(e != ent->client->chase_target);

	ent->client->chase_target = e;
}


/*
P_ChasePrev
*/
void P_ChasePrev(edict_t *ent){
	int i;
	edict_t *e;

	if(!ent->client->chase_target)
		return;

	i = ent->client->chase_target - g_edicts;
	do {
		i--;

		if(i < 1)
			i = sv_maxclients->value;

		e = g_edicts + i;

		if(!e->inuse)
			continue;

		if(!e->client->locals.spectator)
			break;

	} while(e != ent->client->chase_target);

	ent->client->chase_target = e;
}


/*
P_GetChaseTarget
*/
void P_GetChaseTarget(edict_t *ent){
	int i;
	edict_t *other;

	for(i = 1; i <= sv_maxclients->value; i++){
		other = g_edicts + i;
		if(other->inuse && !other->client->locals.spectator){
			ent->client->chase_target = other;
			P_UpdateChaseCam(ent);
			return;
		}
	}
}
