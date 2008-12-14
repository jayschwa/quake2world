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

/*
G_OnSameTeam

Returns true if ent1 and ent2 are on the same qmass mod team.
*/
qboolean G_OnSameTeam(edict_t *ent1, edict_t *ent2){

	if(!level.teams && !level.ctf)
		return false;

	if(!ent1->client || !ent2->client)
		return false;

	return ent1->client->locals.team == ent2->client->locals.team;
}

/*
G_CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
*/
qboolean G_CanDamage(edict_t *targ, edict_t *inflictor){
	vec3_t dest;
	trace_t trace;

	// bmodels need special checking because their origin is 0,0,0
	if(targ->movetype == MOVETYPE_PUSH){
		VectorAdd(targ->absmin, targ->absmax, dest);
		VectorScale(dest, 0.5, dest);
		trace = gi.Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
		if(trace.fraction == 1.0)
			return true;
		if(trace.ent == targ)
			return true;
		return false;
	}

	trace = gi.Trace(inflictor->s.origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, MASK_SOLID);
	if(trace.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	trace = gi.Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if(trace.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	trace = gi.Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if(trace.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	trace = gi.Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if(trace.fraction == 1.0)
		return true;

	VectorCopy(targ->s.origin, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	trace = gi.Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
	if(trace.fraction == 1.0)
		return true;

	return false;
}


/*
G_Killed
*/
static void G_Killed(edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point){
	if(targ->health < -999)
		targ->health = -999;

	targ->enemy = attacker;

	if(targ->movetype == MOVETYPE_PUSH || targ->movetype == MOVETYPE_STOP || targ->movetype == MOVETYPE_NONE){  // doors, triggers, etc
		targ->die(targ, inflictor, attacker, damage, point);
		return;
	}

	targ->die(targ, inflictor, attacker, damage, point);
}


/*
G_SpawnDamage
*/
static void G_SpawnDamage(int type, vec3_t origin, vec3_t normal, int damage){
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(type);
	gi.WritePosition(origin);
	gi.WriteDir(normal);
	gi.Multicast(origin, MULTICAST_PVS);
}


/*
G_CheckArmor
*/
static int G_CheckArmor(edict_t *ent, vec3_t point, vec3_t normal, int damage, int dflags){
	gclient_t *client;
	int saved;

	if(damage < 1)
		return 0;

	if(damage < 2)  // sometimes protect very small damage
		damage = rand() & 1;
	else
		damage *= 0.80;  // mostly protect large damage

	client = ent->client;

	if(!client)
		return 0;

	if(dflags & DAMAGE_NO_ARMOR)
		return 0;

	if(damage > ent->client->locals.armor)
		saved = ent->client->locals.armor;
	else
		saved = damage;

	ent->client->locals.armor -= saved;

	G_SpawnDamage(TE_BLOOD, point, normal, saved);

	return saved;
}


/*
G_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=player2, inflictor=rocket, attacker=player1

dir			direction of the attack
point       point at which the damage is being inflicted
normal		normal vector from that point
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

dflags		these flags are used to control how G_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_ENERGY			damage is from an energy based weapon
	DAMAGE_BULLET			damage is from a bullet (used for ricochets)
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
*/
void G_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir,
		vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod){

	gclient_t *client;
	int take;
	int save;
	int asave;
	int te_sparks;
	float scale;

	if(!targ->takedamage)
		return;

	if(!inflictor) // use world
		inflictor = &g_edicts[0];

	if(!attacker)  // use world
		attacker = &g_edicts[0];

	// friendly fire avoidance
	if(targ != attacker && (level.teams || level.ctf)){
		if(G_OnSameTeam(targ, attacker)){  // targ and attacker are teammates
			if((int)(g_dmflags->value) & DF_NO_FRIENDLY_FIRE && mod != MOD_TELEFRAG)
				damage = 0;  // no ff is on, and it's not a telefrag
			else
				mod |= MOD_FRIENDLY_FIRE;
		}
	}

	if(targ == attacker && level.gameplay)  // no self damage in instagib or arena
		damage = 0;

	meansOfDeath = mod;

	client = targ->client;

	VectorNormalize(dir);

	// calculate velocity change due to knockback
	if(knockback && (targ->movetype != MOVETYPE_NONE) &&
		(targ->movetype != MOVETYPE_BOUNCE) &&
		(targ->movetype != MOVETYPE_PUSH) &&
		(targ->movetype != MOVETYPE_STOP) &&
		(targ->movetype != MOVETYPE_THINK)){

		vec3_t kvel;
		float mass;

		if(targ->mass < 50.0)
			mass = 50.0;
		else
			mass = targ->mass;

		scale = 1000.0;  // default knockback scale

		if(targ == attacker){  // weapon jump hacks
			if(mod == MOD_BFG_BLAST)
				scale = 300.0;
			else if(mod == MOD_R_SPLASH)
				scale = 1600.0;
			else if(mod == MOD_GRENADE)
				scale = 1200.0;
		}

		VectorScale(dir, scale * (float)knockback / mass, kvel);
		VectorAdd(targ->velocity, kvel, targ->velocity);
	}

	take = damage;
	save = 0;

	// check for godmode
	if((targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION)){
		take = 0;
		save = damage;
		G_SpawnDamage(TE_BLOOD, point, normal, save);
	}

	asave = G_CheckArmor(targ, point, normal, take, dflags);
	take -= asave;

	// treat cheat savings the same as armor
	asave += save;

	// do the damage
	if(take){
		if(client)
			G_SpawnDamage(TE_BLOOD, point, normal, take);
		else {
			// impact effects for things we can hurt which shouldn't bleed
			if(dflags & DAMAGE_BULLET)
				te_sparks = TE_BULLET;
			else
				te_sparks = TE_SPARKS;

			G_SpawnDamage(te_sparks, point, normal, take);
		}

		targ->health = targ->health - take;

		if(targ->health <= 0){
			G_Killed(targ, inflictor, attacker, take, point);
			return;
		}
	}

	if(client){
		if(!(targ->flags & FL_GODMODE) && take)
			targ->pain(targ, attacker, take, knockback);
	} else if(take){
		if(targ->pain)
			targ->pain(targ, attacker, take, knockback);
	}

	// add to the damage inflicted on a player this frame
	if(client){
		client->damage_armor += asave;
		client->damage_blood += take;
		VectorCopy(point, client->damage_from);
	}
}


/*
G_RadiusDamage
*/
void G_RadiusDamage(edict_t *inflictor, edict_t *attacker, edict_t *ignore,
		int damage, int knockback, float radius, int mod){
	edict_t *ent;
	float d, k, dist;
	vec3_t dir;

	ent = NULL;

	while((ent = G_FindRadius(ent, inflictor->s.origin, radius)) != NULL){

		if(ent == ignore)
			continue;

		if(!ent->takedamage)
			continue;

		VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
		dist = VectorLength(dir);

		d = damage - 0.5 * dist;
		k = knockback - 0.5 * dist;

		if(d <= 0 && k <= 0)  // too far away to be damaged
			continue;

		if(ent == attacker){  // reduce self damage
			if(mod == MOD_BFG_BLAST)
				d = d * 0.15;
			else
				d = d * 0.5;
		}

		if(!G_CanDamage(ent, inflictor))
			continue;

		G_Damage(ent, inflictor, attacker, dir, inflictor->s.origin,
				vec3_origin, (int)d, (int)k, DAMAGE_RADIUS, mod);
	}
}