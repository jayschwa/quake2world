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

#ifndef __GAME_TYPES_H__ // don't collide with <glib/gtypes.h>
#define __GAME_TYPES_H__

#include "shared.h"

/*
 * @brief Game-specific server protocol commands. These are parsed directly by
 * the client game module.
 */
typedef enum {
	SV_CMD_CENTER_PRINT = SV_CMD_CGAME,
	SV_CMD_MUZZLE_FLASH,
	SV_CMD_SCORES,
	SV_CMD_TEMP_ENTITY
} g_sv_packet_cmd_t;

/*
 * @brief Game-specific client protocol commands. These are parsed directly by
 * the game module.
 */
typedef enum {
	CL_CMD_EXAMPLE
} g_cl_packet_cmd_t;

/*
 * @brief ConfigStrings that are local to the game module.
 */
#define CS_GAMEPLAY			(CS_GENERAL + 0) // gameplay string
#define CS_TEAMS			(CS_GENERAL + 1) // are teams enabled?
#define CS_CTF				(CS_GENERAL + 2) // is capture enabled?
#define CS_MATCH			(CS_GENERAL + 3) // is match mode enabled?
#define CS_ROUNDS			(CS_GENERAL + 4) // are rounds enabled?
#define CS_TEAM_GOOD		(CS_GENERAL + 5) // good team name
#define CS_TEAM_EVIL		(CS_GENERAL + 6) // evil team name
#define CS_TIME				(CS_GENERAL + 7) // level or match timer
#define CS_ROUND			(CS_GENERAL + 8) // round number
#define CS_VOTE				(CS_GENERAL + 9) // vote string\yes count\no count
/*
 * @brief Player state statistics (inventory, score, etc).
 */
typedef enum {
	STAT_AMMO,
	STAT_AMMO_ICON,
	STAT_AMMO_LOW,
	STAT_ARMOR,
	STAT_ARMOR_ICON,
	STAT_CAPTURES,
	STAT_CHASE,
	STAT_DAMAGE_ARMOR,
	STAT_DAMAGE_HEALTH,
	STAT_DAMAGE_INFLICT,
	STAT_FRAGS,
	STAT_HEALTH,
	STAT_HEALTH_ICON,
	STAT_PICKUP_ICON,
	STAT_PICKUP_STRING,
	STAT_READY,
	STAT_ROUND,
	STAT_SCORES,
	STAT_SPECTATOR,
	STAT_TEAM,
	STAT_TIME,
	STAT_VOTE,
	STAT_WEAPON,
	STAT_WEAPON_ICON
} g_stat_t;

/*
 * @brief Forces a statistic field to be re-sent, even if the value has not changed.
 */
#define STAT_TOGGLE_BIT		0x8000

/*
 * @brief Muzzle flashes are bound to the entity that created them. This allows
 * the protocol to forego sending the origin and angles for the effect, as they
 * can be inferred from the referenced entity.
 */
typedef enum {
	MZ_BLASTER,
	MZ_SHOTGUN,
	MZ_SSHOTGUN,
	MZ_MACHINEGUN,
	MZ_GRENADE,
	MZ_ROCKET,
	MZ_HYPERBLASTER,
	MZ_LIGHTNING,
	MZ_RAILGUN,
	MZ_BFG,
	MZ_LOGOUT,
} g_muzzle_flash_t;

/*
 * @brief Temporary entities are positional events that are not explicitly
 * bound to a game entity (g_edict_t). Examples are explosions, certain weapon
 * trails and other short-lived effects.
 */
typedef enum {
	TE_BLASTER,
	TE_TRACER,
	TE_BULLET,
	TE_BURN,
	TE_BLOOD,
	TE_SPARKS,
	TE_HYPERBLASTER,
	TE_LIGHTNING,
	TE_RAIL,
	TE_EXPLOSION,
	TE_BUBBLES,
	TE_BFG,
	TE_GIB
} g_temp_entity_t;

