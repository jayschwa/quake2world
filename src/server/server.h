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

#include "cvar.h"
#include "cmd.h"
#include "common.h"
#include "console.h"
#include "cmodel.h"
#include "game/game.h"
#include "net.h"
#include "pmove.h"
#include "sys.h"

typedef enum {
	ss_dead,   // no level loaded
	ss_loading,   // spawning level edicts
	ss_game,   // actively running
	ss_demo
} sv_state_t;

typedef struct sv_server_s {
	sv_state_t state;  // precache commands are only valid during load

	int time;  // always sv.frame_num * 1000 / sv_packetrate->value
	int frame_num;

	char name[MAX_QPATH];  // map name
	struct c_model_s *models[MAX_MODELS];

	char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];
	entity_state_t baselines[MAX_EDICTS];

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until Sv_Multicast is called
	size_buf_t multicast;
	byte multicast_buffer[MAX_MSGLEN];

	// demo server information
	FILE *demo_file;
} sv_server_t;

extern sv_server_t sv;  // local server

typedef enum {
	cs_free,   // can be used for a new connection
	cs_connected,   // client is connecting, but has not yet spawned
	cs_spawned  // client is spawned
} sv_client_state_t;

typedef struct sv_frame_s {
	int area_bytes;
	byte area_bits[MAX_BSP_AREAS / 8];  // portal area visibility bits
	player_state_t ps;
	int num_entities;
	int first_entity;  // index into svs.entity_states array
	int sent_time;  // for ping calculations
} sv_frame_t;

#define CLIENT_LATENCY_COUNTS 16  // frame latency, averaged to determine ping
#define CLIENT_RATE_MESSAGES 10  // message size, used to enforce rate throttle

/*
 * We check users movement command duration every so often to ensure that
 * they are not cheating.  If their movement is too far out of sync with the
 * server's clock, we take notice and eventually kick them.
 */

#define CMD_MSEC_CHECK_INTERVAL 1000
#define CMD_MSEC_ALLOWABLE_DRIFT  150
#define CMD_MSEC_MAX_DRIFT_ERRORS  10

typedef struct sv_client_s {
	sv_client_state_t state;

	char user_info[MAX_INFO_STRING];  // name, skin, etc

	int last_frame;  // for delta compression
	user_cmd_t last_cmd;  // for filling in big drops

	int cmd_msec;  // for sv_enforcetime
	int cmd_msec_errors;  // maintain how many problems we've seen

	int frame_latency[CLIENT_LATENCY_COUNTS];
	int ping;

	int message_size[CLIENT_RATE_MESSAGES];  // used to rate drop packets
	int rate;
	int surpress_count;  // number of messages rate suppressed

	edict_t *edict;  // EDICT_FOR_NUM(client_num + 1)
	char name[32];  // extracted from user_info, high bits masked
	int message_level;  // for filtering printed messages

	// the datagram is written to by sound calls, prints, temp ents, etc.
	// it can be overflowed without consequence.
	size_buf_t datagram;
	byte datagram_buf[MAX_MSGLEN];

	sv_frame_t frames[UPDATE_BACKUP];  // updates can be delta'd from here

	byte *download;  // file being downloaded
	int download_size;  // total bytes (can't use EOF because of paks)
	int download_count;  // bytes sent

	int last_message;  // svs.real_time when packet was last received

	qboolean recording;  // client is currently recording a demo

	netchan_t netchan;
} sv_client_t;

// the server runs fixed-interval frames at a configurable rate (Hz)
#define SERVER_FRAME_RATE_MIN 20
#define SERVER_FRAME_RATE_MAX 120
#define SERVER_FRAME_RATE 30

// clients will be dropped after no activity in so many seconds
#define SERVER_TIMEOUT 30

#define MAX_MASTERS	8  // max recipients for heartbeat packets

// challenges are a request for a connection; a handshake the client receives
// and must then re-use to acquire a client slot
typedef struct sv_challenge_s {
	netaddr_t addr;
	int challenge;
	int time;
} sv_challenge_t;

// MAX_CHALLENGES is made large to prevent a denial of service attack that
// could cycle all of them out before legitimate users connected.
#define MAX_CHALLENGES 1024

