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

#include "sv_local.h"

/*
 * @brief Searches sv.config_strings from the specified start, searching for the
 * desired name. If not found, the name can be optionally created and sent to
 * all connected clients. This allows the game to lazily load assets.
 */
static uint16_t Sv_FindIndex(const char *name, uint16_t start, uint16_t max, _Bool create) {
	uint16_t i;

	if (!name || !name[0])
		return 0;

	for (i = 0; i < max && sv.config_strings[start + i][0]; i++)
		if (!g_strcmp0(sv.config_strings[start + i], name))
			return i;

	if (!create)
		return 0;

	if (i == max) {
		Com_Warn("Max index for %s reached\n", name);
		return 0;
	}

	g_strlcpy(sv.config_strings[start + i], name, sizeof(sv.config_strings[i]));

	if (sv.state != SV_LOADING) { // send the update to everyone
		Mem_ClearBuffer(&sv.multicast);
		Net_WriteByte(&sv.multicast, SV_CMD_CONFIG_STRING);
		Net_WriteShort(&sv.multicast, start + i);
		Net_WriteString(&sv.multicast, name);
		Sv_Multicast(vec3_origin, MULTICAST_ALL_R);
	}

	return i;
}

uint16_t Sv_ModelIndex(const char *name) {
	return Sv_FindIndex(name, CS_MODELS, MAX_MODELS, true);
}

uint16_t Sv_SoundIndex(const char *name) {
	return Sv_FindIndex(name, CS_SOUNDS, MAX_SOUNDS, true);
}

uint16_t Sv_ImageIndex(const char *name) {
	return Sv_FindIndex(name, CS_IMAGES, MAX_IMAGES, true);
}

/*
 * @brief Entity baselines are used to compress the update messages
 * to the clients -- only the fields that differ from the
 * baseline will be transmitted
 */
static void Sv_CreateBaseline(void) {
	g_edict_t *ent;
	uint32_t i;

	for (i = 1; i < svs.game->num_edicts; i++) {

		ent = EDICT_FOR_NUM(i);

		if (!ent->in_use)
			continue;

		if (!ent->s.model1 && !ent->s.sound && !ent->s.effects)
			continue;

		ent->s.number = i;

		// take current state as baseline
		VectorCopy(ent->s.origin, ent->s.old_origin);
		sv.baselines[i] = ent->s;
	}
}

/*
 * @brief Sends the shutdown message message to all connected clients. The message
 * is sent immediately, because the server could completely terminate after
 * returning from this function.
 */
static void Sv_ShutdownMessage(const char *msg, _Bool reconnect) {
	sv_client_t * cl;
	int32_t i;

	if (!svs.initialized)
		return;

	Mem_ClearBuffer(&net_message);

	if (msg) { // send message
		Net_WriteByte(&net_message, SV_CMD_PRINT);
		Net_WriteByte(&net_message, PRINT_HIGH);
		Net_WriteString(&net_message, msg);
	}

	if (reconnect) // send reconnect
		Net_WriteByte(&net_message, SV_CMD_RECONNECT);
	else
		// or just disconnect
		Net_WriteByte(&net_message, SV_CMD_DISCONNECT);

	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++)
		if (cl->state >= SV_CLIENT_CONNECTED)
			Netchan_Transmit(&cl->net_chan, net_message.data, net_message.size);
}

/*
 * @brief Wipes the sv_server_t structure after freeing any references it holds.
 */
static void Sv_ClearState() {

	if (svs.initialized) { // if we were intialized, cleanup

		if (sv.demo_file) {
			Fs_Close(sv.demo_file);
		}
	}

	memset(&sv, 0, sizeof(sv));
	Com_QuitSubsystem(Q2W_SERVER);

	svs.real_time = 0;
	svs.next_heartbeat = 0;
}

/*
 * @brief Applies any pending variable changes and clamps ones we really care about.
 */
static void Sv_UpdateLatchedVars(void) {
	extern _Bool c_no_areas;

	Cvar_UpdateLatched();

	sv_max_clients->integer = Clamp(sv_max_clients->integer, MIN_CLIENTS, MAX_CLIENTS);

	sv_hz->integer = Clamp(sv_hz->integer, SV_HZ_MIN, SV_HZ_MAX);

	c_no_areas = sv_no_areas->integer;
}

/*
 * @brief Gracefully frees all resources allocated to svs.clients.
 */
static void Sv_ShutdownClients(void) {
	sv_client_t * cl;
	int32_t i;

	if (!svs.initialized)
		return;

	for (i = 0, cl = svs.clients; i < sv_max_clients->integer; i++, cl++) {

		if (cl->download.buffer) {
			Fs_Free(cl->download.buffer);
		}
	}

	Mem_Free(svs.clients);
	svs.clients = NULL;

	Mem_Free(svs.entity_states);
	svs.entity_states = NULL;
}

/*
 * @brief Reloads svs.clients, svs.client_entities, the game programs, etc. Because
 * we must allocate clients and edicts based on sizes the game module requests,
 * we refresh the game module
 */