/*
 * @brief Player scores are transmitted as binary to the client game module.
 */
typedef struct {
	uint16_t client;
	uint16_t ping;
	uint8_t team;
	uint8_t color;
	int16_t score;
	int16_t captures;
	uint8_t flags;
} g_score_t;

/*
 * @brief Game-specific entity events.
 */
typedef enum {
	EV_CLIENT_DROWN = EV_GAME,
	EV_CLIENT_FALL,
	EV_CLIENT_FALL_FAR,
	EV_CLIENT_FOOTSTEP,
	EV_CLIENT_GURP,
	EV_CLIENT_JUMP,
	EV_CLIENT_LAND,
	EV_ITEM_RESPAWN,
	EV_ITEM_PICKUP,
} g_entity_event_t;

/*
 * @brief Game-specific entity effects.
 */
#define EF_BLASTER			(EF_GAME << 0) // particle trail above water, bubble trail in water
#define EF_GRENADE			(EF_GAME << 1) // smoke trail above water, bubble trail in water
#define EF_ROCKET			(EF_GAME << 2) // smoke trail above water, bubble trail in water
#define EF_HYPERBLASTER		(EF_GAME << 3) // bubble trail in water
#define EF_LIGHTNING		(EF_GAME << 4) // lightning bolt
#define EF_BFG				(EF_GAME << 5) // big particle snotball
#define EF_TELEPORTER		(EF_GAME << 6) // particle fountain
#define EF_QUAD				(EF_GAME << 7) // quad damage
#define EF_CTF_BLUE			(EF_GAME << 8) // blue flag carrier
#define EF_CTF_RED			(EF_GAME << 9) // red flag carrier
#define EF_BEAM				(EF_GAME << 10) // overload old_origin for 2nd endpoint
#define EF_INACTIVE			(EF_GAME << 11) // inactive icon for when input is not going to game
#define EF_RESPAWN			(EF_GAME << 12) // respawn protection

/*
 * @brief Player scores flags.
 */
#define SCORES_NOT_READY	(1 << 0)
#define SCORES_CTF_FLAG		(1 << 1)

/*
 * @brief Effect colors for particle trails and dynamic light flashes.
 */
#define EFFECT_COLOR_RED 232
#define EFFECT_COLOR_GREEN 201
#define EFFECT_COLOR_BLUE 119
#define EFFECT_COLOR_YELLOW 219
#define EFFECT_COLOR_ORANGE 225
#define EFFECT_COLOR_WHITE 216
#define EFFECT_COLOR_PINK 247
#define EFFECT_COLOR_PURPLE 187
#define EFFECT_COLOR_DEFAULT 0

/*
 * @brief Scoreboard background colors.
 */
#define TEAM_COLOR_RED 242
#define TEAM_COLOR_BLUE 243

#ifdef __GAME_LOCAL_H__

/*
 * @brief This file will define the game-visible definitions of g_client_t
 * and g_edict_t. They are much larger than the server-visible definitions,
 * which are intentionally truncated stubs.
 */
typedef struct g_client_s g_client_t;
typedef struct g_edict_s g_edict_t;

/*
 * @brief Spawn flags for g_edict_t are set in the level editor.
 */
#define SF_ITEM_TRIGGER			0x00000001
#define SF_ITEM_NO_TOUCH		0x00000002
#define SF_ITEM_HOVER			0x00000004

/*
 * @brief These are legacy spawn flags from Quake II. We maintain these simply
 * for backwards compatibility with old levels. They do nothing in Q2W.
 */
#define SF_NOT_EASY				0x00000100
#define SF_NOT_MEDIUM			0x00000200
#define SF_NOT_HARD				0x00000400
#define SF_NOT_DEATHMATCH		0x00000800
#define SF_NOT_COOP				0x00001000

/*
 * @brief These spawn flags are actually set by the game module on edicts that
 * are programmatically instantiated.
 */
#define SF_ITEM_DROPPED			0x00010000
#define SF_ITEM_TARGETS_USED	0x00020000