typedef struct sv_static_s {
	qboolean initialized;  // sv_init has completed
	int real_time;  // always increasing, no clamping, etc

	int spawn_count;  // incremented each level start, used to check late spawns

	int frame_rate;  // configurable server frame rate

	sv_client_t *clients;  // server-side client structures

	// the server maintains an array of entity states it uses to calculate
	// delta compression from frame to frame

	// the size of this array is based on the number of clients we might be
	// asked to support at any point in time during the current game

	int num_entity_states;  // sv_max_clients->integer * UPDATE_BACKUP * MAX_PACKET_ENTITIES
	int next_entity_state;  // next entity_state to use for newly spawned entities
	entity_state_t *entity_states;  // entity states array used for delta compression

	netaddr_t masters[MAX_MASTERS];
	int last_heartbeat;

	sv_challenge_t challenges[MAX_CHALLENGES];  // to prevent invalid IPs from connecting

	g_export_t *game;
} sv_static_t;

extern sv_static_t svs;  // persistent server info

// macros for resolving game entities on the server
#define EDICT_FOR_NUM(n)( (edict_t *)((void *)svs.game->edicts + svs.game->edict_size * (n)) )
#define NUM_FOR_EDICT(e)( ((void *)(e) - (void *)svs.game->edicts) / svs.game->edict_size )

// cvars
extern cvar_t *sv_rcon_password;
extern cvar_t *sv_download_url;
extern cvar_t *sv_enforce_time;
extern cvar_t *sv_hostname;
extern cvar_t *sv_max_clients;
extern cvar_t *sv_framerate;
extern cvar_t *sv_public;
extern cvar_t *sv_timeout;
extern cvar_t *sv_udp_download;

// current client / player edict
extern sv_client_t *sv_client;
extern edict_t *sv_player;

// sv_main.c
void Sv_Init(void);
void Sv_Shutdown(const char *msg);
void Sv_Frame(int msec);
char *Sv_NetaddrToString(sv_client_t *cl);
void Sv_KickClient(sv_client_t *cl, const char *msg);
void Sv_DropClient(sv_client_t *cl);
void Sv_UserInfoChanged(sv_client_t *cl);

// sv_init.c
int Sv_ModelIndex(const char *name);
int Sv_SoundIndex(const char *name);
int Sv_ImageIndex(const char *name);
void Sv_ShutdownServer(const char *msg);
void Sv_InitServer(const char *name, sv_state_t state);

// sv_send.c
typedef enum {
	RD_NONE,
	RD_CLIENT,
	RD_PACKET
} sv_redirect_t;

#define SV_OUTPUTBUF_LENGTH	(MAX_MSGLEN - 16)
extern char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void Sv_FlushRedirect(int target, char *outputbuf);
void Sv_SendClientMessages(void);
void Sv_Unicast(edict_t *ent, qboolean reliable);
void Sv_Multicast(vec3_t origin, multicast_t to);
void Sv_PositionedSound(vec3_t origin, edict_t *entity, int soundindex, int atten);
void Sv_ClientPrint(edict_t *ent, int level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Sv_ClientCenterPrint(edict_t *ent, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Sv_BroadcastPrint(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Sv_BroadcastCommand(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// sv_user.c
void Sv_ParseClientMessage(sv_client_t *cl);

// sv_ccmds.c
void Sv_InitOperatorCommands(void);

// sv_ents.c
void Sv_WriteFrameToClient(sv_client_t *client, size_buf_t *msg);
void Sv_BuildClientFrame(sv_client_t *client);

// sv_game.c
void Sv_InitGameProgs(void);
void Sv_ShutdownGameProgs(void);

// sv_world.c
void Sv_InitWorld(void);
// called after the world model has been loaded, before linking any entities

void Sv_UnlinkEdict(edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void Sv_LinkEdict(edict_t *ent);
// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->v.abs_mins and ent->v.abs_maxs
// sets ent->leaf_nums[] for pvs determination even if the entity
// is not solid

int Sv_AreaEdicts(vec3_t mins, vec3_t maxs, edict_t **list, int maxcount, int areatype);
// fills in a table of edict pointers with edicts that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// ??? does this always return the world?


// functions that interact with everything apropriate
int Sv_PointContents(vec3_t p);
// returns the CONTENTS_* value from the world at the given point.
// Quake 2 extends this to also check entities, to allow moving liquids


trace_t Sv_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask);
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.all_solid will be set,
// trace.start_solid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passedict is explicitly excluded from clipping checks(normally NULL)