static void Sv_InitClients(void) {
	int32_t i;

	if (!svs.initialized || Cvar_PendingLatched()) {

		Sv_ShutdownGame();

		Sv_ShutdownClients();

		Sv_UpdateLatchedVars();

		// initialize the clients array
		svs.clients = Mem_TagMalloc(sizeof(sv_client_t) * sv_max_clients->integer, MEM_TAG_SERVER);

		// and the entity states array
		svs.num_entity_states = sv_max_clients->integer * PACKET_BACKUP * MAX_PACKET_ENTITIES;
		svs.entity_states = Mem_TagMalloc(sizeof(entity_state_t) * svs.num_entity_states,
				MEM_TAG_SERVER);

		svs.frame_rate = sv_hz->integer;

		svs.spawn_count = Random();

		Sv_InitGame();
	} else {
		svs.spawn_count++;
	}

	// align the game entities with the server's clients
	for (i = 0; i < sv_max_clients->integer; i++) {

		g_edict_t *edict = EDICT_FOR_NUM(i + 1);
		edict->s.number = i + 1;

		// assign their edict
		svs.clients[i].edict = edict;

		// reset state of spawned clients back to connected
		if (svs.clients[i].state > SV_CLIENT_CONNECTED)
			svs.clients[i].state = SV_CLIENT_CONNECTED;

		// invalidate last frame to force a baseline
		svs.clients[i].last_frame = -1;
		svs.clients[i].last_message = svs.real_time;
	}
}

/*
 * @brief Loads the map or demo file and populates the server-controlled "config
 * strings."  We hand off the entity string to the game module, which will
 * load the rest.
 */
static void Sv_LoadMedia(const char *server, sv_state_t state) {
	int32_t i, map_size;

	strcpy(sv.name, server);
	strcpy(sv.config_strings[CS_NAME], server);

	if (state == SV_ACTIVE_DEMO) { // loading a demo
		sv.models[0] = Cm_LoadBsp(NULL, &map_size);

		sv.demo_file = Fs_OpenRead(va("demos/%s.dem", sv.name));
		svs.spawn_count = 0;

		Com_Print("  Loaded demo %s.\n", sv.name);
	} else { // loading a map
		g_snprintf(sv.config_strings[CS_MODELS], MAX_QPATH, "maps/%s.bsp", sv.name);

		sv.models[0] = Cm_LoadBsp(sv.config_strings[CS_MODELS], &map_size);

		const char *dir = Fs_RealDir(sv.config_strings[CS_MODELS]);
		if (g_str_has_suffix(dir, ".zip")) {
			g_strlcpy(sv.config_strings[CS_ZIP], Basename(dir), MAX_QPATH);
		}

		for (i = 1; i < Cm_NumModels(); i++) {

			char *s = sv.config_strings[CS_MODELS + i];
			g_snprintf(s, MAX_QPATH, "*%d", i);

			sv.models[i] = Cm_Model(s);
		}

		sv.state = SV_LOADING;

		Sv_InitWorld();

		svs.game->SpawnEntities(sv.name, Cm_EntityString());

		/*
		 * Run a few game frames for entities to settle down. Failure to do
		 * this will cause the entities to produce all but useless baselines,
		 * which will in turn blow out the packet entities in Sv_EmitEntities.
		 */

		for (i = 0; i < 3; i++) {
			svs.game->Frame();
		}

		Sv_CreateBaseline();

		Com_Print("  Loaded map %s, %d entities.\n", sv.name, svs.game->num_edicts);
	}
	g_snprintf(sv.config_strings[CS_BSP_SIZE], MAX_QPATH, "%i", map_size);

	Cvar_FullSet("map_name", sv.name, CVAR_SERVER_INFO | CVAR_NO_SET);
}

/*
 * @brief Entry point for spawning a new server or changing maps / demos. Brings any
 * connected clients along for the ride by broadcasting a reconnect before
 * clearing state. Special effort is made to ensure that a locally connected
 * client sees the reconnect message immediately.
 */
void Sv_InitServer(const char *server, sv_state_t state) {
#ifdef BUILD_CLIENT
	extern void Cl_Disconnect(void);
#endif
	char path[MAX_QPATH];

	Com_Debug("Sv_InitServer: %s (%d)\n", server, state);

	Cbuf_CopyToDefer();

	// ensure that the requested map or demo exists
	if (state == SV_ACTIVE_DEMO)
		g_snprintf(path, sizeof(path), "demos/%s.dem", server);
	else
		g_snprintf(path, sizeof(path), "maps/%s.bsp", server);

	if (!Fs_Exists(path)) {
		Com_Print("Couldn't open %s\n", path);
		return;
	}

	// inform any connected clients to reconnect to us
	Sv_ShutdownMessage("Server restarting...\n", true);

#ifdef BUILD_CLIENT
	// disconnect any local client, they'll immediately reconnect
	Cl_Disconnect();
#endif

	// clear the sv_server_t structure
	Sv_ClearState();

	Com_Print("Server initialization...\n");

	Mem_InitBuffer(&sv.multicast, sv.multicast_buffer, sizeof(sv.multicast_buffer));

	// initialize the clients, loading the game module if we need it
	Sv_InitClients();

	// load the map or demo and related media
	Sv_LoadMedia(server, state);
	sv.state = state;

	Com_Print("Server initialized\n");
	Com_InitSubsystem(Q2W_SERVER);

	svs.initialized = true;
}

/*
 * @brief Called when the game is shutting down.
 */
void Sv_ShutdownServer(const char *msg) {

	Com_Debug("Sv_ShutdownServer: %s\n", msg);

	Com_Print("Server shutdown...\n");

	Sv_ShutdownMessage(msg, false);

	Sv_ShutdownGame();

	Sv_ShutdownClients();

	Sv_ClearState();

	Com_Print("Server down\n");

	svs.initialized = false;
}