/*
 * @brief Edict flags (g_edict_locals.flags). These again are mostly for
 * backwards compatibility with Quake II. Team slaves and respawn are
 * still valid.
 */
#define FL_FLY					0x00000001
#define FL_SWIM					0x00000002  // implied immunity to drowning
#define FL_GOD_MODE				0x00000004
#define FL_TEAM_SLAVE			0x00000008  // not the first on the team
#define FL_RESPAWN				0x80000000

/*
 * @brief Ammunition types.
 */
typedef enum {
	AMMO_NONE,
	AMMO_SHELLS,
	AMMO_BULLETS,
	AMMO_GRENADES,
	AMMO_ROCKETS,
	AMMO_CELLS,
	AMMO_BOLTS,
	AMMO_SLUGS,
	AMMO_NUKES
} g_ammo_t;

/*
 * @brief Armor types.
 */
typedef enum {
	ARMOR_NONE,
	ARMOR_JACKET,
	ARMOR_COMBAT,
	ARMOR_BODY,
	ARMOR_SHARD
} g_armor_t;

/*
 * @brief Health types.
 */
typedef enum {
	HEALTH_NONE,
	HEALTH_SMALL,
	HEALTH_MEDIUM,
	HEALTH_LARGE,
	HEALTH_MEGA
} g_health_t;

/*
 * @brief Move types govern the physics dispatch in G_RunEntity.
 */
typedef enum {
	MOVE_TYPE_NONE, // never moves
	MOVE_TYPE_NO_CLIP, // origin and angles change with no interaction
	MOVE_TYPE_PUSH, // no clip to world, push on box contact
	MOVE_TYPE_STOP, // no clip to world, stops on box contact

	MOVE_TYPE_WALK, // gravity
	MOVE_TYPE_FLY,
	MOVE_TYPE_TOSS,
	// gravity
} g_move_type_t;

/*
 * @brief A synonym for readability; MOVE_TYPE_THINK implies that the entity's
 * Think function will update its origin and handle other interactions.
 */
#define MOVE_TYPE_THINK MOVE_TYPE_NONE

/*
 * @brief Item types.
 */
typedef enum {
	ITEM_AMMO,
	ITEM_ARMOR,
	ITEM_FLAG,
	ITEM_HEALTH,
	ITEM_POWERUP,
	ITEM_WEAPON
} g_item_type_t;

/*
 * @brief Items are touchable entities that players visit to acquire inventory.
 */
typedef struct g_item_s {
	const char *class_name; // spawning name

	_Bool (*Pickup)(g_edict_t *ent, g_edict_t *other);
	void (*Use)(g_edict_t *ent, const struct g_item_s *item);
	void (*Drop)(g_edict_t *ent, const struct g_item_s *item);
	void (*Think)(g_edict_t *ent);

	const char *pickup_sound;
	const char *model;
	uint32_t effects;

	const char *icon; // for the HUD and pickup
	const char *name; // for printing on pickup

	uint16_t quantity; // for ammo: how much, for weapons: how much per shot
	const char *ammo; // for weapons: the ammo item name

	g_item_type_t type; // g_item_type_t, see above
	uint16_t tag; // type-specific flags
	vec_t priority; // AI priority level

	const char *precaches; // string of all models, sounds, and images this item will use
} g_item_t;

/*
 * @brief A singleton container used to hold entity information that is set
 * in the editor (and thus the entities string) but that does not map directly
 * to a field in g_edict_t.
 */
typedef struct {
	// world vars, we use strings to avoid ambiguity between 0 and unset
	char *sky;
	char *weather;
	char *gravity;
	char *gameplay;
	char *teams;
	char *ctf;
	char *match;
	char *rounds;
	char *frag_limit;
	char *round_limit;
	char *capture_limit;
	char *time_limit;
	char *give;
	char *music;

	int32_t lip;
	int32_t distance;
	int32_t height;
	char *noise;
	char *item;
} g_spawn_temp_t;

