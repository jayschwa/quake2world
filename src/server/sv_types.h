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

#ifndef __SV_TYPES_H__
#define __SV_TYPES_H__

#ifdef __SV_LOCAL_H__

typedef enum {
	SV_UNINITIALIZED, // no level loaded
	SV_LOADING, // spawning level edicts
	SV_ACTIVE_GAME, // actively running
	SV_ACTIVE_DEMO
} sv_state_t;

/*
 * @brief The sv_server_t struct is wiped at each level load.
 */
typedef struct {
	sv_state_t state; // precache commands are only valid during load

	uint32_t time; // always sv.frame_num * 1000 / sv_packetrate->value
	int32_t frame_num;

	char name[MAX_QPATH]; // map name
	struct c_model_s *models[MAX_BSP_MODELS];

	char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];
	entity_state_t baselines[MAX_EDICTS];

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until Sv_Multicast is called
	mem_buf_t multicast;
	byte multicast_buffer[MAX_MSG_SIZE];

	// demo server information
	file_t *demo_file;
} sv_server_t;

typedef enum {
	SV_CLIENT_FREE, // can be used for a new connection
	SV_CLIENT_CONNECTED, // client is connecting, but has not yet spawned
	SV_CLIENT_ACTIVE // client is spawned
} sv_client_state_t;

typedef struct {
	int32_t area_bytes;
	byte area_bits[MAX_BSP_AREAS >> 3]; // portal area visibility bits
	player_state_t ps;
	uint16_t num_entities;
	uint32_t first_entity; // index into svs.entity_states array
	uint32_t sent_time; // for ping calculations
} sv_frame_t;

#define CLIENT_LATENCY_COUNTS 16  // frame latency, averaged to determine ping
#define CLIENT_RATE_MESSAGES 10  // message size, used to enforce rate throttle

/*
 * @brief User movement command duration is inspected regularly to ensure that
 * they are not cheating. If their movement is too far out of sync with the
 * server's clock, we take notice and eventually kick them.
 */
#define CMD_MSEC_CHECK_INTERVAL 1000
#define CMD_MSEC_ALLOWABLE_DRIFT CMD_MSEC_CHECK_INTERVAL + 150
#define CMD_MSEC_MAX_DRIFT_ERRORS 10

typedef struct {
	byte *buffer;
	int32_t size;
	int32_t count;
} sv_download_t;

/*
 * The absolute maximum size of a frame (packet entities). Large frames are
 * buffered and packetized for network transmission in smaller envelopes. See
 * MAX_MSG_SIZE.
 */
#define MAX_FRAME_SIZE 8192

typedef struct {
	size_t offset;
	size_t len;
} sv_client_message_t;

/*
 * @brief A datagram structure that maintains individual message offsets so
 * that it may be safely fragmented for delivery.
 */
typedef struct {
	mem_buf_t buffer; // the managed size buffer
	byte data[MAX_FRAME_SIZE]; // the raw message buffer
	GList *messages; // message segmentation
} sv_client_datagram_t;

/*
 * @brief Per-client accounting for protocol flow control and low-level
 * connection state management.
 */
typedef struct {
	sv_client_state_t state;

	char user_info[MAX_USER_INFO_STRING]; // name, skin, etc

	int32_t last_frame; // for delta compression
	user_cmd_t last_cmd; // for filling in big drops

	uint32_t cmd_msec; // for sv_enforce_time
	uint16_t cmd_msec_errors; // maintain how many problems we've seen

	uint32_t frame_latency[CLIENT_LATENCY_COUNTS]; // used to calculate ping

	uint32_t message_size[CLIENT_RATE_MESSAGES]; // used to rate drop packets
	uint32_t rate;
	uint32_t surpress_count; // number of messages rate suppressed

	g_edict_t *edict; // EDICT_FOR_NUM(client_num + 1)
	char name[32]; // extracted from user_info, high bits masked
	int32_t message_level; // for filtering printed messages

	// the datagram is written to by sound calls, prints, temp ents, etc.
	// it can be overflowed without consequence.
	// it is packetized and written to the client, then wiped, each frame.
	sv_client_datagram_t datagram;

	sv_frame_t frames[PACKET_BACKUP]; // updates can be delta'd from here

	sv_download_t download; // UDP file downloads

	uint32_t last_message; // svs.real_time when packet was last received
	net_chan_t net_chan;
} sv_client_t;

/*
 * @brief The server runs at fixed-interval frames at a configurable rate.
 */
#define SV_HZ_MIN 10
#define SV_HZ_MAX 120

/*
 * @brief The default server frame rate.
 */
#define SV_HZ 30

/*
 * @brief Clients are dropped after 60 seconds without receiving a packet.
 */
#define SV_TIMEOUT 60

#define MAX_MASTERS	8  // max recipients for heartbeat packets
// challenges are a request for a connection; a handshake the client receives
// and must then re-use to acquire a client slot
typedef struct {
	net_addr_t addr;
	uint32_t challenge;
	uint32_t time;
} sv_challenge_t;

/*
 * @brief MAX_CHALLENGES is large to prevent a denial of service attack that
 * could cycle all of them out before legitimate users connected.
 */
#define MAX_CHALLENGES 1024

/*
 * @brief The sv_static_t structure is persistent for the execution of the
 * game. It is only cleared when Sv_Init is called. It is not exposed to the
 * game module.
 */
typedef struct {
	_Bool initialized; // sv_init has completed
	uint32_t real_time; // always increasing, no clamping, etc

	uint32_t spawn_count; // incremented each level start, used to check late spawns

	uint16_t frame_rate; // configurable server frame rate (sv_hz)

	sv_client_t *clients; // server-side client structures

	// the server maintains an array of entity states it uses to calculate
	// delta compression from frame to frame

	// the size of this array is based on the number of clients we might be
	// asked to support at any point in time during the current game

	uint32_t num_entity_states; // sv_max_clients->integer * UPDATE_BACKUP * MAX_PACKET_ENTITIES
	uint32_t next_entity_state; // next entity_state to use for newly spawned entities
	entity_state_t *entity_states; // entity states array used for delta compression

	net_addr_t masters[MAX_MASTERS];
	uint32_t next_heartbeat;

	sv_challenge_t challenges[MAX_CHALLENGES]; // to prevent invalid IPs from connecting

	g_export_t *game;
} sv_static_t;

// macros for resolving game entities on the server
#define EDICT_FOR_NUM(n)( (g_edict_t *)((char *) svs.game->edicts + svs.game->edict_size * (n)) )
#define NUM_FOR_EDICT(e)( ((char *)(e) - (char *) svs.game->edicts) / svs.game->edict_size )

#endif /* __SV_LOCAL_H__ */

#endif /* __SV_TYPES_H__ */