#define EOFS(x) (ptrdiff_t)&(((g_edict_t *) 0)->x)
#define LOFS(x) (ptrdiff_t)&(((g_edict_t *) 0)->locals.x)
#define SOFS(x) (ptrdiff_t)&(((g_spawn_temp_t *) 0)->x)

/*
 * @brief Movement states.
 */
typedef enum {
	MOVE_STATE_TOP,
	MOVE_STATE_BOTTOM,
	MOVE_STATE_GOING_UP,
	MOVE_STATE_GOING_DOWN
} g_move_state_t;

/*
 * @brief Physics parameters and think functions for entities which move.
 */
typedef struct {
	// fixed data
	vec3_t start_origin;
	vec3_t start_angles;
	vec3_t end_origin;
	vec3_t end_angles;

	uint16_t sound_start;
	uint16_t sound_middle;
	uint16_t sound_end;

	vec_t accel;
	vec_t speed;
	vec_t decel;
	vec_t distance;

	vec_t wait;

	// state data
	g_move_state_t state;
	vec3_t dir;
	vec_t current_speed;
	vec_t move_speed;
	vec_t next_speed;
	vec_t remaining_distance;
	vec_t decel_distance;
	void (*Done)(g_edict_t *);
} g_move_info_t;

/*
 * @brief This structure is initialized when the game module is loaded and
 * remains in tact until it is unloaded. The server receives the pointers
 * within this structure so that it may e.g. iterate over entities.
 */
typedef struct {
	g_edict_t *edicts; // [g_max_entities]
	g_client_t *clients; // [sv_max_clients]

	g_spawn_temp_t spawn;
} g_game_t;

extern g_game_t g_game;

/*
 * @brief This structure holds references to frequently accessed media.
 */
typedef struct {
	uint16_t grenade_model;
	uint16_t grenade_hit_sound;

	uint16_t rocket_model;
	uint16_t rocket_fly_sound;

	uint16_t lightning_fly_sound;

	uint16_t quad_damage;
} g_media_t;

/*
 * @brief The main structure for all world management. This is cleared at each
 * level load.
 */
typedef struct {
	uint32_t frame_num;
	uint32_t time;

	char title[MAX_STRING_CHARS]; // the descriptive name (Stress Fractures, etc)
	char name[MAX_QPATH]; // the server name (fractures, etc)
	int32_t gravity; // defaults to 800
	int32_t gameplay; // DEATHMATCH, INSTAGIB, ARENA
	int32_t teams;
	int32_t ctf;
	int32_t match;
	int32_t rounds;
	int32_t frag_limit;
	int32_t round_limit;
	int32_t capture_limit;
	uint32_t time_limit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];

	// intermission state
	uint32_t intermission_time; // time intermission started
	vec3_t intermission_origin;
	vec3_t intermission_angle;
	const char *changemap;

	_Bool warmup; // shared by match and round

	_Bool start_match;
	uint32_t match_time; // time match started
	uint32_t match_num;

	_Bool start_round;
	uint32_t round_time; // time round started
	uint32_t round_num;

	char vote_cmd[64]; // current vote in question
	uint32_t votes[3]; // current vote count (yes/no/undecided)
	uint32_t vote_time; // time vote started

	g_media_t media;

	g_edict_t *current_entity; // entity running from G_RunFrame
} g_level_t;

/*
 * @brief Means of death.
 */
#define MOD_UNKNOWN					0
#define MOD_BLASTER					1
#define MOD_SHOTGUN					2
#define MOD_SUPER_SHOTGUN			3
#define MOD_MACHINEGUN				4
#define MOD_GRENADE					5
#define MOD_GRENADE_SPLASH			6
#define MOD_ROCKET					7
#define MOD_ROCKET_SPLASH			8
#define MOD_HYPERBLASTER			9
#define MOD_LIGHTNING				10
#define MOD_LIGHTNING_DISCHARGE		11
#define MOD_RAILGUN					12
#define MOD_BFG_LASER				13
#define MOD_BFG_BLAST				14
#define MOD_WATER					15
#define MOD_SLIME					16
#define MOD_LAVA					17
#define MOD_CRUSH					18
#define MOD_TELEFRAG				19
#define MOD_FALLING					20
#define MOD_SUICIDE					21
#define MOD_EXPLOSIVE				22
#define MOD_TRIGGER_HURT			23
#define MOD_FRIENDLY_FIRE			0x8000000

/*
 * @brief Voting constants.
 */
#define MAX_VOTE_TIME 60000
#define VOTE_MAJORITY 0.51

typedef enum {
	VOTE_NO_OP,
	VOTE_YES,
	VOTE_NO
} g_vote_t;

/*
 * @brief Game modes. These are selected via g_gameplay.
 */
typedef enum {
	DEFAULT,
	DEATHMATCH,
	INSTAGIB,
	ARENA
} g_gameplay_t;

/*
 * @brief Team name and team skin changes are throttled.
 */
#define TEAM_CHANGE_TIME 5000

/*
 * @brief Damage flags. These can be and often are combined.
 */
#define DAMAGE_RADIUS			0x00000001  // damage was indirect
#define DAMAGE_NO_ARMOR			0x00000002  // armor does not protect from this damage
#define DAMAGE_ENERGY			0x00000004  // damage is from an energy based weapon
#define DAMAGE_BULLET			0x00000008  // damage is from a bullet (used for ricochets)
#define DAMAGE_NO_PROTECTION	0x00000010  // armor and godmode have no effect

/*
 * @brief There are two teams in the default game module.
 */
typedef struct {
	char name[16]; // kept short for HUD consideration
	char skin[32];
	int16_t score;
	int16_t captures;
	uint32_t name_time;
	uint32_t skin_time;
} g_team_t;

#define MAX_NET_NAME 64

/*
 * @brief This structure contains client data that persists over multiple
 * respawns.
 */
typedef struct {
	uint32_t first_frame; // g_level.frame_num the client entered the game

	char user_info[MAX_USER_INFO_STRING];
	char net_name[MAX_NET_NAME];
	char sql_name[20];
	char skin[32];
	int16_t score;
	int16_t captures;

	int16_t health;
	int16_t max_health;

	int16_t armor;
	int16_t max_armor;

	int16_t inventory[MAX_ITEMS];

	// ammo capacities
	int16_t max_shells;
	int16_t max_bullets;
	int16_t max_grenades;
	int16_t max_rockets;
	int16_t max_cells;
	int16_t max_bolts;
	int16_t max_slugs;
	int16_t max_nukes;

	const g_item_t *weapon;
	const g_item_t *last_weapon;

	_Bool spectator; // client is a spectator
	_Bool ready; // ready

	g_team_t *team; // current team (good/evil)
	g_vote_t vote; // current vote (yes/no)
	uint32_t match_num; // most recent match
	uint32_t round_num; // most recent arena round
	int32_t color; // weapon effect colors
} g_client_persistent_t;

/*
 * @brief This structure is cleared on each spawn, with the persistent structure
 * explicitly copied over to preserve team membership, etc. This structure
 * extends the server-visible definition to provide all of the state management
 * the game module requires.
 */
typedef struct {
	user_cmd_t cmd;

	g_client_persistent_t persistent;

	_Bool show_scores; // sets layout bit mask in player state
	uint32_t scores_time; // eligible for scores when time > this

	uint16_t ammo_index;

	uint32_t buttons;
	uint32_t old_buttons;
	uint32_t latched_buttons;

	uint32_t weapon_think_time; // time when the weapon think was called
	uint32_t weapon_fire_time; // can fire when time > this
	const g_item_t *new_weapon;

	int16_t damage_armor; // damage absorbed by armor
	int16_t damage_health; // damage taken out of health
	int16_t damage_inflicted; // damage done to other clients

	vec_t speed; // x/y speed after moving
	vec3_t angles; // aiming direction
	vec3_t forward, right, up; // aiming direction vectors
	vec3_t cmd_angles; // angles sent over in the last command

	uint32_t respawn_time; // eligible for respawn when time > this
	uint32_t respawn_protection_time; // respawn protected till this time
	uint32_t ground_time; // last touched ground whence
	uint32_t drown_time; // eligible for drowning damage when time > this
	uint32_t sizzle_time; // eligible for sizzle damage when time > this
	uint32_t land_time; // eligible for landing event when time > this
	uint32_t jump_time; // eligible for jump when time > this
	uint32_t pain_time; // eligible for pain sound when time > this
	uint32_t footstep_time; // play a footstep when time > this
	uint32_t animation1_time; // eligible for animation update when time > this
	uint32_t animation2_time; // eligible for animation update when time > this

	uint32_t pickup_msg_time; // display message until time > this

	uint32_t chat_time; // can chat when time > this
	_Bool muted;

	uint32_t quad_damage_time; // has quad when time < this
	uint32_t quad_attack_time; // play attack sound when time > this

	g_edict_t *chase_target; // player we are chasing
	g_edict_t *old_chase_target; // player we were chasing

	const g_item_t *last_dropped; // last dropped item, used for variable expansion
} g_client_locals_t;

/*
 * @brief Finally the g_edict_locals structure extends the server stub to
 * provide all of the state management the game module requires.
 */
typedef struct {
	uint32_t spawn_flags; // SF_ITEM_HOVER, etc..
	uint32_t flags; // FL_GOD_MODE, etc..

	g_move_type_t move_type;
	g_move_info_t move_info;

	uint32_t timestamp;

	char *target;
	char *target_name;
	char *path_target;
	char *kill_target;
	char *message;
	char *team;
	char *command;
	char *script;

	g_edict_t *target_ent;

	vec_t speed, accel, decel;
	vec3_t move_dir;
	vec3_t pos1, pos2;

	vec3_t velocity;
	vec3_t avelocity;

	vec_t mass;

	uint32_t next_think;
	void (*Think)(g_edict_t *self);
	void (*Blocked)(g_edict_t *self, g_edict_t *other); // move to move_info?
	void (*Touch)(g_edict_t *self, g_edict_t *other, c_bsp_plane_t *plane, c_bsp_surface_t *surf);
	void (*Use)(g_edict_t *self, g_edict_t *other, g_edict_t *activator);
	void (*Pain)(g_edict_t *self, g_edict_t *other, int32_t damage, int32_t knockback);
	void (*Die)(g_edict_t *self, g_edict_t *inflictor, g_edict_t *attacker, int32_t damage,
			vec3_t point);

	uint32_t touch_time;
	uint32_t push_time;

	int16_t health;
	int16_t max_health;
	_Bool dead;

	_Bool take_damage;
	int16_t dmg;
	int16_t knockback;
	vec_t dmg_radius;
	int16_t sounds; // make this a spawntemp var?
	int32_t count;

	g_edict_t *chain;
	g_edict_t *enemy;
	g_edict_t *activator;
	g_edict_t *ground_entity;
	uint32_t ground_entity_link_count;
	g_edict_t *team_chain;
	g_edict_t *team_master;
	g_edict_t *lightning;

	uint16_t noise_index;
	int16_t attenuation;

	// timing variables
	vec_t wait;
	vec_t delay; // before firing targets
	vec_t random;

	int32_t water_type;
	uint8_t old_water_level;
	uint8_t water_level;

	int32_t area_portal; // the area portal to toggle

	const g_item_t *item; // for bonus items

	c_bsp_plane_t plane; // last touched
	c_bsp_surface_t *surf;

	vec3_t map_origin; // where the map says we spawn
} g_edict_locals_t;

#include "game/game.h"

#endif /* __GAME_LOCAL_H__ */

#endif /* __GAME_TYPES_H__ */
